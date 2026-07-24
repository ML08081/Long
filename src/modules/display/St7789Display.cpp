#include "modules/display/St7789Display.h"
#include "modules/display/CjkFont16.h"
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

// ─── 中文点阵 + UTF-8 混排 ─────────────────────────────────────────────────
const uint8_t* St7789Display::cjkGlyph(uint32_t cp) {
    int lo = 0, hi = kCjk16Count - 1;   // 码点升序，二分查找
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        uint32_t c = kCjk16[mid].cp;
        if (c == cp) return kCjk16[mid].rows;
        if (c < cp) lo = mid + 1; else hi = mid - 1;
    }
    return nullptr;
}

// ASCII：基础 8(宽)×16(高)，按 scale 放大（水平 scale、垂直 2*scale）
void St7789Display::drawAscii16(int x, int y, char c, uint16_t fg, uint16_t bg, int scale) {
    uint8_t ch = static_cast<uint8_t>(c);
    const uint8_t* g = (ch >= 0x20 && ch < 0x80) ? kFont8x8[ch - 0x20] : kFont8x8[0];
    for (int row = 0; row < 8; ++row)
        for (int col = 0; col < 8; ++col) {
            uint16_t color = (g[row] & (1 << col)) ? fg : bg;
            fillRect(x + col * scale, y + row * 2 * scale, scale, 2 * scale, color);
        }
}

// 中文：16×16 点阵（每行 2 字节，MSB=最左），按 scale 放大
void St7789Display::drawCjk(int x, int y, uint32_t cp, uint16_t fg, uint16_t bg, int scale) {
    const uint8_t* g = cjkGlyph(cp);
    if (!g) { fillRect(x, y, 16 * scale, 16 * scale, bg); return; }   // 未收录 -> 空格
    for (int row = 0; row < 16; ++row) {
        uint8_t b0 = g[row * 2], b1 = g[row * 2 + 1];
        for (int col = 0; col < 16; ++col) {
            uint8_t byte = (col < 8) ? b0 : b1;
            int bit = 7 - (col & 7);
            uint16_t color = (byte & (1 << bit)) ? fg : bg;
            fillRect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}

int St7789Display::drawU8(int x, int y, const std::string& s, uint16_t fg, uint16_t bg, int scale) {
    size_t i = 0; int cx = x;
    while (i < s.size()) {
        uint8_t b = static_cast<uint8_t>(s[i]);
        uint32_t cp; int adv;
        if (b < 0x80) {                                   // ASCII
            cp = b; i += 1;
            if (cp == '\n') { cx = x; y += 16 * scale; continue; }
            drawAscii16(cx, y, static_cast<char>(cp), fg, bg, scale);
            adv = 8 * scale;
        } else if ((b & 0xE0) == 0xC0 && i + 1 < s.size()) {
            cp = ((b & 0x1F) << 6) | (static_cast<uint8_t>(s[i+1]) & 0x3F); i += 2;
            drawCjk(cx, y, cp, fg, bg, scale); adv = 16 * scale;
        } else if ((b & 0xF0) == 0xE0 && i + 2 < s.size()) {
            cp = ((b & 0x0F) << 12) | ((static_cast<uint8_t>(s[i+1]) & 0x3F) << 6)
               | (static_cast<uint8_t>(s[i+2]) & 0x3F); i += 3;
            drawCjk(cx, y, cp, fg, bg, scale); adv = 16 * scale;
        } else { i += 1; continue; }                      // 非法字节跳过
        cx += adv;
    }
    return cx;
}

// ─── 页面公用 ──────────────────────────────────────────────────────────────
// 标题条：加高到 32px，标题中文放大到 scale 1.5 效果（用 scale=1 但整体更大留白）
void St7789Display::titleBar(const char* zh) {
    fillRect(0, 0, cfg_.width, 34, rgb(18,64,96));
    drawU8(10, 8, zh, rgb(0,225,245), rgb(18,64,96));
}
int St7789Display::rowLabel(int y, const char* zh, uint16_t col) {
    int x = drawU8(10, y, zh, col, rgb(0,0,0));
    return x + 8;
}

// ─── 第 1 页：巡检传感数据 ─────────────────────────────────────────────────
void St7789Display::showSensorPage(const net::SensorData& s) {
    if (spiFd_ < 0) return;
    const uint16_t BG=rgb(0,0,0), WHITE=rgb(235,235,235), CYAN=rgb(0,210,230);
    const uint16_t GREEN=rgb(0,200,80), YELL=rgb(245,205,0), RED=rgb(235,45,45);
    // 模式名中文化，与三端语义一致：0 遥控 / 1 循迹 / 2 避障 / 3 巡检
    static const char* modeZh[] = {"遥控","循迹","避障","巡检"};
    // 底部风险块按要求保留英文（RISK 与 SAFE 那一栏）
    static const char* riskEn[] = {"SAFE","NOTE","WARN","DANGER"};
    static const uint16_t riskCol[] = {GREEN, CYAN, YELL, RED};

    const uint16_t GRAY=rgb(90,90,90);
    clear(BG);
    titleBar("巡检状态");

    // 链路是否新鲜：fault=1 表示 F4 遥测超时(>1s 无帧)。此时 F4 来的实时量一律显示 "无"，
    // 绝不把上一帧旧值当实时显示（根治"掉线后数据被冻结"的误导）；标题右侧加离线角标。
    const bool stale = (s.fault != 0);
    if (stale) drawU8(cfg_.width - 8 - 2*16, 6, "离线", RED, CYAN);

    // 6 行放大留白：pitch 30，起始 48，铺满到风险块上沿
    const int pitch = 30;
    int y = 48; char v[40];
    // 模式行青色高亮，便于一眼确认当前处于哪种运行模式
    { int x = rowLabel(y,"模式", CYAN);
      drawU8(x,y, modeZh[s.mode & 0x03], CYAN, BG); } y += pitch;
    { int x = rowLabel(y,"距离");
      if (!stale && s.distance_cm) {
          std::snprintf(v,sizeof v,"%u", s.distance_cm);
          int xv = drawU8(x,y, v, WHITE, BG);
          drawU8(xv+4,y, "厘米", WHITE, BG);
      } else drawU8(x,y, "无", GRAY, BG); } y += pitch;
    { int x = rowLabel(y,"激光");
      if (!stale && s.laser_cm) {
          std::snprintf(v,sizeof v,"%u", s.laser_cm);
          int xv = drawU8(x,y, v, WHITE, BG);
          drawU8(xv+4,y, "厘米", WHITE, BG);
      } else drawU8(x,y, "无", GRAY, BG); } y += pitch;
    { int x = rowLabel(y,"气体");
      if (!stale) {
          std::snprintf(v,sizeof v,"%u", s.gas_ppm);
          drawU8(x,y, v, (s.flags & net::SF_GAS_ALARM) ? RED : WHITE, BG);
      } else drawU8(x,y, "无", GRAY, BG); } y += pitch;
    // 温湿度：DHT 有效位未置位说明 F4 侧传感器无读数，显示"无"而非旧值，避免误判为实时
    { int x = rowLabel(y,"温度");
      if (!stale && (s.flags & net::SF_DHT_OK)) {
          std::snprintf(v,sizeof v,"%d", s.temperature_01c/10);
          int xv = drawU8(x,y, v, WHITE, BG);
          drawU8(xv+4,y, "度", WHITE, BG);
      } else drawU8(x,y, "无", GRAY, BG); } y += pitch;
    { int x = rowLabel(y,"湿度");
      if (!stale && (s.flags & net::SF_DHT_OK)) {
          std::snprintf(v,sizeof v,"%u %%", s.humidity_01/10);
          drawU8(x,y, v, WHITE, BG);
      } else drawU8(x,y, "无", GRAY, BG); } y += pitch;

    // 底部风险大块（放大：高 92px，英文风险词 scale 4 => 32x64）。
    //   链路超时时不显示可能已过期的风险，改灰底 "NO LINK"，如实反映连接状态。
    int rl = s.risk_level & 0x03;
    int bh = 92, by = cfg_.height - bh;
    uint16_t blkCol = stale ? GRAY : riskCol[rl];
    fillRect(0, by, cfg_.width, bh, blkCol);
    if (stale) {
        // 链路断开不属于 RISK/SAFE 语义，用中文如实反映连接状态
        drawU8(10, by + 8, "连接", rgb(0,0,0), blkCol);
        const char* rw = "无连接";
        int rwWidth = 3 * 16 * 3;                                  // scale3 汉字宽 48/字
        int rx = (cfg_.width - rwWidth) / 2;  if (rx < 4) rx = 4;
        drawU8(rx, by + 24, rw, rgb(0,0,0), blkCol, 3);
    } else {
        drawU8(10, by + 8, "RISK", rgb(0,0,0), blkCol);   // 按要求：本栏保留英文
        const char* rw = riskEn[rl];
        int rwWidth = static_cast<int>(std::strlen(rw)) * 8 * 4;   // scale4 ASCII 宽 32/字
        int rx = (cfg_.width - rwWidth) / 2;  if (rx < 4) rx = 4;
        drawU8(rx, by + 24, rw, rgb(0,0,0), blkCol, 4);
        // 告警角标改中文（避障锁优先级最高：它意味着车已被强制停住，操作者最需要知道）
        if (s.flags & net::SF_OBS_LOCK)       drawU8(cfg_.width-2*16-6, by + 8, "锁停", RED, blkCol);
        else if (s.flags & net::SF_FLAME)     drawU8(cfg_.width-2*16-6, by + 8, "火警", RED, blkCol);
        else if (s.flags & net::SF_GAS_ALARM) drawU8(cfg_.width-2*16-6, by + 8, "气体", RED, blkCol);
    }

    flush();
}

// ─── 第 2 页：中转 / 连接状态 ──────────────────────────────────────────────
void St7789Display::showRelayPage(const RelayInfo& r) {
    if (spiFd_ < 0) return;
    const uint16_t BG=rgb(0,0,0), WHITE=rgb(235,235,235), CYAN=rgb(0,210,230);
    const uint16_t GREEN=rgb(0,210,90), YELL=rgb(245,205,0), RED=rgb(235,45,45);
    const uint16_t GRAY=rgb(90,90,90);

    clear(BG);
    titleBar("中转状态");

    const int pitch = 27;
    int y = 44; char v[48];
    // 上位机在线状态（醒目）
    { int x = rowLabel(y,"上位机");
      if (r.upperOnline) drawU8(x,y,"在线", GREEN, BG);
      else               drawU8(x,y,"离线", RED,   BG); } y += pitch;
    // 龙芯本机 IP（中转站自身地址）
    { int x = rowLabel(y,"本机");
      drawU8(x,y, r.localIp.empty()? std::string("--") : r.localIp, CYAN, BG); } y += pitch;
    // 上位机对端地址
    { int x = rowLabel(y,"地址");
      drawU8(x,y, r.upperOnline ? r.upperPeer : std::string("--"), WHITE, BG); } y += pitch;
    { int x = rowLabel(y,"时长");
      if (r.upperOnline) { std::snprintf(v,sizeof v,"%u 秒", r.upperDurS); drawU8(x,y,v,WHITE,BG); }
      else drawU8(x,y,"无",GRAY,BG); } y += pitch;
    // 下位机 F4
    { int x = rowLabel(y,"下位机");
      if (r.f4Open && r.telemFresh) drawU8(x,y,"正常", GREEN, BG);
      else if (r.f4Open)            drawU8(x,y,"无数据", YELL, BG);
      else                          drawU8(x,y,"未接", RED, BG); } y += pitch;
    // 链路计数：遥测 / 环境
    { int x = rowLabel(y,"遥测");  std::snprintf(v,sizeof v,"%u", r.telemCnt);
      x = drawU8(x,y, v, r.telemFresh?GREEN:WHITE, BG);
      x = drawU8(x+12,y,"环境", CYAN, BG); std::snprintf(v,sizeof v,"%u", r.envCnt);
      drawU8(x,y, v, r.envFresh?GREEN:WHITE, BG); } y += pitch;
    // 热成像
    { int x = rowLabel(y,"热成像"); std::snprintf(v,sizeof v,"%u 帧", r.thermalCnt);
      x = drawU8(x,y, v, WHITE, BG);
      if (r.thermalMaxC10 > -1000) { std::snprintf(v,sizeof v," %d 度", r.thermalMaxC10/10);
        drawU8(x,y, v, r.hotspot?RED:WHITE, BG); } } y += pitch;
    // 视觉
    { int x = rowLabel(y,"视觉");
      if (r.visionOn) { std::snprintf(v,sizeof v,"%s %u%%", r.visionName.c_str(), r.visionConf);
        drawU8(x,y, v, GREEN, BG); }
      else drawU8(x,y,"关闭", WHITE, BG); } y += pitch;
    // 版本 / 运行时间
    { int x = rowLabel(y,"版本"); drawU8(x,y, std::string("v")+r.version, WHITE, BG); } y += pitch;
    { int x = rowLabel(y,"运行"); std::snprintf(v,sizeof v,"%u 秒", r.uptimeS);
      drawU8(x,y, v, WHITE, BG); }

    flush();
}

// 兼容旧接口
void St7789Display::showStatus(const net::SensorData& s, const std::string& statusLine) {
    (void)statusLine;
    showSensorPage(s);
}

} // namespace patrol
