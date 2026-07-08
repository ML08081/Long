#include "modules/display/St7789Display.h"
#include "modules/logger/Logger.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>

namespace patrol {

// ─── 8x8 ASCII 字体 (0x20~0x7F, 每字符 8 字节, 每字节一行, bit0=最左像素) ──
// 公有领域字体 font8x8_basic (dhepper/font8x8) 的可打印子集。
static const uint8_t kFont8x8[96][8] = {
{0,0,0,0,0,0,0,0},{0x18,0x3C,0x3C,0x18,0x18,0,0x18,0},{0x36,0x36,0,0,0,0,0,0},
{0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0},{0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0},
{0,0x63,0x33,0x18,0x0C,0x66,0x63,0},{0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0},
{0x06,0x06,0x03,0,0,0,0,0},{0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0},
{0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0},{0,0x66,0x3C,0xFF,0x3C,0x66,0,0},
{0,0x0C,0x0C,0x3F,0x0C,0x0C,0,0},{0,0,0,0,0,0x0C,0x0C,0x06},
{0,0,0,0x3F,0,0,0,0},{0,0,0,0,0,0x0C,0x0C,0},{0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0},
{0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0},{0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0},
{0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0},{0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0},
{0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0},{0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0},
{0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0},{0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0},
{0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0},{0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0},
{0,0x0C,0x0C,0,0,0x0C,0x0C,0},{0,0x0C,0x0C,0,0,0x0C,0x0C,0x06},
{0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0},{0,0,0x3F,0,0,0x3F,0,0},
{0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0},{0x1E,0x33,0x30,0x18,0x0C,0,0x0C,0},
{0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0},{0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0},
{0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0},{0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0},
{0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0},{0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0},
{0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0},{0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0},
{0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0},{0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
{0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0},{0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0},
{0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0},{0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0},
{0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0},{0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0},
{0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0},{0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0},
{0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0},{0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0},
{0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0},{0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0},
{0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0},{0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0},
{0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0},{0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0},
{0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0},{0x1E,0x06,0x06,0x06,0x06,0x06,0x1E,0},
{0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0},{0x1E,0x18,0x18,0x18,0x18,0x18,0x1E,0},
{0x08,0x1C,0x36,0x63,0,0,0,0},{0,0,0,0,0,0,0,0xFF},{0x0C,0x0C,0x18,0,0,0,0,0},
{0,0,0x1E,0x30,0x3E,0x33,0x6E,0},{0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0},
{0,0,0x1E,0x33,0x03,0x33,0x1E,0},{0x38,0x30,0x30,0x3e,0x33,0x33,0x6E,0},
{0,0,0x1E,0x33,0x3f,0x03,0x1E,0},{0x1C,0x36,0x06,0x0f,0x06,0x06,0x0F,0},
{0,0,0x6E,0x33,0x33,0x3E,0x30,0x1F},{0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0},
{0x0C,0,0x0E,0x0C,0x0C,0x0C,0x1E,0},{0x30,0,0x30,0x30,0x30,0x33,0x33,0x1E},
{0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0},{0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0},
{0,0,0x33,0x7F,0x7F,0x6B,0x63,0},{0,0,0x1F,0x33,0x33,0x33,0x33,0},
{0,0,0x1E,0x33,0x33,0x33,0x1E,0},{0,0,0x3B,0x66,0x66,0x3E,0x06,0x0F},
{0,0,0x6E,0x33,0x33,0x3E,0x30,0x78},{0,0,0x3B,0x6E,0x66,0x06,0x0F,0},
{0,0,0x3E,0x03,0x1E,0x30,0x1F,0},{0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0},
{0,0,0x33,0x33,0x33,0x33,0x6E,0},{0,0,0x33,0x33,0x33,0x1E,0x0C,0},
{0,0,0x63,0x6B,0x7F,0x7F,0x36,0},{0,0,0x63,0x36,0x1C,0x36,0x63,0},
{0,0,0x33,0x33,0x33,0x3E,0x30,0x1F},{0,0,0x3F,0x19,0x0C,0x26,0x3F,0},
{0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0},{0x18,0x18,0x18,0,0x18,0x18,0x18,0},
{0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0},{0x6E,0x3B,0,0,0,0,0,0},{0,0,0,0,0,0,0,0},
};

// ─── sysfs GPIO ──────────────────────────────────────────────────────────
bool St7789Display::gpioExport(int n) {
    char path[64];
    std::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d", n);
    if (::access(path, F_OK) == 0) return true;   // 已导出
    int fd = ::open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return false;
    char buf[16]; int l = std::snprintf(buf, sizeof(buf), "%d", n);
    bool ok = ::write(fd, buf, l) == l;
    ::close(fd);
    ::usleep(50000);   // 等 udev 建节点
    return ok;
}
bool St7789Display::gpioDir(int n, const char* dir) {
    char path[64];
    std::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", n);
    int fd = ::open(path, O_WRONLY);
    if (fd < 0) return false;
    bool ok = ::write(fd, dir, std::strlen(dir)) > 0;
    ::close(fd);
    return ok;
}
bool St7789Display::gpioWrite(int n, int val) {
    char path[64];
    std::snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", n);
    int fd = ::open(path, O_WRONLY);
    if (fd < 0) return false;
    char c = val ? '1' : '0';
    bool ok = ::write(fd, &c, 1) == 1;
    ::close(fd);
    return ok;
}

// ─── SPI ─────────────────────────────────────────────────────────────────
bool St7789Display::spiWrite(const uint8_t* d, size_t n) {
    while (n > 0) {
        size_t chunk = n > 4096 ? 4096 : n;   // spidev 单次传输上限保守取 4KB
        spi_ioc_transfer tr{};
        tr.tx_buf = reinterpret_cast<unsigned long>(d);
        tr.len    = static_cast<uint32_t>(chunk);
        tr.speed_hz = cfg_.spiHz;
        tr.bits_per_word = 8;
        if (::ioctl(spiFd_, SPI_IOC_MESSAGE(1), &tr) < 0) return false;
        d += chunk; n -= chunk;
    }
    return true;
}
void St7789Display::writeCmd(uint8_t c)  { gpioSet(cfg_.gpioDC, 0); spiWrite(&c, 1); }
void St7789Display::writeData(const uint8_t* d, size_t n) { gpioSet(cfg_.gpioDC, 1); spiWrite(d, n); }
void St7789Display::writeData8(uint8_t d) { writeData(&d, 1); }

void St7789Display::setAddrWindow(int x, int y, int w, int h) {
    int x1 = x + w - 1, y1 = y + h - 1;
    uint8_t buf[4];
    writeCmd(0x2A);   // CASET
    buf[0]=x>>8; buf[1]=x&0xFF; buf[2]=x1>>8; buf[3]=x1&0xFF; writeData(buf,4);
    writeCmd(0x2B);   // RASET
    buf[0]=y>>8; buf[1]=y&0xFF; buf[2]=y1>>8; buf[3]=y1&0xFF; writeData(buf,4);
    writeCmd(0x2C);   // RAMWR
}

bool St7789Display::init(const Config& cfg) {
    cfg_ = cfg;
    if (!cfg_.enabled) return false;

    spiFd_ = ::open(cfg_.spiDev.c_str(), O_RDWR);
    if (spiFd_ < 0) {
        LOG_ERROR("屏: 打开 %s 失败: %s", cfg_.spiDev.c_str(), std::strerror(errno));
        return false;
    }
    uint8_t mode = 0, bits = 8;
    ::ioctl(spiFd_, SPI_IOC_WR_MODE, &mode);
    ::ioctl(spiFd_, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ::ioctl(spiFd_, SPI_IOC_WR_MAX_SPEED_HZ, &cfg_.spiHz);

    // GPIO 初始化
    for (int g : {cfg_.gpioDC, cfg_.gpioRST, cfg_.gpioBL}) {
        if (g < 0) continue;
        if (!gpioExport(g) || !gpioDir(g, "out"))
            LOG_WARN("屏: GPIO%d 初始化失败(检查权限/占用)", g);
    }

    // 硬复位
    gpioSet(cfg_.gpioRST, 1); ::usleep(10000);
    gpioSet(cfg_.gpioRST, 0); ::usleep(20000);
    gpioSet(cfg_.gpioRST, 1); ::usleep(120000);

    // ST7789 初始化序列
    writeCmd(0x01); ::usleep(150000);            // SWRESET
    writeCmd(0x11); ::usleep(120000);            // SLPOUT
    writeCmd(0x3A); writeData8(0x55);            // COLMOD = 16bit/RGB565
    uint8_t madctl = 0x00;
    switch (cfg_.rotation) { case 90: madctl=0x60; break; case 180: madctl=0xC0; break; case 270: madctl=0xA0; break; }
    writeCmd(0x36); writeData8(madctl);          // MADCTL
    writeCmd(0x21);                              // INVON (ST7789 IPS 常需反显)
    writeCmd(0x13);                              // NORON
    writeCmd(0x29); ::usleep(50000);             // DISPON

    fb_.assign(static_cast<size_t>(cfg_.width) * cfg_.height, 0);
    gpioSet(cfg_.gpioBL, 1);                      // 背光开
    clear(0x0000);
    flush();
    LOG_INFO("屏: ST7789 %dx%d 就绪 (%s, DC/RST/BL=%d/%d/%d)",
             cfg_.width, cfg_.height, cfg_.spiDev.c_str(),
             cfg_.gpioDC, cfg_.gpioRST, cfg_.gpioBL);
    return true;
}

void St7789Display::close() {
    if (spiFd_ >= 0) { gpioSet(cfg_.gpioBL, 0); ::close(spiFd_); spiFd_ = -1; }
}
St7789Display::~St7789Display() { close(); }

// ─── 绘制（操作后备缓冲）──────────────────────────────────────────────────
void St7789Display::clear(uint16_t color) {
    for (auto& p : fb_) p = color;
}
void St7789Display::fillRect(int x, int y, int w, int h, uint16_t color) {
    for (int j = 0; j < h; ++j) {
        int yy = y + j; if (yy < 0 || yy >= cfg_.height) continue;
        for (int i = 0; i < w; ++i) {
            int xx = x + i; if (xx < 0 || xx >= cfg_.width) continue;
            fb_[static_cast<size_t>(yy) * cfg_.width + xx] = color;
        }
    }
}
void St7789Display::drawText(int x, int y, const std::string& s, uint16_t fg, uint16_t bg, int scale) {
    int cx = x;
    for (char ch : s) {
        if (ch == '\n') { cx = x; y += 8 * scale; continue; }
        uint8_t c = static_cast<uint8_t>(ch);
        const uint8_t* g = (c >= 0x20 && c < 0x80) ? kFont8x8[c - 0x20] : kFont8x8[0];
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col) {
                uint16_t color = (g[row] & (1 << col)) ? fg : bg;
                fillRect(cx + col * scale, y + row * scale, scale, scale, color);
            }
        cx += 8 * scale;
    }
}

void St7789Display::flush() {
    if (spiFd_ < 0) return;
    setAddrWindow(0, 0, cfg_.width, cfg_.height);
    // RGB565 转大端上屏
    std::vector<uint8_t> line(static_cast<size_t>(cfg_.width) * 2);
    gpioSet(cfg_.gpioDC, 1);
    for (int y = 0; y < cfg_.height; ++y) {
        for (int x = 0; x < cfg_.width; ++x) {
            uint16_t p = fb_[static_cast<size_t>(y) * cfg_.width + x];
            line[x * 2]     = p >> 8;
            line[x * 2 + 1] = p & 0xFF;
        }
        spiWrite(line.data(), line.size());
    }
}

// ─── 高层：巡检状态页 ──────────────────────────────────────────────────────
void St7789Display::showStatus(const net::SensorData& s, const std::string& statusLine) {
    if (spiFd_ < 0) return;
    const uint16_t BG = rgb(0, 0, 0), WHITE = rgb(230,230,230), CYAN = rgb(0,200,220);
    const uint16_t GREEN = rgb(0,200,80), YELL = rgb(240,200,0), RED = rgb(230,40,40);
    static const char* modeName[] = {"MANUAL","AUTO","AVOID","CRUISE"};
    static const char* riskName[] = {"SAFE","NOTE","WARN","DANGER"};
    static const uint16_t riskCol[] = {GREEN, CYAN, YELL, RED};

    clear(BG);
    // 顶部标题条
    fillRect(0, 0, cfg_.width, 22, rgb(20,60,90));
    drawText(6, 4, "PATROL", CYAN, rgb(20,60,90), 2);

    int y = 30;
    char buf[48];
    std::snprintf(buf, sizeof(buf), "MODE %s", modeName[s.mode & 0x03]);
    drawText(6, y, buf, WHITE, BG, 2); y += 22;
    std::snprintf(buf, sizeof(buf), "DIST %ucm", s.distance_cm);
    drawText(6, y, buf, WHITE, BG, 2); y += 22;
    std::snprintf(buf, sizeof(buf), "LASER %ucm", s.laser_cm);
    drawText(6, y, buf, WHITE, BG, 2); y += 22;
    std::snprintf(buf, sizeof(buf), "GAS %u", s.gas_ppm);
    drawText(6, y, buf, WHITE, BG, 2); y += 22;
    if (s.flags & net::SF_DHT_OK) {
        std::snprintf(buf, sizeof(buf), "T%d H%u", s.temperature_01c/10, s.humidity_01/10);
        drawText(6, y, buf, WHITE, BG, 2);
    }
    y += 22;
    std::snprintf(buf, sizeof(buf), "SPD %d/%d", s.speed_L, s.speed_R);
    drawText(6, y, buf, CYAN, BG, 2); y += 22;
    std::snprintf(buf, sizeof(buf), "ENC %ld/%ld", (long)s.encoder1, (long)s.encoder2);
    drawText(6, y, buf, CYAN, BG, 2); y += 22;
    // 告警行（火焰/气体）
    if (s.flags & net::SF_FLAME)          drawText(6, y, "FLAME!", RED, BG, 2);
    else if (s.flags & net::SF_GAS_ALARM) drawText(6, y, "GAS ALARM", RED, BG, 2);

    // 风险大字块
    int rl = s.risk_level & 0x03;
    fillRect(0, cfg_.height - 46, cfg_.width, 46, riskCol[rl]);
    drawText(6, cfg_.height - 38, riskName[rl], rgb(0,0,0), riskCol[rl], 3);
    if (s.flags & net::SF_FLAME)     drawText(150, cfg_.height - 38, "FIRE", rgb(0,0,0), riskCol[rl], 2);
    else if (s.flags & net::SF_GAS_ALARM) drawText(150, cfg_.height - 38, "GAS!", rgb(0,0,0), riskCol[rl], 2);

    (void)statusLine;
    flush();
}

} // namespace patrol
