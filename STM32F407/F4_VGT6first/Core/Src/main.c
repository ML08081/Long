/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "cmsis_os.h"
#include "adc.h"
#include "can.h"
#include "dma.h"
#include "i2c.h"
#include "iwdg.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "watchdog.h"
#include "motor.h"
#include "servo.h"
#include "servo2.h"
#include "encoder.h"
#include "hc.h"
#include "buzzer.h"
#include "mq2.h"
#include "flame.h"
#include "dht11.h"
#include "vl53l0x.h"
#include "linetrack.h"
#include "can_link.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

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
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  早期紧急错误输出（直接寄存器操作，不依赖 HAL/外设初始化状态）
 *         在任何阶段都能可靠输出到 USART6 (PC6 TX)。
 */
static void EarlyError_PutChar(char c)
{
    volatile uint32_t delay;
    for (delay = 0; delay < 72000; delay++) {
        if (USART6->SR & USART_SR_TXE) break;
    }
    USART6->DR = (uint8_t)c;
    if (c == '\n') {
        for (delay = 0; delay < 72000; delay++) {
            if (USART6->SR & USART_SR_TXE) break;
        }
        USART6->DR = '\r';
    }
}

static void EarlyError_Print(const char *s)
{
    if (!s) return;
    while (*s) EarlyError_PutChar(*s++);
}

static void EarlyError_Hex32(uint32_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    EarlyError_PutChar('0'); EarlyError_PutChar('x');
    int started = 0;
    for (int i = 28; i >= 0; i -= 4) {
        uint8_t nib = (uint8_t)((val >> i) & 0x0F);
        if (nib || started || i == 0) {
            EarlyError_PutChar(hex[nib]);
            started = 1;
        }
    }
}

static void EarlyError_InitUART6(void)
{
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {
        .Pin       = GPIO_PIN_6,
        .Mode      = GPIO_MODE_AF_PP,
        .Pull      = GPIO_NOPULL,
        .Speed     = GPIO_SPEED_FREQ_VERY_HIGH,
        .Alternate = GPIO_AF8_USART6
    };
    HAL_GPIO_Init(GPIOC, &gpio);

    USART6->CR1 = 0;
    USART6->BRR = (uint16_t)((HAL_RCC_GetPCLK2Freq() + 57600) / 115200);
    USART6->CR3 = 0;
    USART6->CR2 = 0;
    USART6->CR1 = USART_CR1_UE | USART_CR1_TE;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  MX_I2C3_Init();
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_TIM8_Init();
  MX_USART1_UART_Init();
  MX_CAN1_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */

  /* 看门狗聚合喂狗：必须在 osKernelStart 之前清标记，否则任务尚未开始打点、
     WDG_Service 一上来就判超时，会在启动阶段直接触发复位。 */
  WDG_Init();

  /* ADC1 四通道 DMA 循环扫描（MQ2 + 3 路 TCRT 模拟量）。
     必须在 MX_DMA_Init() 与 MX_ADC1_Init() 都完成之后启动。 */
  ADC_StartDma();

  /* ---- 故障精确化：分离 Bus/Usage/MemManage 异常(不再一律升级为 HardFault)
     + 关闭默认写缓冲(DISDEFWBUF)让数据总线错误变“精确”(BFAR 给出确切地址)。
     配合 stm32f4xx_it.c 里的 Fault_Report, 崩溃时会从 USART6(PC6) 打印
     CFSR/BFAR/PC, 直接定位非法内存访问。 ---- */
  SCB->SHCSR |= (SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk | SCB_SHCSR_MEMFAULTENA_Msk);
  SCnSCB->ACTLR |= SCnSCB_ACTLR_DISDEFWBUF_Msk;

  /* ---- 复位原因诊断：定位"启动不正常"根因（欠压/看门狗/软件/按键）----
     BORRST=电源欠压(供电不足) IWDGRST=任务卡死被看门狗复位
     SFTRST=软件复位 PINRST=按键/NRST(正常手动复位) PORRST=正常上电 */
  {
    const char *cause = "UNKNOWN";
    if      (__HAL_RCC_GET_FLAG(RCC_FLAG_BORRST))  cause = "BOR(欠压-供电不足!)";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) cause = "IWDG(看门狗-任务卡死!)";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST)) cause = "WWDG(窗口看门狗)";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))  cause = "SOFT(软件复位)";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))  cause = "PIN(按键/NRST)";
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))  cause = "POR(上电)";
    LOG("[RESET] cause=%s", cause);
    __HAL_RCC_CLEAR_RESET_FLAGS();
  }

  /* ---- 电机：设载波频率 → 启动 TIM1 四通道 PWM + 使能驱动 ---- */
  /* Motor_Init 把 PWM 载波从 2.33kHz 提到 21kHz（消除啸叫，见 motor.h）。
     必须在 HAL_TIM_PWM_Start 之前设好预分频。 */
  Motor_Init();
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);   // Motor1 EN (PE9)
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);   // Motor1 PH (PE11)
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);   // Motor2 EN (PE13)
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);   // Motor2 PH (PE14)
  DRV8701_Enable();                            // 使能电机驱动（外部开关）
  Motor1_Set(0);
  Motor2_Set(0);
  LOG("[BRINGUP] Motors PWM started, DRV8701 enabled, stopped");

  /* ---- 转向舵机 (TIM2_CH1 PA0)，回中 ---- */
  Servo_Init();
  Servo_SetAngle(0);       // 中位
  LOG("[BRINGUP] Steering servo ready, centered (PA0)");

  /* ---- OLED 已移除（2026-07-08 硬件取消，CubeMX 已删 I2C1） ---- */

  /* ---- 编码器 (TIM3 ETR=PA6 / TIM4 ETR=PD12) ---- */
  Encoder_Init();
  LOG("[BRINGUP] Encoders ready (TIM3 PA6 / TIM4 PD12)");

  /* ---- HC-SR04 超声波 (PB0 TRIG / PB3 TIM2_CH2 ECHO) ---- */
  HC_Init();
  LOG("[BRINGUP] HC-SR04 ready (TRIG=PB0, ECHO=TIM2_CH2 PB3)");

  /* ---- 环境/安全传感器 ---- */
  Buzzer_Init();                     // PB8 有源蜂鸣器
  HAL_GPIO_WritePin(ALARM_OUT_GPIO_Port, ALARM_OUT_Pin, GPIO_PIN_RESET);  // PD2 报警联动初始关
  MQ2_Init();                        // PA1(AO)/PC4(DO) 烟雾气体
  Flame_Init();                      // PD7 火焰
  DHT11_Init();                      // PD5 温湿度
  LineTrack_Init();                  // PC0/PC1/PC3 循迹 TCRT×3（GPIO 由 CubeMX 配）
  LOG("[BRINGUP] Env sensors init (MQ2/Flame/DHT11/Buzzer/LineTrack)");
  /* VL53L0X：先总线诊断，再最多 3 次重试初始化（冷启动/上电时序偶发抖动）。
     失败也不阻塞——RangingTask 会在运行时每 2s 自恢复重试(见 Ranging())。 */
  uint8_t vl_id = 0;
  uint8_t vl_probe = VL53_BusProbe(&vl_id);
  int vl = -99;
  for (int attempt = 0; attempt < 3; attempt++) {
    vl = VL53_Init();
    if (vl == 0) break;
    HAL_Delay(20);
  }
  if (vl == 0) {
    LOG("[BRINGUP] VL53L0X ready (I2C2, cont.)  probe=%u id=0x%02X", vl_probe, vl_id);
  } else if (vl_probe == 0) {
    LOG("[BRINGUP] VL53L0X FAIL: 总线无ACK(id=0x%02X) -> 查 PB10=SCL/PB11=SDA/PB13=XSHUT/3.3V/上拉。运行时会自恢复", vl_id);
  } else {
    LOG("[BRINGUP] VL53L0X FAIL: init=%d probe=%u id=0x%02X(应0xEE) -> 芯片应答但初始化未过。运行时会自恢复", vl, vl_probe, vl_id);
  }
  /* VL53 失败 → 扫 I2C2 总线一锤定音：
     0 器件应答 = 总线级问题(上拉缺失/未供电/SDA-SCL接反/未共地/XSHUT压低)；
     有器件 = 芯片在线(0x29=VL53默认)，问题在地址/初始化。 */
  if (vl != 0) {
    int i2c_found = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
      if (HAL_I2C_IsDeviceReady(&hi2c2, (uint16_t)(a << 1), 1, 3) == HAL_OK) {
        LOG("[I2C2] device ACK @0x%02X (7-bit)", a);
        i2c_found++;
      }
    }
    if (i2c_found == 0)
      LOG("[I2C2] 总线全哑: 0 器件应答 -> 上拉缺失 / 未供3.3V / SDA-SCL接反 / 未共地");
    else
      LOG("[I2C2] 共 %d 器件应答 (VL53 默认 7-bit 0x29)", i2c_found);
  }

  /* ---- 云台舵机 Servo2 (PB1, TIM2 中断硬件定时 PWM)，回中 ---- */
  Servo2_Init();
  Servo2_SetPulse(SERVO2_MID);  // 云台回中（非阻塞，脉冲由 TIM2 中断持续生成）
  LOG("[BRINGUP] Pan servo (Servo2) ready, centered (PB1)");

  /* ---- CAN 链路（500kbps，CANopen Node 1，见实施方案 §3）----
     CubeMX 只建外设(MX_CAN1_Init)；过滤器/启动/中断通知须手写，缺了收不到任何帧。
     只放行 0x201 命令 / 0x301 巡检避障 / 0x601 SDO；上行遥测走 0x181/0x281/0x381/0x701。
     USART3 保留为回退链路（LINK_USE_CAN 开关，见 freertos.c）。 */
  CANLink_Init();
#if CAN_LOOPBACK_TEST
  LOG("========================================================");
  LOG("  !!! CAN SILENT-LOOPBACK SELF-TEST MODE (P1) !!!");
  LOG("  TX internally looped to RX; CANTX pin held recessive.");
  LOG("  NOT on the real bus -- PCAN will see NOTHING. Expected.");
  LOG("  PASS = [CAN] tx+ rxoth+ err=0 boff=0 TEC=0 REC=0 LEC=NoErr");
  LOG("  -> then set CAN_LOOPBACK_TEST=0 in can_link.h and reflash!");
  LOG("========================================================");
#else
  LOG("[BRINGUP] CAN1 up @500kbps (RX:0x201/0x301/0x601  TX:0x181/0x281/0x381/0x701)");
#endif

  /* 固件版本横幅：烧录后看这行即可确认跑的是不是最新固件（含各修复） */
  LOG("[FW] F4 build 2026-07-17a: +CAN link(bidir) 500kbps alongside USART3(fallback)");

  LOG("[BRINGUP] All peripherals OK -> starting FreeRTOS scheduler");
  LOG("[INFO] Power-up sequence: pan servo sweeps continuously;");
  LOG("[INFO]   steering self-check LEFT->RIGHT->CENTER; then buttons:");
  LOG("[INFO]   PA11 = Forward, PA12 = Backward (active-low)");

  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  printf() 重定向到 USART6 (PC6 TX)
 *         这样 printf() 输出可以从调试串口看到。
 */
int __io_putchar(int ch)
{
    /* 如果在 USART6 初始化完成前调用，则不输出 */
    if (huart6.Instance == USART6 && (USART6->CR1 & USART_CR1_UE))
    {
        /* 轮询等待 TXE，然后写入 DR */
        while (!(USART6->SR & USART_SR_TXE));
        USART6->DR = (uint8_t)(ch & 0xFF);
    }
    return ch;
}

/**
 * @brief  printf 风格日志输出到 USART6 (PC6 TX)，供 main.h 的 LOG() 宏调用。
 *         FreeRTOS 任务通过 LOG() → UART_Log() 输出调试信息到独立调试串口，
 *         不占用 USART3（蓝牙遥控数据帧通道）。
 */
void UART_Log(const char *format, ...)
{
    char buffer[128];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    /* ★关键：vsnprintf 返回的是“未截断时的完整长度”，可能 > sizeof(buffer)。
       必须夹取到实际写入缓冲区的字节数，否则 HAL_UART_Transmit 会越界读取
       buffer 之后的栈内存——当日志行超过 128B 时会一路读到 RAM 顶 0x20020000
       触发精确总线错误(PRECISERR)。夹到 sizeof-1(截断后有效字符数)。 */
    if (len > (int)sizeof(buffer) - 1) len = (int)sizeof(buffer) - 1;
    if (len > 0)
    {
        /* 有限超时(20ms)：即使 USART6/PC6 调试口异常也不会把启动/任务冻死。
           128B @115200 约 11ms，20ms 留足余量；超时则丢弃本行、继续运行。 */
        HAL_UART_Transmit(&huart6, (uint8_t *)buffer, (uint16_t)len, 20);
    }
}

/**
 * @brief  FreeRTOS configASSERT 调用此函数输出诊断信息
 */
void vAssertCalled(const char *file, int line)
{
    /* 直接写 USART6 寄存器（不依赖 HAL/OS） */
    if (USART6->CR1 & USART_CR1_UE)
    {
        const char *p;
        p = "\r\n!!! RTOS ASSERT: ";
        while (*p) { while(!(USART6->SR & USART_SR_TXE)); USART6->DR = *p++; }
        while (*file) { while(!(USART6->SR & USART_SR_TXE)); USART6->DR = *file++; }
        /* 冒号 + 行号 */
        char tmp[16];
        int n = snprintf(tmp, sizeof(tmp), ":%d !!!\r\n", line);
        for (int i = 0; i < n && i < 16; i++) {
            while(!(USART6->SR & USART_SR_TXE));
            USART6->DR = (uint8_t)tmp[i];
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */

  /* 第一步：使能 USART6 并输出（总是尝试，即使此时外设尚未初始化） */
  EarlyError_InitUART6();
  EarlyError_Print("\r\n");
  EarlyError_Print("!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");
  EarlyError_Print("  ERROR HANDLER TRIGGERED\r\n");
  EarlyError_Print("  CR1="); EarlyError_Hex32(USART6->CR1);
  EarlyError_Print("  SR=");  EarlyError_Hex32(USART6->SR);
  EarlyError_Print("\r\n  USART3: CR1="); EarlyError_Hex32(USART3->CR1);
  EarlyError_Print("  SR=");  EarlyError_Hex32(USART3->SR);
  EarlyError_Print("\r\n!!!!!!!!!!!!!!!!!!!!!!!!!!!\r\n");

  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  EarlyError_InitUART6();
  EarlyError_Print("\r\n!!! ASSERT FAILED: ");
  EarlyError_Print((const char*)file);
  EarlyError_PutChar(':');
  /* 手动输出行号 */
  uint32_t n = line;
  char rev[12]; int ri = 0;
  do { rev[ri++] = '0' + (n % 10); n /= 10; } while (n);
  while (ri > 0) EarlyError_PutChar(rev[--ri]);
  EarlyError_Print(" !!!\r\n");
  __disable_irq();
  while (1);
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
