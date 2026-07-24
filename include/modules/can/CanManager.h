#ifndef PATROL_MODULES_CAN_CANMANAGER_H
#define PATROL_MODULES_CAN_CANMANAGER_H

#include "modules/can/CanProtocol.h"
#include "modules/serial/Protocol.h"

#include <string>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstddef>

// ==========================================================================
//  CanManager —— 龙芯 SocketCAN 传输层（对齐 SerialManager 接口，见实施方案 §3.7.4）
//
//  · 与 F4 bxCAN1 经 can0 通信：过程数据(命令/遥测/环境) + 事件报文 + 类 SDO 参数服务。
//  · 线程模型：open()/close()/poll()/send*/sdo* 只在控制线程调用（独占 socket）。
//    与 SerialManager 同构 —— RobotController 只换持有的对象，上层消费逻辑 100% 复用。
//  · socket 非阻塞：poll() 绝不阻塞 ~30Hz 控制线程；SDO 是同步短事务，只在配置期调用。
//  · 收 CAN 错误帧(CAN_RAW_ERR_FILTER)：TEC/REC/bus-off 计入 Health —— 用数字量化 EMI 改善。
// ==========================================================================

namespace patrol {

class CanManager {
public:
    // 回调（保持与 SerialManager 同构，上层零改动；遥测/环境复用 serial_proto 结构）
    using TelemetryCallback    = std::function<void(const serial_proto::Telemetry&)>;
    using EnvCallback          = std::function<void(const serial_proto::EnvData&)>;
    using PatrolEventCallback  = std::function<void(const can_proto::PatrolEvent&)>;
    using AvoidProfileCallback = std::function<void(const can_proto::AvoidProfile&)>;
    using HeartbeatCallback    = std::function<void(const can_proto::Heartbeat&)>;

    CanManager() = default;
    ~CanManager();

    CanManager(const CanManager&) = delete;
    CanManager& operator=(const CanManager&) = delete;

    // socket + bind + filter(收 F4 上行 6 个 COB-ID) + 错误帧过滤 + 非阻塞
    bool open(const std::string& ifname);
    void close();
    bool isOpen() const { return fd_ >= 0; }
    const std::string& ifname() const { return ifname_; }

    // 长时间无上行帧时重建 socket（应用层兜底自恢复，语义对齐 reopenControlSerial）
    bool reopen();

    void setTelemetryCallback(TelemetryCallback cb)       { cb_ = std::move(cb); }
    void setEnvCallback(EnvCallback cb)                   { envCb_ = std::move(cb); }
    void setPatrolEventCallback(PatrolEventCallback cb)   { patrolEvtCb_ = std::move(cb); }
    void setAvoidProfileCallback(AvoidProfileCallback cb) { avoidProfCb_ = std::move(cb); }
    void setHeartbeatCallback(HeartbeatCallback cb)       { hbCb_ = std::move(cb); }

    // —— 过程数据（无确认，直接 write，非阻塞）——
    // 命令帧(0x201，兼心跳)：speed/steering 会被限幅到 [-1000,1000]。
    bool sendCommand(int16_t speed, int16_t steering, uint8_t mode);
    // 0x301 sub=0x01 巡检命令
    bool sendPatrolCmd(uint8_t cmd, uint8_t flags);
    // 0x301 sub=0x02 到点动作（放行 + 下一步）
    bool sendPatrolAct(uint8_t pointId, uint8_t action, uint8_t resume);
    // 0x301 sub=0x03 避障策略（Tier2 语义）
    bool sendAvoidPolicy(uint8_t policy, uint8_t obsClass);
    // 注：0x301 sub=0x04 PID 测试激励已于 v1.19.0 移除，F4 端亦已屏蔽该命令。

    // —— 类 SDO 客户端（同步，带超时；只在控制线程、配置期调用，绝不放进 tick 热路径）——
    //   返回 true=成功；失败时 abortCode(可空)带回 CiA 301 abort code(0=超时/本地错)。
    bool sdoWrite(uint16_t idx, uint8_t sub, const void* d, size_t len,
                  uint32_t* abortCode = nullptr);
    bool sdoRead(uint16_t idx, uint8_t sub, void* out, size_t* len,
                 uint32_t* abortCode = nullptr);
    // 分段下发（路线表 domain 等 >8B 数据）
    bool sdoWriteDomain(uint16_t idx, uint8_t sub, const std::vector<uint8_t>& blob,
                        uint32_t* abortCode = nullptr);

    // —— 轮询（控制线程 tick 调用）：read() 取尽所有帧 → 分发回调 + 统计错误帧 ——
    void poll();

    // —— 链路健康（★ EMI 诊断关键）——
    struct Health {
        uint64_t rx        = 0;   // 收到的有效数据帧数
        uint64_t tx        = 0;   // 成功发出的帧数
        uint64_t errFrames = 0;   // 收到的 CAN 错误帧数
        uint64_t busOff    = 0;   // bus-off 次数
        uint32_t tec       = 0;   // 发送错误计数（最近一次错误帧携带）
        uint32_t rec       = 0;   // 接收错误计数
        uint32_t lastRxMs  = 0;   // 最近一次收到上行帧的时刻
    };
    Health health() const { return h_; }

private:
    // 底层：发一帧标准数据帧（COB-ID + ≤8B），成功累加 h_.tx
    bool sendRaw(uint32_t cobId, const uint8_t* d, uint8_t len);
    // 分发一帧上行数据帧到对应回调（poll 与 SDO 内部读循环共用，保证 SDO 期间遥测不丢）
    void dispatch(uint32_t cobId, const uint8_t* d, uint8_t dlc);
    // 处理一帧 CAN 错误帧（更新 Health）
    void handleErrorFrame(const uint8_t* d, uint8_t dlc);
    // SDO 内部：阻塞等一帧 0x581 响应（期间把非 SDO 帧交 dispatch），超时返回 false
    bool waitSdoResponse(uint8_t out[8], uint32_t timeoutMs);

    int         fd_ = -1;
    std::string ifname_;
    Health      h_{};

    TelemetryCallback    cb_;
    EnvCallback          envCb_;
    PatrolEventCallback  patrolEvtCb_;
    AvoidProfileCallback avoidProfCb_;
    HeartbeatCallback    hbCb_;

    // TELE_CORE(0x181) 与 TELE_ENC(0x281) 分属两帧，合并成一个 Telemetry 再回调上层。
    serial_proto::Telemetry teleAsm_{};
    bool                    haveEnc_ = false;   // 收到过 0x281（enc 有效）
};

} // namespace patrol

#endif // PATROL_MODULES_CAN_CANMANAGER_H
