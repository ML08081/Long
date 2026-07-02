#include "modules/network/TcpServer.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

namespace patrol {

TcpServer::~TcpServer() {
    close();
}

bool TcpServer::listen(uint16_t port, const std::string& bindAddr) {
    close();

    listenFd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd_ < 0) {
        LOG_ERROR("创建监听套接字失败: %s", std::strerror(errno));
        return false;
    }

    int yes = 1;
    ::setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (bindAddr.empty() || bindAddr == "0.0.0.0") {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    } else if (::inet_pton(AF_INET, bindAddr.c_str(), &addr.sin_addr) != 1) {
        LOG_ERROR("无效的绑定地址: %s", bindAddr.c_str());
        close();
        return false;
    }

    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        LOG_ERROR("bind %s:%u 失败: %s", bindAddr.c_str(), port, std::strerror(errno));
        close();
        return false;
    }
    if (::listen(listenFd_, 1) < 0) {
        LOG_ERROR("listen 失败: %s", std::strerror(errno));
        close();
        return false;
    }

    LOG_INFO("TCP 服务端已监听 %s:%u，等待上位机 LongLook 连接...", bindAddr.c_str(), port);
    return true;
}

void TcpServer::close() {
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
}

int TcpServer::acceptClient(int timeoutMs, std::string* peerOut) {
    if (listenFd_ < 0) return -2;

    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(listenFd_, &fds);
    timeval tv{};
    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int r = ::select(listenFd_ + 1, &fds, nullptr, nullptr, &tv);
    if (r < 0) {
        if (errno == EINTR) return -1;     // 被信号打断，让上层检查退出标志
        LOG_ERROR("accept select 失败: %s", std::strerror(errno));
        return -2;
    }
    if (r == 0) return -1;                  // 超时，无连接

    sockaddr_in peer{};
    socklen_t   plen = sizeof(peer);
    int fd = ::accept(listenFd_, reinterpret_cast<sockaddr*>(&peer), &plen);
    if (fd < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) return -1;
        LOG_ERROR("accept 失败: %s", std::strerror(errno));
        return -2;
    }

    // 关闭 Nagle，降低视频帧延迟
    int yes = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));

    char ipstr[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &peer.sin_addr, ipstr, sizeof(ipstr));
    std::string peerDesc = std::string(ipstr) + ":" + std::to_string(ntohs(peer.sin_port));
    if (peerOut) *peerOut = peerDesc;
    LOG_INFO("上位机已连接: %s", peerDesc.c_str());
    return fd;
}

bool TcpServer::sendAll(int fd, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        // MSG_NOSIGNAL：对端断开时返回 EPIPE，而不是触发 SIGPIPE 杀进程
        ssize_t n = ::send(fd, data + sent, len - sent, MSG_NOSIGNAL);
        if (n > 0) {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && (errno == EINTR)) continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;  // 阻塞套接字一般不会到这
        LOG_WARN("发送失败 (已发 %zu/%zu): %s", sent, len, std::strerror(errno));
        return false;
    }
    return true;
}

bool TcpServer::sendFrame(int fd, uint8_t type, const uint8_t* payload, size_t len) {
    uint8_t hdr[net::LL_HEADER_SIZE];
    net::buildHeader(type, static_cast<uint32_t>(len), hdr);
    if (!sendAll(fd, hdr, sizeof(hdr))) return false;
    if (len > 0 && !sendAll(fd, payload, len)) return false;
    return true;
}

bool TcpServer::sendText(int fd, const std::string& text) {
    return sendFrame(fd, net::FRAME_TEXT,
                     reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

int TcpServer::pollCommands(int fd, std::vector<uint8_t>& buf,
                            std::vector<net::Command>& out,
                            std::vector<net::DriveCommand>& drives) {
    // 非阻塞读取所有可用数据
    uint8_t tmp[512];
    int total = 0;
    for (;;) {
        ssize_t n = ::recv(fd, tmp, sizeof(tmp), MSG_DONTWAIT);
        if (n > 0) {
            buf.insert(buf.end(), tmp, tmp + n);
            total += static_cast<int>(n);
            continue;
        }
        if (n == 0) return -1;                         // 对端关闭
        if (errno == EAGAIN || errno == EWOULDBLOCK) break;   // 暂无更多数据
        if (errno == EINTR) continue;
        return -2;                                     // 读错误
    }

    // 解析累积缓冲，抽取完整帧；这里只关心下行命令(0x40)
    size_t off = 0;
    while (buf.size() - off >= static_cast<size_t>(net::LL_HEADER_SIZE)) {
        // 同步到 SOF
        if (buf[off] != net::LL_SOF) { ++off; continue; }

        uint8_t  type = buf[off + 1];
        uint32_t len  = uint32_t(buf[off + 2])        | (uint32_t(buf[off + 3]) << 8)
                      | (uint32_t(buf[off + 4]) << 16) | (uint32_t(buf[off + 5]) << 24);
        if (len > net::LL_MAX_PAYLOAD) { ++off; continue; }   // 失步，丢一字节重找

        size_t need = static_cast<size_t>(net::LL_HEADER_SIZE) + len;
        if (buf.size() - off < need) break;            // 半包，等待更多数据

        if (type == net::FRAME_COMMAND && len >= 2) {
            net::Command c;
            c.cmdId = buf[off + net::LL_HEADER_SIZE + 0];
            c.value = buf[off + net::LL_HEADER_SIZE + 1];
            out.push_back(c);
        } else if (type == net::FRAME_DRIVE && len >= 4) {
            const uint8_t* p = buf.data() + off + net::LL_HEADER_SIZE;
            net::DriveCommand d;   // payload 小端：speed i16 | steering i16
            d.speed    = static_cast<int16_t>(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
            d.steering = static_cast<int16_t>(uint16_t(p[2]) | (uint16_t(p[3]) << 8));
            drives.push_back(d);
        }
        off += need;
    }
    // 丢弃已消费部分
    if (off > 0) buf.erase(buf.begin(), buf.begin() + off);
    return total;
}

void TcpServer::closeClient(int fd) {
    if (fd >= 0) ::close(fd);
}

} // namespace patrol
