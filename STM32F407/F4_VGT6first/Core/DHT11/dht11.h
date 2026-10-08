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
 * 时序：使用 DWT->CYCCNT 微秒级延时，一次整帧读取 ~4~5ms。敏感段短暂
 *       屏蔽中断，避免高电平脉宽被抢占拉长而误判。调用间隔必须 >=2s。
 *
 * 注意：DHT11 上电需稳定 ~1s；首帧可能失败属正常。
 *============================================================================*/

/* 初始化：使能 DWT 周期计数器 + 引脚拉高空闲 */
void DHT11_Init(void);

typedef enum {
    DHT11_ERR_NONE = 0,
    DHT11_ERR_NO_RESPONSE,
    DHT11_ERR_RESPONSE_LOW,
    DHT11_ERR_RESPONSE_HIGH,
    DHT11_ERR_BIT_LOW,
    DHT11_ERR_BIT_HIGH,
    DHT11_ERR_CHECKSUM,
    DHT11_ERR_RANGE
} DHT11_Error;

/**
 * @brief  读取一帧温湿度
 * @param  temp  输出：整数温度（°C，DHT11 为整数，范围 0~50）
 * @param  humi  输出：整数湿度（%RH，范围 20~90）
 * @retval 1 = 成功（校验通过），0 = 失败（超时或校验错，temp、humi 不更新）
 */
uint8_t DHT11_Read(int8_t *temp, uint8_t *humi);

/* 最近一次读取结果，供现场日志区分接线、时序和校验故障。 */
DHT11_Error DHT11_GetLastError(void);

#ifdef __cplusplus
}
#endif

#endif /* __DHT11_H */
