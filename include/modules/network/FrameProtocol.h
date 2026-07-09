#ifndef PATROL_MODULES_NETWORK_FRAMEPROTOCOL_H
#define PATROL_MODULES_NETWORK_FRAMEPROTOCOL_H

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

// =============================================================================
//  FrameProtocol — 与上位机 LongLook 对接的网络帧协议 (v2)
//
//  链路：龙芯 2K0300 (TCP 服务端) ←—WiFi/TCP—→ LongLook PC 前端 (TCP 客户端)
//
//  帧格式（流式，TCP 已保证可靠，故不加 CRC）：
//    ┌──────┬──────┬────────────┬────────────────┐
//    │ 0xA5 │ TYPE │ LEN(4B LE) │ PAYLOAD(LEN B) │
//    └──────┴──────┴────────────┴────────────────┘
//
//  本头文件与 LongLook 工程的 protocol.h 一一对应，二者必须保持一致。
// =============================================================================

namespace patrol {
namespace net {

constexpr uint8_t  LL_SOF         = 0xA5;
constexpr int      LL_HEADER_SIZE = 6;                     // SOF(1)+TYPE(1)+LEN(4)
constexpr uint32_t LL_MAX_PAYLOAD = 16u * 1024u * 1024u;   // 16 MiB 上限

// 帧类型
enum FrameType : uint8_t {
    FRAME_SENSOR  = 0x01,   // 龙芯→前端 综合传感器遥测（40 字节，小端，变长容忍）
    FRAME_VIDEO   = 0x10,   // 龙芯→前端 视频帧：JPEG/PNG 编码字节流
    FRAME_THERMAL = 0x20,   // 龙芯→前端 热成像帧：uint16 w,h + w*h 个 int16 温度(0.01°C)
    FRAME_TEXT    = 0x30,   // 龙芯→前端 文本/日志：UTF-8
    FRAME_COMMAND = 0x40,   // 前端→龙芯 下行命令：payload = [cmdId u8][value u8]
    FRAME_DRIVE   = 0x41,   // 前端→龙芯 手动驱动：payload = [speed i16 LE][steering i16 LE]
                            //   龙芯据此切 MANUAL 并转发同步命令给 F4（链路验证/手动操控）
    FRAME_VISION  = 0x42,   // 前端→龙芯 视觉识别结果（上位机对视频流做 YOLO 分析后回传，
                            //   由龙芯"大脑"纳入判断）：payload =
                            //   [count u8][maxConf u8(0~100)][flags u8][nameLen u8][name UTF-8...]
    FRAME_PID_TELE = 0x22,  // 龙芯→前端 PID 调参遥测（转发 F4 0x62，payload 小端 23B）：
                            //   seq u16 | flags u8 | targL i16 measL i16 outL i16
                            //   | targR i16 measR i16 outR i16 | enc1 i32 | enc2 i32
    FRAME_PID_CMD  = 0x43,  // 前端→龙芯 PID 调试命令（龙芯翻译成 F4 0x60/0x61，小端）：
                            //   [sub u8] sub=1 参数: kp/ki/kd_x1000 u32×3 | max_delta u16 | flags u8
                            //            sub=2 测试: mode u8 | left i16 | right i16 | duration_ms u16
};

// PID 调试命令解析结果（FRAME_PID_CMD）
struct PidCommand {
    uint8_t  sub      = 0;      // 1=参数 2=测试
    // sub=1
    float    kp = 0, ki = 0, kd = 0;
    uint16_t maxDelta = 0;
    bool     closedLoop = false;
    // sub=2
    uint8_t  testMode = 0;      // 0停/1开环PWM/2闭环目标
    int16_t  left = 0, right = 0;
    uint16_t durationMs = 0;
};

// 视觉结果标志位（上位机识别到的关注目标）
enum VisionFlag : uint8_t {
    VIS_PERSON = 0x01,   // 检测到人
    VIS_FIRE   = 0x02,   // 检测到火焰/火源
};

// 下行命令 ID（执行与联动模块控制）
enum CmdId : uint8_t {
    CMD_FAN    = 0x01,   // 风扇   value: 0关/1开
    CMD_BUZZER = 0x02,   // 蜂鸣器 value: 0关/1开
    CMD_RELAY  = 0x03,   // 继电器 value: 0断/1通
    CMD_LED    = 0x04,   // LED    value: 0灭/1亮
    CMD_MODE   = 0x05,   // 模式   value: 0手动/1自动/2避障/3巡检
    CMD_ESTOP  = 0x06,   // 急停   value: 1
};

// v2 基础负载 40 字节；v3 在其后追加激光测距(2B)，共 42 字节。
// FRAME_SENSOR 为“变长容忍”：旧 LongLook 只读前 40 字节忽略其余，新 LongLook 读满 42。
constexpr int LL_SENSOR_PAYLOAD_SIZE_V2 = 40;
constexpr int LL_SENSOR_PAYLOAD_SIZE    = 42;

// SensorData.flags 位定义（与 F4 环境帧 ENV_FLAG_* / serial_proto 一一对应）
enum SensorFlag : uint8_t {
    SF_GAS_ALARM = 0x01,   // 气体浓度超阈
    SF_FLAME     = 0x02,   // 火焰检测
    SF_DHT_OK    = 0x04,   // 温湿度有效
    SF_VL53_OK   = 0x08,   // 激光测距有效
    SF_OBSTACLE  = 0x10,   // 障碍确认（激光+超声波双重验证）
    SF_BUZZER    = 0x20,   // 蜂鸣器鸣响中
};

// -----------------------------------------------------------------------------
//  构造 6 字节帧头：[0xA5][type][len 小端 4B]
// -----------------------------------------------------------------------------
inline void buildHeader(uint8_t type, uint32_t len, uint8_t out[LL_HEADER_SIZE]) {
    out[0] = LL_SOF;
    out[1] = type;
    out[2] = static_cast<uint8_t>( len        & 0xFF);
    out[3] = static_cast<uint8_t>((len >> 8)  & 0xFF);
    out[4] = static_cast<uint8_t>((len >> 16) & 0xFF);
    out[5] = static_cast<uint8_t>((len >> 24) & 0xFF);
}

// -----------------------------------------------------------------------------
//  打包一整帧到 vector（含头+负载），用于一次性写出
// -----------------------------------------------------------------------------
inline std::vector<uint8_t> buildFrame(uint8_t type, const uint8_t* payload, size_t len) {
    std::vector<uint8_t> frame;
    frame.reserve(LL_HEADER_SIZE + len);
    uint8_t hdr[LL_HEADER_SIZE];
    buildHeader(type, static_cast<uint32_t>(len), hdr);
    frame.insert(frame.end(), hdr, hdr + LL_HEADER_SIZE);
    if (payload && len) frame.insert(frame.end(), payload, payload + len);
    return frame;
}

// -----------------------------------------------------------------------------
//  综合传感器遥测负载（小端，规范长度 40 字节）。
//  当前阶段仅用于让上位机的卡片显示有数据流动，字段多为占位值。
// -----------------------------------------------------------------------------
struct SensorData {
    uint32_t timestamp_ms    = 0;
    int16_t  temperature_01c = 0;   // 0.1°C
    uint16_t humidity_01     = 0;   // 0.1 %
    uint16_t gas_ppm         = 0;
    uint32_t pressure_pa     = 0;
    uint16_t distance_cm     = 0;
    int32_t  encoder1        = 0;
    int32_t  encoder2        = 0;
    int16_t  speed_L         = 0;
    int16_t  speed_R         = 0;
    uint16_t servo_us        = 0;
    uint16_t voltage_mV      = 0;
    uint8_t  mode            = 0;
    uint8_t  fault           = 0;
    uint8_t  risk_level      = 0;
    uint8_t  flags           = 0;
    uint8_t  fan             = 0;
    uint8_t  buzzer          = 0;
    uint8_t  relay           = 0;
    uint8_t  led             = 0;
    // ---- v3 追加字段（在 40 字节之后，向后兼容）----
    uint16_t laser_cm        = 0;   // VL53L0X 激光测距(cm)，0=无效/超量程
};

// 将 SensorData 序列化为 40 字节小端负载
namespace detail {
    inline void put_u8 (std::vector<uint8_t>& b, uint8_t  v) { b.push_back(v); }
    inline void put_u16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8)); }
    inline void put_u32(std::vector<uint8_t>& b, uint32_t v) {
        b.push_back(uint8_t(v)); b.push_back(uint8_t(v >> 8));
        b.push_back(uint8_t(v >> 16)); b.push_back(uint8_t(v >> 24));
    }
} // namespace detail

inline std::vector<uint8_t> packSensor(const SensorData& s) {
    std::vector<uint8_t> b;
    b.reserve(LL_SENSOR_PAYLOAD_SIZE);
    using namespace detail;
    put_u32(b, s.timestamp_ms);
    put_u16(b, static_cast<uint16_t>(s.temperature_01c));
    put_u16(b, s.humidity_01);
    put_u16(b, s.gas_ppm);
    put_u32(b, s.pressure_pa);
    put_u16(b, s.distance_cm);
    put_u32(b, static_cast<uint32_t>(s.encoder1));
    put_u32(b, static_cast<uint32_t>(s.encoder2));
    put_u16(b, static_cast<uint16_t>(s.speed_L));
    put_u16(b, static_cast<uint16_t>(s.speed_R));
    put_u16(b, s.servo_us);
    put_u16(b, s.voltage_mV);
    put_u8 (b, s.mode);
    put_u8 (b, s.fault);
    put_u8 (b, s.risk_level);
    put_u8 (b, s.flags);
    put_u8 (b, s.fan);
    put_u8 (b, s.buzzer);
    put_u8 (b, s.relay);
    put_u8 (b, s.led);
    // ---- v3 追加：激光测距（第 40~41 字节）----
    put_u16(b, s.laser_cm);
    return b; // 42 字节（前 40 字节与 v2 完全一致）
}

// 解析出的下行命令
struct Command {
    uint8_t cmdId = 0;
    uint8_t value = 0;
};

// 解析出的手动驱动命令（FRAME_DRIVE）
struct DriveCommand {
    int16_t speed    = 0;   // -1000~+1000
    int16_t steering = 0;   // -1000~+1000
};

// 解析出的视觉识别结果（FRAME_VISION）：上位机对视频流分析后回传，供龙芯判断
struct VisionResult {
    uint8_t     count   = 0;    // 检测目标数
    uint8_t     maxConf = 0;    // 最高置信度 0~100
    uint8_t     flags   = 0;    // VisionFlag 位（VIS_PERSON/VIS_FIRE...）
    std::string topClass;       // 最高分目标类别名
};

} // namespace net
} // namespace patrol

#endif // PATROL_MODULES_NETWORK_FRAMEPROTOCOL_H
