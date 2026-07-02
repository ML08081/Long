#ifndef PATROL_BUSINESS_ROBOTCONTROLLER_H
#define PATROL_BUSINESS_ROBOTCONTROLLER_H

#include "model/RobotState.h"
#include "modules/network/FrameProtocol.h"
#include "modules/serial/SerialManager.h"
#include "modules/serial/Protocol.h"

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace patrol {

// 避障参数（距离单位 cm；速度/转向量 -1000~+1000）
struct AvoidParams {
    uint16_t stopDistCm  = 25;    // < 此距离 -> 停止前进 + 原地转向脱困
    uint16_t slowDistCm  = 60;    // < 此距离 -> 减速 + 转向绕行
    int16_t  cruiseSpeed = 400;   // 自动/巡检基础前进速度
    int16_t  obstacleSpeed = 300; // 避障演示模式基础速度（更保守）
    int16_t  slowSpeed   = 200;   // 减速区速度
    int16_t  turnSteer   = 700;   // 避障转向量（+ 向右）
};

// 机器人底层控制：经串口按 F4 协议下发运动命令 + 接收遥测 + 距离避障。
// 线程模型：init()/tick() 在控制线程调用（独占 SerialManager）；
//           handleCommand()/setManual()/setMode()/fillSensorData() 可由网络线程调用。
//           共享状态由 mtx_ 保护；串口收发只在控制线程。
class RobotController {
public:
    RobotController() = default;

    bool init(const std::string& device, int baud);
    void close();
    bool isOpen() const { return serial_.isOpen(); }

    // 控制线程周期调用：poll 串口遥测 + 按模式计算并下发命令帧
    void tick();

    // 来自上位机 LongLook 的下行命令（模式/急停/执行器占位）
    void handleCommand(const net::Command& cmd);

    // 手动速度/转向（MANUAL 模式使用；预留给上位机摇杆扩展）
    void setManual(int16_t speed, int16_t steering);
    // 上位机手动驱动（FRAME_DRIVE）：切到 MANUAL、解除急停并设定速度/转向
    void driveManual(int16_t speed, int16_t steering);
    void setMode(uint8_t mode);
    void emergencyStop();

    // 把最新 F4 遥测 + 机器人状态填入发给上位机的 SensorData
    void fillSensorData(net::SensorData& s) const;

    // 取出最新热成像帧（若自上次取用后有新帧）。有新帧返回 true 并填 out/cols/rows。
    bool takeThermal(std::vector<int16_t>& out, int& cols, int& rows);

    RobotMode mode() const;

private:
    void onTelemetry(const serial_proto::Telemetry& t);            // 串口回调（控制线程）
    void onThermal(const int16_t* temps, int cols, int rows);      // 串口回调（热成像帧）
    void computeAvoid(uint16_t distCm, int16_t baseSpeed,
                      int16_t& speed, int16_t& steering) const;    // 距离 -> 运动

    SerialManager serial_;
    AvoidParams   av_;

    mutable std::mutex mtx_;
    // ---- 共享状态（mtx_ 保护）----
    uint8_t                 mode_        = serial_proto::MODE_MANUAL;
    int16_t                 manualSpeed_ = 0;
    int16_t                 manualSteer_ = 0;
    bool                    estop_       = false;
    serial_proto::Telemetry telem_{};
    bool                    telemValid_  = false;
    uint32_t                lastTelemMs_ = 0;
    ActuatorState           actuators_;   // F4 无对应硬件，仅回显给上位机
    RobotStatus             status_      = RobotStatus::Idle;

    // 热成像帧（单独锁，避免大拷贝阻塞运动共享态）
    mutable std::mutex      thermalMtx_;
    std::vector<int16_t>    thermal_;
    int                     thermalCols_ = 0;
    int                     thermalRows_ = 0;
    bool                    thermalNew_  = false;
};

} // namespace patrol

#endif // PATROL_BUSINESS_ROBOTCONTROLLER_H
