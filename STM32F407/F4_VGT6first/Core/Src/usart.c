/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
/* Includes ------------------------------------------------------------------*/
#include "usart.h"

/* USER CODE BEGIN 0 */
#include "bt_debug.h"   /* BTDbg_On* 蓝牙链路诊断回调声明 */
/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart3_tx;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}
/* USART3 init function */

void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}
/* USART6 init function */

void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspInit 0 */

  /* USER CODE END USART3_MspInit 0 */
    /* USART3 clock enable */
    __HAL_RCC_USART3_CLK_ENABLE();

    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**USART3 GPIO Configuration
    PD8     ------> USART3_TX
    PD9     ------> USART3_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* USART3 DMA Init */
    /* USART3_TX Init */
    hdma_usart3_tx.Instance = DMA1_Stream3;
    hdma_usart3_tx.Init.Channel = DMA_CHANNEL_4;
    hdma_usart3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    hdma_usart3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    hdma_usart3_tx.Init.MemInc = DMA_MINC_ENABLE;
    hdma_usart3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    hdma_usart3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
    hdma_usart3_tx.Init.Mode = DMA_NORMAL;
    hdma_usart3_tx.Init.Priority = DMA_PRIORITY_LOW;
    hdma_usart3_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&hdma_usart3_tx) != HAL_OK)
    {
      Error_Handler();
    }

    __HAL_LINKDMA(uartHandle,hdmatx,hdma_usart3_tx);

    /* USART3 interrupt Init */
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspInit 1 */

  /* USER CODE END USART3_MspInit 1 */
  }
  else if(uartHandle->Instance==USART6)
  {
  /* USER CODE BEGIN USART6_MspInit 0 */

  /* USER CODE END USART6_MspInit 0 */
    /* USART6 clock enable */
    __HAL_RCC_USART6_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    /**USART6 GPIO Configuration
    PC6     ------> USART6_TX
    PC7     ------> USART6_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_6|GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN USART6_MspInit 1 */

  /* USER CODE END USART6_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART3)
  {
  /* USER CODE BEGIN USART3_MspDeInit 0 */

  /* USER CODE END USART3_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART3_CLK_DISABLE();

    /**USART3 GPIO Configuration
    PD8     ------> USART3_TX
    PD9     ------> USART3_RX
    */
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8|GPIO_PIN_9);

    /* USART3 DMA DeInit */
    HAL_DMA_DeInit(uartHandle->hdmatx);

    /* USART3 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART3_IRQn);
  /* USER CODE BEGIN USART3_MspDeInit 1 */

  /* USER CODE END USART3_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART6)
  {
  /* USER CODE BEGIN USART6_MspDeInit 0 */

  /* USER CODE END USART6_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART6_CLK_DISABLE();

    /**USART6 GPIO Configuration
    PC6     ------> USART6_TX
    PC7     ------> USART6_RX
    */
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_6|GPIO_PIN_7);

  /* USER CODE BEGIN USART6_MspDeInit 1 */

  /* USER CODE END USART6_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

// ====================================================================
// USART3 中断接收 — 蓝牙遥控帧解析
// ====================================================================
//
// 帧格式（与 protocol.h 一致）：7 字节
//   [0]   0xAA       帧头
//   [1-2] speed      速度 (int16 BE — 大端序，高字节在前)
//   [3-4] steering   转向 (int16 BE — 大端序，高字节在前)
//   [5]   mode       模式 (uint8，遥测帧 bit7=1)
//   [6]   checksum   XOR 校验 (对字节 0~5)
//
// 接收状态机：
//   IDLE → 收到 0xAA → RECV → 收满 7 字节 → 校验 → 存入 g_remote_data
// ====================================================================

/* 接收状态机（支持变长帧：7B 命令帧 / 19B PID参数帧 0x60 / 11B PID测试帧 0x61） */
#define RX_STATE_IDLE   0
#define RX_STATE_RECV   1

#define RX_FRAME_MAX    FRAME_PIDPARAM_LENGTH   /* 最长帧 = 19 字节 */

static volatile uint8_t  rx_state  = RX_STATE_IDLE;
static volatile uint8_t  rx_frame[RX_FRAME_MAX];
static volatile uint8_t  rx_idx    = 0;
static volatile uint8_t  rx_expect = FRAME_LENGTH;   /* 本帧期望总长（byte[1] 定） */
static       uint8_t     rx_byte   = 0;       // HAL 单字节接收缓冲

volatile uint8_t  g_remote_ready = 0;          // 1 = 收到有效遥控帧
ControlData       g_remote_data  = {0, 0, 0};  // 最新遥控数据
volatile uint8_t  g_pid_param_ready = 0;       // 1 = 收到有效 PID 参数帧(0x60)
PidParams         g_pid_param_rx;              // 最新 PID 参数（ISR 写, 任务读）
volatile uint8_t  g_pid_test_ready  = 0;       // 1 = 收到有效 PID 测试帧(0x61)
PidTestCmd        g_pid_test_rx;               // 最新 PID 测试命令
volatile uint32_t g_rx_byte_cnt  = 0;          // 收到字节总数（含无效字节）
volatile uint32_t g_rx_frame_cnt = 0;          // 收到有效遥控帧数
volatile uint32_t g_rx_pid_frame_cnt = 0;      // 收到有效 PID 帧数(0x60+0x61)
volatile uint32_t g_rx_crc_err   = 0;          // 校验和错误计数
volatile uint32_t g_rx_header_err = 0;         // 帧头错误计数（RECV态中收到非预期字节）
volatile uint32_t g_rx_tele_reject = 0;        // 拒绝的遥测回环帧计数
volatile uint32_t g_rx_poll_cnt    = 0;        // 轮询补收字节数

uint8_t USART3_GetFsmState(void)
{
    return rx_state;
}

/*------------------------------------------------------------------*/
static void USART3_ProcessRxByte(uint8_t byte)
{
    g_rx_byte_cnt++;
    BTDbg_OnRxByte(byte);

    switch (rx_state)
    {
    case RX_STATE_IDLE:
        if (byte == FRAME_HEADER)
        {
            rx_frame[0] = byte;
            rx_idx = 1;
            rx_expect = FRAME_LENGTH;          /* 默认按 7B 命令帧收，byte[1] 再定 */
            rx_state = RX_STATE_RECV;
        }
        break;

    case RX_STATE_RECV:
        /* byte[1] = 帧类型甄别：0x60/0x61 为变长 PID 帧（命令帧 speed 高字节
           合法值仅 0x00~0x03 / 0xFC~0xFF，与 PID 标记不冲突，可安全解复用）。 */
        if (rx_idx == 1)
        {
            if      (byte == FRAME_PIDPARAM_MARKER) rx_expect = FRAME_PIDPARAM_LENGTH;
            else if (byte == FRAME_PIDTEST_MARKER)  rx_expect = FRAME_PIDTEST_LENGTH;
            else                                    rx_expect = FRAME_LENGTH;
        }
        /* 中途 0xAA 重同步启发式：仅对 7B 命令帧生效（PID 帧 payload 可能含
           0xAA，靠 XOR 校验兜底，不做中途重同步以免撕裂合法帧）。 */
        if (rx_expect == FRAME_LENGTH && byte == FRAME_HEADER && rx_idx < 6)
        {
            rx_frame[0] = byte;
            rx_idx = 1;
            rx_state = RX_STATE_RECV;
            g_rx_header_err++;
            break;
        }
        rx_frame[rx_idx++] = byte;
        if (rx_idx >= rx_expect)
        {
            uint8_t raw[RX_FRAME_MAX];
            for (uint8_t i = 0; i < rx_expect; i++) {
                raw[i] = rx_frame[i];
            }

            if (rx_expect == FRAME_PIDPARAM_LENGTH)
            {
                /* 0x60 PID 参数帧：XOR 校验 + 解析 */
                if (Protocol_CalculateChecksum(raw, FRAME_PIDPARAM_LENGTH - 1)
                        == raw[FRAME_PIDPARAM_LENGTH - 1]
                    && Protocol_UnpackPidParam(raw, &g_pid_param_rx))
                {
                    g_pid_param_ready = 1;
                    g_rx_pid_frame_cnt++;
                }
                else g_rx_crc_err++;
            }
            else if (rx_expect == FRAME_PIDTEST_LENGTH)
            {
                /* 0x61 PID 测试帧 */
                if (Protocol_CalculateChecksum(raw, FRAME_PIDTEST_LENGTH - 1)
                        == raw[FRAME_PIDTEST_LENGTH - 1]
                    && Protocol_UnpackPidTest(raw, &g_pid_test_rx))
                {
                    g_pid_test_ready = 1;
                    g_rx_pid_frame_cnt++;
                }
                else g_rx_crc_err++;
            }
            else if (raw[5] & TELEMETRY_FLAG)
            {
                g_rx_tele_reject++;
                BTDbg_OnTelemetryReject(raw);
            }
            else
            {
                ControlData tmp;
                if (Protocol_UnpackCommandFrame(raw, &tmp))
                {
                    g_remote_data  = tmp;
                    g_remote_ready = 1;
                    g_rx_frame_cnt++;
                    BTDbg_OnGoodFrame(raw, tmp.speed, tmp.steering, tmp.mode);
                }
                else
                {
                    g_rx_crc_err++;
                    BTDbg_OnBadFrame(raw, Protocol_CalculateChecksum(raw, 6));
                }
            }
            rx_state = RX_STATE_IDLE;
            rx_idx   = 0;
        }
        break;
    }
}

/*------------------------------------------------------------------*/
static void USART3_RestartRx(void)
{
    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_OREFLAG(&huart3);
        BTDbg_OnRxRestart(0);
    }

    huart3.ErrorCode = HAL_UART_ERROR_NONE;
    huart3.RxState = HAL_UART_STATE_READY;
    rx_state = RX_STATE_IDLE;
    rx_idx   = 0;
    if (HAL_UART_Receive_IT(&huart3, &rx_byte, 1) != HAL_OK)
    {
        BTDbg_OnRxRestart(1);
    }
}

/*------------------------------------------------------------------*/
void MX_USART3_StartRx(void)
{
    // 设置 USART3 中断优先级（不高于 configMAX_SYSCALL_INTERRUPT_PRIORITY）
    HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART3_IRQn);
    USART3_RestartRx();
}

/*------------------------------------------------------------------*/
void USART3_EnsureRxActive(void)
{
    if (huart3.RxState != HAL_UART_STATE_BUSY_RX)
    {
        BTDbg_OnRxRestart(2);
        USART3_RestartRx();
    }
}

/*------------------------------------------------------------------*/
void USART3_SendFrame(const uint8_t *frame, uint8_t len)
{
    /* 忙等 TXE/TC 加超时守卫：UART 异常(ORE/FE 等)时 TXE/TC 可能永不置位，
       原无限忙等会永久卡死发送任务→遥测停→龙芯判 F4 掉线。guard 约 ms 级放弃。 */
    for (uint8_t i = 0; i < len; i++)
    {
        uint32_t guard = 200000U;
        while (!__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TXE))
        {
            if (--guard == 0U) return;   /* 放弃本帧，任务继续，不卡死 */
        }
        huart3.Instance->DR = frame[i];
    }
    uint32_t guard = 200000U;
    while (!__HAL_UART_GET_FLAG(&huart3, UART_FLAG_TC))
    {
        if (--guard == 0U) return;
    }
}

/*------------------------------------------------------------------*/
void USART3_PollRx(void)
{
    /* 若中断未及时读 DR，轮询补收 */
    while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE))
    {
        uint8_t byte = (uint8_t)(huart3.Instance->DR & 0xFFU);
        g_rx_poll_cnt++;
        USART3_ProcessRxByte(byte);
    }

    if (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_ORE))
    {
        __HAL_UART_CLEAR_OREFLAG(&huart3);
        BTDbg_OnRxRestart(0);
        USART3_RestartRx();
    }
}

/*------------------------------------------------------------------*/
void USART3_LogHwStatus(void)
{
    uint32_t sr  = huart3.Instance->SR;
    uint32_t cr1 = huart3.Instance->CR1;
    uint32_t cr3 = huart3.Instance->CR3;
    /* 注：USART3 已迁至 PD8/9，原 PB11 现为 I2C2_SDA，故删除 PB11 电平诊断 */
    uint32_t nvic_en = (NVIC->ISER[USART3_IRQn >> 5] >> (USART3_IRQn & 0x1FU)) & 1U;

    LOG("BTDBG HW: SR=0x%04lX CR1=0x%04lX CR3=0x%04lX", sr, cr1, cr3);
    LOG("BTDBG HW: UE=%lu RE=%lu TE=%lu RXNEIE=%lu NVIC=%lu poll=%lu",
        (cr1 >> 13) & 1U, (cr1 >> 2) & 1U, (cr1 >> 3) & 1U,
        (cr1 >> 5) & 1U, nvic_en, g_rx_poll_cnt);
    if (((cr1 >> 2) & 1U) == 0U) {
        LOG("BTDBG HW WARN: USART3 RE=0 receiver disabled!");
    }
}

/*------------------------------------------------------------------*/
/**
  * @brief  UART 接收完成回调（中断中调用）
  *         每次收到 1 字节后被 HAL 调用，自动继续接收下一字节。
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        USART3_ProcessRxByte(rx_byte);

        if (HAL_UART_Receive_IT(huart, &rx_byte, 1) != HAL_OK)
        {
            BTDbg_OnRxRestart(1);
            USART3_RestartRx();
        }
    }
}

/*------------------------------------------------------------------*/
/**
  * @brief  UART 错误回调 — ORE/FE 等错误后恢复 USART3 接收
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3)
    {
        BTDbg_OnRxRestart(0);
        USART3_RestartRx();
    }
}

/* USER CODE END 1 */
