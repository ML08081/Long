#ifndef __SERVO2_H
#define __SERVO2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 超声波云台舵机驱动 (Servo2)  —— PC8 (YT_PWM) = TIM8_CH3 硬件 PWM
 *   (2026-07-07 CubeMX 重配后：由 TIM2 中断软件 PWM 迁移为 TIM8 硬件 PWM)
 *
 * 驱动方式：TIM8_CH3 硬件 PWM（普通通道，非互补 CH3N，AF3 → PC8）
 *   计数器硬件生成占空比，完全不占中断、无抖动、不受 FreeRTOS 调度影响。
 *   TIM8 为独立空闲定时器，与编码器(TIM3/4)/HC-SR04(TIM2)/电机(TIM1)互不干扰。
 *
 *   ★时钟约定：TIM8 在 APB2(168MHz)，需 Prescaler=167 → 1MHz(1 计数=1us)，
 *     配合 ARR(Period)=19999 得 20000us=50Hz。故脉宽以 us 为单位直接写入 CCR。
 *     （CubeMX 里若仍是 71 则频率/脉宽错误，务必改为 167。）
 *
 * 全角度范围：500~2500us ↔ 0°~180°
 *============================================================================*/

/*---------- 脉宽定义 ----------*/
#define SERVO2_MID        1500U   /* 中位 ~90°   */
#define SERVO2_MIN         500U   /* 最小 ~0°    */
#define SERVO2_MAX        2500U   /* 最大 ~180°  */
#define SERVO2_PERIOD_US 20000U   /* 50Hz 周期   */

/*---------- 通道定义 ----------*/
/* 引脚(PC8/YT_PWM)与 AF 由 CubeMX 生成的 MX_TIM8_Init / HAL MSP 配置，此处不手动碰 GPIO */
#define SERVO2_TIM        htim8
#define SERVO2_CHANNEL    TIM_CHANNEL_3

/*---------- API ----------*/

/**
 * @brief  初始化云台舵机 (启动 TIM8_CH3 硬件 PWM，回中)
 * @note   在 MX_TIM8_Init() 之后调用即可，无中断依赖
 */
void Servo2_Init(void);

/**
 * @brief  非阻塞设置脉宽，立即返回；实际 PWM 由 TIM2 中断持续生成
 * @param  pulse  脉宽 500~2500us (0°~180°)，超范围自动限幅
 */
void Servo2_SetPulse(uint16_t pulse);

/**
 * @brief  将角度 (0~180) 转换为脉宽并设置（非阻塞）
 */
void Servo2_SetAngle(uint16_t deg);

#ifdef __cplusplus
}
#endif

#endif /* __SERVO2_H */
