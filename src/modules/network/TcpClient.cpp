#include "modules/network/TcpClient.h"
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

TcpClient::~TcpClient() { close(); }

bool TcpClient::connect(const std::string& host, uint16_t port, int timeoutMs) {
    close();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;

    // 非阻塞 connect（配合超时）
    int flags = ::fcntl(fd_, F_GETFL, 0);
    ::fcntl(fd_, F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        LOG_ERROR("TcpClient: 无效地址 %s", host.c_str());
        close(); return false;
    }

    int r = ::connect(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (r < 0 && errno != EINPROGRESS) { close(); return false; }

    fd_set wfds; FD_ZERO(&wfds); FD_SET(fd_, &wfds);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    r = ::select(fd_ + 1, nullptr, &wfds, nullptr, &tv);
    if (r <= 0) { close(); return false; }

    int err = 0; socklen_t errlen = sizeof(err);
    ::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &err, &errlen);
    if (err != 0) { close(); return false; }

    // 恢复阻塞，关闭 Nagle
    ::fcntl(fd_, F_SETFL, flags);
    int yes = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    LOG_INFO("TcpClient: 已连接 %s:%u", host.c_str(), port);
    return true;
}

void TcpClient::close() {
    if (fd_ >= 0) { ::close(fd_); fd_ = -1; }
}

bool TcpClient::sendAll(const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ::send(fd_, data + sent, len - sent, MSG_NOSIGNAL);
        if (n > 0) { sent += static_cast<size_t>(n); continue; }
        if (n < 0 && errno == EINTR) continue;
        return false;
    }
    return true;
}

int TcpClient::recv(uint8_t* buf, size_t bufLen, int timeoutMs) {
    fd_set rfds; FD_ZERO(&rfds); FD_SET(fd_, &rfds);
    timeval tv{timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    int r = ::select(fd_ + 1, &rfds, nullptr, nullptr, &tv);
    if (r == 0) return 0;
    if (r < 0)  return -2;
    ssize_t n = ::read(fd_, buf, bufLen);
    if (n == 0) return -1;
    if (n < 0)  return -2;
    return static_cast<int>(n);
}

} // namespace patrol