/**
  ******************************************************************************
  * @file    can_link.h
  * @brief   CAN 链路抽象层（F4 = CANopen Node 1）
  *          见《自主巡检系统实施方案_CAN与PCB.md》§3.4~§3.6。
  *
  *  本层只负责「帧从哪来 / 往哪去」——收到的命令仍复用现有 g_remote_data /
  *  g_remote_ready 消费逻辑（与 USART3 完全同构），可用 LINK_USE_CAN 一键回退。
  *
  *  过程数据一律定点大端打包，单帧 ≤8B（禁止 float 拼超 8B）。
  ******************************************************************************
  */
#ifndef __CAN_LINK_H
#define __CAN_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* ===== ★ P1 自环自证开关（实施方案 §3.9 P1）==============================
 *  1 = CAN 外设进入「静默自环 Silent Loopback」：TX 在【芯片内部】环回到 RX，
 *      CANTX 引脚保持隐性、CANRX 引脚被忽略 —— 完全不接触外部总线，
 *      且【不需要任何 ACK】。用途：把「软件+CAN外设」与「收发器+接线」切开定位。
 *
 *  ★★ 重要：这是外设【内部】环回，【不需要】把 PD0/PD1 用导线接在一起。
 *     物理短接 TX/RX 在 NORMAL 模式下依然会因「无人回 ACK」而报 ACK Error →
 *     照样 bus-off 风暴，测不出任何东西。
 *
 *  判据：自环下 tx 涨、rxoth 涨、err/boff=0、TEC/REC=0
 *        → 软件与 CAN 外设 100% 正常，问题 100% 在外部硬件。
 *
 *  ⚠⚠ 测完必须改回 0 并重烧，否则 F4 永远不会真正上总线！
 */
/* ✅ 2026-07-17 P1 自环已通过：rxoth 稳定递增、err/boff/TEC/REC=0、LEC=NoErr
 *    → 软件与 CAN 外设已自证 100% 正常，问题定位在收发器/接线。故改回 0 上真实总线。 */
#ifndef CAN_LOOPBACK_TEST
#define CAN_LOOPBACK_TEST   0
#endif

/* ===== CANopen 预定义连接集 COB-ID（Node ID = 1，见实施方案 §3.4） ========= */
#define COBID_TELE_CORE   0x181U   /* TPDO1  F4→龙芯  核心遥测            */
#define COBID_CMD         0x201U   /* RPDO1  龙芯→F4  命令（兼心跳源）     */
#define COBID_TELE_ENC    0x281U   /* TPDO2  F4→龙芯  编码器              */
#define COBID_PATROL_CMD  0x301U   /* RPDO2  龙芯→F4  巡检/避障/PID（sub） */
#define COBID_ENV         0x381U   /* TPDO3  F4→龙芯  环境                */
#define COBID_EVT         0x481U   /* TPDO4  F4→龙芯  事件/避障/PID（sub） */
#define COBID_SDO_TX      0x581U   /* F4→龙芯  SDO 参数服务响应           */
#define COBID_SDO_RX      0x601U   /* 龙芯→F4  SDO 参数服务请求           */
#define COBID_HEARTBEAT   0x701U   /* F4→龙芯  节点心跳（F4 存活）         */

/* Heartbeat / NMT 状态（CiA 301） */
#define CAN_NODE_STATE_OPERATIONAL  0x05U

/* 0x301 RPDO2 的 sub_id（byte0，见实施方案 §3.5.2） */
#define PATROL_SUB_CMD      0x01U   /* PATROL_CMD                */
#define PATROL_SUB_ACT      0x02U   /* PATROL_ACT                */
#define AVOID_SUB_POLICY    0x03U   /* AVOID_POLICY              */
#define PATROL_SUB_PIDTEST  0x04U   /* PID_TEST                  */

/* 0x481 TPDO4 的 sub_id（byte0） */
#define EVT_SUB_PIDT        0x03U   /* PID 调参遥测，3 帧分段     */
#define EVT_SUB_PATROL      0x01U   /* 巡检事件: event u8, seq u8, state u8 */

/* 类 SDO 对象字典：PID 参数 0x2000 sub1..5 */
#define SDO_OD_PID          0x2000U

/* SDO 命令/响应（expedited only，本阶段只承载 PID 参数） */
#define SDO_CCS_WRITE_1B    0x2FU
#define SDO_CCS_WRITE_2B    0x2BU
#define SDO_CCS_WRITE_4B    0x23U
#define SDO_CCS_READ        0x40U
#define SDO_SCS_WRITE_ACK   0x60U
#define SDO_SCS_READ_1B     0x4FU
#define SDO_SCS_READ_2B     0x4BU
#define SDO_SCS_READ_4B     0x43U
#define SDO_ABORT           0x80U

#define SDO_ABT_NO_OBJECT      0x06020000UL
#define SDO_ABT_NO_SUBINDEX    0x06090011UL
#define SDO_ABT_TYPE_MISMATCH  0x06070010UL
#define SDO_ABT_VALUE_RANGE    0x06090030UL

/* ===== 生命周期 ==========================================================
 *  过滤器（只放行 0x201/0x301/0x601）+ 启动 + 使能 RX/错误中断通知。
 *  必须在 osKernelStart 之前、MX_CAN1_Init 之后调用（CubeMX 只建外设，不建这些）。 */
void CANLink_Init(void);
void CANLink_Service(void);

/* ===== 发送 =============================================================
 *  非阻塞：3 个发送邮箱，满则丢最旧（保证低时延、不卡任务）。
 *  返回 1=已投递邮箱，0=丢弃。len>8 截断到 8。 */
uint8_t CANLink_Send(uint16_t cob_id, const uint8_t *data, uint8_t len);

/* 类型化过程数据发送（大端定点打包，见实施方案 §3.5.1） */
uint8_t CANLink_SendTeleCore(int16_t spd, int16_t str, uint8_t mode, uint16_t dist_cm, uint8_t line);
uint8_t CANLink_SendTeleEnc(int32_t enc1, int32_t enc2);
uint8_t CANLink_SendEnv(uint16_t gas, uint16_t vl53_mm, int8_t temp,
                        uint8_t humi, uint8_t flags, uint8_t alarm);
uint8_t CANLink_SendHeartbeat(uint8_t state);

/* 巡检事件上报（0x481 sub=0x01）：event/point_seq/state 见 patrol.h 枚举 */
uint8_t CANLink_SendPatrolEvent(uint8_t event, uint8_t point_seq, uint8_t state);

/* ---- 龙芯→F4 巡检命令，由 RX 回调解析后发布（MotionControlTask 消费）---- */
extern volatile uint8_t g_patrol_cmd_rx;      /* PatrolCmd: 0停 1启 2暂停 3恢复 */
extern volatile uint8_t g_patrol_cmd_ready;
extern volatile uint8_t g_patrol_act_point;   /* 到点放行: point_id */
extern volatile uint8_t g_patrol_act_action;  /* 0直行 1左 2右 3结束 */
extern volatile uint8_t g_patrol_act_ready;
uint8_t CANLink_SendPidTele(uint16_t seq, uint8_t flags,
                            int16_t targL, int16_t measL, int16_t outL,
                            int16_t targR, int16_t measR, int16_t outR,
                            int32_t enc1, int32_t enc2);

/* ===== P1 自环自证：自发自收一帧 0x301 ==================================
 *  用 0x301（过滤器放行、当前仅计数、【对运动无副作用】）而不是 0x201 ——
 *  0x201 会写 g_remote_data/g_remote_ready 直接驱动电机，自测时有安全风险。
 *  仅 CAN_LOOPBACK_TEST=1 时存在。 */
#if CAN_LOOPBACK_TEST
void CANLink_LoopbackSelfTest(void);
#endif

/* ===== 链路健康（★ EMI 诊断关键，对应龙芯侧 ip -s link show can0） ======== */
uint32_t CANLink_GetTEC(void);   /* 发送错误计数器 (ESR.TEC) */
uint32_t CANLink_GetREC(void);   /* 接收错误计数器 (ESR.REC) */
uint8_t  CANLink_IsBusOff(void); /* 1=当前处于 bus-off (ESR.BOFF，权威现态) */

/* ★★ 最后错误码 (ESR.LEC) —— 定位物理层问题的关键证据：
 *   0=NoErr 1=Stuff 2=Form 3=ACK 4=BitRecessive 5=BitDominant 6=CRC 7=SW
 *   · LEC=3 (ACK)    → 发出去了但【没人回 ACK】：对端没上电/没接/CANH-CANL 接反/
 *                      收发器 RS 脚没接地(进 standby)/无终端电阻
 *   · LEC=4/5 (Bit)  → 【自己发的和总线上读回的不一致】：收发器坏/接线错/总线被拉死
 *   · LEC=1/2/6      → Stuff/Form/CRC：典型的【波特率或采样点不匹配】
 */
uint32_t    CANLink_GetLEC(void);
const char* CANLink_LECStr(void);

/* ===== 诊断计数（供 DBG 打印，确认收发是否真的在动） ===================== */
extern volatile uint32_t g_can_rx_cmd_cnt;    /* 收到 0x201 命令帧数        */
extern volatile uint32_t g_can_rx_other_cnt;  /* 收到 0x301/0x601 帧数      */
extern volatile uint32_t g_can_tx_cnt;        /* 成功投递发送帧数          */
extern volatile uint32_t g_can_tx_drop;       /* 邮箱满丢弃帧数            */
extern volatile uint32_t g_can_err_cnt;       /* SCE 错误中断计数          */
extern volatile uint32_t g_can_busoff_cnt;    /* bus-off 事件计数          */

#ifdef __cplusplus
}
#endif

extern volatile uint32_t g_can_recover_cnt;

#endif /* __CAN_LINK_H */
