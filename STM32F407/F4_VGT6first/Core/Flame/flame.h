#ifndef __FLAME_H
#define __FLAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 火焰传感器驱动（数字量，轮询）
 *
 * 引脚：DO —— PD7（GPIO 输入）—— main.h: Flame_Pin / Flame_GPIO_Port
 * 说明：红外火焰模块检测到火焰时 DO 通常输出低电平（active-low）。
 *       采用轮询判定；为抗抖动，连续 N 次一致才切换稳定状态。
 *       （未走 EXTI：当前 .ioc 将 PD7 配为普通输入，安全事件由 EnvSensorTask
 *        以 ~10Hz 轮询足够及时。）
 *============================================================================*/

#define FLAME_ACTIVE_LOW   1    /* 检测到火焰输出低电平 */
#define FLAME_DEBOUNCE_N   10   /* 连续 10 次一致(~1s@10Hz)才更新稳定状态，抗 IR 抖动/误报 */

/* 初始化（PD7 已由 MX_GPIO_Init 配为输入，这里仅复位状态） */
void Flame_Init(void);

/* 轮询推进一次去抖状态机（建议在 EnvSensorTask 每周期调用） */
void Flame_Tick(void);

/* 去抖后的稳定状态：1=检测到火焰，0=无 */
uint8_t Flame_IsDetected(void);

/* 直接读取瞬时电平（未去抖），调试用 */
uint8_t Flame_ReadRaw(void);

#ifdef __cplusplus
}
#endif

#endif /* __FLAME_H */
