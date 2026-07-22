#ifndef PATROL_MODULES_CONFIG_CONFIGMANAGER_H
#define PATROL_MODULES_CONFIG_CONFIGMANAGER_H

#include <string>
#include <cstdint>

namespace patrol {

// 从 JSON 配置文件加载运行参数。未出现的键保留默认值。
class ConfigManager {
public:
    struct CameraConf {
        std::string device = "/dev/video0";
        int         width  = 1280;
        int         height = 720;
        int         fps    = 30;
    };

    struct NetworkConf {
        std::string bind = "0.0.0.0";
        uint16_t    port = 8080;
    };

    struct SerialConf {
        std::string device = "/dev/ttyS1";
        int         baud   = 115200;
        // 热成像专用串口（双串口方案：F4 USART1 → 龙芯此口，仅收 0x5B 行帧）
        std::string thermalDevice = "/dev/ttyS2";
        int         thermalBaud   = 115200;
    };

    // 传输层选择（实施方案 §3.7.8）：type="can" 走 SocketCAN；"uart" 走串口(回退)。
    struct LinkConf {
        std::string type    = "uart";     // "can" | "uart"
        std::string canIf   = "can0";
        int         bitrate = 500000;      // 记录用；实际由 can0.service 起总线
        int         nodeId  = 1;           // F4 CANopen Node ID
    };

    // SPI 状态小屏（ST7789，默认关闭）
    struct DisplayConf {
        bool        enabled = false;
        std::string spiDev  = "/dev/spidev1.0";
        int         gpioDC  = 40;
        int         gpioRST = 41;
        int         gpioBL  = 42;
        int         width   = 240;
        int         height  = 320;   // GMT020-02 (ST7789V) 原生 240x320
        int         spiHz   = 40000000;
        int         rotation = 0;
    };

    // 加载 JSON 文件。失败时保留默认值，返回 false。
    bool load(const std::string& path);

    // 将当前配置写回文件
    bool save(const std::string& path) const;

    const CameraConf&  camera()  const { return camera_; }
    const NetworkConf& network() const { return network_; }
    const SerialConf&  serial()  const { return serial_; }
    const LinkConf&    link()    const { return link_; }
    const DisplayConf& display() const { return display_; }

    // 供 CLI 参数覆盖
    CameraConf&  camera()  { return camera_; }
    NetworkConf& network() { return network_; }
    SerialConf&  serial()  { return serial_; }

private:
    CameraConf  camera_;
    NetworkConf network_;
    SerialConf  serial_;
    LinkConf    link_;
    DisplayConf display_;
};

} // namespace patrol

#endif // PATROL_MODULES_CONFIG_CONFIGMANAGER_H