#ifndef PATROL_APP_APPLICATION_H
#define PATROL_APP_APPLICATION_H

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>

#include "modules/camera/CameraManager.h"
#include "modules/network/TcpServer.h"
#include "business/RobotController.h"

// =============================================================================
//  Application -- 应用主流程
//    · 视频链路：摄像头 → TCP → 上位机 LongLook
//    · 运动/避障：控制线程经串口按 F4 协议下发命令 + 接收遥测（RobotController）
// =============================================================================

namespace patrol {

struct AppConfig {
    std::string device   = "/dev/video0";
    int         width    = 1280;
    int         height   = 720;
    int         fps      = 30;
    uint16_t    port     = 8080;
    std::string bindAddr = "0.0.0.0";
    // 下位机 F4 串口
    std::string serialDevice = "/dev/ttyS1";
    int         serialBaud   = 115200;
};

class Application {
public:
    explicit Application(const AppConfig& cfg) : cfg_(cfg) {}

    int run();

    void requestStop() { running_.store(false); }

private:
    void serveClient(int clientFd);
    void controlLoop();     // 控制线程：串口遥测 + 避障 + 下发命令

    // 可被停止请求打断的睡眠；返回 false 表示期间收到退出请求
    bool interruptibleSleep(int totalMs);

    // 启动健康检测：汇总摄像头/网络/串口状态并写日志，返回是否全部正常
    bool healthCheck();

    AppConfig         cfg_;
    CameraManager     camera_;
    TcpServer         server_;
    RobotController   robot_;
    std::thread       controlThread_;
    std::atomic<bool> running_{true};
};

} // namespace patrol

#endif // PATROL_APP_APPLICATION_H
