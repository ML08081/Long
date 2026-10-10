#ifndef __MOTOR_H
#define __MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "main.h"


// 电机速度范围：-1000 ~ 1000
// 正数 = 正转，负数 = 反转，0 = 刹车
#define MAX_SPEED 1000

/* ===== PWM 载波频率（2026-07-23 降噪）=====================================
 *  TIM1 在 APB2，定时器时钟 168MHz。
 *    原配置 Prescaler=71, Period=999 -> 168M/72/1000 = 2.33kHz
 *    2.33kHz 正落在人耳最敏感的 1~4kHz 频段，这就是电机啸叫的直接来源。
 *  改为 Prescaler=7 -> 168M/8/1000 = 21kHz，超出人耳听觉上限（~20kHz）。
 *  附带好处：载波越高，电机电感对电流的平滑作用越强，转矩脉动更小、低速更稳。
 *  DRV8701 支持到 100kHz PWM，21kHz 完全在规格内。
 *  ★ 这里用运行时改预分频，CubeMX 重生成也不会丢；
 *    但建议同时把 CubeMX 里 TIM1 的 Prescaler 也改成 7，保持配置一致。 */
#define MOTOR_PWM_PRESCALER   7

/* PWM output is distinct from the requested encoder speed (CPS).
 * Preserve small continuous outputs; never boost a low gear to PWM=200.
 * Hardware duty is capped at 50%, with an additional 8-unit/loop limiter.
 * The controller limits acceleration to 150 PWM units/s and may coast faster.
 * Zero bypasses the ramp for immediate stop.
 */
#define MOTOR_MIN_DUTY 0
#define MOTOR_DEADZONE 0
#define MOTOR_MAX_DUTY 500
#define MOTOR_SLEW_PER_TICK 8

// 函数声明
void Motor_Init(void);              /* 设置 PWM 载波频率，须在 MX_TIM1_Init 之后调用 */
void Motor1_Set(int16_t speed);
void Motor2_Set(int16_t speed);
void DRV8701_Enable(void);
void DRV8701_Sleep(void);
uint8_t DRV8701_ReadFault(void);

#ifdef __cplusplus
}
#endif

#endif
