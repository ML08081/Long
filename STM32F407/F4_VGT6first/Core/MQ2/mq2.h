#ifndef __MQ2_H
#define __MQ2_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * MQ-2 烟雾/可燃气体传感器驱动
 *
 * 引脚：
 *   AO（模拟量）—— PA1 = ADC1_IN1（单通道，DMA2_Stream0 已配，但本驱动采用
 *                  轮询采样，简单可靠；如需后台连续采样可改 Start_DMA 循环模式）
 *   DO（数字量）—— PC4（模块比较器输出，浓度超板载电位器阈值时有效）
 *                  main.h: MQ2_DO_Pin / MQ2_DO_GPIO_Port
 *
 * 说明：MQ-2 未做 R0 标定，直接以 12bit ADC 原始值 + 软件阈值判定报警，
 *       并结合 DO 硬件比较结果（二者任一触发即报警）。
 *============================================================================*/

/* 软件报警阈值（12bit ADC，0~4095）。烟雾越浓电压越高，值越大。
   出厂环境需实测洁净空气基线后调整；此处给保守默认值。 */
#define MQ2_ADC_ALARM_TH   2200

/* DO 有效电平：多数 MQ-2 模块“检测到超阈值”时 DO 输出低电平 */
#define MQ2_DO_ACTIVE_LOW  1

/* 初始化（ADC 由 MX_ADC1_Init 配好，这里仅做状态复位） */
void MQ2_Init(void);

/* 读取一次 AO 原始值（12bit，0~4095）。失败返回上次值。 */
uint16_t MQ2_ReadRaw(void);

/* 读取 DO 数字报警状态：1=检测到气体，0=正常 */
uint8_t MQ2_ReadDO(void);

/* 综合报警判定：AO 超软件阈值 或 DO 触发 → 1 */
uint8_t MQ2_IsAlarm(void);

#ifdef __cplusplus
}
#endif

#endif /* __MQ2_H */
