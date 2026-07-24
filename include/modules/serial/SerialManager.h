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

    // 热成像专线诊断（0x5D 原始帧 / 0x5E EEPROM 分块）。
    //   用于区分"F4 没发"、"分块收不全"、"CRC 被丢"三种故障，避免只看整帧计数抓瞎。
    struct ThermalDiag {
        uint32_t rawChunks   = 0;   // 收到的 0x5D 分块总数
        uint32_t eeChunks    = 0;   // 收到的 0x5E 分块总数
        uint32_t rawFrames   = 0;   // 组装完成的原始帧数
        uint32_t eeFrames    = 0;   // 组装完成的 EEPROM 份数
        uint32_t crcDrop     = 0;   // 因 XOR 校验失败被丢弃的分块数
        uint8_t  rawMaxIdx   = 0;   // 见过的最大分块序号（判断尾块是否缺失的关键）
        uint8_t  rawLastIdx  = 0;   // 最近一个分块序号
        uint32_t rawIncomplete = 0; // 新一帧开始(idx=0)时上一帧仍未凑齐的次数
    };
    ThermalDiag thermalDiag() const { return tdiag_; }

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

    // 发送任意已打包协议帧（调用方须保证只在控制线程使用）
    bool sendFrame(const uint8_t* data, size_t len);

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
    // 原始帧分块到齐判定：834 字 / 32 字每块 = 27 块(末块只有 2 字)，用位掩码逐块打勾。
    //   旧实现只在"本块末字触达总长"时触发，尾块一旦丢失整帧就永远不完成且无迹可寻。
    uint32_t                rawMask_ = 0;
    bool                    rawFired_ = false;
    ThermalDiag             tdiag_{};
};

} // namespace patrol

#endif // PATROL_MODULES_SERIAL_SERIALMANAGER_H
