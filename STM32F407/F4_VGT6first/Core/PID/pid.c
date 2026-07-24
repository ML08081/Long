#include "pid.h"

void PID_Init(PID_t *p, float kp, float ki, float kd, float out_min, float out_max)
{
    p->kp = kp; p->ki = ki; p->kd = kd;
    p->out_min = out_min; p->out_max = out_max;
    PID_Reset(p);
}

void PID_SetGains(PID_t *p, float kp, float ki, float kd)
{
    p->kp = kp; p->ki = ki; p->kd = kd;
}

void PID_Reset(PID_t *p)
{
    p->integral  = 0.0f;
    p->prev_meas = 0.0f;
    p->has_prev  = 0;
}

float PID_Update(PID_t *p, float target, float measured, float dt)
{
    if (dt <= 0.0f) dt = 0.001f;

    float err = target - measured;

    /* 微分项对"测量"求导(derivative-on-measurement)，避免设定值阶跃引起微分冲击 */
    float d_meas = p->has_prev ? (measured - p->prev_meas) / dt : 0.0f;
    p->prev_meas = measured;
    p->has_prev  = 1;

    /* 积分累加(先累加，后用抗饱和限制) */
    p->integral += err * dt;

    float out = p->kp * err + p->ki * p->integral - p->kd * d_meas;

    /* 输出限幅 + 抗积分饱和(clamping)：饱和时把积分回退到不再继续累积 */
    if (out > p->out_max) {
        out = p->out_max;
        if (p->ki != 0.0f) p->integral -= err * dt;   /* 撤回本次积分，防 windup */
    } else if (out < p->out_min) {
        out = p->out_min;
        if (p->ki != 0.0f) p->integral -= err * dt;
    }
    return out;
}
