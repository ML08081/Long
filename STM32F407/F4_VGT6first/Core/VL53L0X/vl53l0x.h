#ifndef __VL53L0X_H
#define __VL53L0X_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * VL53L0X 激光测距（ToF）驱动 —— 精简寄存器版 / 单次测距
 *
 * 总线：I2C2（PB10=SCL / PB11=SDA），7-bit 地址 0x29（HAL 用 8-bit 0x52）
 * 控制：XSHUT=PB13（拉高使能）—— main.h: VL53_XSHUT_Pin / VL53_XSHUT_GPIO_Port
 *       INT  =PB14（本驱动用轮询，不使用中断）
 *
 * 初始化流程参照 ST 官方 API（DataInit + StaticInit + PerformRefCalibration）
 * 的精简移植（SPAD 读取 / 默认调优表 / VHV+相位参考校正），仅平台层换为 hi2c2。
 * 单次测距 ~30ms（默认测量时间预算 33ms）。
 *
 * 建议：出厂后台架实测校准阈值；本驱动返回原始 mm，越界/失败返回
 *       VL53_OUT_OF_RANGE。
 *============================================================================*/

#define VL53_I2C_ADDR_8BIT   0x52     /* 0x29 << 1 */
#define VL53_OUT_OF_RANGE    8190     /* 无效/超量程标记（mm） */

/* 使能并初始化传感器。0=成功，负=失败（未上电/型号不符/校准失败）
   初始化成功后自动进入连续测距(back-to-back)模式。 */
int VL53_Init(void);

/* 传感器是否成功初始化 */
uint8_t VL53_IsPresent(void);

/* 总线诊断：0=无ACK(接线/供电/地址)  1=ACK且ID=0xEE(正常)  2=ACK但ID错。
   raw_id 回填读到的 Model ID 字节。用于把 init 失败定位到具体环节。 */
uint8_t VL53_BusProbe(uint8_t *raw_id);

/**
 * @brief  启动连续测距(back-to-back)模式（VL53_Init 已自动调用，一般无需再调）
 *         传感器持续背靠背测量，读取时无需重启测量，读数更快、时延更低。
 */
void VL53_StartContinuous(void);

/**
 * @brief  读取一次测距结果
 * @retval 距离（mm）；无回波/超量程/失败返回 VL53_OUT_OF_RANGE
 * @note   连续模式下只取最新结果并清中断，不再每次重启测量（比单次快很多）。
 */
uint16_t VL53_ReadRange_mm(void);

/**
 * @brief  非阻塞读取：仅当新样本就绪时读取，否则立即返回 0（不忙等）。
 *         供高优先级 RangingTask 轮询使用——绝不 spin 占 CPU。
 * @param  out_mm  就绪时写入距离(mm)（超量程写 VL53_OUT_OF_RANGE）
 * @retval 1=取到新样本  0=尚未就绪（out_mm 不变）
 */
uint8_t VL53_TryReadRange_mm(uint16_t *out_mm);

#ifdef __cplusplus
}
#endif

#endif /* __VL53L0X_H */
