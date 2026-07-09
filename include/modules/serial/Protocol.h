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

// ---- PID 在线调试（2026-07-08，与 F4 protocol.h 同源） ----
constexpr uint8_t PID_PARAM_MARKER = 0x60;  // 下行: PID 参数(kp/ki/kd/max_delta/使能)
constexpr uint8_t PID_TEST_MARKER  = 0x61;  // 下行: PID 测试激励(开环PWM/闭环阶跃)
constexpr uint8_t PID_TELE_MARKER  = 0x62;  // 上行: PID 调参遥测(target/meas/out×2轮)
constexpr uint8_t PID_PARAM_PAYLOAD = 15;
constexpr uint8_t PID_PARAM_LEN     = 3 + PID_PARAM_PAYLOAD + 1;  // = 19
constexpr uint8_t PID_TEST_PAYLOAD  = 7;
constexpr uint8_t PID_TEST_LEN      = 3 + PID_TEST_PAYLOAD + 1;   // = 11
constexpr uint8_t PID_TELE_PAYLOAD  = 23;
constexpr uint8_t PID_TELE_LEN      = 3 + PID_TELE_PAYLOAD + 1;   // = 27
constexpr uint8_t PID_FLAG_CLOSED_LOOP = 0x01;
// 测试模式（与 F4 PID_TEST_* 一致）
constexpr uint8_t PID_TEST_OFF       = 0;
constexpr uint8_t PID_TEST_OPEN_LOOP = 1;   // left/right = 直给 PWM(-1000~1000)
constexpr uint8_t PID_TEST_CLOSED    = 2;   // left/right = 目标编码器增量(counts/17ms)

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

// PID 调参遥测解析结果（来自 0x62 帧）
struct PidTele {
    uint16_t seq   = 0;
    uint8_t  flags = 0;    // bit0=闭环使能 bit1=测试进行中 bit4-5=测试模式
    int16_t  targL = 0, measL = 0, outL = 0;
    int16_t  targR = 0, measR = 0, outR = 0;
    int32_t  enc1  = 0, enc2  = 0;
};

// XOR 校验（与 F4 Protocol_CalculateChecksum 一致）
inline uint8_t xorChecksum(const uint8_t* data, size_t len) {
    uint8_t c = 0;
    for (size_t i = 0; i < len; ++i) c ^= data[i];
    return c;
}

// 打包 PID 参数帧(0x60，龙芯 -> F4)：增益 ×1000 定点，大端。
inline std::array<uint8_t, PID_PARAM_LEN>
buildPidParam(float kp, float ki, float kd, uint16_t maxDelta, bool closedLoop) {
    auto clampGain = [](float v) -> uint32_t {
        if (v < 0.0f) v = 0.0f;
        if (v > 4000000.0f) v = 4000000.0f;
        return static_cast<uint32_t>(v * 1000.0f + 0.5f);
    };
    uint32_t kpq = clampGain(kp), kiq = clampGain(ki), kdq = clampGain(kd);
    std::array<uint8_t, PID_PARAM_LEN> f{};
    f[0] = HEADER; f[1] = PID_PARAM_MARKER; f[2] = PID_PARAM_PAYLOAD;
    auto putU32 = [&](int i, uint32_t v) {
        f[i]   = static_cast<uint8_t>((v >> 24) & 0xFF);
        f[i+1] = static_cast<uint8_t>((v >> 16) & 0xFF);
        f[i+2] = static_cast<uint8_t>((v >>  8) & 0xFF);
        f[i+3] = static_cast<uint8_t>( v        & 0xFF);
    };
    putU32(3,  kpq); putU32(7, kiq); putU32(11, kdq);
    f[15] = static_cast<uint8_t>((maxDelta >> 8) & 0xFF);
    f[16] = static_cast<uint8_t>( maxDelta       & 0xFF);
    f[17] = closedLoop ? PID_FLAG_CLOSED_LOOP : 0;
    f[18] = xorChecksum(f.data(), 18);
    return f;
}

// 打包 PID 测试帧(0x61，龙芯 -> F4)。durationMs=0 时 F4 用默认 3s、上限 10s。
inline std::array<uint8_t, PID_TEST_LEN>
buildPidTest(uint8_t mode, int16_t left, int16_t right, uint16_t durationMs) {
    std::array<uint8_t, PID_TEST_LEN> f{};
    f[0] = HEADER; f[1] = PID_TEST_MARKER; f[2] = PID_TEST_PAYLOAD;
    f[3] = mode;
    f[4] = static_cast<uint8_t>((static_cast<uint16_t>(left)  >> 8) & 0xFF);
    f[5] = static_cast<uint8_t>( static_cast<uint16_t>(left)        & 0xFF);
    f[6] = static_cast<uint8_t>((static_cast<uint16_t>(right) >> 8) & 0xFF);
    f[7] = static_cast<uint8_t>( static_cast<uint16_t>(right)       & 0xFF);
    f[8] = static_cast<uint8_t>((durationMs >> 8) & 0xFF);
    f[9] = static_cast<uint8_t>( durationMs       & 0xFF);
    f[10] = xorChecksum(f.data(), 10);
    return f;
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
