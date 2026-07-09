/* st7789_probe.c —— GMT020-02-8P (ST7789V 240x320) 独立点屏测试
 * 直接驱动 /dev/spidev1.0 (SPI2) + sysfs GPIO, 不依赖 patrol 服务/spidev.h。
 * 接线: SCL=IO PIN21  SDA=IO PIN27  CS=IO PIN23  VCC=IO PIN2(3.3V)  GND=IO PIN1
 *       DC=IO PIN7=GPIO72   RST=IO PIN9=GPIO73   BL=IO PIN11=GPIO74
 * 用法: ./st7789_probe [spidev] [dc] [rst] [bl] [speedHz]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

/* spidev.h 在板子上缺失, 这里手动定义所需 ioctl */
#ifndef SPI_IOC_WR_MODE
#define SPI_IOC_WR_MODE          _IOW('k', 1, uint8_t)
#define SPI_IOC_WR_BITS_PER_WORD _IOW('k', 3, uint8_t)
#define SPI_IOC_WR_MAX_SPEED_HZ  _IOW('k', 4, uint32_t)
#endif

#define W 240
#define H 320
#define CHUNK 4096            /* spidev bufsiz 上限 */

static int spi_fd = -1;
static int dc_fd  = -1;       /* DC value fd, 常开加速 */

static void msleep(int ms){ usleep(ms*1000); }

/* ---- sysfs GPIO ---- */
static int gpio_export(int g){
    char p[64]; int fd;
    snprintf(p,sizeof p,"/sys/class/gpio/gpio%d/direction", g);
    if(access(p,F_OK)!=0){                       /* 未导出才导出 */
        fd=open("/sys/class/gpio/export",O_WRONLY);
        if(fd<0){perror("export");return -1;}
        char b[16]; int n=snprintf(b,sizeof b,"%d",g);
        if(write(fd,b,n)<0 && errno!=EBUSY){perror("write export");}
        close(fd); msleep(60);
    }
    fd=open(p,O_WRONLY); if(fd<0){perror("direction");return -1;}
    write(fd,"out",3); close(fd);
    return 0;
}
static int gpio_value_fd(int g){
    char p[64]; snprintf(p,sizeof p,"/sys/class/gpio/gpio%d/value",g);
    int fd=open(p,O_WRONLY); if(fd<0)perror("value open"); return fd;
}
static void gpio_set_fd(int fd,int v){ write(fd, v?"1":"0", 1); }
static void gpio_set(int g,int v){ int fd=gpio_value_fd(g); if(fd>=0){gpio_set_fd(fd,v);close(fd);} }

/* ---- SPI ---- */
static void spi_write(const uint8_t*buf,int len){
    int off=0;
    while(off<len){
        int n=len-off; if(n>CHUNK)n=CHUNK;
        int w=write(spi_fd, buf+off, n);
        if(w<0){perror("spi write");exit(1);}
        off+=w;
    }
}
static void wr_cmd(uint8_t c){ gpio_set_fd(dc_fd,0); spi_write(&c,1); }
static void wr_dat(const uint8_t*d,int n){ gpio_set_fd(dc_fd,1); spi_write(d,n); }
static void wr_d1(uint8_t d){ wr_dat(&d,1); }

static void set_window(int x0,int y0,int x1,int y1){
    uint8_t b[4];
    wr_cmd(0x2A); b[0]=x0>>8;b[1]=x0&0xff;b[2]=x1>>8;b[3]=x1&0xff; wr_dat(b,4);
    wr_cmd(0x2B); b[0]=y0>>8;b[1]=y0&0xff;b[2]=y1>>8;b[3]=y1&0xff; wr_dat(b,4);
    wr_cmd(0x2C);
}
static void fill(uint16_t color){
    static uint8_t line[W*2];
    for(int i=0;i<W;i++){ line[2*i]=color>>8; line[2*i+1]=color&0xff; }
    set_window(0,0,W-1,H-1);
    gpio_set_fd(dc_fd,1);
    for(int y=0;y<H;y++) spi_write(line,sizeof line);
}
static void bars(void){
    static const uint16_t c[8]={0xF800,0x07E0,0x001F,0xFFE0,0xF81F,0x07FF,0xFFFF,0x0000};
    static uint8_t line[W*2];
    for(int i=0;i<W;i++){ uint16_t col=c[(i*8)/W]; line[2*i]=col>>8; line[2*i+1]=col&0xff; }
    set_window(0,0,W-1,H-1);
    gpio_set_fd(dc_fd,1);
    for(int y=0;y<H;y++) spi_write(line,sizeof line);
}

int main(int argc,char**argv){
    const char*dev = argc>1?argv[1]:"/dev/spidev1.0";
    int dc  = argc>2?atoi(argv[2]):72;
    int rst = argc>3?atoi(argv[3]):73;
    int bl  = argc>4?atoi(argv[4]):74;
    uint32_t hz = argc>5?(uint32_t)strtoul(argv[5],0,10):24000000;

    printf("[probe] dev=%s DC=%d RST=%d BL=%d %uHz\n",dev,dc,rst,bl,hz);

    spi_fd=open(dev,O_RDWR); if(spi_fd<0){perror("open spidev");return 1;}
    uint8_t mode=0, bits=8;
    ioctl(spi_fd,SPI_IOC_WR_MODE,&mode);
    ioctl(spi_fd,SPI_IOC_WR_BITS_PER_WORD,&bits);
    ioctl(spi_fd,SPI_IOC_WR_MAX_SPEED_HZ,&hz);

    if(gpio_export(dc)||gpio_export(rst)||gpio_export(bl)){fprintf(stderr,"gpio export failed\n");return 1;}
    dc_fd=gpio_value_fd(dc); if(dc_fd<0)return 1;

    /* 硬复位 */
    gpio_set(rst,1); msleep(10);
    gpio_set(rst,0); msleep(20);
    gpio_set(rst,1); msleep(120);

    /* 初始化序列 */
    wr_cmd(0x01); msleep(150);            /* SWRESET */
    wr_cmd(0x11); msleep(120);            /* SLPOUT  */
    wr_cmd(0x3A); wr_d1(0x55);            /* COLMOD 16bit */
    wr_cmd(0x36); wr_d1(0x00);            /* MADCTL */
    wr_cmd(0x21);                         /* INVON (IPS) */
    wr_cmd(0x13); msleep(10);             /* NORON  */
    gpio_set(bl,1);                       /* 背光 ON */
    wr_cmd(0x29); msleep(100);            /* DISPON */

    printf("[probe] 背光已开, 开始刷色: 红->绿->蓝->彩条\n"); fflush(stdout);
    fill(0xF800); printf("  RED\n");   fflush(stdout); sleep(1);
    fill(0x07E0); printf("  GREEN\n"); fflush(stdout); sleep(1);
    fill(0x001F); printf("  BLUE\n");  fflush(stdout); sleep(1);
    bars();       printf("  彩条 (若颜色/顺序不对见提示)\n"); fflush(stdout);

    printf("[probe] 完成。屏应停在竖向彩条。\n");
    printf("  * 全黑不亮: 查 VCC(3.3V!)/GND/BL/RST 接线\n");
    printf("  * 亮白但无图: 查 SCL/SDA/CS/DC 接线或降速 (末参数给 8000000)\n");
    printf("  * 颜色反(负片): 去掉 0x21;  红蓝反: MADCTL 0x00->0x08\n");
    return 0;
}
