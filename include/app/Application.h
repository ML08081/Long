#ifndef PATROL_APP_APPLICATION_H
#define PATROL_APP_APPLICATION_H

#include <atomic>
#include <cstdint>
#include <string>

#include "modules/camera/CameraManager.h"
#include "modules/network/TcpServer.h"

// =============================================================================
//  Application — 应用主流程
//
//  当前阶段只做「视频链路测试」：
//     打开摄像头 → 启动 TCP 服务端 → 等上位机连接 → 循环推送视频帧。
//  其余业务（传感器采集、串口、运动控制等）留作后续扩展。
// =============================================================================

namespace patrol {

// 运行参数（可由命令行覆盖，见 main.cpp）
struct AppConfig {
    std::string device   = "/dev/video0";  // 摄像头设备节点
    int         width    = 1280;            // 期望分辨率宽
    int         height   = 720;             // 期望分辨率高
    int         fps      = 30;              // 期望帧率
    uint16_t    port     = 8080;            // TCP 监听端口（上位机默认连 8080）
    std::string bindAddr = "0.0.0.0";       // 监听地址
};

class Application {
public:
    explicit Application(const AppConfig& cfg) : cfg_(cfg) {}

    // 主循环；正常退出返回 0
    int run();

    // 请求停止（信号处理函数中调用，需异步信号安全：仅写 atomic）
    void requestStop() { running_.store(false); }

private:
    // 服务单个已连接客户端：循环推流，直到断开或收到停止请求
    void serveClient(int clientFd);

    AppConfig        cfg_;
    CameraManager    camera_;
    TcpServer        server_;
    std::atomic<bool> running_{true};
};

} // namespace patrol

#endif // PATROL_APP_APPLICATION_H
