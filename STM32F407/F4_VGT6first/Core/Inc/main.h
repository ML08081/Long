/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32f4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void UART_Log(const char *format, ...);
#define LOG(fmt, ...) UART_Log("[%lu] " fmt "\r\n", HAL_GetTick(), ##__VA_ARGS__)
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LINE_l_Pin GPIO_PIN_0
#define LINE_l_GPIO_Port GPIOC
#define LINE_C_Pin GPIO_PIN_1
#define LINE_C_GPIO_Port GPIOC
#define LINE_R_Pin GPIO_PIN_3
#define LINE_R_GPIO_Port GPIOC
#define SERVO_STEER_Pin GPIO_PIN_0
#define SERVO_STEER_GPIO_Port GPIOA
#define HC_TRIG_Pin GPIO_PIN_0
#define HC_TRIG_GPIO_Port GPIOB
#define PWM_1_Pin GPIO_PIN_9
#define PWM_1_GPIO_Port GPIOE
#define IO_1_Pin GPIO_PIN_11
#define IO_1_GPIO_Port GPIOE
#define PWM_2_Pin GPIO_PIN_13
#define PWM_2_GPIO_Port GPIOE
#define IO_2_Pin GPIO_PIN_14
#define IO_2_GPIO_Port GPIOE
#define VL53_SCL_Pin GPIO_PIN_10
#define VL53_SCL_GPIO_Port GPIOB
#define VL53_SDA_Pin GPIO_PIN_11
#define VL53_SDA_GPIO_Port GPIOB
#define VL53_XSHUT_Pin GPIO_PIN_13
#define VL53_XSHUT_GPIO_Port GPIOB
#define VL53_INT_Pin GPIO_PIN_14
#define VL53_INT_GPIO_Port GPIOB
#define ENC2_DIR_Pin GPIO_PIN_13
#define ENC2_DIR_GPIO_Port GPIOD
#define YT_PWM_Pin GPIO_PIN_8
#define YT_PWM_GPIO_Port GPIOC
#define MLX_SDA_Pin GPIO_PIN_9
#define MLX_SDA_GPIO_Port GPIOC
#define MLX_SCL_Pin GPIO_PIN_8
#define MLX_SCL_GPIO_Port GPIOA
#define BTN_FWD_Pin GPIO_PIN_11
#define BTN_FWD_GPIO_Port GPIOA
#define BTN_BACK_Pin GPIO_PIN_12
#define BTN_BACK_GPIO_Port GPIOA
#define Flame_Pin GPIO_PIN_7
#define Flame_GPIO_Port GPIOD
#define HC_ECHO_Pin GPIO_PIN_3
#define HC_ECHO_GPIO_Port GPIOB
#define ENC1_DIR_Pin GPIO_PIN_5
#define ENC1_DIR_GPIO_Port GPIOB
#define Buzzer_Pin GPIO_PIN_8
#define Buzzer_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */
/* 以下三个引脚 CubeMX 未打 Label，按实际接线在此命名（放 USER CODE 区，重生成不丢失）：
 *   PC4 = MQ2 数字报警输出 DO（输入）
 *   PD5 = DHT11 单总线 DATA（驱动内动态切换输入/输出）
 *   PD2 = 报警联动输出（蜂鸣器/继电器/LED，高电平有效） */
#define MQ2_DO_Pin          GPIO_PIN_4
#define MQ2_DO_GPIO_Port    GPIOC
#define DHT11_Pin           GPIO_PIN_5
#define DHT11_GPIO_Port     GPIOD
#define ALARM_OUT_Pin       GPIO_PIN_2
#define ALARM_OUT_GPIO_Port GPIOD
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
