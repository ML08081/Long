#ifndef PATROL_MODULES_SERIAL_PROTOCOL_H
#define PATROL_MODULES_SERIAL_PROTOCOL_H

#include <cstdint>
#include <cstddef>
#include <vector>

// ==========================================================================
//  串口协议（龙芯 SBC <-> 下位机 MCU）
//
//  帧格式：[0xAA][0x55][LEN(1B)][CMD(1B)][PAYLOAD(LEN-1 B)][CRC8]
//    LEN  = CMD字节 + payload 字节数
//    CRC8 = poly 0x07 覆盖 LEN..payload
//
//  方向：
//    0x1x  龙芯 -> MCU  (控制命令)
//    0x2x  MCU -> 龙芯  (上报/应答)
// ==========================================================================

namespace patrol {
namespace serial_proto {

constexpr uint8_t SOF1 = 0xAA;
constexpr uint8_t SOF2 = 0x55;
constexpr int     HEADER_SIZE = 4;   // SOF1+SOF2+LEN+CMD

// 龙芯 -> MCU
enum CmdByte : uint8_t {
    CMD_SET_SPEED  = 0x11,   // [int16 L mm/s][int16 R mm/s]
    CMD_SET_SERVO  = 0x12,   // [uint16 pulse_us]
    CMD_SET_FAN    = 0x13,   // [uint8 0/1]
    CMD_SET_BUZZER = 0x14,   // [uint8 0/1]
    CMD_SET_RELAY  = 0x15,   // [uint8 0/1]
    CMD_SET_LED    = 0x16,   // [uint8 0/1]
    CMD_ESTOP      = 0x17,   // 无 payload
    CMD_RESET      = 0x18,   // 无 payload
};

// MCU -> 龙芯
enum ReportByte : uint8_t {
    RPT_SENSORS  = 0x21,   // 传感器全量 16 字节
    RPT_ENCODERS = 0x22,   // [int32 enc1][int32 enc2]
    RPT_ACK      = 0x2F,   // [uint8 cmd_id][uint8 status]
};

// CRC-8 (poly 0x07)
inline uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
    }
    return crc;
}

inline std::vector<uint8_t> buildFrame(uint8_t cmd,
                                        const uint8_t* payload = nullptr,
                                        uint8_t payloadLen = 0) {
    std::vector<uint8_t> f;
    f.push_back(SOF1);
    f.push_back(SOF2);
    f.push_back(static_cast<uint8_t>(payloadLen + 1));  // LEN
    f.push_back(cmd);
    if (payload && payloadLen > 0)
        f.insert(f.end(), payload, payload + payloadLen);
    f.push_back(crc8(f.data() + 2, f.size() - 2));
    return f;
}

} // namespace serial_proto
} // namespace patrol

#endif // PATROL_MODULES_SERIAL_PROTOCOL_H