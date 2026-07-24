#ifndef __ENCODER_H
#define __ENCODER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 编码器驱动 (硬件正交编码器接口方案，2026-07-07 CubeMX 重配后)
 *
 * 原理：
 *   TIM3/TIM4 配置为 Encoder Mode (TI1 and TI2)，A/B 两相直接接定时器
 *   两个通道，硬件按正交相位自动加/减计数 —— TIMx->CNT 本身即含方向，
 *   无需再读方向 GPIO。软件定时读取 CNT 增量 (int16 转型处理 16-bit 回绕)
 *   累加到 position 计数器中。ARR=65535 保证回绕技巧成立。
 *
 * 引脚映射 (.ioc 为准, F407)：
 *   编码器1 (TIM3)：ENC1_A = PA6 (CH1) , ENC1_B = PB5  (CH2)
 *   编码器2 (TIM4)：ENC2_A = PD12(CH1) , ENC2_B = PD13 (CH2)
 *============================================================================*/

/* 定时器句柄（定义在 tim.h） */
extern TIM_HandleTypeDef htim3;
extern TIM_HandleTypeDef htim4;

/*---------- API ----------*/

/**
 * @brief  初始化编码器（启动 TIM3/TIM4 正交编码器接口，清零计数值）
 */
void Encoder_Init(void);

/**
 * @brief  更新编码器计数值（读取 TIMx->CNT 硬件正交增量，含方向）
 *         需以固定周期调用（建议 10~50ms）
 */
void Encoder_Update(void);

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
