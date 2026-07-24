/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
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
#include "main.h"
#include "stm32f4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "hc.h"
#include "tim.h"
#include "servo2.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ============================================================================
 *  硬件故障(HardFault/Bus/Usage/MemManage)诊断打印
 *  --------------------------------------------------------------------------
 *  直接写 USART6(PC6, 115200) 寄存器输出——故障上下文里 HAL/FreeRTOS/SysTick
 *  都不可靠(HAL_UART_Transmit 依赖 tick, 故障优先级 -1 会屏蔽 SysTick), 故用
 *  与 vAssertCalled 相同的裸寄存器轮询法, 保证任何时候都能吐出诊断。
 *
 *  输出关键字段:
 *    CFSR  故障状态寄存器: 低8位=MemManage / 8~15=BusFault / 16~31=UsageFault
 *          · PRECISERR(bit9): 精确数据总线错误 → BFAR 即被非法访问的地址
 *          · IMPRECISERR(bit10): 非精确(多为 DMA/延迟写), BFAR 无效
 *          · IBUSERR(bit8): 取指错误(跳到非法地址)
 *          · UNALIGNED(bit24)/DIVBYZERO(bit25)/UNDEFINSTR(bit16)
 *    BFAR  出错的数据地址(BFARVALID=CFSR bit15 时有效)
 *    PC    出错指令地址(在 .map / 反汇编里定位是哪个函数哪一行)
 *  ==========================================================================*/
static void fault_putc(char c)
{
    while (!(USART6->SR & USART_SR_TXE)) { }
    USART6->DR = (uint8_t)c;
}
static void fault_puts(const char *s)
{
    while (*s) { fault_putc(*s++); }
}
static void fault_puthex(uint32_t v)
{
    fault_putc('0'); fault_putc('x');
    for (int i = 28; i >= 0; i -= 4)
    {
        uint32_t nib = (v >> i) & 0xFU;
        fault_putc((char)(nib < 10U ? ('0' + nib) : ('A' + nib - 10U)));
    }
}

/* 由各故障处理函数的内联汇编跳板调用：sp=出错时的异常栈帧, exc=EXC_RETURN(LR) */
void Fault_Report(uint32_t *sp, uint32_t exc)
{
    uint32_t cfsr = SCB->CFSR;
    uint32_t hfsr = SCB->HFSR;

    fault_puts("\r\n\r\n!!! HARD FAULT (BusFault/HardFault) !!!\r\n");
    fault_puts("CFSR="); fault_puthex(cfsr);
    fault_puts(" HFSR="); fault_puthex(hfsr); fault_puts("\r\n");

    if (cfsr & (1U << 15)) { fault_puts("BFAR(faulting addr)="); fault_puthex(SCB->BFAR); fault_puts("\r\n"); }
    if (cfsr & (1U << 7))  { fault_puts("MMFAR(faulting addr)="); fault_puthex(SCB->MMFAR); fault_puts("\r\n"); }

    if (cfsr & (1U << 9))  fault_puts("-> PRECISERR: 精确数据总线错误(BFAR=出错地址)\r\n");
    if (cfsr & (1U << 10)) fault_puts("-> IMPRECISERR: 非精确总线错误(多为DMA/延迟写, BFAR无效)\r\n");
    if (cfsr & (1U << 8))  fault_puts("-> IBUSERR: 取指总线错误(PC跳到非法地址)\r\n");
    if (cfsr & (1U << 11)) fault_puts("-> STKERR: 入栈时总线错误(疑似栈溢出/SP非法)\r\n");
    if (cfsr & (1U << 12)) fault_puts("-> UNSTKERR: 出栈时总线错误\r\n");
    if (cfsr & (1U << 24)) fault_puts("-> UNALIGNED: 非对齐访问\r\n");
    if (cfsr & (1U << 25)) fault_puts("-> DIVBYZERO: 除零\r\n");
    if (cfsr & (1U << 16)) fault_puts("-> UNDEFINSTR: 未定义指令\r\n");
    if (cfsr & (1U << 0))  fault_puts("-> IACCVIOL: 取指访问越权(MPU)\r\n");
    if (cfsr & (1U << 1))  fault_puts("-> DACCVIOL: 数据访问越权(MPU, MMFAR=地址)\r\n");

    /* 异常栈帧: [R0 R1 R2 R3 R12 LR PC xPSR]。任务上下文(PSP)时完全准确 */
    fault_puts("R0=");  fault_puthex(sp[0]); fault_puts(" R1=");  fault_puthex(sp[1]); fault_puts("\r\n");
    fault_puts("R2=");  fault_puthex(sp[2]); fault_puts(" R3=");  fault_puthex(sp[3]); fault_puts("\r\n");
    fault_puts("R12="); fault_puthex(sp[4]); fault_puts(" LR=");  fault_puthex(sp[5]); fault_puts("\r\n");
    fault_puts("PC(出错指令)="); fault_puthex(sp[6]);
    fault_puts(" xPSR="); fault_puthex(sp[7]); fault_puts("\r\n");
    fault_puts("EXC_RETURN="); fault_puthex(exc);
    fault_puts((exc & 0x4U) ? " (故障发生在任务/PSP, 栈帧准确)\r\n"
                            : " (故障发生在内核/MSP, 栈帧可能偏移)\r\n");
    fault_puts(">>> 拿 PC 值到 build/Debug/F4_VGT6first.map 或反汇编定位出错函数\r\n");
    fault_puts(">>> BFAR 是被非法访问的地址: 0x4000_xxxx=外设(时钟?)  0x0800_xxxx=写Flash  0x0000_00xx=空指针\r\n\r\n");

    while (1) { }
}

/* 汇编跳板宏: 取出异常栈指针(MSP/PSP)与 EXC_RETURN 后跳到 Fault_Report。
   放在各故障处理函数体首行, 不返回。 */
#define FAULT_TRAMPOLINE()                       \
    __asm volatile (                             \
        "tst   lr, #4              \n"           \
        "ite   eq                  \n"           \
        "mrseq r0, msp             \n"           \
        "mrsne r0, psp             \n"           \
        "mov   r1, lr              \n"           \
        "ldr   r2, =Fault_Report   \n"           \
        "bx    r2                  \n"           \
    )

/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_adc1;
extern CAN_HandleTypeDef hcan1;
extern DMA_HandleTypeDef hdma_usart3_tx;
extern UART_HandleTypeDef huart3;
extern TIM_HandleTypeDef htim6;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */
   while (1)
  {
  }
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */
  FAULT_TRAMPOLINE();
  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */
    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */
  FAULT_TRAMPOLINE();
  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */
  FAULT_TRAMPOLINE();
  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */
  FAULT_TRAMPOLINE();
  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles DMA1 stream3 global interrupt.
  */
void DMA1_Stream3_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream3_IRQn 0 */

  /* USER CODE END DMA1_Stream3_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_usart3_tx);
  /* USER CODE BEGIN DMA1_Stream3_IRQn 1 */

  /* USER CODE END DMA1_Stream3_IRQn 1 */
}

/**
  * @brief This function handles CAN1 RX0 interrupts.
  */
void CAN1_RX0_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_RX0_IRQn 0 */

  /* USER CODE END CAN1_RX0_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_RX0_IRQn 1 */

  /* USER CODE END CAN1_RX0_IRQn 1 */
}

/**
  * @brief This function handles CAN1 SCE interrupt.
  */
void CAN1_SCE_IRQHandler(void)
{
  /* USER CODE BEGIN CAN1_SCE_IRQn 0 */

  /* USER CODE END CAN1_SCE_IRQn 0 */
  HAL_CAN_IRQHandler(&hcan1);
  /* USER CODE BEGIN CAN1_SCE_IRQn 1 */

  /* USER CODE END CAN1_SCE_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */

  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */

  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles TIM6 global interrupt, DAC1 and DAC2 underrun error interrupts.
  */
void TIM6_DAC_IRQHandler(void)
{
  /* USER CODE BEGIN TIM6_DAC_IRQn 0 */

  /* USER CODE END TIM6_DAC_IRQn 0 */
  HAL_TIM_IRQHandler(&htim6);
  /* USER CODE BEGIN TIM6_DAC_IRQn 1 */

  /* USER CODE END TIM6_DAC_IRQn 1 */
}

/**
  * @brief This function handles DMA2 stream0 global interrupt.
  */
void DMA2_Stream0_IRQHandler(void)
{
  /* USER CODE BEGIN DMA2_Stream0_IRQn 0 */

  /* USER CODE END DMA2_Stream0_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_adc1);
  /* USER CODE BEGIN DMA2_Stream0_IRQn 1 */

  /* USER CODE END DMA2_Stream0_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief  TIM2 全局中断
  *         - 更新中断 (UIF) ：累加 32 位微秒时间戳的高位溢出计数 (HC_GetUs)
  *         - 捕获中断 (CC2IF)：处理 HC-SR04 ECHO 上升/下降沿（非阻塞测距）
  *
  *  注意：TIM2_CH1 用作转向舵机 PWM，CH2 用作 HC-SR04 输入捕获。
  *  NVIC 与 CC2IE/UIE 由 HC_Init() 使能；此处必须提供 ISR，
  *  否则 TIM2 每 ~20ms 溢出会跳转到默认死循环处理函数导致系统卡死。
  */
void TIM2_IRQHandler(void)
{
  /* 更新中断（计数器溢出）→ 累加微秒计数高位 */
  /* 【2026-07-06 诊断回退】暂时移除云台舵机的 Servo2_OnUpdate/OnCompare，
     TIM2 ISR 恢复为改动前(仅 HC)，以隔离"启动异常"是否与舵机 TIM2 改动有关。 */
  if ((TIM2->DIER & TIM_DIER_UIE) && (TIM2->SR & TIM_SR_UIF))
  {
    TIM2->SR = ~TIM_SR_UIF;     /* 清除更新标志（写 0 清除，写 1 不影响其它 rc_w0 位） */
    HC_OnTimerOverflow();
  }

  /* 通道 2 捕获中断 → ECHO 边沿处理（HC_TIM2_IRQHandler 内部清 CC2IF） */
  if ((TIM2->DIER & TIM_DIER_CC2IE) && (TIM2->SR & TIM_SR_CC2IF))
  {
    HC_TIM2_IRQHandler();
  }
}

/* USER CODE END 1 */
