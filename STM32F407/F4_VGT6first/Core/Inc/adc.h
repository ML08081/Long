/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    adc.h
  * @brief   This file contains all the function prototypes for
  *          the adc.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ADC_H__
#define __ADC_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern ADC_HandleTypeDef hadc1;

/* USER CODE BEGIN Private defines */

/* ===== ADC1 + DMA 循环扫描缓冲（2026-07-23 循迹改模拟量）=====================
 *  ADC1 以 Scan + Continuous + DMA(Circular) 方式后台连续转换 4 个通道，
 *  结果自动写进 g_adc_dma[]，任何任务直接读数组即可，无需 Start/Poll/Stop。
 *  扫描顺序即 CubeMX 里的 Rank 顺序，改 Rank 必须同步改下面的下标。
 *
 *  Rank1 = ADC_CHANNEL_1  (PA1)  MQ2 气体      采样 84  周期
 *  Rank2 = ADC_CHANNEL_10 (PC0)  TCRT 左 AO    采样 480 周期
 *  Rank3 = ADC_CHANNEL_11 (PC1)  TCRT 中 AO    采样 480 周期
 *  Rank4 = ADC_CHANNEL_13 (PC3)  TCRT 右 AO    采样 480 周期
 *  一轮 ≈ (84+3*480+4*12)/14MHz ≈ 113us，循环扫描约 8.8kHz。 */
#define ADC_CH_MQ2      0
#define ADC_CH_LINE_L   1
#define ADC_CH_LINE_C   2
#define ADC_CH_LINE_R   3
#define ADC_CH_COUNT    4

extern volatile uint16_t g_adc_dma[ADC_CH_COUNT];

/* USER CODE END Private defines */

void MX_ADC1_Init(void);

/* USER CODE BEGIN Prototypes */

/* 启动 ADC1 的 DMA 循环扫描。须在 MX_ADC1_Init() 与 MX_DMA_Init() 之后调用一次。 */
void ADC_StartDma(void);

/* 采样是否在正常推进（DMA 有在搬数据）。用于诊断"读数恒定不变"这类故障。 */
uint8_t ADC_IsStreaming(void);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __ADC_H__ */

