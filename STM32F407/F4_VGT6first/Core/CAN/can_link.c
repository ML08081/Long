/**
  ******************************************************************************
  * @file    can_link.c
  * @brief   CAN 链路抽象层实现（F4 = CANopen Node 1�?  *          见《自主巡检系统实施方案_CAN与PCB.md》�?.6�?  *
  *  设计要点�?  *   · ISR 只做搬运：HAL_CAN_GetRxMessage �?�?COB-ID 直写全局，绝不在 ISR
  *     里做解析或长逻辑（CAN 硬件已交付完整、CRC 已校验的�?+ ID）�?  *   · 命令 0x201 的消�?100% 复用现有 g_remote_data / g_remote_ready 逻辑
  *     （与 USART3_ProcessRxByte 同构）——只换「帧从哪来」�?  *   · 发送非阻塞：写完邮箱即返回，无 USART3 �?TXE/TC 忙等死锁风险�?  ******************************************************************************
  */
#include "can_link.h"
#include "can.h"        /* hcan1                     */
#include "usart.h"      /* g_remote_data / g_remote_ready / ControlData (via protocol.h) */
#include "watchdog.h"   /* CAN 持续异常上报给看门狗 */

#include <string.h>

#ifndef PID_TUNING_ENABLED
#define PID_TUNING_ENABLED 0
#endif

/* ---- 诊断计数（定义处；声明见 can_link.h�?---- */
volatile uint32_t g_can_rx_cmd_cnt   = 0;
volatile uint32_t g_can_rx_other_cnt = 0;
volatile uint32_t g_can_tx_cnt       = 0;
volatile uint32_t g_can_tx_drop      = 0;
volatile uint32_t g_can_err_cnt      = 0;
volatile uint32_t g_can_busoff_cnt   = 0;
volatile uint32_t g_can_recover_cnt  = 0;

/* PID 在线调试已于 v1.19.0 停用（PID_TUNING_ENABLED=0），下面这批 SDO 参数服务
   的辅助函数与影子参数仅在该分支内使用。用同一个宏包起来，既消除
   -Wunused 噪声，也保留将来重新启用参数服务时的完整基础设施。 */
#if PID_TUNING_ENABLED
static PidParams s_pid_shadow = { 2.0f, 4.0f, 0.0f, 300.0f, 0U };
#endif

/*==========================================================================*/
/* 生命周期                                                                  */
/*==========================================================================*/
void CANLink_Init(void)
{
#if CAN_LOOPBACK_TEST
    /* �?P1 自环自证：覆�?CubeMX 生成�?CAN_MODE_NORMAL 为静默自环�?       为什么放这里而不是改 can.c：can.c �?CubeMX 生成区，重生成会被冲掉；
       放在 can_link.c 属于用户代码，永久有效（这正是抽象层的意义）�?       时序：MX_CAN1_Init 已跑�?State=READY)、尚�?HAL_CAN_Start�?             此时重跑 HAL_CAN_Init 只改�?BTR/MCR，不会重�?MspInit�?*/
    hcan1.Init.Mode = CAN_MODE_SILENT_LOOPBACK;
    if (HAL_CAN_Init(&hcan1) != HAL_OK) { Error_Handler(); }
#endif

    /* 只放�?F4 需收的 3 �?COB-ID�?x201 CMD / 0x301 巡检避障 / 0x601 SDO�?       16 �?ID 列表模式：四个寄存器全是 ID（非 ID+掩码），标准 ID 左移 5 �?       对齐�?STID[10:0]。其余帧硬件直接丢弃，减�?ISR 负担�?*/
    CAN_FilterTypeDef f = {0};
    f.FilterBank           = 0;
    f.FilterMode           = CAN_FILTERMODE_IDLIST;
    f.FilterScale          = CAN_FILTERSCALE_16BIT;
    f.FilterIdHigh         = (uint16_t)(COBID_CMD        << 5); /* �?: 0x201 命令(兼心�? */
    f.FilterIdLow          = (uint16_t)(COBID_PATROL_CMD << 5); /* �?: 0x301 巡检/避障/PID */
    f.FilterMaskIdHigh     = (uint16_t)(COBID_SDO_RX     << 5); /* �?: 0x601 SDO 请求      */
    f.FilterMaskIdLow      = (uint16_t)(COBID_SDO_RX     << 5); /* �?: 未用，重复填�?     */
    f.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    f.FilterActivation     = ENABLE;
    f.SlaveStartFilterBank = 14;

    if (HAL_CAN_ConfigFilter(&hcan1, &f) != HAL_OK) { Error_Handler(); }
    if (HAL_CAN_Start(&hcan1) != HAL_OK)            { Error_Handler(); }

    /* RX FIFO0 到帧中断 + 错误/bus-off 通知（SCE）�?       ABOM=ENABLE �?bus-off 由硬件自动恢复，SCE 仅用于计数诊断�?*/
    HAL_CAN_ActivateNotification(&hcan1,
        CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF);
}

static uint8_t CANLink_NeedsRecovery(void)
{
    HAL_CAN_StateTypeDef state = HAL_CAN_GetState(&hcan1);
    if (CANLink_IsBusOff()) return 1U;
    if (state == HAL_CAN_STATE_ERROR) return 1U;
    if (state == HAL_CAN_STATE_RESET) return 1U;
    if (state == HAL_CAN_STATE_READY) return 1U;
    return 0U;
}

void CANLink_Service(void)
{
    if (!CANLink_NeedsRecovery()) {
        /* 链路健康 -> 清异常累计。否则长期运行中零星的自恢复会缓慢累加，
           最终误判为"不可恢复"而复位。只有【连续】异常才应触发看门狗。 */
        WDG_ClearFault();
        return;
    }

    HAL_CAN_Stop(&hcan1);
    HAL_CAN_ResetError(&hcan1);
    if (HAL_CAN_Start(&hcan1) == HAL_OK)
    {
        HAL_CAN_ActivateNotification(&hcan1,
            CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF);
        g_can_recover_cnt++;
        /* 软件级自恢复成功一次。若短时间内反复进这里，说明总线/收发器层面
           持续异常（EMI、线序、终端电阻、供电），软件已无能为力 ——
           上报给看门狗累计，超过 WDG_FAULT_LIMIT 次即停止喂狗让 IWDG 复位。 */
        WDG_ReportFault(WDG_FAULT_CAN_DEAD);
    }
    else
    {
        /* 连 Start 都失败：外设层面已不可用，直接计为异常 */
        WDG_ReportFault(WDG_FAULT_CAN_DEAD);
    }
}

/*==========================================================================*/
/* 发�?                                                                     */
/*==========================================================================*/
uint8_t CANLink_Send(uint16_t cob_id, const uint8_t *data, uint8_t len)
{
    CANLink_Service();

    if (len > 8U) len = 8U;

    uint8_t buf[8] = {0};
    for (uint8_t i = 0; i < len; i++) buf[i] = data[i];

    CAN_TxHeaderTypeDef h;
    h.StdId              = cob_id;
    h.ExtId              = 0;
    h.IDE                = CAN_ID_STD;
    h.RTR                = CAN_RTR_DATA;
    h.DLC                = len;
    h.TransmitGlobalTime = DISABLE;

    /* 三邮箱全�?�?丢最旧（中止 mailbox0），保证非阻塞、命令低时延 */
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U)
    {
        HAL_CAN_AbortTxRequest(&hcan1, CAN_TX_MAILBOX0);
        g_can_tx_drop++;
    }

    uint32_t mailbox;
    if (HAL_CAN_AddTxMessage(&hcan1, &h, buf, &mailbox) != HAL_OK)
    {
        g_can_tx_drop++;
        return 0;
    }
    g_can_tx_cnt++;
    return 1;
}

uint8_t CANLink_SendTeleCore(int16_t spd, int16_t str, uint8_t mode, uint16_t dist_cm, uint8_t line)
{
    uint8_t p[8];
    p[0] = (uint8_t)(spd >> 8);      p[1] = (uint8_t)spd;
    p[2] = (uint8_t)(str >> 8);      p[3] = (uint8_t)str;
    p[4] = mode;
    p[5] = (uint8_t)(dist_cm >> 8);  p[6] = (uint8_t)dist_cm;
    p[7] = line;                     /* byte7: 循迹位域 L/C/R/junction/lost（见 linetrack.h�?*/
    return CANLink_Send(COBID_TELE_CORE, p, 8);
}

uint8_t CANLink_SendTeleEnc(int32_t enc1, int32_t enc2)
{
    uint8_t p[8];
    p[0] = (uint8_t)(enc1 >> 24); p[1] = (uint8_t)(enc1 >> 16);
    p[2] = (uint8_t)(enc1 >> 8);  p[3] = (uint8_t)enc1;
    p[4] = (uint8_t)(enc2 >> 24); p[5] = (uint8_t)(enc2 >> 16);
    p[6] = (uint8_t)(enc2 >> 8);  p[7] = (uint8_t)enc2;
    return CANLink_Send(COBID_TELE_ENC, p, 8);
}

uint8_t CANLink_SendEnv(uint16_t gas, uint16_t vl53_mm, int8_t temp,
                        uint8_t humi, uint8_t flags, uint8_t alarm)
{
    uint8_t p[8];
    p[0] = (uint8_t)(gas >> 8);      p[1] = (uint8_t)gas;
    p[2] = (uint8_t)(vl53_mm >> 8);  p[3] = (uint8_t)vl53_mm;
    p[4] = (uint8_t)temp;            p[5] = humi;
    p[6] = flags;                    p[7] = alarm;
    return CANLink_Send(COBID_ENV, p, 8);
}

uint8_t CANLink_SendHeartbeat(uint8_t state)
{
    return CANLink_Send(COBID_HEARTBEAT, &state, 1);
}

/* ---- 巡检命令接收缓冲（RX 回调写，MotionControlTask 读）---- */
volatile uint8_t g_patrol_cmd_rx     = 0;
volatile uint8_t g_patrol_cmd_ready  = 0;
volatile uint8_t g_patrol_act_point  = 0;
volatile uint8_t g_patrol_act_action = 0;
volatile uint8_t g_patrol_act_ready  = 0;

uint8_t CANLink_SendPatrolEvent(uint8_t event, uint8_t point_seq, uint8_t state)
{
    uint8_t p[4] = { EVT_SUB_PATROL, event, point_seq, state };
    return CANLink_Send(COBID_EVT, p, 4);
}

uint8_t CANLink_SendPidTele(uint16_t seq, uint8_t flags,
                            int16_t targL, int16_t measL, int16_t outL,
                            int16_t targR, int16_t measR, int16_t outR,
                            int32_t enc1, int32_t enc2)
{
    uint8_t pl[23];
    pl[0]  = (uint8_t)(seq >> 8);      pl[1]  = (uint8_t)seq;
    pl[2]  = flags;
    pl[3]  = (uint8_t)(targL >> 8);    pl[4]  = (uint8_t)targL;
    pl[5]  = (uint8_t)(measL >> 8);    pl[6]  = (uint8_t)measL;
    pl[7]  = (uint8_t)(outL >> 8);     pl[8]  = (uint8_t)outL;
    pl[9]  = (uint8_t)(targR >> 8);    pl[10] = (uint8_t)targR;
    pl[11] = (uint8_t)(measR >> 8);    pl[12] = (uint8_t)measR;
    pl[13] = (uint8_t)(outR >> 8);     pl[14] = (uint8_t)outR;
    pl[15] = (uint8_t)((uint32_t)enc1 >> 24); pl[16] = (uint8_t)((uint32_t)enc1 >> 16);
    pl[17] = (uint8_t)((uint32_t)enc1 >> 8);  pl[18] = (uint8_t)enc1;
    pl[19] = (uint8_t)((uint32_t)enc2 >> 24); pl[20] = (uint8_t)((uint32_t)enc2 >> 16);
    pl[21] = (uint8_t)((uint32_t)enc2 >> 8);  pl[22] = (uint8_t)enc2;

    uint8_t ok = 1;
    for (uint8_t part = 0; part < 4U; ++part)
    {
        uint32_t guard = 10000U;
        while (HAL_CAN_GetTxMailboxesFreeLevel(&hcan1) == 0U && guard-- > 0U) { }
        uint8_t off = (uint8_t)(part * 6U);
        uint8_t rem = (uint8_t)(sizeof(pl) - off);
        uint8_t cnt = (rem > 6U) ? 6U : rem;
        uint8_t d[8] = { EVT_SUB_PIDT, part, 0, 0, 0, 0, 0, 0 };
        memcpy(&d[2], &pl[off], cnt);
        ok &= CANLink_Send(COBID_EVT, d, (uint8_t)(2U + cnt));
    }
    return ok;
}

#if PID_TUNING_ENABLED
static void put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32le(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static uint16_t get_u16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t sdo_len_from_ccs(uint8_t cs)
{
    if (cs == SDO_CCS_WRITE_1B) return 1;
    if (cs == SDO_CCS_WRITE_2B) return 2;
    if (cs == SDO_CCS_WRITE_4B) return 4;
    return 0;
}

static void CANLink_SendSdoAbort(uint16_t idx, uint8_t sub, uint32_t code)
{
    uint8_t r[8] = { SDO_ABORT, 0, 0, sub, 0, 0, 0, 0 };
    put_u16le(&r[1], idx);
    put_u32le(&r[4], code);
    CANLink_Send(COBID_SDO_TX, r, 8);
}

static void CANLink_SendSdoWriteAck(uint16_t idx, uint8_t sub)
{
    uint8_t r[8] = { SDO_SCS_WRITE_ACK, 0, 0, sub, 0, 0, 0, 0 };
    put_u16le(&r[1], idx);
    CANLink_Send(COBID_SDO_TX, r, 8);
}

static void CANLink_SendSdoReadRsp(uint16_t idx, uint8_t sub, const void *data, uint8_t len)
{
    uint8_t r[8] = { 0, 0, 0, sub, 0, 0, 0, 0 };
    r[0] = (len == 1U) ? SDO_SCS_READ_1B : (len == 2U) ? SDO_SCS_READ_2B : SDO_SCS_READ_4B;
    put_u16le(&r[1], idx);
    memcpy(&r[4], data, len);
    CANLink_Send(COBID_SDO_TX, r, 8);
}

static uint32_t pid_gain_q1000(float v)
{
    if (v < 0.0f) v = 0.0f;
    if (v > 4000000.0f) v = 4000000.0f;
    return (uint32_t)(v * 1000.0f + 0.5f);
}
#endif /* PID_TUNING_ENABLED */

static void CANLink_HandleSdo(const uint8_t *d, uint8_t dlc)
{
#if !PID_TUNING_ENABLED
    (void)d;
    (void)dlc;
    return;
#else
    if (dlc < 8U) return;
    uint16_t idx = get_u16le(&d[1]);
    uint8_t sub = d[3];
    if (idx != SDO_OD_PID) {
        CANLink_SendSdoAbort(idx, sub, SDO_ABT_NO_OBJECT);
        return;
    }

    if (d[0] == SDO_CCS_READ)
    {
        if (sub >= 1U && sub <= 3U) {
            uint32_t q = (sub == 1U) ? pid_gain_q1000(s_pid_shadow.kp)
                       : (sub == 2U) ? pid_gain_q1000(s_pid_shadow.ki)
                                     : pid_gain_q1000(s_pid_shadow.kd);
            CANLink_SendSdoReadRsp(idx, sub, &q, 4);
        } else if (sub == 4U) {
            uint16_t md = (uint16_t)s_pid_shadow.max_delta;
            CANLink_SendSdoReadRsp(idx, sub, &md, 2);
        } else if (sub == 5U) {
            uint8_t cl = s_pid_shadow.closed_loop ? 1U : 0U;
            CANLink_SendSdoReadRsp(idx, sub, &cl, 1);
        } else {
            CANLink_SendSdoAbort(idx, sub, SDO_ABT_NO_SUBINDEX);
        }
        return;
    }

    uint8_t len = sdo_len_from_ccs(d[0]);
    if (len == 0U) {
        CANLink_SendSdoAbort(idx, sub, SDO_ABT_TYPE_MISMATCH);
        return;
    }

    PidParams np = s_pid_shadow;
    if (sub >= 1U && sub <= 3U && len == 4U) {
        float v = (float)get_u32le(&d[4]) / 1000.0f;
        if      (sub == 1U) np.kp = v;
        else if (sub == 2U) np.ki = v;
        else                np.kd = v;
    } else if (sub == 4U && len == 2U) {
        uint16_t md = get_u16le(&d[4]);
        if (md == 0U || md > 2000U) { CANLink_SendSdoAbort(idx, sub, SDO_ABT_VALUE_RANGE); return; }
        np.max_delta = (float)md;
    } else if (sub == 5U && len == 1U) {
        np.closed_loop = d[4] ? 1U : 0U;
    } else {
        CANLink_SendSdoAbort(idx, sub, SDO_ABT_TYPE_MISMATCH);
        return;
    }

    s_pid_shadow = np;
    g_pid_param_rx = np;
    g_pid_param_ready = 1;
    g_rx_pid_frame_cnt++;
    CANLink_SendSdoWriteAck(idx, sub);
#endif
}

#if PID_TUNING_ENABLED
static int16_t get_i16be(const uint8_t *p)
{
    return (int16_t)(((uint16_t)p[0] << 8) | p[1]);
}
#endif

static void CANLink_HandlePidTest(const uint8_t *d, uint8_t dlc)
{
#if !PID_TUNING_ENABLED
    (void)d;
    (void)dlc;
    return;
#else
    if (dlc < 8U) return;
    PidTestCmd tc;
    tc.mode = d[1];
    tc.left = get_i16be(&d[2]);
    tc.right = get_i16be(&d[4]);
    tc.duration_ms = (uint16_t)(((uint16_t)d[6] << 8) | d[7]);
    if (tc.duration_ms == 0U) tc.duration_ms = PID_TEST_DEFAULT_MS;
    if (tc.duration_ms > PID_TEST_MAX_MS) tc.duration_ms = PID_TEST_MAX_MS;
    if (tc.mode > PID_TEST_CLOSED) return;

    g_pid_test_rx = tc;
    g_pid_test_ready = 1;
    g_rx_pid_frame_cnt++;
#endif
}

#if CAN_LOOPBACK_TEST
/* P1 自环自证：自发自收一�?0x301�?   �?刻意不用 0x201 —�?那条会写 g_remote_data/g_remote_ready 直接驱动电机�?     0x301 当前只在 RX 回调里计�?rxoth)，对运动零副作用，自测安全�?*/
void CANLink_LoopbackSelfTest(void)
{
    static uint8_t seq = 0;
    uint8_t p[8] = { PATROL_SUB_CMD, seq++, 0x5A, 0xA5, 0U, 0U, 0U, 0U };
    CANLink_Send(COBID_PATROL_CMD, p, 8);
}
#endif

/*==========================================================================*/
/* 接收（ISR 上下文，抢占优先�?5 = FreeRTOS syscall 门限，可安全直写全局�?   */
/*==========================================================================*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef rh;
    uint8_t d[8];

    /* 取尽 FIFO0（一次中断可能积压多帧） */
    while (HAL_CAN_GetRxFifoFillLevel(hcan, CAN_RX_FIFO0) > 0U)
    {
        if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rh, d) != HAL_OK) break;
        if (rh.IDE != CAN_ID_STD) continue;

        switch (rh.StdId)
        {
        case COBID_CMD:                 /* 0x201：命令帧（兼心跳源） */
            /* payload(大端)：spd i16 | str i16 | mode u8，DLC=5�?               直写 g_remote_data/g_remote_ready —�?MotionControlTask �?               taskENTER_CRITICAL 内读取（临界区屏�?≥prio5 中断，无撕裂）�?*/
            if (rh.DLC >= 5U)
            {
                ControlData c;
                c.speed    = (int16_t)(((uint16_t)d[0] << 8) | d[1]);
                c.steering = (int16_t)(((uint16_t)d[2] << 8) | d[3]);
                c.mode     = d[4];
                g_remote_data  = c;
                g_remote_ready = 1;
                g_can_rx_cmd_cnt++;
            }
            break;

        case COBID_PATROL_CMD:
            if (rh.DLC >= 1U)
            {
                if (d[0] == PATROL_SUB_PIDTEST) {
                    CANLink_HandlePidTest(d, rh.DLC);
                }
                else if (d[0] == PATROL_SUB_CMD && rh.DLC >= 2U) {
                    /* 巡检启停：cmd u8(0停/1启/2暂停/3恢复) */
                    g_patrol_cmd_rx    = d[1];
                    g_patrol_cmd_ready = 1;
                }
                else if (d[0] == PATROL_SUB_ACT && rh.DLC >= 3U) {
                    /* 到点放行：point_id u8, action u8 */
                    g_patrol_act_point  = d[1];
                    g_patrol_act_action = d[2];
                    g_patrol_act_ready  = 1;
                }
            }
            g_can_rx_other_cnt++;
            break;

        case COBID_SDO_RX:
            CANLink_HandleSdo(d, rh.DLC);
            g_can_rx_other_cnt++;
            break;

        default:
            break;
        }
    }
}

/*==========================================================================*/
/* 错误 / bus-off（SCE ISR）——只计数，恢复交给硬�?ABOM                       */
/*==========================================================================*/
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{
    uint32_t err = HAL_CAN_GetError(hcan);
    g_can_err_cnt++;
    if (err & HAL_CAN_ERROR_BOF) g_can_busoff_cnt++;

    /* �?修复(2026-07-17)：HAL �?hcan->ErrorCode 是【累积】的、自身永不清零�?       不清的话，第一�?bus-off 之后 BOF 位就常置 �?此后【每一个】错误都被误�?       �?bus-off �?现象就是 err �?boff 恒等且严重虚�?实测 58172/58172)�?       清掉�?boff 才是真实的新增次数；当前是否 bus-off �?ESR.BOFF 为准
       (�?CANLink_IsBusOff)，那个才是权威现态�?*/
    HAL_CAN_ResetError(hcan);

    /* ABOM=ENABLE：bus-off 后硬件自动重新加入总线，无需软件干预�?*/
}

/*==========================================================================*/
/* 链路健康                                                                  */
/*==========================================================================*/
uint32_t CANLink_GetTEC(void) { return (hcan1.Instance->ESR & CAN_ESR_TEC) >> CAN_ESR_TEC_Pos; }
uint32_t CANLink_GetREC(void) { return (hcan1.Instance->ESR & CAN_ESR_REC) >> CAN_ESR_REC_Pos; }
uint8_t  CANLink_IsBusOff(void) { return (hcan1.Instance->ESR & CAN_ESR_BOFF) ? 1U : 0U; }
uint32_t CANLink_GetLEC(void) { return (hcan1.Instance->ESR & CAN_ESR_LEC) >> CAN_ESR_LEC_Pos; }

/* LEC 人类可读 —�?定位物理层问题的最快路径（�?can_link.h 的判读表�?*/
const char* CANLink_LECStr(void)
{
    static const char *const s[8] = {
        "NoErr", "Stuff", "Form", "ACK", "BitRec", "BitDom", "CRC", "SW"
    };
    return s[CANLink_GetLEC() & 7U];
}
