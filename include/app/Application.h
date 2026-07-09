#ifndef PATROL_APP_APPLICATION_H
#define PATROL_APP_APPLICATION_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

#include "modules/camera/CameraManager.h"
#include "modules/network/TcpServer.h"
#include "modules/display/St7789Display.h"
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
    // 热成像专用串口（F4 USART1 → 龙芯此口）
    std::string serialThermalDevice = "/dev/ttyS2";
    int         serialThermalBaud   = 115200;
    // SPI 状态小屏（ST7789，默认关闭）
    St7789Display::Config display;
};

class Application {
public:
    explicit Application(const AppConfig& cfg) : cfg_(cfg) {}

    int run();

    void requestStop() { running_.store(false); }

private:
    void serveClient(int clientFd);
    void controlLoop();     // 控制线程：串口遥测 + 避障 + 下发命令
    void displayLoop();     // 显示线程：定时把状态渲染到 SPI 小屏
    void discoveryLoop();   // 发现线程：UDP 广播龙芯自身 IP，供上位机自动发现/重连
    St7789Display::RelayInfo buildRelayInfo();  // 汇总"中转状态页"信息

    // 可被停止请求打断的睡眠；返回 false 表示期间收到退出请求
    bool interruptibleSleep(int totalMs);

    // 启动健康检测：汇总摄像头/网络/串口状态并写日志，返回是否全部正常
    bool healthCheck();

    AppConfig         cfg_;
    CameraManager     camera_;
    TcpServer         server_;
    RobotController   robot_;
    St7789Display     display_;
    std::thread       controlThread_;
    std::thread       displayThread_;
    std::thread       discoveryThread_;
    std::atomic<bool> running_{true};

    // 上位机连接状态（供 SPI 小屏"中转状态页"显示；网络线程写，显示线程读）
    std::atomic<bool>     upperOnline_{false};
    std::atomic<uint32_t> upperSinceMs_{0};   // 本次连接建立时刻(ms)
    std::mutex            upperMtx_;
    std::string           upperPeer_;         // 对端 IP:port
    uint32_t              startMs_ = 0;        // 进程启动时刻(ms)，算运行时长

    // PID 测试期间静默热成像的截止时刻(ms)：接收线程写、上报线程读。
    std::atomic<uint32_t> pidHushThermalUntil_{0};
};

} // namespace patrol

#endif // PATROL_APP_APPLICATION_H
