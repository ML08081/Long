// Host-side SPI/GPIO capture: never touches physical hardware.
#include "modules/display/St7789Display.h"
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <map>
#include <vector>
#include <fcntl.h>
#include <unistd.h>
#include <linux/spi/spidev.h>

struct Transfer { bool data; std::vector<unsigned char> bytes; };
static std::vector<Transfer> transfers;
static std::map<int,std::string> handles;
static int nextFd=10000, dc=0, spiMode=-1;
static bool failIo=false, failGpio=false;
extern "C" int __real_open(const char*, int, ...);
extern "C" int __real_close(int);
extern "C" ssize_t __real_write(int,const void*,size_t);
extern "C" int __real_access(const char*,int);
extern "C" int __wrap_open(const char* p, int flags, ...) {
    if (std::strstr(p,"/sys/class/gpio") || std::strstr(p,"/dev/spidev")) {
        const int fd=nextFd++; handles[fd]=p; return fd;
    }
    assert(!(flags & O_CREAT));
    return __real_open(p,flags);
}
extern "C" int __wrap_close(int fd) {
    if (handles.erase(fd)) return 0;
    return __real_close(fd);
}
extern "C" ssize_t __wrap_write(int fd,const void* p,size_t n) {
    if (!handles.count(fd)) return __real_write(fd,p,n);
    if (failGpio) { errno=EIO; return -1; }
    if (handles[fd]=="/sys/class/gpio/gpio73/value") dc=*(const char*)p=='1';
    return static_cast<ssize_t>(n);
}
extern "C" int __wrap_access(const char* p,int mode) {
    return std::strstr(p,"/sys/class/gpio") ? 0 : __real_access(p,mode);
}
extern "C" int __wrap_usleep(useconds_t) { return 0; }
extern "C" int __wrap_ioctl(int fd,unsigned long request,...) {
    assert(handles.count(fd));
    va_list ap; va_start(ap,request); void* p=va_arg(ap,void*); va_end(ap);
    if (failIo) { errno=EIO; return -1; }
    if (request==SPI_IOC_WR_MODE) spiMode=*(unsigned char*)p;
    if (request!=SPI_IOC_MESSAGE(1)) return 0;
    const auto* t=static_cast<spi_ioc_transfer*>(p);
    const auto* b=reinterpret_cast<const unsigned char*>(t->tx_buf);
    transfers.push_back({dc!=0, std::vector<unsigned char>(b,b+t->len)});
    return static_cast<int>(t->len);
}
namespace patrol {
struct DisplayTestAccess {
    static void snapshot(const St7789Display& d,const std::string& path) {
        std::ofstream f(path,std::ios::binary);
        assert(f.good());
        f << "P6\n" << d.cfg_.width << " " << d.cfg_.height << "\n255\n";
        for (uint16_t p:d.fb_) {
            const char rgb[]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};
            f.write(rgb,3);
        }
    }
    static bool glyph(uint32_t cp) { return St7789Display::cjkGlyph(cp)!=nullptr; }
};
}
static std::vector<unsigned char> commandData(unsigned char cmd) {
    for (size_t i=0;i+1<transfers.size();++i)
        if (!transfers[i].data && transfers[i].bytes==std::vector<unsigned char>{cmd})
            return transfers[i+1].data ? transfers[i+1].bytes : std::vector<unsigned char>{};
    return {};
}
static void frameCheck() {
    assert(commandData(0x2A)==(std::vector<unsigned char>{0,0,0,127}));
    assert(commandData(0x2B)==(std::vector<unsigned char>{0,0,0,159}));
    bool pixels=false; size_t bytes=0;
    for (const auto& t:transfers) {
        if (!t.data && t.bytes==std::vector<unsigned char>{0x2C}) pixels=true;
        else if (pixels) { assert(t.data); bytes+=t.bytes.size(); }
    }
    assert(bytes==128*160*2);
}
int main(int argc,char** argv) {
    assert(argc==2);
    using namespace patrol;
    St7789Display::Config c;
    c.enabled=true; c.controller="ST7735S"; c.width=128; c.height=160;
    c.gpioDC=73; c.gpioRST=72; c.gpioBL=-1; c.spiHz=1000000; c.spiMode=0;
    St7789Display d;
    assert(d.init(c)); assert(spiMode==0); assert(d.relayPageCount()==3);
    // Reference: TEXR/lloled/Core/Src/oled.c OLED_Init, not a generic tab profile.
    std::vector<unsigned char> cmds;
    for (const auto& t:transfers) if (!t.data) cmds.push_back(t.bytes.at(0));
    assert(cmds==(std::vector<unsigned char>{1,0x11,0x3A,0x36,0x21,0x13,0x29,0x2A,0x2B,0x2C}));
    assert(commandData(0x3A)==std::vector<unsigned char>{0x55});
    assert(commandData(0x36)==std::vector<unsigned char>{0xC0}); frameCheck();
    net::SensorData s{}; s.mode=3; s.distance_cm=123; s.laser_cm=80;
    s.gas_ppm=1740; s.temperature_01c=265; s.humidity_01=630; s.flags=net::SF_DHT_OK;
    for (int risk=0;risk<4;++risk) {
        s.risk_level=risk; transfers.clear(); d.showSensorPage(s); frameCheck();
        DisplayTestAccess::snapshot(d,std::string(argv[1])+"/sensor"+std::to_string(risk)+".ppm");
    }
    s.fault=1; transfers.clear(); d.showSensorPage(s); frameCheck();
    DisplayTestAccess::snapshot(d,std::string(argv[1])+"/offline.ppm");
    St7789Display::RelayInfo r;
    r.localIp="192.168.3.112"; r.upperOnline=true; r.upperPeer="192.168.255.255:65535";
    r.upperDurS=4294967295u; r.f4Open=true; r.telemFresh=true; r.envFresh=true;
    r.telemCnt=4294967295u; r.envCnt=4294967295u; r.thermalCnt=4294967295u;
    r.thermalMaxC10=658; r.hotspot=true; r.visionOn=true; r.visionName="fire";
    r.visionConf=97; r.version="1.24.0"; r.uptimeS=4294967295u;
    for (int p=0;p<3;++p) {
        transfers.clear(); d.showRelayPage(r,p); frameCheck();
        DisplayTestAccess::snapshot(d,std::string(argv[1])+"/relay"+std::to_string(p)+".ppm");
    }
    failIo=true; assert(!d.init(c)); assert(!d.isOpen()); failIo=false;
    failGpio=true; assert(!d.init(c)); assert(!d.isOpen()); failGpio=false;
    c.width=240; assert(!d.init(c)); assert(!d.isOpen());
    c.width=128; c.rotation=90; assert(!d.init(c)); c.rotation=0;
    // Old panel remains available through explicit ST7789 config.
    c.controller="ST7789"; c.width=240; c.height=320; c.spiMode=0;
    transfers.clear(); assert(d.init(c)); assert(d.relayPageCount()==1);
    assert(commandData(0x36)==std::vector<unsigned char>{0});
    assert(spiMode==0);
    std::puts("PASS: lloled init, frame bounds/length, four pages, offline, I/O errors, ST7789 compatibility");
}
