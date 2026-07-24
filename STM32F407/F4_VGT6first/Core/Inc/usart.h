/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
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
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* USER CODE BEGIN Includes */
#include "protocol.h"
/* USER CODE END Includes */

extern UART_HandleTypeDef huart1;

extern UART_HandleTypeDef huart2;

extern UART_HandleTypeDef huart3;

extern UART_HandleTypeDef huart6;

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

void MX_USART1_UART_Init(void);
void MX_USART2_UART_Init(void);
void MX_USART3_UART_Init(void);
void MX_USART6_UART_Init(void);

/* USER CODE BEGIN Prototypes */
void MX_USART3_StartRx(void);         // 启动 USART3 中断接收
void USART3_EnsureRxActive(void);     // 确保 USART3 中断接收处于活动状态
void USART3_SendFrame(const uint8_t *frame, uint8_t len);  // 发送协议帧（不破坏 RX）
void USART3_PollRx(void);               // 轮询补收（防止中断丢字节）
void USART3_LogHwStatus(void);          // 打印 USART3 硬件状态
extern volatile uint32_t g_rx_poll_cnt; // 轮询补收字节数
extern volatile uint8_t  g_remote_ready;  // 收到有效遥控帧标志
extern ControlData       g_remote_data;   // 最新遥控帧数据
extern volatile uint32_t g_rx_byte_cnt;   // USART3 收到字节总数（调试用）
extern volatile uint32_t g_rx_frame_cnt;  // USART3 收到有效帧数（调试用）
extern volatile uint32_t g_rx_crc_err;    // USART3 校验和错误计数（调试用）
extern volatile uint32_t g_rx_header_err; // USART3 帧头同步错误计数（调试用）
extern volatile uint32_t g_rx_tele_reject; // USART3 拒绝的遥测回环帧数
extern volatile uint8_t  g_pid_param_ready;   // 收到有效 PID 参数帧(0x60)
extern PidParams         g_pid_param_rx;      // 最新 PID 参数
extern volatile uint8_t  g_pid_test_ready;    // 收到有效 PID 测试帧(0x61)
extern PidTestCmd        g_pid_test_rx;       // 最新 PID 测试命令
extern volatile uint32_t g_rx_pid_frame_cnt;  // 收到有效 PID 帧数
uint8_t USART3_GetFsmState(void);          // 接收状态机当前状态
/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

