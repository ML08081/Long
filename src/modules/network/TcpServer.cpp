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
    // backlog 提到 4：LongLook 断线重连时，旧连接可能尚未被检测到（仍在 serveClient），
    // 新连接需要能进内核队列而不是被 RST 拒绝，避免"连接失败需重试几次"。
    if (::listen(listenFd_, 4) < 0) {
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

    configureClientSocket(fd);

    char ipstr[INET_ADDRSTRLEN] = {0};
    ::inet_ntop(AF_INET, &peer.sin_addr, ipstr, sizeof(ipstr));
    std::string peerDesc = std::string(ipstr) + ":" + std::to_string(ntohs(peer.sin_port));
    if (peerOut) *peerOut = peerDesc;
    LOG_INFO("上位机已连接: %s", peerDesc.c_str());
    return fd;
}

// 配置已 accept 的客户端 socket，核心解决"连接不稳定/重连失败"：
//   1) TCP_NODELAY：关闭 Nagle，降低视频/命令延迟。
//   2) SO_KEEPALIVE + 短探测周期：WiFi 下 LongLook 掉电/断网不会发 FIN，
//      服务端若无 keepalive 会一直卡在 serveClient(旧连接) 里推流，新连接进不来。
//      开启后 ~ (idle 5s + 3×2s) ≈ 11s 内探测到死连接并让 send 报错，serveClient 退出回到 accept。
//   3) SO_SNDTIMEO：对端假死时内核发送缓冲写满会永久阻塞 send；设 5s 超时让 sendAll 报错跳出，
//      不再让一个僵死连接独占服务端。
void TcpServer::configureClientSocket(int fd) {
    int yes = 1;
    ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY,  &yes, sizeof(yes));
    ::setsockopt(fd, SOL_SOCKET,  SO_KEEPALIVE, &yes, sizeof(yes));
#ifdef TCP_KEEPIDLE
    int idle = 5, intvl = 2, cnt = 3;
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
#endif
    timeval snd{};
    snd.tv_sec = 5;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &snd, sizeof(snd));
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
        // 配了 SO_SNDTIMEO：EAGAIN/EWOULDBLOCK = 发送超时（对端假死缓冲写满），
        // 视为失败跳出，让 serveClient 断开该僵死连接、回到 accept 迎接新连接。
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            LOG_WARN("发送超时，判定对端假死断开 (已发 %zu/%zu)", sent, len);
            return false;
        }
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
                            std::vector<net::DriveCommand>& drives,
                            std::vector<net::VisionResult>& visions) {
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
        } else if (type == net::FRAME_VISION && len >= 4) {
            const uint8_t* p = buf.data() + off + net::LL_HEADER_SIZE;
            net::VisionResult v;   // [count][maxConf][flags][nameLen][name...]
            v.count   = p[0];
            v.maxConf = p[1];
            v.flags   = p[2];
            uint8_t nameLen = p[3];
            if (4u + nameLen <= len)
                v.topClass.assign(reinterpret_cast<const char*>(p + 4), nameLen);
            visions.push_back(v);
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
