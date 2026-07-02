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

    SerialManager() = default;
    ~SerialManager();

    SerialManager(const SerialManager&) = delete;
    SerialManager& operator=(const SerialManager&) = delete;

    bool open(const std::string& device, int baud = 115200);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    void setTelemetryCallback(TelemetryCallback cb) { cb_ = std::move(cb); }

    // 发送命令帧（龙芯 -> F4）。speed/steering 会被限幅到 [-1000,1000]。
    bool sendCommand(int16_t speed, int16_t steering, uint8_t mode);

    // 主循环调用：读取串口数据并解析遥测帧，对每个完整帧调用回调
    void poll();

private:
    int                     fd_ = -1;
    TelemetryCallback       cb_;
    std::vector<uint8_t>    rxBuf_;
};

} // namespace patrol

#endif // PATROL_MODULES_SERIAL_SERIALMANAGER_H
