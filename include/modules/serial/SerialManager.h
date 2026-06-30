#ifndef PATROL_MODULES_SERIAL_SERIALMANAGER_H
#define PATROL_MODULES_SERIAL_SERIALMANAGER_H

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

// 串口管理（龙芯 <-> 下位机 MCU）。待硬件引脚确认后完整实现。
namespace patrol {

class SerialManager {
public:
    using RxCallback = std::function<void(uint8_t cmd, const uint8_t* payload, size_t len)>;

    SerialManager() = default;
    ~SerialManager();

    SerialManager(const SerialManager&) = delete;
    SerialManager& operator=(const SerialManager&) = delete;

    bool open(const std::string& device, int baud = 115200);
    void close();
    bool isOpen() const { return fd_ >= 0; }

    void setRxCallback(RxCallback cb) { rxCb_ = std::move(cb); }

    bool sendCmd(uint8_t cmd, const uint8_t* payload = nullptr, uint8_t payloadLen = 0);

    // 主循环调用：读取串口数据并解帧
    void poll();

private:
    int fd_ = -1;
    RxCallback rxCb_;
    std::vector<uint8_t> rxBuf_;
};

} // namespace patrol

#endif // PATROL_MODULES_SERIAL_SERIALMANAGER_H