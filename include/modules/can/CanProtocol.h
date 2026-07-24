#ifndef PATROL_MODULES_CAN_CANPROTOCOL_H
#define PATROL_MODULES_CAN_CANPROTOCOL_H

#include "modules/serial/Protocol.h"

#include <cstdint>
#include <cstddef>

// ==========================================================================
//  CAN 协议定义 —— 与 F4(STM32F407) 固件 Core/CAN/can_link.{h,c} 完全一致
//  物理链路：龙芯 SocketCAN can0 <-> F4 bxCAN1(PD0/PD1)，CAN 2.0A 标准帧，500 kbps
//
//  设计依据：实施方案《自主巡检系统实施方案_CAN与PCB.md》§3
//    · COB-ID 采用 CANopen 预定义连接集（F4 = Node ID 1），零成本换升级路径。
//    · 过程数据无确认、可丢、靠周期刷新；事件报文以载荷首字节 sub_id 复用 COB-ID。
//    · 类 SDO 参数服务借鉴 CiA 301：expedited/segmented + toggle + abort code。
//    · 铁律：CAN 单帧 ≤8 字节，一律定点 int16/u16 缩放，禁止 float 拼超 8B。
//    · 载荷统一大端序（与串口协议同构，复用 serial_proto::rdI16/rdU16/rdI32）。
//      唯一例外：SDO 的 index 与数据段按 CiA 301 用小端序（见下）。
//    · 热成像不上 CAN，继续走 F4 USART1(PA9) → 龙芯 ttyS2 专线。
// ==========================================================================

namespace patrol {
namespace can_proto {

// F4 节点号（CANopen 预定义连接集：COB-ID = 功能码 + Node ID）。
constexpr uint8_t NODE_ID_F4 = 1;

// ---- COB-ID 分配（§3.4）--------------------------------------------------
constexpr uint32_t COBID_NMT        = 0x000;  // 预留（暂不实现）
constexpr uint32_t COBID_EMCY       = 0x081;  // 预留（故障上报，可选）
constexpr uint32_t COBID_TELE_CORE  = 0x181;  // TPDO1  F4→龙芯：核心遥测
constexpr uint32_t COBID_CMD        = 0x201;  // RPDO1  龙芯→F4：命令（兼心跳源）
constexpr uint32_t COBID_TELE_ENC   = 0x281;  // TPDO2  F4→龙芯：编码器
constexpr uint32_t COBID_PATROL_RX  = 0x301;  // RPDO2  龙芯→F4：巡检/避障/PID测试（sub 复用）
constexpr uint32_t COBID_ENV        = 0x381;  // TPDO3  F4→龙芯：环境
constexpr uint32_t COBID_EVT        = 0x481;  // TPDO4  F4→龙芯：巡检事件/避障剖面/PID遥测（sub 复用）
constexpr uint32_t COBID_SDO_TX     = 0x581;  // 服务端→客户端：参数服务响应
constexpr uint32_t COBID_SDO_RX     = 0x601;  // 客户端→服务端：参数服务请求
constexpr uint32_t COBID_HEARTBEAT  = 0x701;  // F4 心跳（存活）

// ---- 0x301 RPDO2 —— 龙芯→F4，byte0 = sub_id（§3.5.2）---------------------
constexpr uint8_t SUB_PATROL_CMD    = 0x01;   // cmd u8(0停/1启/2暂停/3恢复), flags u8
constexpr uint8_t SUB_PATROL_ACT    = 0x02;   // point_id u8, action u8(0直行/1左/2右/3结束), resume u8
constexpr uint8_t SUB_AVOID_POLICY  = 0x03;   // policy u8(0仅停车/1允许绕行), obs_class u8(0未知/1人/2静物)
// sub 0x04 原为 PID 测试激励，v1.19.0 已废弃移除；F4 端亦已屏蔽。该 sub 号保留空缺、不得复用。

// ---- 0x481 TPDO4 —— F4→龙芯，byte0 = sub_id（§3.5.2）---------------------
constexpr uint8_t SUB_PATROL_EVT    = 0x01;   // event u8, point_seq u8, state u8
constexpr uint8_t SUB_AVOID_PROFILE = 0x02;   // dist_L u16, dist_C u16, dist_R u16（降精度或拆帧）
// sub 0x03 原为 PID 调参遥测，v1.19.0 已废弃移除。该 sub 号保留空缺、不得复用。

// ---- 事件枚举（PATROL_EVT.event，§3.5.2）--------------------------------
enum PatrolEventType : uint8_t {
    EVT_ARRIVE        = 1,   // 到点
    EVT_LINE_LOST     = 2,   // 丢线
    EVT_BLOCKED       = 3,   // 受阻
    EVT_INSPECT_DONE  = 4,   // 巡检完成
    EVT_BYPASS_START  = 5,   // 绕行开始
    EVT_BYPASS_DONE   = 6,   // 绕行完成
    EVT_BYPASS_FAIL   = 7,   // 绕行失败(HOLD)
    EVT_FAKE_JUNCTION = 8,   // 假路口放行
    EVT_MISSED_POINT  = 9,   // 漏点
};

// ---- 状态机状态枚举（PATROL_EVT.state / TELE_CORE.mode 语义，§3.5.2）----
enum PatrolState : uint8_t {
    ST_FOLLOW = 0, ST_SLOW, ST_BLOCKED, ST_SCAN, ST_BACKUP,
    ST_BYPASS, ST_REACQUIRE, ST_ARRIVE, ST_HOLD, ST_LOST, ST_SAFE_STOP,
};

// ---- 巡检命令 / 到点动作枚举（0x301 载荷）--------------------------------
enum PatrolCmd : uint8_t { PC_STOP = 0, PC_START = 1, PC_PAUSE = 2, PC_RESUME = 3 };
enum PointAction : uint8_t { ACT_STRAIGHT = 0, ACT_LEFT = 1, ACT_RIGHT = 2, ACT_END = 3 };
enum AvoidPolicy : uint8_t { AP_STOP_ONLY = 0, AP_ALLOW_BYPASS = 1 };

// ---- Heartbeat 状态字（0x701）------------------------------------------
constexpr uint8_t HB_OPERATIONAL = 0x05;   // CANopen Operational

// ---- 类 SDO 命令字（§3.5.3，语义对齐 CiA 301）--------------------------
//  客户端→服务端（0x601）
constexpr uint8_t SDO_CCS_WRITE_1B   = 0x2F;  // expedited 写 1 字节
constexpr uint8_t SDO_CCS_WRITE_2B   = 0x2B;  // expedited 写 2 字节
constexpr uint8_t SDO_CCS_WRITE_3B   = 0x27;  // expedited 写 3 字节
constexpr uint8_t SDO_CCS_WRITE_4B   = 0x23;  // expedited 写 4 字节
constexpr uint8_t SDO_CCS_WRITE_INIT = 0x21;  // segmented 写初始化（byte4-7 = 总长度 u32）
constexpr uint8_t SDO_CCS_READ       = 0x40;  // 读请求
constexpr uint8_t SDO_CCS_READ_SEG   = 0x60;  // 读分段请求（bit4 = toggle）
constexpr uint8_t SDO_ABORT          = 0x80;  // abort（双向通用）
//  服务端→客户端（0x581）
constexpr uint8_t SDO_SCS_WRITE_ACK  = 0x60;  // 写确认（expedited / 初始化）
constexpr uint8_t SDO_SCS_READ_1B    = 0x4F;  // expedited 读响应 1 字节
constexpr uint8_t SDO_SCS_READ_2B    = 0x4B;  // expedited 读响应 2 字节
constexpr uint8_t SDO_SCS_READ_3B    = 0x47;  // expedited 读响应 3 字节
constexpr uint8_t SDO_SCS_READ_4B    = 0x43;  // expedited 读响应 4 字节
constexpr uint8_t SDO_SCS_READ_INIT  = 0x41;  // segmented 读初始化响应（byte4-7 = 总长度）
// 写分段命令/响应 0x00/0x10（客户端）、0x20/0x30（服务端）：bit4=toggle, bit0=末段。

// ---- Abort Code（直接用 CiA 301 标准值，§3.5.3）------------------------
constexpr uint32_t SDO_ABT_NO_OBJECT     = 0x06020000;  // 对象不存在
constexpr uint32_t SDO_ABT_NO_SUBINDEX   = 0x06090011;  // 子索引不存在
constexpr uint32_t SDO_ABT_TYPE_MISMATCH = 0x06070010;  // 数据类型/长度不匹配
constexpr uint32_t SDO_ABT_VALUE_RANGE   = 0x06090030;  // 值超出范围
constexpr uint32_t SDO_ABT_STORE_FAIL    = 0x08000020;  // 数据传输/存储失败
constexpr uint32_t SDO_ABT_TIMEOUT       = 0x05040000;  // SDO 协议超时

// ---- 对象字典常用 index（§3.5.3，F4 侧实现）----------------------------
constexpr uint16_t OD_STORE_PARAMS   = 0x1010;  // 写 "save"(0x65766173) → 固化 Flash
constexpr uint16_t OD_RESTORE_DEFAULT= 0x1011;  // 写 "load"(0x64616F6C) → 恢复出厂
// 0x2000 OD_PID(Kp/Ki/Kd/MaxΔ/ClosedLoop) 于 v1.19.0 停用：PID 已在 F4 固化定版，
//   龙芯不再做 SDO 读写。index 保留空缺、不得复用。
constexpr uint16_t OD_LINE           = 0x2010;  // sub1..4: STEER_GAIN/PATROL_SPEED/LOST_TIMEOUT/LINE_ON_LEVEL
constexpr uint16_t OD_AVOID          = 0x2020;  // sub1..6: SLOW_TH/STOP_TH/CLEAR_TH/BACKUP_CM/WALL_BAND/BYPASS_MAX_CM
constexpr uint16_t OD_CALIB          = 0x2030;  // sub1..2: COUNTS_PER_CM/COUNTS_PER_RAD
constexpr uint16_t OD_ROUTE          = 0x2040;  // sub0: 点数 N; sub1: 路线表 domain(分段)
constexpr uint32_t OD_STORE_SIGNATURE   = 0x65766173;  // "save"（小端存入即 's''a''v''e'）
constexpr uint32_t OD_RESTORE_SIGNATURE = 0x64616F6C;  // "load"

// ==========================================================================
//  解析结果结构（F4→龙芯事件报文，本轮新增；遥测/环境复用 serial_proto 同构结构）
// ==========================================================================

// 0x481 sub=0x01 巡检事件
struct PatrolEvent {
    uint8_t event    = 0;    // PatrolEventType
    uint8_t pointSeq = 0;    // 点序号（用于漏点检测）
    uint8_t state    = 0;    // PatrolState
};

// 0x481 sub=0x02 避障扫描剖面（云台扫 L/C/R 得到的距离剖面）
struct AvoidProfile {
    uint16_t distL = 0;      // cm
    uint16_t distC = 0;
    uint16_t distR = 0;
};

// 0x701 心跳
struct Heartbeat {
    uint8_t state = 0;       // HB_OPERATIONAL 等
};

// ---- 小端读写辅助（仅 SDO 用；过程数据/事件仍用 serial_proto 的大端辅助）----
inline uint16_t rdU16le(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}
inline uint32_t rdU32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline void wrU16le(uint8_t* p, uint16_t v) { p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; }
inline void wrU32le(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = (v >> 24) & 0xFF;
}

// TELE_CORE(0x181) 的 dist_cm 无目标哨兵（与 F4 HC_NO_TARGET_CM 一致，§4.1a）。
// 龙芯下游用 "0 = 无效/超量程" 语义（computeRisk/EMA 均跳过 0），故收帧时把它归一为 0。
constexpr uint16_t TELE_NO_TARGET_CM = 0xFFFF;

// ENV(0x381) flags 新增位：VL53 在位（区分死值/超量程，§4.1a）。
constexpr uint8_t ENV_FLAG_VL53_PRESENT = 0x40;

} // namespace can_proto
} // namespace patrol

#endif // PATROL_MODULES_CAN_CANPROTOCOL_H
