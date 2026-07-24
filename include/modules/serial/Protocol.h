#ifndef PATROL_MODULES_SERIAL_PROTOCOL_H
#define PATROL_MODULES_SERIAL_PROTOCOL_H

#include <cstdint>
#include <cstddef>
#include <array>

// ==========================================================================
//  串口协议 —— 与 F4(STM32F407) 固件 Core/Protocol/protocol.{h,c} 完全一致
//  物理链路：龙芯 /dev/ttyS1 (USART) <-> F4 USART3，115200 8N1
//
//  命令帧（龙芯 -> F4），定长 7 字节，大端序：
//    [0]=0xAA [1]=spd_hi [2]=spd_lo [3]=str_hi [4]=str_lo [5]=mode(bit7=0) [6]=XOR(0..5)
//    speed/steering : int16, -1000~+1000（speed +前进/-后退；steering +右/-左）
//    mode           : 0=Manual 1=Auto 2=Obstacle 3=Cruise
//
//  遥测帧（F4 -> 龙芯）：
//    · 旧 7 字节遥测（与命令帧同构，mode 的 bit7=1）——仅 speed/steer/mode
//    · 扩展遥测帧（本工程新增，携带超声波距离 + 编码器）：
//        [0]=0xAA [1]=0x5A(EXT) [2]=LEN(=15) [3..]=payload [末]=XOR(0..2+LEN)
//        payload(15B，大端)：spd i16 | str i16 | mode u8 | dist_cm u16 | enc1 i32 | enc2 i32
//    · 环境/安全遥测帧（本工程新增，携带气体/激光测距/温湿度/报警）：
//        [0]=0xAA [1]=0x5C(ENV) [2]=LEN(=8) [3..]=payload [末]=XOR(0..2+LEN)
//        payload(8B，大端)：gas_raw u16 | vl53_mm u16 | temp i8 | humi u8 | flags u8 | alarm u8
//    · 热成像行帧：[0]=0xAA [1]=0x5B(THERMAL) [2]=row [3..]=32*i16 BE [末]=XOR，共 68 字节
// ==========================================================================

namespace patrol {
namespace serial_proto {

constexpr uint8_t HEADER          = 0xAA;
constexpr uint8_t EXT_MARKER      = 0x5A;   // 扩展遥测帧第二字节
constexpr uint8_t THERMAL_MARKER  = 0x5B;   // 热成像行帧第二字节(F4已解算, 旧路径, 保留兼容)
constexpr uint8_t ENV_MARKER      = 0x5C;   // 环境/安全遥测帧第二字节
constexpr uint8_t RAW_THERM_MARKER= 0x5D;   // 热成像"原始帧"分块(F4只转发, 龙芯解算, 新路径)
constexpr uint8_t EEPROM_MARKER   = 0x5E;   // MLX90640 EEPROM 标定数据分块(上电发一次)

// ---- PID 在线调试链路已于 2026-07-23 整体移除（v1.19.0）----
//  PID 参数已在 F4 端固化定版(Kp=1.0 Ki=4.0 Kd=0 MaxDelta=1100)，F4 固件亦已屏蔽
//  0x60/0x61 下行入口与 0x62 上行遥测。龙芯不再定义、解析或转发任何 PID 帧。

// 原始热成像/EEPROM 分块帧: [AA][MARK][idx u8][cnt u8][cnt*2 字节 大端][XOR]
//   idx=分块序号, cnt=本块字数; 目标数组偏移 = idx*MLX_CHUNK_WORDS。
//   MLX90640 原始帧=834 字, EEPROM=832 字（Melexis 官方规格）。
constexpr int MLX_EE_WORDS    = 832;
constexpr int MLX_FRAME_WORDS = 834;
constexpr int MLX_CHUNK_WORDS = 32;         // 每块 32 字=64 字节数据
constexpr uint8_t TELEMETRY_FLAG  = 0x80;   // 旧遥测帧 mode 的 bit7
constexpr uint8_t CMD_FRAME_LEN   = 7;
constexpr uint8_t EXT_PAYLOAD_LEN = 15;
constexpr uint8_t EXT_FRAME_LEN   = 3 + EXT_PAYLOAD_LEN + 1;  // = 19
constexpr uint8_t ENV_PAYLOAD_LEN = 8;
constexpr uint8_t ENV_FRAME_LEN   = 3 + ENV_PAYLOAD_LEN + 1;  // = 12

// 环境帧 flags 位（与 F4 protocol.h ENV_FLAG_* 完全一致）
constexpr uint8_t ENV_FLAG_GAS      = 0x01;   // 气体浓度超阈值
constexpr uint8_t ENV_FLAG_FLAME    = 0x02;   // 火焰检测
constexpr uint8_t ENV_FLAG_DHT_OK   = 0x04;   // 温湿度有效
constexpr uint8_t ENV_FLAG_VL53_OK  = 0x08;   // 激光测距有效
constexpr uint8_t ENV_FLAG_OBSTACLE = 0x10;   // 障碍确认（VL53 与超声波双重验证）
constexpr uint8_t ENV_FLAG_BUZZER   = 0x20;   // 蜂鸣器鸣响中
constexpr uint8_t ENV_FLAG_VL53_PRESENT = 0x40; // 激光传感器已初始化在位（区分死值/缺失与超量程）
constexpr uint8_t ENV_FLAG_OBS_LOCK = 0x80;   // ★遥控避障锁生效中（前方过近，F4 已封锁前进）

// 报警等级（环境帧 alarm 字节）
constexpr uint8_t ALARM_LV_NONE = 0;
constexpr uint8_t ALARM_LV_WARN = 1;
constexpr uint8_t ALARM_LV_FIRE = 2;

// VL53L0X 无效/超量程标记（mm，与 F4 vl53l0x.h 一致）
constexpr uint16_t VL53_OUT_OF_RANGE = 8190;

// 热成像（MLX90640 32x24，分行传输）
constexpr int THERMAL_COLS    = 32;
constexpr int THERMAL_ROWS    = 24;
constexpr int THERMAL_PIXELS  = THERMAL_COLS * THERMAL_ROWS;   // 768
constexpr int THERMAL_ROW_LEN = 3 + THERMAL_COLS * 2 + 1;      // 68: [AA][5B][row][32*i16 BE][XOR]

enum Mode : uint8_t {
    MODE_MANUAL   = 0,
    MODE_AUTO     = 1,
    MODE_OBSTACLE = 2,
    MODE_CRUISE   = 3,
};

// ===== 三种运行模式（2026-07-21）=====================================
//  沿用上面既有编码值，不破坏协议/上位机兼容，只给出明确语义。
//  三端(F4 protocol.h / 龙芯 / LongLook)必须一致：
//    MODE_REMOTE      遥控     —— 完全由上位机下发 speed/steering
//    MODE_LINE_ONLY   纯循迹   —— F4 本地 TCRT 循线自动巡检
//    MODE_LINE_VISION 循迹+视觉 —— 循线仍是 F4 本地快环(断网可跑)，
//                                 龙芯 YOLO/ArUco 作为叠加约束介入
enum PatrolRunMode : uint8_t {
    MODE_REMOTE      = MODE_MANUAL,   // 0
    MODE_LINE_ONLY   = MODE_AUTO,     // 1
    MODE_LINE_VISION = MODE_CRUISE,   // 3

};

// 遥测解析结果
struct Telemetry {
    int16_t  speed       = 0;
    int16_t  steering    = 0;
    uint8_t  mode        = 0;
    uint16_t dist_cm     = 0;      // 0 = 无效/超量程
    int32_t  enc1        = 0;
    int32_t  enc2        = 0;
    bool     hasDistance = false;  // true=来自扩展帧(dist/enc 有效); false=旧 7 字节遥测
};

// 环境/安全遥测解析结果（来自 0x5C 帧）
struct EnvData {
    uint16_t gas_raw = 0;      // MQ2 原始 ADC（0~4095，越大越浓）
    uint16_t vl53_mm = 0;      // VL53L0X 激光测距(mm)，==VL53_OUT_OF_RANGE 表示无效
    int8_t   temp_c  = 0;      // DHT11 温度(°C)
    uint8_t  humi    = 0;      // DHT11 湿度(%RH)
    uint8_t  flags   = 0;      // ENV_FLAG_* 位组合
    uint8_t  alarm   = 0;      // 0=正常 1=警告 2=报警
    bool     valid   = false;  // 是否已收到过有效环境帧
};

// XOR 校验（与 F4 Protocol_CalculateChecksum 一致）
inline uint8_t xorChecksum(const uint8_t* data, size_t len) {
    uint8_t c = 0;
    for (size_t i = 0; i < len; ++i) c ^= data[i];
    return c;
}

// 打包命令帧（7 字节）。速度/转向自动限幅到 [-1000,1000]。
inline std::array<uint8_t, CMD_FRAME_LEN>
buildCommand(int16_t speed, int16_t steering, uint8_t mode) {
    if (speed > 1000)      speed = 1000;
    else if (speed < -1000) speed = -1000;
    if (steering > 1000)      steering = 1000;
    else if (steering < -1000) steering = -1000;

    std::array<uint8_t, CMD_FRAME_LEN> f{};
    f[0] = HEADER;
    f[1] = static_cast<uint8_t>((static_cast<uint16_t>(speed) >> 8) & 0xFF);
    f[2] = static_cast<uint8_t>( static_cast<uint16_t>(speed)       & 0xFF);
    f[3] = static_cast<uint8_t>((static_cast<uint16_t>(steering) >> 8) & 0xFF);
    f[4] = static_cast<uint8_t>( static_cast<uint16_t>(steering)       & 0xFF);
    f[5] = static_cast<uint8_t>(mode & 0x03);   // bit7=0 => 命令帧
    f[6] = xorChecksum(f.data(), 6);
    return f;
}

// 大端读取辅助
inline int16_t  rdI16(const uint8_t* p) { return static_cast<int16_t>((p[0] << 8) | p[1]); }
inline uint16_t rdU16(const uint8_t* p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }
inline int32_t  rdI32(const uint8_t* p) {
    return static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 24) |
                                (static_cast<uint32_t>(p[1]) << 16) |
                                (static_cast<uint32_t>(p[2]) <<  8) |
                                 static_cast<uint32_t>(p[3]));
}

} // namespace serial_proto
} // namespace patrol

#endif // PATROL_MODULES_SERIAL_PROTOCOL_H
