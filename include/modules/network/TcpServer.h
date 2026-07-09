#ifndef PATROL_MODULES_NETWORK_TCPSERVER_H
#define PATROL_MODULES_NETWORK_TCPSERVER_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

#include "modules/network/FrameProtocol.h"

// =============================================================================
//  TcpServer — 龙芯端 TCP 服务端（与上位机 LongLook 约定：龙芯为服务端）
//
//  当前阶段（链路测试）只服务单个客户端：accept 到 LongLook 后，
//  循环推送视频帧；客户端断开后回到 accept 等待重连。
//
//  发送均为阻塞「全量写」，并屏蔽 SIGPIPE（对端断开时返回错误而非杀进程）。
// =============================================================================

namespace patrol {

class TcpServer {
public:
    TcpServer() = default;
    ~TcpServer();

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // 在指定端口监听（bindAddr 默认 0.0.0.0，监听所有网卡）
    bool listen(uint16_t port, const std::string& bindAddr = "0.0.0.0");

    // 关闭监听套接字
    void close();

    bool isListening() const { return listenFd_ >= 0; }

    // 阻塞等待客户端连接，最多 timeoutMs 毫秒。
    //   成功返回 >=0 的客户端 fd；超时返回 -1；出错返回 -2。
    int acceptClient(int timeoutMs, std::string* peerOut = nullptr);

    // ---- 静态收发工具（操作具体客户端 fd） ----

    // 全量发送，处理部分写。成功返回 true。
    static bool sendAll(int fd, const uint8_t* data, size_t len);

    // 按帧协议发送一帧（头+负载）。成功返回 true。
    static bool sendFrame(int fd, uint8_t type, const uint8_t* payload, size_t len);

    // 发送一段 UTF-8 文本作为 FRAME_TEXT
    static bool sendText(int fd, const std::string& text);

    // 发送结果三态：区分"发出/因拥塞丢弃/真错误"。丢弃时保持连接。
    enum class SendStatus { Sent, Dropped, Error };
    // ★可丢弃发送：整帧要么完整发出，要么在内核发送缓冲不足时整帧丢弃
    //   （绝不阻塞、绝不断链、绝不把帧写一半损坏 TCP 流）。
    //   适用于视频/热成像/传感器/状态等周期帧——弱网下丢大帧保小帧，链路不因拥塞而断。
    static SendStatus sendFrameDroppable(int fd, uint8_t type, const uint8_t* payload, size_t len);
    // 文本状态行的可丢弃版本
    static SendStatus sendTextDroppable(int fd, const std::string& text);

    // 非阻塞读取并解析对端发来的下行帧：命令(0x40)→cmds，手动驱动(0x41)→drives，
    //   视觉(0x42)→visions，PID 调试(0x43)→pids。
    //   buf 为该连接的累积缓冲区（调用方持有，跨调用保留半包）。
    //   返回值：>=0 本次读到的字节数；-1 对端关闭；-2 读错误。
    static int pollCommands(int fd, std::vector<uint8_t>& buf,
                            std::vector<net::Command>& cmds,
                            std::vector<net::DriveCommand>& drives,
                            std::vector<net::VisionResult>& visions,
                            std::vector<net::PidCommand>& pids);

    static void closeClient(int fd);

private:
    // 配置已 accept 的客户端 socket：TCP_NODELAY + keepalive + 发送超时（连接稳定性）
    static void configureClientSocket(int fd);

    int listenFd_ = -1;
};

} // namespace patrol

#endif // PATROL_MODULES_NETWORK_TCPSERVER_H
