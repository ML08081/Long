#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QtGlobal>

/*============================================================================
 * LongLook 龙芯前端 — 网络帧协议 (v2)
 *
 * 系统：基于龙芯2K0300 + STM32 双主控的智能巡检系统（见 资料文件/开题报告初版）
 * 链路：PC 前端 (TCP 客户端) ←—WiFi—→ 龙芯 2K0300 (TCP 服务端)
 *
 * 帧格式（流式，TCP 已保证可靠，故不加 CRC）：
 *   ┌──────┬──────┬───────────┬──────────────┐
 *   │ 0xA5 │ TYPE │ LEN(4B LE)│ PAYLOAD(LEN) │
 *   └──────┴──────┴───────────┴──────────────┘
 *   龙芯端打包: header = bytes([0xA5, type]) + struct.pack('<I', len) ; sendall(header+payload)
 *==========================================================================*/

namespace LL {

constexpr quint8  SOF         = 0xA5;
constexpr int     HEADER_SIZE = 6;                    // SOF(1)+TYPE(1)+LEN(4)
constexpr quint32 MAX_PAYLOAD = 16u * 1024u * 1024u;

enum FrameType : quint8 {
    FRAME_SENSOR  = 0x01,   // 综合传感器遥测（见 SensorData，小端，变长容忍）
    FRAME_VIDEO   = 0x10,   // 视频帧：JPEG/PNG 编码字节流
    FRAME_THERMAL = 0x20,   // 热成像帧：uint16 w,h + w*h 个 int16 温度(0.01°C)
    FRAME_TEXT    = 0x30,   // 文本/日志：UTF-8
    FRAME_COMMAND = 0x40,   // 前端→龙芯 下行命令：payload = [cmdId u8][value u8]
    FRAME_DRIVE   = 0x41,   // 前端→龙芯 手动驱动：payload = [speed i16 LE][steering i16 LE]
    FRAME_VISION  = 0x42,   // 前端→龙芯 视觉识别结果（上位机对视频流做 YOLO 分析后回传，
                            //   龙芯"大脑"据此判断）：payload =
                            //   [count u8][maxConf u8(0~100)][flags u8][nameLen u8][name UTF-8...]
    // 0x22 FRAME_PID_TELE 与 0x43 FRAME_PID_CMD 已于 2026-07-23 废弃并移除：
    //   PID 已在 F4 端固化定版，三端均不再提供在线调参入口。
    //   这两个类型号保留空缺、不再复用，避免与现场旧版本产生语义混淆。
};

// 视觉结果标志位（与龙芯 net::VisionFlag 同源）
enum VisionFlag : quint8 {
    VIS_PERSON = 0x01,   // 检测到人
    VIS_FIRE   = 0x02,   // 检测到火焰/火源
};

// 下行命令 ID（执行与联动模块控制）
enum CmdId : quint8 {
    CMD_FAN    = 0x01,   // 风扇   value: 0关/1开
    CMD_BUZZER = 0x02,   // 蜂鸣器 value: 0关/1开
    CMD_RELAY  = 0x03,   // 继电器 value: 0断/1通
    CMD_LED    = 0x04,   // LED    value: 0灭/1亮
    CMD_MODE   = 0x05,   // 模式   value: 0手动/1自动/2避障/3巡检
    CMD_ESTOP  = 0x06,   // 急停   value: 1
};

/*
 * 综合传感器遥测负载（小端，规范长度 40 字节；解析端按可用长度容忍变长）
 *  偏移 大小 字段             模块
 *   0    4  timestamp_ms       —      系统 tick(ms)
 *   4    2  temperature_01c    环境   温度 0.1°C (DHT22)
 *   6    2  humidity_01        环境   湿度 0.1 % (DHT22)
 *   8    2  gas_ppm            环境   烟雾/可燃气体 (MQ-2)
 *  10    4  pressure_pa        环境   气压 Pa —— 本机未装 BMP280，恒为 0，仅占位保持偏移
 *  14    2  distance_cm        环境   超声波距离 cm (HC-SR04)
 *  16    4  encoder1           运动   编码器1 累计脉冲
 *  20    4  encoder2           运动   编码器2 累计脉冲
 *  24    2  speed_L            运动   左轮速度
 *  26    2  speed_R            运动   右轮速度
 *  28    2  servo_us           运动   转向舵机脉宽 us
 *  30    2  voltage_mV         运动   电池电压 mV
 *  32    1  mode               运动   模式 0手动/1自动/2避障/3巡检
 *  33    1  fault              运动   故障标志
 *  34    1  risk_level         视觉   风险等级 0安全/1注意/2警告/3危险
 *  35    1  flags              视觉   bit0火焰 bit1烟雾 bit2气体报警
 *  36    1  fan                联动   风扇状态
 *  37    1  buzzer             联动   蜂鸣器状态
 *  38    1  relay              联动   继电器状态
 *  39    1  led                联动   LED 状态
 */
constexpr int SENSOR_PAYLOAD_SIZE = 42;   // v3：末尾追加 laser_cm(2B)，变长容忍

struct SensorData {
    quint32 timestamp_ms    = 0;
    // 环境感知
    qint16  temperature_01c = 0;
    quint16 humidity_01     = 0;
    quint16 gas_ppm         = 0;
    quint32 pressure_pa     = 0;
    quint16 distance_cm     = 0;
    // 运动控制
    qint32  encoder1        = 0;
    qint32  encoder2        = 0;
    qint16  speed_L         = 0;
    qint16  speed_R         = 0;
    quint16 servo_us        = 0;
    quint16 voltage_mV      = 0;
    quint8  mode            = 0;
    quint8  fault           = 0;
    // 视觉/风险
    quint8  risk_level      = 0;
    quint8  flags           = 0;
    // 执行联动状态回读
    quint8  fan             = 0;
    quint8  buzzer          = 0;
    quint8  relay           = 0;
    quint8  led             = 0;
    // v3 扩展
    quint16 laser_cm        = 0;   // VL53L0X 激光测距(cm)，0=无效/超量程

    // flags 位定义与 F4/龙芯 ENV_FLAG 同源（勿改位值）：
    //   0x01 气体  0x02 火焰  0x04 DHT有效  0x08 激光有效  0x10 障碍确认
    //   0x20 蜂鸣  0x40 激光在位  0x80 遥控避障锁
    bool gasAlarm() const { return flags & 0x01; }
    bool flame()    const { return flags & 0x02; }
    bool dhtOk()    const { return flags & 0x04; }
    bool laserOk()  const { return flags & 0x08; }
    bool obstacle() const { return flags & 0x10; }
    bool buzzerOn() const { return flags & 0x20; }
    bool laserPresent() const { return flags & 0x40; }
    // 遥控避障锁：前方 ≤8cm，F4 已强制封锁前进，需操作者松手后再推两次前进才解锁
    bool obsLock()  const { return flags & 0x80; }
    bool smoke()    const { return flags & 0x01; }   // MQ2 气体/烟雾同一信号
};

} // namespace LL

#endif // PROTOCOL_H
