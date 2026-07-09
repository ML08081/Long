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
    // 8x8 位图字体（纯 ASCII），scale 放大整数倍；越界自动裁剪
    void drawText(int x, int y, const std::string& s, uint16_t fg, uint16_t bg, int scale = 2);
    // UTF-8 混排：ASCII 8x16、中文 16x16（乘 scale）。返回绘制结束 x（便于同行接着画值）。
    int  drawU8(int x, int y, const std::string& s, uint16_t fg, uint16_t bg, int scale = 1);
    void flush();   // 把后备缓冲整帧推到屏

    // 中转/连接状态页所需信息（第 2 页）
    struct RelayInfo {
        std::string localIp;               // 龙芯本机 IP（中转站自身地址）
        bool        upperOnline = false;   // 上位机 LongLook 是否已连接
        std::string upperPeer;             // 对端 IP:port
        uint32_t    upperDurS   = 0;       // 本次在线时长(秒)
        bool        f4Open      = false;   // 下位机 F4 串口是否打开
        uint32_t    telemCnt    = 0, envCnt = 0, thermalCnt = 0;
        bool        telemFresh  = false, envFresh = false;  // 近 2s 有帧=链路活
        bool        visionOn    = false;   // 近 3s 有视觉结果
        std::string visionName;
        uint8_t     visionConf  = 0;
        int         thermalMaxC10 = -1000;
        bool        hotspot     = false;
        std::string version;               // 固件版本串
        uint32_t    uptimeS     = 0;       // 本进程运行时长(秒)
    };

    // 高层页面（1Hz 刷新，轮播切换）
    void showSensorPage(const net::SensorData& s);   // 第 1 页：巡检传感数据
    void showRelayPage(const RelayInfo& r);          // 第 2 页：中转/连接状态
    // 兼容旧接口（等价于 showSensorPage）
    void showStatus(const net::SensorData& s, const std::string& statusLine);

private:
    // 16x16 中文点阵（码点二分查找），未收录字返回 nullptr
    static const uint8_t* cjkGlyph(uint32_t cp);
    void drawCjk(int x, int y, uint32_t cp, uint16_t fg, uint16_t bg, int scale = 1);  // 16*scale
    void drawAscii16(int x, int y, char c, uint16_t fg, uint16_t bg, int scale = 1);   // 8x16*scale
    // 页面公用小工具：标题条 + 一行“标签: 值”
    void titleBar(const char* zh);
    int  rowLabel(int y, const char* zhLabel, uint16_t col = 0xFFFF);
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
