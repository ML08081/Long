#ifndef PATROL_MODULES_SERIAL_SERIALMANAGER_H
#define PATROL_MODULES_SERIAL_SERIALMANAGER_H

#include "modules/serial/Protocol.h"

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// 串口管理（龙芯 <-> F4）。收发 F4 协议帧（见 Protocol.h）。
namespace patrol {

class SerialManager {
public:
    using TelemetryCallback = std::function<void(const serial_proto::Telemetry&)>;
    // 组装完一整幅热成像(32x24 int16, 0.01°C, 行主序)后回调
    using ThermalCallback = std::function<void(const int16_t* temps, int cols, int rows)>;
    // 收到环境/安全遥测帧(0x5C)后回调
    using EnvCallback = std::function<void(const serial_proto::EnvData&)>;
    // 收到完整 MLX90640 原始帧(834 字)后回调（龙芯据此解算）
    using RawThermalCallback = std::function<void(const uint16_t* raw, int words)>;
    // 收到完整 MLX90640 EEPROM(832 字)后回调（一次性，用于提取标定参数）
    using EepromCallback = std::function<void(const uint16_t* ee, int words)>;

    SerialManager() = default;
    ~SerialManager();

    SerialManager(const SerialManager&) = delete;
    SerialManager& operator=(const SerialManager&) = delete;

    bool open(const std::string& device, int baud = 115200);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    void setTelemetryCallback(TelemetryCallback cb) { cb_ = std::move(cb); }
    void setThermalCallback(ThermalCallback cb) { thermalCb_ = std::move(cb); }
    void setEnvCallback(EnvCallback cb) { envCb_ = std::move(cb); }
    void setRawThermalCallback(RawThermalCallback cb) { rawThermalCb_ = std::move(cb); }
    void setEepromCallback(EepromCallback cb) { eepromCb_ = std::move(cb); }

    // 发送命令帧（龙芯 -> F4）。speed/steering 会被限幅到 [-1000,1000]。
    bool sendCommand(int16_t speed, int16_t steering, uint8_t mode);

    // 主循环调用：读取串口数据并解析遥测帧，对每个完整帧调用回调
    void poll();

private:
    int                     fd_ = -1;
    TelemetryCallback       cb_;
    ThermalCallback         thermalCb_;
    EnvCallback             envCb_;
    RawThermalCallback      rawThermalCb_;
    EepromCallback          eepromCb_;
    std::vector<uint8_t>    rxBuf_;
    int16_t                 thermalBuf_[serial_proto::THERMAL_PIXELS] = {0};  // 行组装缓冲
    // 原始热成像帧 / EEPROM 分块组装缓冲
    uint16_t                rawAsm_[serial_proto::MLX_FRAME_WORDS] = {0};
    uint16_t                eeAsm_[serial_proto::MLX_EE_WORDS]     = {0};
};

} // namespace patrol

#endif // PATROL_MODULES_SERIAL_SERIALMANAGER_H
