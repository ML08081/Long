#ifndef __DHT11_H
#define __DHT11_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * DHT11 温湿度传感器驱动（单总线）
 *
 * 引脚：DATA —— PD5（单总线，开漏，驱动内动态切换输入/输出）
 *              main.h: DHT11_Pin / DHT11_GPIO_Port
 *
 * 时序：使用 DWT->CYCCNT 微秒级延时（不依赖中断），一次整帧读取 ~4~5ms。
 *       不进临界区：靠 8bit 校验和拒绝被中断打断而损坏的帧，读失败则本周期
 *       标记数据无效（保留上次有效值）。建议 ~1Hz 调用一次。
 *
 * 注意：DHT11 上电需稳定 ~1s；首帧可能失败属正常。
 *============================================================================*/

/* 初始化：使能 DWT 周期计数器 + 引脚拉高空闲 */
void DHT11_Init(void);

/**
 * @brief  读取一帧温湿度
 * @param  temp  输出：整数温度（°C，DHT11 为整数，范围 0~50）
 * @param  humi  输出：整数湿度（%RH，范围 20~90）
 * @retval 1 = 成功（校验通过），0 = 失败（超时或校验错，temp、humi 不更新）
 */
uint8_t DHT11_Read(int8_t *temp, uint8_t *humi);

#ifdef __cplusplus
}
#endif

#endif /* __DHT11_H */
