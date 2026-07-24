/**
  ******************************************************************************
  * @file    patrol.h
  * @brief   循线巡检控制（F4 本地快环）
  *          见《自主巡检系统实施方案_CAN与PCB.md》§2.1 / §2.5
  *
  *  职责边界（★ 重要）：
  *    本模块只做「循线快环」——读 TCRT 偏差 → 算 (speed, steering)，17ms 一拍，
  *    完全在 F4 本地闭合，不依赖 CAN/龙芯，链路抖动也不影响巡线。
  *
  *    【不做】路线表 / 到点该左转还是右转 / 点位语义 —— 那些是【慢环】，
  *    归龙芯 MissionManager（车已停稳，容忍延迟）。F4 只在到达路口时停车并
  *    上报 EVT_ARRIVE，等龙芯经 CAN 0x301 sub=0x02 下发下一步动作。
  ******************************************************************************
  */
#ifndef __PATROL_H
#define __PATROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ---- 巡检状态（与龙芯 CanProtocol.h PatrolState 对齐）---- */
typedef enum {
    PATROL_ST_IDLE = 0,      /* 未启动 */
    PATROL_ST_FOLLOW,        /* 循线巡航 */
    PATROL_ST_ARRIVE,        /* 到达路口/定点，已停车，等龙芯放行 */
    PATROL_ST_BLOCKED,       /* 前方受阻，已停车 */
    PATROL_ST_LOST,          /* 丢线超时，安全停车 */
    PATROL_ST_HOLD,          /* 暂停（龙芯指令） */
} PatrolStateT;

/* ---- 事件（与龙芯 CanProtocol.h PatrolEventType 对齐）---- */
typedef enum {
    PATROL_EV_NONE = 0,
    PATROL_EV_ARRIVE      = 1,   /* 到点 */
    PATROL_EV_LINE_LOST   = 2,   /* 丢线 */
    PATROL_EV_BLOCKED     = 3,   /* 受阻 */
} PatrolEventT;

/* ---- 到点后龙芯下发的动作（0x301 sub=0x02 action）---- */
typedef enum {
    PATROL_ACT_STRAIGHT = 0,
    PATROL_ACT_LEFT     = 1,
    PATROL_ACT_RIGHT    = 2,
    PATROL_ACT_END      = 3,
} PatrolActionT;

/* ===== 生命周期 ===== */
void Patrol_Init(void);
void Patrol_Start(void);
void Patrol_Stop(void);
void Patrol_Pause(void);
void Patrol_Resume(void);

/* 龙芯到点放行：给出本点动作，F4 据此离开 ARRIVE 态 */
void Patrol_ResumeFromPoint(uint8_t action);

/* ===== 状态查询 ===== */
uint8_t      Patrol_IsActive(void);      /* 1=正在巡检(非 IDLE) */
PatrolStateT Patrol_GetState(void);
uint8_t      Patrol_GetPointSeq(void);   /* 已经过的路口计数(去重后) */

/* ===== 每控制周期(17ms)调用一次 =====
 *  产出期望 speed/steer（交由 MotionControlTask 既有的舵机+PID 管线执行），
 *  返回本拍事件（PATROL_EV_NONE 表示无事件）。
 *  front_cm: 前向距离(cm)，0xFFFF=无目标；用于减速/停障（Tier0 安全）。 */
PatrolEventT Patrol_Update(int16_t *out_speed, int16_t *out_steer, uint16_t front_cm);

#ifdef __cplusplus
}
#endif

#endif /* __PATROL_H */
