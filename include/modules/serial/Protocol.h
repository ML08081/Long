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
// ==========================================================================

namespace patrol {
namespace serial_proto {

constexpr uint8_t HEADER          = 0xAA;
constexpr uint8_t EXT_MARKER      = 0x5A;   // 扩展遥测帧第二字节
constexpr uint8_t THERMAL_MARKER  = 0x5B;   // 热成像行帧第二字节
constexpr uint8_t TELEMETRY_FLAG  = 0x80;   // 旧遥测帧 mode 的 bit7
constexpr uint8_t CMD_FRAME_LEN   = 7;
constexpr uint8_t EXT_PAYLOAD_LEN = 15;
constexpr uint8_t EXT_FRAME_LEN   = 3 + EXT_PAYLOAD_LEN + 1;  // = 19

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
