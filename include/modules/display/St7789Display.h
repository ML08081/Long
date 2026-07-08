#ifndef PATROL_MODULES_DISPLAY_ST7789_H
#define PATROL_MODULES_DISPLAY_ST7789_H

#include <cstdint>
#include <string>
#include <vector>

#include "modules/network/FrameProtocol.h"

// =============================================================================
//  St7789Display — 龙芯端 SPI 小屏驱动（ST7789 240x240，用户空间）
//
//  硬件：SPI 走 /dev/spidev1.0（板载 SPI2，已实测 status=okay）；
//        DC/RST/BL 用 sysfs GPIO（/sys/class/gpio，已实测 export 可用）。
//  不依赖任何内核 fbtft/DRM 面板驱动——纯用户空间，随 PatrolSystem 一起编译部署。
//
//  用法：init(cfg) -> showStatus()/drawText()/fillRect() -> 析构自动 close。
//  RGB565 颜色（大端上屏）。内部维护整帧后备缓冲，flush() 一次性推屏。
// =============================================================================

namespace patrol {

class St7789Display {
public:
    struct Config {
        bool        enabled = false;
        std::string spiDev  = "/dev/spidev1.0";
        int         gpioDC  = 40;    // 数据/命令选择
        int         gpioRST = 41;    // 复位
        int         gpioBL  = 42;    // 背光（-1=不控制/常亮）
        int         width   = 240;
        int         height  = 320;   // GMT020-02 (ST7789V) 原生 240x320
        uint32_t    spiHz   = 40000000;
        int         rotation = 0;    // 0/90/180/270（MADCTL）
    };

    ~St7789Display();

    bool init(const Config& cfg);
    void close();
    bool isOpen() const { return spiFd_ >= 0; }

    // RGB565 颜色构造
    static uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
        return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
    }

    void clear(uint16_t color);
    void fillRect(int x, int y, int w, int h, uint16_t color);
    // 8x8 位图字体，scale 放大整数倍；越界自动裁剪
    void drawText(int x, int y, const std::string& s, uint16_t fg, uint16_t bg, int scale = 2);
    void flush();   // 把后备缓冲整帧推到屏

    // 高层：把巡检状态渲染到屏（模式/距离/激光/风险/告警 + 一行状态文本）
    void showStatus(const net::SensorData& s, const std::string& statusLine);

private:
    void writeCmd(uint8_t c);
    void writeData(const uint8_t* d, size_t n);
    void writeData8(uint8_t d);
    void setAddrWindow(int x, int y, int w, int h);
    bool spiWrite(const uint8_t* d, size_t n);

    // sysfs GPIO
    static bool gpioExport(int n);
    static bool gpioDir(int n, const char* dir);
    static bool gpioWrite(int n, int val);
    void gpioSet(int n, int val) { if (n >= 0) gpioWrite(n, val); }

    Config cfg_;
    int    spiFd_ = -1;
    std::vector<uint16_t> fb_;   // 后备缓冲（RGB565，主机字节序）
};

} // namespace patrol

#endif // PATROL_MODULES_DISPLAY_ST7789_H
