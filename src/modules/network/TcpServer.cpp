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
#include <sys/ioctl.h>   // TIOCOUTQ：查内核发送队列已占用字节
#include <sys/uio.h>     // sendmsg / iovec：头+负载一次发出

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
    // keepalive 放宽到 ~25s 才判死：弱网/短暂拥塞不误杀活连接（原 ~11s 过于激进）。
    // 持续有小帧发送时连接不 idle，keepalive 基本不触发；仅真静默时兜底检测死连接。
    int idle = 10, intvl = 3, cnt = 5;
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE,  &idle,  sizeof(idle));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &intvl, sizeof(intvl));
    ::setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT,   &cnt,   sizeof(cnt));
#endif
    // 明确的发送缓冲：让 sendFrameDroppable 的"整帧放得下才发"判定有稳定依据。
    int sndbuf = 256 * 1024;
    ::setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));
    // 发送超时缩到 1s：仅作兜底（正常路径靠拥塞判定丢帧，不会走到阻塞超时）。
    timeval snd{};
    snd.tv_sec = 1;
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

// ★可丢弃发送：整帧发出 / 拥塞时整帧丢弃 / 真错误。核心=弱网下链路不因拥塞而断。
TcpServer::SendStatus TcpServer::sendFrameDroppable(int fd, uint8_t type,
                                                    const uint8_t* payload, size_t len) {
    const size_t total = static_cast<size_t>(net::LL_HEADER_SIZE) + len;

    // 1) 拥塞判定：整帧放不进内核发送缓冲剩余空间就【整帧丢弃】(绝不写一半损坏流)。
    //    小帧(传感器/状态<1KB)只需极小空间→拥塞时仍能发出；大帧(视频)需大空间→拥塞时被丢。
    //    这天然实现"弱网丢视频、保实时状态"，且链路始终不断。
    int outq = 0;
    if (::ioctl(fd, TIOCOUTQ, &outq) == 0) {
        int sndbuf = 0; socklen_t sl = sizeof(sndbuf);
        if (::getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &sl) == 0 && sndbuf > 0) {
            // Linux 的 SO_SNDBUF 读回值约为实际 2 倍；留 1/4 余量避免边界部分写
            if (static_cast<int>(total) > sndbuf * 3 / 4 - outq)
                return SendStatus::Dropped;
        }
    }

    // 2) 头+负载一次 sendmsg 发出（空间已确认→立即全量拷入内核并返回）。
    uint8_t hdr[net::LL_HEADER_SIZE];
    net::buildHeader(type, static_cast<uint32_t>(len), hdr);
    iovec iov[2];
    iov[0].iov_base = hdr;                            iov[0].iov_len = sizeof(hdr);
    iov[1].iov_base = const_cast<uint8_t*>(payload);  iov[1].iov_len = len;
    msghdr msg{};
    msg.msg_iov    = iov;
    msg.msg_iovlen = (len > 0) ? 2 : 1;

    ssize_t n = ::sendmsg(fd, &msg, MSG_NOSIGNAL);
    if (n == static_cast<ssize_t>(total)) return SendStatus::Sent;
    if (n < 0) {
        // SO_SNDTIMEO 兜底超时(对端假死缓冲满) → 丢弃本帧但保持连接
        if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
            return SendStatus::Dropped;
        return SendStatus::Error;   // EPIPE/ECONNRESET 等 → 对端真断了
    }
    // 3) 罕见部分写(空间已查基本不会发生)：剩余有限阻塞补齐，保证帧不被截断。
    std::vector<uint8_t> whole;
    whole.reserve(total);
    whole.insert(whole.end(), hdr, hdr + sizeof(hdr));
    if (len > 0) whole.insert(whole.end(), payload, payload + len);
    if (!sendAll(fd, whole.data() + n, total - static_cast<size_t>(n)))
        return SendStatus::Error;
    return SendStatus::Sent;
}

TcpServer::SendStatus TcpServer::sendTextDroppable(int fd, const std::string& text) {
    return sendFrameDroppable(fd, net::FRAME_TEXT,
                              reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

int TcpServer::pollCommands(int fd, std::vector<uint8_t>& buf,
                            std::vector<net::Command>& out,
                            std::vector<net::DriveCommand>& drives,
                            std::vector<net::VisionResult>& visions,
                            std::vector<net::PidCommand>& pids) {
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
        } else if (type == net::FRAME_PID_CMD && len >= 1) {
            const uint8_t* p = buf.data() + off + net::LL_HEADER_SIZE;
            auto rdU16le = [&](int i) { return uint16_t(p[i]) | (uint16_t(p[i+1]) << 8); };
            auto rdU32le = [&](int i) {
                return uint32_t(p[i]) | (uint32_t(p[i+1]) << 8) |
                       (uint32_t(p[i+2]) << 16) | (uint32_t(p[i+3]) << 24);
            };
            net::PidCommand pc;
            pc.sub = p[0];
            if (pc.sub == 1 && len >= 1 + 15) {
                // 参数: kp/ki/kd_x1000 u32×3 | max_delta u16 | flags u8
                pc.kp = rdU32le(1)  / 1000.0f;
                pc.ki = rdU32le(5)  / 1000.0f;
                pc.kd = rdU32le(9)  / 1000.0f;
                pc.maxDelta   = rdU16le(13);
                pc.closedLoop = (p[15] & 0x01) != 0;
                pids.push_back(pc);
            } else if (pc.sub == 2 && len >= 1 + 7) {
                // 测试: mode u8 | left i16 | right i16 | duration_ms u16
                pc.testMode   = p[1];
                pc.left       = static_cast<int16_t>(rdU16le(2));
                pc.right      = static_cast<int16_t>(rdU16le(4));
                pc.durationMs = rdU16le(6);
                pids.push_back(pc);
            }
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
