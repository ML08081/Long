// Core/Protocol/protocol.h
#ifndef __PROTOCOL_H
#define __PROTOCOL_H

#include "stm32f4xx_hal.h"

#define FRAME_HEADER    0xAA
#define FRAME_LENGTH    7

// 扩展遥测帧（F4 -> 龙芯 SBC，携带超声波距离 + 编码器）
//   [0]=0xAA [1]=0x5A(EXT) [2]=LEN(=15) [3..]=payload(大端) [末]=XOR(0..2+LEN)
//   payload: speed i16 | steering i16 | mode u8 | dist_cm u16 | enc1 i32 | enc2 i32
#define FRAME_EXT_MARKER   0x5A
#define FRAME_EXT_PAYLOAD  15
#define FRAME_EXT_LENGTH   (3 + FRAME_EXT_PAYLOAD + 1)   /* = 19 */

// 热成像行帧（F4 -> 龙芯，MLX90640 32x24 分 24 行发送，避免单帧过大阻塞）
//   [0]=0xAA [1]=0x5B(THERMAL) [2]=rowIdx(0~23) [3..]=32*int16 温度(大端,0.01°C) [末]=XOR
#define FRAME_THERMAL_MARKER  0x5B
#define FRAME_THERMAL_COLS    32
#define FRAME_THERMAL_ROW_LEN (3 + FRAME_THERMAL_COLS * 2 + 1)   /* = 68 */

// 热成像"原始帧"分块（F4 -> 龙芯）：F4 只读原始数据转发，解算下放龙芯（省 M4 CPU、提帧率）
//   [0]=0xAA [1]=MARK [2]=idx u8 [3]=cnt u8 [4..]=cnt*2 字节(大端) [末]=XOR
//   MARK: 0x5D=MLX90640 原始帧(834字), 0x5E=EEPROM 标定(832字，上电发一次)
//   idx=分块序号，目标数组偏移=idx*MLX_CHUNK_WORDS
#define FRAME_RAWTHERM_MARKER  0x5D
#define FRAME_EEPROM_MARKER    0x5E
#define MLX_EE_WORDS       832
#define MLX_FRAME_WORDS    834
#define MLX_CHUNK_WORDS    32     /* 每块 32 字=64 字节数据；与龙芯 MLX_CHUNK_WORDS 一致 */
#define FRAME_MLXCHUNK_MAXLEN (4 + MLX_CHUNK_WORDS * 2 + 1)   /* = 69 */

// 环境/安全遥测帧（F4 -> 龙芯 SBC）：气体 / 激光测距 / 温湿度 / 报警标志
//   [0]=0xAA [1]=0x5C(ENV) [2]=LEN(=8) [3..]=payload(大端) [末]=XOR(0..2+LEN)
//   payload: gas_raw u16 | vl53_mm u16 | temp i8 | humi u8 | flags u8 | alarm u8
#define FRAME_ENV_MARKER   0x5C
#define FRAME_ENV_PAYLOAD  8
#define FRAME_ENV_LENGTH   (3 + FRAME_ENV_PAYLOAD + 1)   /* = 12 */

// 环境帧 flags 位定义
#define ENV_FLAG_GAS       0x01   // 气体浓度超阈值（MQ2 AO/DO 报警）
#define ENV_FLAG_FLAME     0x02   // 火焰检测（PD7）
#define ENV_FLAG_DHT_OK    0x04   // 温湿度数据有效（DHT11 本周期读取成功）
#define ENV_FLAG_VL53_OK   0x08   // 激光测距数据有效（VL53L0X 本次量程内）
#define ENV_FLAG_OBSTACLE  0x10   // 障碍确认：VL53 与超声波双重验证一致
#define ENV_FLAG_BUZZER    0x20   // 蜂鸣器当前处于鸣响状态
#define ENV_FLAG_VL53_PRESENT 0x40 // 激光传感器已初始化在位（区分"死值/缺失"与"超量程"）
#define ENV_FLAG_OBS_LOCK  0x80   // ★遥控避障锁生效中：前方过近已强制停止，需二次确认才解锁

// 报警等级（环境帧 alarm 字节）
#define ALARM_LV_NONE   0   // 正常
#define ALARM_LV_WARN   1   // 警告（单一传感器触发 / 障碍临近）
#define ALARM_LV_FIRE   2   // 报警（火焰 或 气体 或 双重障碍确认）

// ============ PID 在线调试帧（2026-07-08，与龙芯 serial/Protocol.h 同源） ============
// 下行 PID 参数帧（龙芯 -> F4）：运行时改 Kp/Ki/Kd/MAX_DELTA/使能，无需重烧
//   [0]=0xAA [1]=0x60 [2]=LEN(=15) [3..17]=payload(大端) [18]=XOR(0..17)
//   payload: kp_x1000 u32 | ki_x1000 u32 | kd_x1000 u32 | max_delta u16 | flags u8
//   flags: bit0 = PID 闭环使能（0=开环直通 PWM，出厂默认）
#define FRAME_PIDPARAM_MARKER  0x60
#define FRAME_PIDPARAM_PAYLOAD 15
#define FRAME_PIDPARAM_LENGTH  (3 + FRAME_PIDPARAM_PAYLOAD + 1)   /* = 19 */
#define PID_FLAG_CLOSED_LOOP   0x01

// 下行 PID 测试帧（龙芯 -> F4）：台架调参激励（开环标定 / 闭环阶跃）
//   [0]=0xAA [1]=0x61 [2]=LEN(=7) [3..9]=payload(大端) [10]=XOR(0..9)
//   payload: test_mode u8 | left i16 | right i16 | duration_ms u16
//   test_mode: 0=停止测试  1=开环直给 PWM(left/right=-1000~1000, 方向自检/MAX_DELTA标定)
//              2=闭环目标(left/right=目标编码器增量 counts/周期, Kp/Ki/Kd阶跃整定)
//   duration_ms: 到时自动停(安全);0→默认 3000;上限 PID_TEST_MAX_MS
#define FRAME_PIDTEST_MARKER   0x61
#define FRAME_PIDTEST_PAYLOAD  7
#define FRAME_PIDTEST_LENGTH   (3 + FRAME_PIDTEST_PAYLOAD + 1)    /* = 11 */
#define PID_TEST_OFF        0
#define PID_TEST_OPEN_LOOP  1
#define PID_TEST_CLOSED     2
#define PID_TEST_MAX_MS     10000U
#define PID_TEST_DEFAULT_MS 3000U

// 上行 PID 遥测帧（F4 -> 龙芯）：调参曲线数据（测试时 50Hz，平时低频）
//   [0]=0xAA [1]=0x62 [2]=LEN(=23) [3..25]=payload(大端) [26]=XOR(0..25)
//   payload: seq u16 | flags u8 | targL i16 | measL i16 | outL i16
//            | targR i16 | measR i16 | outR i16 | enc1 i32 | enc2 i32
//   flags: bit0=闭环使能 bit1=测试进行中 bit4-5=test_mode
#define FRAME_PIDTELE_MARKER   0x62
#define FRAME_PIDTELE_PAYLOAD  23
#define FRAME_PIDTELE_LENGTH   (3 + FRAME_PIDTELE_PAYLOAD + 1)    /* = 27 */

// 运行时 PID 参数（由 0x60 帧更新；初值来自 freertos.c 编译期宏）
typedef struct {
    float    kp, ki, kd;
    float    max_delta;     /* 全速(±1000)每控制周期目标脉冲增量（须标定） */
    uint8_t  closed_loop;   /* 1=编码器闭环, 0=开环直通 */
} PidParams;

// 运行时 PID 测试命令（由 0x61 帧更新）
typedef struct {
    uint8_t  mode;          /* PID_TEST_* */
    int16_t  left, right;   /* 开环:PWM; 闭环:目标 counts/周期 */
    uint16_t duration_ms;
} PidTestCmd;

// 遥测标记位（mode 字节 bit 7，PC 端用此区分命令与遥测）
//   命令帧：mode = 0x00~0x03（bit7=0）
//   遥测帧：mode = 0x80~0x83（bit7=1）
#define TELEMETRY_FLAG  0x80

#define MODE_MANUAL     0x00
#define MODE_AUTO       0x01
#define MODE_OBSTACLE   0x02
#define MODE_CRUISE     0x03

/* ===== 三种运行模式（2026-07-21 新增语义别名）=====================
 *  沿用上面既有编码值，不破坏既有协议/上位机兼容性，只给出明确语义：
 *    MODE_REMOTE      : 遥控 —— 完全由龙芯/上位机下发 speed/steering
 *    MODE_LINE_ONLY   : 纯循迹 —— F4 本地 TCRT 循线自动巡检，不依赖视觉
 *    MODE_LINE_VISION : 循迹+视觉 —— 循线仍是 F4 本地快环（保证断网可跑），
 *                       龙芯的 YOLO/ArUco 语义结果作为【叠加约束】介入
 *                       （降速/停车/到点放行），而不是接管转向。
 *  ★ 三端(F4/龙芯/LongLook)必须用同一套值。 */
#define MODE_REMOTE       MODE_MANUAL    /* 0 */
#define MODE_LINE_ONLY    MODE_AUTO      /* 1 */
#define MODE_LINE_VISION  MODE_CRUISE    /* 3 */

typedef struct {
    int16_t speed;    // -1000~+1000，正=前进，负=后退，0=刹车
    int16_t steering; // -1000~+1000，正=右转，负=左转，0=直线
    uint8_t mode;     // 0=Manual, 1=Auto, 2=ObstacleAvoid, 3=Cruise
} ControlData;

uint8_t Protocol_CalculateChecksum(const uint8_t* data, uint8_t length);
void Protocol_PackFrame(const ControlData* data, uint8_t* frame);
uint8_t Protocol_UnpackCommandFrame(const uint8_t* frame, ControlData* data);
uint8_t Protocol_UnpackFrame(const uint8_t* frame, ControlData* data);

// 打包扩展遥测帧（携带距离 + 编码器），写入 frame（需 >= FRAME_EXT_LENGTH 字节），返回帧长。
uint8_t Protocol_PackTelemetryEx(int16_t speed, int16_t steering, uint8_t mode,
                                 uint16_t dist_cm, int32_t enc1, int32_t enc2,
                                 uint8_t* frame);

// 打包一行热成像（32 个 int16 大端），写入 frame（需 >= FRAME_THERMAL_ROW_LEN），返回帧长。
uint8_t Protocol_PackThermalRow(uint8_t rowIdx, const int16_t* rowTemps, uint8_t* frame);

// 打包一块原始热成像/EEPROM 数据（uint16 大端），写入 frame（需 >= FRAME_MLXCHUNK_MAXLEN），返回帧长。
//   marker: FRAME_RAWTHERM_MARKER(0x5D) 或 FRAME_EEPROM_MARKER(0x5E); cnt<=MLX_CHUNK_WORDS
uint8_t Protocol_PackMlxChunk(uint8_t marker, uint8_t idx,
                              const uint16_t* words, uint8_t cnt, uint8_t* frame);

// 打包环境/安全遥测帧，写入 frame（需 >= FRAME_ENV_LENGTH 字节），返回帧长。
uint8_t Protocol_PackEnvFrame(uint16_t gas_raw, uint16_t vl53_mm,
                              int8_t temp, uint8_t humi,
                              uint8_t flags, uint8_t alarm, uint8_t* frame);

// 解析 PID 参数帧 payload（frame 为完整 0x60 帧，已过校验），返回 1=成功。
uint8_t Protocol_UnpackPidParam(const uint8_t* frame, PidParams* p);
// 解析 PID 测试帧 payload（frame 为完整 0x61 帧，已过校验），返回 1=成功。
uint8_t Protocol_UnpackPidTest(const uint8_t* frame, PidTestCmd* t);
// 打包 PID 遥测帧，写入 frame（需 >= FRAME_PIDTELE_LENGTH 字节），返回帧长。
uint8_t Protocol_PackPidTele(uint16_t seq, uint8_t flags,
                             int16_t targL, int16_t measL, int16_t outL,
                             int16_t targR, int16_t measR, int16_t outR,
                             int32_t enc1, int32_t enc2, uint8_t* frame);

#endif