#ifndef __PID_H
#define __PID_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/*============================================================================
 * pid.h — 增量式/位置式通用 PID(位置式 + 抗积分饱和 + 输出限幅)
 *
 * 用于电机速度闭环：以编码器每控制周期的脉冲增量(实测速度)为反馈，
 * 把命令速度稳定到目标转速，改善低速爬行、负载扰动、左右轮不一致等问题。
 *
 * 用法：
 *   PID_t pid;
 *   PID_Init(&pid, Kp, Ki, Kd, -1000, 1000);   // 输出限幅到电机量程
 *   每个固定周期(dt秒)：
 *     float out = PID_Update(&pid, target, measured, dt);
 *     Motor_Set((int16_t)out);
 *   目标为 0(停车) 时建议 PID_Reset() 清积分，避免残留积分导致爬行。
 *============================================================================*/

typedef struct {
    float kp, ki, kd;
    float integral;      /* 积分累计 */
    float prev_meas;     /* 上次测量(微分用测量微分，抗设定值突变冲击) */
    float out_min, out_max;
    uint8_t has_prev;    /* 首次调用不算微分 */
} PID_t;

/* 初始化并清零状态 */
void  PID_Init(PID_t *p, float kp, float ki, float kd, float out_min, float out_max);

/* 运行时改增益(在线整定用) */
void  PID_SetGains(PID_t *p, float kp, float ki, float kd);

/* 清零积分与历史(切模式/停车/换向时调用) */
void  PID_Reset(PID_t *p);

/* 单步计算：target/measured 单位一致(如脉冲/周期)，dt 为周期(秒)，返回限幅后的输出 */
float PID_Update(PID_t *p, float target, float measured, float dt);

#ifdef __cplusplus
}
#endif

#endif /* __PID_H */
