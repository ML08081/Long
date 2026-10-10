#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/* Mini512Z pulse + DIR interface, NOT quadrature.
 * TIM3 external rising-edge counter PA6, DIR PB5: Motor2 / right wheel.
 * TIM4 external rising-edge counter PD12, DIR PD13: Motor1 / left wheel.
 * CNT wraps at 65535; software applies the DIR sign to each interval.
 * Encoder_UpdateElapsed receives the actual interval in microseconds.
 */

/* 定时器句柄（定义在 tim.h） */
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

/*---------- API ----------*/

/**
 * @brief  初始化编码器（启动 TIM3/TIM4 外部脉冲计数器，清零计数值）
 */
void Encoder_Init(void);

/**
 * @brief  更新编码器计数值（读取脉冲增量，并由 DIR 引脚决定方向）
 *         兼容接口默认17ms；控制任务调用 UpdateElapsed 传入实际间隔
 */
void Encoder_Update(void);
void Encoder_UpdateElapsed(uint32_t elapsed_us);
uint8_t Encoder_IsSampleValid(void);

/**
 * @brief  获取编码器1 有符号累计脉冲数
 */
int32_t Encoder1_GetCount(void);

/**
 * @brief  获取编码器2 有符号累计脉冲数
 */
int32_t Encoder2_GetCount(void);

/**
 * @brief  获取编码器1 上一次采样区间的脉冲增量（有符号）
 */
int16_t Encoder1_GetDelta(void);

/**
 * @brief  获取编码器2 上一次采样区间的脉冲增量（有符号）
 */
int16_t Encoder2_GetDelta(void);

/**
 * @brief  复位两个编码器的计数值
 */
void Encoder_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __ENCODER_H */
