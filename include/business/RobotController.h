#ifndef PATROL_BUSINESS_ROBOTCONTROLLER_H
#define PATROL_BUSINESS_ROBOTCONTROLLER_H

#include "model/RobotState.h"
#include "modules/network/FrameProtocol.h"
#include "modules/serial/SerialManager.h"
#include "modules/serial/Protocol.h"
#include "modules/thermal/ThermalSolver.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
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

    ~RobotController();

    bool init(const std::string& device, int baud,
              const std::string& thermalDevice = "", int thermalBaud = 115200);
    void close();
    bool isOpen() const { return serial_.isOpen(); }

    // 控制线程周期调用：poll 串口遥测 + 按模式计算并下发命令帧
    void tick();

    // 来自上位机 LongLook 的下行命令（模式/急停/执行器占位）
    void handleCommand(const net::Command& cmd);

    // 上位机对视频流做视觉识别后回传的结果（龙芯"大脑"纳入判断）
    void setVision(const net::VisionResult& v);

    // 上位机 PID 调试命令（FRAME_PID_CMD）：排队，由控制线程 tick() 打包 0x60/0x61 下发 F4。
    void setPidCommand(const net::PidCommand& pc);
    // 取走积累的 PID 调参遥测(F4 0x62)，供 serveClient 转发 FRAME_PID_TELE；返回取到条数。
    size_t takePidTele(std::vector<serial_proto::PidTele>& out);

    // 生成一行精简状态（版本/RX链路计数/关键传感器/风险/视觉），
    // 既用于龙芯本地精简日志，也经 FRAME_TEXT 发给上位机的"龙芯日志"栏。
    std::string statusLine() const;

    // 手动速度/转向（MANUAL 模式使用；预留给上位机摇杆扩展）
    void setManual(int16_t speed, int16_t steering);
    // 上位机手动驱动（FRAME_DRIVE）：切到 MANUAL、解除急停并设定速度/转向
    void driveManual(int16_t speed, int16_t steering);
    void setMode(uint8_t mode);
    void emergencyStop();

    // 把最新 F4 遥测 + 机器人状态填入发给上位机的 SensorData
    void fillSensorData(net::SensorData& s) const;

    // 中转/链路状态快照（供 SPI 小屏"中转状态页"显示）：各链路计数、新鲜度、视觉、热点。
    struct LinkStatus {
        bool        f4Open        = false;
        uint32_t    telemCnt      = 0, envCnt = 0, thermalCnt = 0;
        bool        telemFresh    = false;   // 近 2s 收到遥测帧
        bool        envFresh      = false;   // 近 2s 收到环境帧
        bool        visionActive  = false;   // 近 3s 有上位机视觉结果
        uint8_t     visionCount   = 0;
        uint8_t     visionConf    = 0;        // 最高置信度 0~100
        std::string visionName;               // 最高分类别
        int         thermalMaxC10 = -1000;    // 热成像最高温 ×10°C，-1000=无
        bool        hotspot       = false;    // 热点告警
    };
    LinkStatus linkStatus() const;

    // 取出最新热成像帧（若自上次取用后有新帧）。有新帧返回 true 并填 out/cols/rows。
    bool takeThermal(std::vector<int16_t>& out, int& cols, int& rows);

    RobotMode mode() const;

private:
    void onTelemetry(const serial_proto::Telemetry& t);            // 串口回调（控制线程）
    void onThermal(const int16_t* temps, int cols, int rows);      // 串口回调（F4已解算行帧, 旧路径）
    void onEnv(const serial_proto::EnvData& e);                    // 串口回调（环境/安全帧）
    void onRawThermal(const uint16_t* raw, int words);             // 串口回调（原始帧 -> 暂存，解算线程消费）
    void onEeprom(const uint16_t* ee, int words);                  // 串口回调（EEPROM -> 暂存，解算线程消费）
    void solverLoop();                                             // 热成像解算独立线程主体
    void onPidTele(const serial_proto::PidTele& t);                // 串口回调（PID 调参遥测 0x62）
    void registerControlCallbacks();                               // 给控制口 serial_ 挂全部回调
    void reopenControlSerial();                                    // 遥测长超时→重开控制串口自恢复
    void computeAvoid(uint16_t distCm, int16_t baseSpeed,
                      int16_t& speed, int16_t& steering) const;    // 距离 -> 运动

    // ---- 数据处理（把 F4 原始遥测加工成更稳/更有意义的量再上报）----
    // 距离 EMA 低通：HC-SR04/VL53L0X 单次读数抖动大，滤波后避障与显示更稳。
    static float ema(float prev, float sample, bool& init, float alpha);
    // 由编码器增量与时间差估算真实轮速（counts/s），比 F4 回显的速度设定值更真实。
    void updateEncoderSpeed(int32_t enc1, int32_t enc2);

    // 风险等级综合判断（调用方须持 mtx_）：距离/环境报警/热点/视觉 取最高。
    //   这是龙芯"大脑"的核心判断，fillSensorData 与 statusLine 共用。
    uint8_t computeRiskLocked(uint16_t distCm) const;

    SerialManager serial_;         // 控制/遥测/环境（ttyS1，双向）
    SerialManager thermalSerial_;  // 热成像专线（ttyS2，仅收 0x5B 行帧）
    AvoidParams   av_;

    // 控制串口自恢复：记住设备/波特率，遥测长时间断流时重开 fd 自愈。
    std::string   ctrlDevice_;
    int           ctrlBaud_    = 115200;
    uint32_t      lastReopenMs_ = 0;   // 上次重开串口时刻（限流，最多每 5s 一次）

    mutable std::mutex mtx_;
    // ---- 共享状态（mtx_ 保护）----
    uint8_t                 mode_        = serial_proto::MODE_MANUAL;
    int16_t                 manualSpeed_ = 0;
    int16_t                 manualSteer_ = 0;
    bool                    estop_       = false;
    serial_proto::Telemetry telem_{};
    bool                    telemValid_  = false;
    uint32_t                lastTelemMs_ = 0;
    serial_proto::EnvData   env_{};        // 最新环境/安全帧（气体/激光/温湿度/报警）
    uint32_t                lastEnvMs_   = 0;
    net::VisionResult       vision_{};     // 上位机回传的视觉识别结果
    uint32_t                lastVisionMs_= 0;
    // PID 调试：待下发命令队列(网络线程入队, 控制线程 tick 出队发串口) + 遥测缓存
    std::vector<net::PidCommand>        pidPending_;
    std::vector<serial_proto::PidTele>  pidTeleQ_;      // 上行遥测队列(上限 kPidTeleQMax)
    uint32_t                            pidTeleRxCnt_ = 0;
    static constexpr size_t             kPidTeleQMax  = 128;
    // 接收诊断计数（判断 F4->龙芯 各链路是否真的收到数据）
    uint32_t                telemRxCnt_  = 0;   // 0x5A/7字节 遥测帧
    uint32_t                envRxCnt_    = 0;   // 0x5C 环境帧
    uint32_t                thermalRxCnt_= 0;   // 0x5B 热成像整幅
    ActuatorState           actuators_;   // F4 无对应硬件，仅回显给上位机
    RobotStatus             status_      = RobotStatus::Idle;

    // ---- 数据处理中间量（mtx_ 保护）----
    float    distFiltCm_  = 0.0f;  bool distInit_  = false;  // 超声波距离 EMA
    float    vl53FiltMm_  = 0.0f;  bool vl53Init_  = false;  // 激光距离 EMA
    int32_t  lastEnc1_    = 0,  lastEnc2_ = 0;               // 上次编码器计数
    uint32_t lastEncMs_   = 0;                               // 上次计算轮速的时刻
    bool     encInit_     = false;
    int16_t  encSpeed1_   = 0,  encSpeed2_ = 0;              // 估算轮速 counts/s（限幅 int16）
    // 热成像热点（atomic，fillSensorData 无锁读取，避免与 thermalMtx_ 交叉锁）
    std::atomic<int>  thermalMaxC10_{-1000};   // 最高温 ×10（°C），-1000=无数据
    std::atomic<bool> thermalHotspot_{false};  // 最高温超阈（疑似火源/过热）
    static constexpr float kHotspotThreshC = 50.0f;  // 热点告警阈值 °C

    // 热成像解算器（龙芯端解算, 替代 F4 解算）+ 解算输出缓冲
    // ★解算(ExtractParameters/CalculateTo)只在 solverThread_ 内跑——绝不占用控制线程，
    //   否则解算耗时会撑破 F4 的 ~510ms 心跳窗口，导致 F4 门控关闭全部上行遥测(“掉线”)。
    ThermalSolver           thermalSolver_;
    int16_t                 thermalSolved_[serial_proto::THERMAL_PIXELS] = {0};

    // 热成像解算独立线程 + 待解算暂存（原始帧/EEPROM 由串口回调投入, 解算线程消费; 新盖旧）
    std::thread             solverThread_;
    std::atomic<bool>       solverRun_{false};
    std::mutex              rawMtx_;
    std::vector<uint16_t>   pendingRaw_;   bool pendingRawNew_ = false;
    std::vector<uint16_t>   pendingEe_;    bool pendingEeNew_  = false;

    // 热成像帧（单独锁，避免大拷贝阻塞运动共享态）
    mutable std::mutex      thermalMtx_;
    std::vector<int16_t>    thermal_;
    int                     thermalCols_ = 0;
    int                     thermalRows_ = 0;
    bool                    thermalNew_  = false;
};

} // namespace patrol

#endif // PATROL_BUSINESS_ROBOTCONTROLLER_H
