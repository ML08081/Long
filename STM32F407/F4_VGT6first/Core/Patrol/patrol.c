/**
  ******************************************************************************
  * @file    patrol.c
  * @brief   循线巡检控制实现（F4 本地快环，17ms 一拍）
  ******************************************************************************
  */
#include "patrol.h"
#include "linetrack.h"
#include "hc.h"          /* HC_NO_TARGET_CM */
#include "cmsis_os.h"    /* osKernelGetTickCount */

/* ==== 可调参数（实车整定；上限见实施方案 §2.7 标定表）==== */
#define PATROL_SPEED        300     /* 巡航基速(直行) (-1000..1000)，先低速调试 */
#define PATROL_MIN_SPEED    140     /* 速度下限：转弯/近障时不低于此(太低电机推不动/爬行) */
#define PATROL_TURN_SPEED   220     /* 路口通过/找线时的创速 */
#define STEER_GAIN          350     /* 线位置误差(-2..2) → 转向量 */
#define LOST_TIMEOUT_MS     800     /* 丢线超过此时长判定丢线停车 */
#define TICK_MS             17      /* 与 MotionControlTask osDelay(17) 一致 */

/* ---- 距离-速度配比（前向避障，Tier0 本地反射，不等龙芯）----
   三档：净空全速 → 减速区线性降 → 停障区停车。阈值/速度均可实车整定。 */
#define FRONT_CLEAR_CM      80      /* ≥此距离视为净空，走巡航全速 */
#define FRONT_SLOW_CM       60      /* 进入减速区(CLEAR~SLOW 之间也已开始温和降速) */
#define FRONT_STOP_CM       25      /* ≤此距离停障 */

/* ---- 转弯降速：转向越大速度越低，循线过弯更稳、不冲出 ----
   speed = base - |steer|/1000 * (base - PATROL_MIN_SPEED) */
#define TURN_SLOWDOWN_ON    1

static PatrolStateT s_st         = PATROL_ST_IDLE;
static uint32_t     s_lost_acc   = 0;      /* 丢线累计时长(ms) */
static uint8_t      s_point_seq  = 0;      /* 路口计数 */
static uint8_t      s_left_junc  = 1;      /* 1=已离开上一个路口，可再次触发（防重复计数） */
static uint8_t      s_action     = PATROL_ACT_STRAIGHT;  /* 龙芯放行时给的动作 */
static uint8_t      s_turning    = 0;      /* 正在执行路口转向 */

/*============================================================================*/

void Patrol_Init(void)
{
    s_st = PATROL_ST_IDLE;
    s_lost_acc = 0; s_point_seq = 0; s_left_junc = 1;
    s_action = PATROL_ACT_STRAIGHT; s_turning = 0;
}

void Patrol_Start(void)
{
    LineTrack_Init();
    s_st = PATROL_ST_FOLLOW;
    s_lost_acc = 0; s_point_seq = 0; s_left_junc = 1; s_turning = 0;
}

void Patrol_Stop(void)   { s_st = PATROL_ST_IDLE;  s_turning = 0; }
void Patrol_Pause(void)  { if (Patrol_IsActive()) s_st = PATROL_ST_HOLD; }
void Patrol_Resume(void) { if (s_st == PATROL_ST_HOLD) s_st = PATROL_ST_FOLLOW; }

void Patrol_ResumeFromPoint(uint8_t action)
{
    if (s_st != PATROL_ST_ARRIVE) return;
    s_action = action;
    if (action == PATROL_ACT_END) {
        s_st = PATROL_ST_IDLE;
    } else {
        /* 直行也要先"穿过"当前路口色块，故统一进转向态，由 TCRT 重新压线判定完成 */
        s_turning = 1;
        s_st = PATROL_ST_FOLLOW;
    }
}

uint8_t      Patrol_IsActive(void)  { return (s_st != PATROL_ST_IDLE) ? 1u : 0u; }
PatrolStateT Patrol_GetState(void)  { return s_st; }
uint8_t      Patrol_GetPointSeq(void){ return s_point_seq; }

/*============================================================================*/

PatrolEventT Patrol_Update(int16_t *out_speed, int16_t *out_steer, uint16_t front_cm)
{
    *out_speed = 0;
    *out_steer = 0;

    if (s_st == PATROL_ST_IDLE) return PATROL_EV_NONE;

    LineState ls = LineTrack_Read();

    /* ---- Tier0 安全反射：前向停障优先于一切循线动作 ----
       front_cm==HC_NO_TARGET_CM 表示无目标(空旷)，不可当成 0cm 贴脸。 */
    uint8_t front_valid = (front_cm != HC_NO_TARGET_CM) && (front_cm != 0u);
    if (front_valid && front_cm < FRONT_STOP_CM)
    {
        if (s_st != PATROL_ST_BLOCKED) {
            s_st = PATROL_ST_BLOCKED;
            return PATROL_EV_BLOCKED;      /* 只在进入时报一次，避免刷屏 */
        }
        return PATROL_EV_NONE;
    }
    else if (s_st == PATROL_ST_BLOCKED)
    {
        s_st = PATROL_ST_FOLLOW;           /* 障碍移开，自动恢复 */
    }

    switch (s_st)
    {
    case PATROL_ST_LOST:
        /* 丢线停车后：一旦重新看到线(任一探头压线)，自动恢复循线。
           （原为终态；台架联调/实车重新上线都需要它能自愈） */
        if (!LineTrack_IsLost(ls)) {
            s_st = PATROL_ST_FOLLOW;
            s_lost_acc = 0;
        }
        return PATROL_EV_NONE;

    case PATROL_ST_HOLD:
    case PATROL_ST_ARRIVE:
        return PATROL_EV_NONE;             /* 停车态，等外部指令 */

    case PATROL_ST_FOLLOW:
    {
        /* ---- 路口检测（三路全压线）---- */
        if (LineTrack_IsJunction(ls))
        {
            if (s_turning) {
                /* 正在穿越刚放行的那个路口：保持创速直行通过 */
                *out_speed = PATROL_TURN_SPEED;
                *out_steer = 0;
                return PATROL_EV_NONE;
            }
            if (s_left_junc) {
                s_left_junc = 0;
                s_point_seq++;
                s_st = PATROL_ST_ARRIVE;   /* 停车，等龙芯认点(ArUco)+下发动作 */
                return PATROL_EV_ARRIVE;
            }
            *out_speed = PATROL_TURN_SPEED;
            return PATROL_EV_NONE;
        }

        /* 已离开路口色块 */
        if (s_turning) {
            /* 转向执行：打舵到指定方向，直到中探头重新压线 */
            if (ls.c) { s_turning = 0; }    /* 重新对准线，转向完成 */
            else {
                *out_speed = PATROL_TURN_SPEED;
                *out_steer = (s_action == PATROL_ACT_LEFT)  ? -1000
                           : (s_action == PATROL_ACT_RIGHT) ?  1000 : 0;
                return PATROL_EV_NONE;
            }
        }
        s_left_junc = 1;

        /* ---- 丢线保护 ---- */
        if (LineTrack_IsLost(ls))
        {
            s_lost_acc += TICK_MS;
            if (s_lost_acc >= LOST_TIMEOUT_MS) {
                s_st = PATROL_ST_LOST;
                return PATROL_EV_LINE_LOST;
            }
            /* 短暂丢线：低速直行尝试重新找到线 */
            *out_speed = PATROL_TURN_SPEED;
            *out_steer = 0;
            return PATROL_EV_NONE;
        }
        s_lost_acc = 0;

        /* ---- 正常循线：误差 → 转向（阿克曼下 steer 只驱动舵机）---- */
        int16_t steer = (int16_t)(LineTrack_Error(ls) * STEER_GAIN);
        *out_steer = steer;

        /* ===== 速度配比（两级调制：转弯程度 + 障碍距离，取更小者）===== */
        int32_t base = PATROL_SPEED;

        /* (1) 转弯降速：|steer| 越大越慢，直行 base、满打舵降到 MIN。过弯更稳不冲线。 */
#if TURN_SLOWDOWN_ON
        {
            int32_t a = steer < 0 ? -steer : steer;          /* |steer| 0..1000 */
            if (a > 1000) a = 1000;
            base = PATROL_SPEED - a * (PATROL_SPEED - PATROL_MIN_SPEED) / 1000;
        }
#endif

        /* (2) 障碍距离降速：三档。front_valid=有回波且非哨兵。 */
        int32_t vdist = base;
        if (front_valid) {
            if (front_cm <= FRONT_STOP_CM) {
                vdist = 0;                                    /* 停障 */
            } else if (front_cm < FRONT_CLEAR_CM) {
                /* STOP~CLEAR 之间线性：STOP 处=0，CLEAR 处=base */
                vdist = base * (front_cm - FRONT_STOP_CM)
                             / (FRONT_CLEAR_CM - FRONT_STOP_CM);
            }
            /* ≥CLEAR_CM 净空：vdist 保持 base 全速 */
        }

        /* 取两级里更小的速度；非 0 时不低于 MIN（避免爬行推不动） */
        int32_t v = (vdist < base) ? vdist : base;
        if (v > 0 && v < PATROL_MIN_SPEED) v = PATROL_MIN_SPEED;
        *out_speed = (int16_t)v;
        return PATROL_EV_NONE;
    }

    default:
        return PATROL_EV_NONE;
    }
}
