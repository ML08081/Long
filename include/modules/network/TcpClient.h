#ifndef PATROL_MODULES_NETWORK_TCPCLIENT_H
#define PATROL_MODULES_NETWORK_TCPCLIENT_H

#include <cstdint>
#include <cstddef>
#include <string>

// TCP 客户端（保留接口，用于主动连接外部服务，如 MQTT broker）
namespace patrol {

class TcpClient {
public:
    TcpClient() = default;
    ~TcpClient();

    TcpClient(const TcpClient&) = delete;
    TcpClient& operator=(const TcpClient&) = delete;

    bool connect(const std::string& host, uint16_t port, int timeoutMs = 3000);
    void close();
    bool isConnected() const { return fd_ >= 0; }

    bool sendAll(const uint8_t* data, size_t len);
    // 返回实际读到字节数；0=超时；-1=对端关闭；-2=错误
    int  recv(uint8_t* buf, size_t bufLen, int timeoutMs = 1000);

private:
    int fd_ = -1;
};

} // namespace patrol

#endif // PATROL_MODULES_NETWORK_TCPCLIENT_H