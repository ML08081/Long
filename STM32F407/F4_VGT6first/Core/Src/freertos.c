/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "servo.h"
#include "servo2.h"
#include "motor.h"
#include "encoder.h"
#include "hc.h"
#include "protocol.h"
#include "stdio.h"
#include "string.h"
#include "tim.h"
#include "usart.h"
#include "bt_debug.h"
#include "boot_trace.h"
#include "thermal.h"
#include "buzzer.h"
#include "mq2.h"
#include "flame.h"
#include "dht11.h"
#include "vl53l0x.h"
#include "linetrack.h"
#include "patrol.h"
#include "watchdog.h"
#include "i2c_recover.h"
#include "pid.h"
#include "can_link.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* ============================================================================
 *  任务分层（2026-07-08 重构，对应《深度优化报告》方案A + CubeMX 任务表）：
 *    MotionControlTa (Realtime,   17ms) 运动闭环：遥控解析/差速/PID/测试激励
 *    RangingTask     (AboveNorm1,  1ms) 测距采集：HC 状态机推进 + VL53 非阻塞读
 *    ScanTask        (AboveNormal)      云台三态：开机建图扫一次 → 居中静止
 *    TelemetryTask   (Normal,     20ms) 上报专职：7B 遥测 + 扩展帧 + PID 遥测
 *    EnvTask         (BelowNormal,100ms)慢速环境：MQ2/火焰/DHT11/蜂鸣 + 0x5C
 *    ThermalTask     (Low,        200ms)热成像原始帧转发（USART1 专线）
 *  原则：采集/决策/上报分离；阻塞大户(DHT11/热成像TX)下沉低优先级；
 *        Encoder_Update 仅 MotionControlTask 调用（修双任务竞态）；
 *        OLED 已移除（硬件取消，2026-07-08）。
 * ==========================================================================*/

/* 平滑过渡速度档位 */
#define RAMP_REMOTE_STEP    SERVO_RAMP_FAST_STEP   // 遥控模式：步长上限，比例步长自动减速收尾
#define RAMP_SWITCH_STEP    8                       // 模式切换：中速步长上限

/* ===== 遥控避障锁（2026-07-23）=========================================
 *  遥控时操作不当容易撞前方障碍。加一道硬锁：前方过近立即停止前进，
 *  且【不会因为障碍移开就自动放行】，必须操作者二次确认才解锁，
 *  避免"贴着障碍物继续推摇杆"这种最危险的情形。
 *
 *  触发：前方有效距离 <= OBS_LOCK_ON_MM（8cm）
 *  解锁：障碍已退到 OBS_LOCK_OFF_MM（12cm，迟滞防抖）之外，
 *        且操作者连续给出 OBS_UNLOCK_NEED 次【新的前进意图】。
 *        "新的前进意图" = speed 从 <=0 跳变到 >0 的上升沿 —— 之所以用边沿
 *        而不是数帧，是因为龙芯 30Hz 持续下发命令帧（兼心跳），
 *        按帧数算 2 帧只要 66ms，锁形同虚设；用边沿则要求操作者
 *        真的松手再推两次，才是有意义的"二次确认"。
 *  锁定期间：前进(speed>0)一律置 0；后退(speed<0)放行，便于倒车脱困。 */
#define OBS_LOCK_ON_MM       80U    /* 8cm：进入锁定 */
#define OBS_LOCK_OFF_MM     120U    /* 12cm：迟滞解除阈值，防止在阈值附近反复抖动 */
#define OBS_UNLOCK_NEED       2U    /* 需要的前进意图边沿次数 */
#define OBS_UNLOCK_WINDOW_MS 5000U  /* 两次意图的最大间隔，超时清零重新计数 */

/* ★ 2026-07-23 实测修正：触发锁必须连续确认，且优先信任激光。
 *   现象：行进中超声波读数剧烈跳变（85cm→17cm→9cm→85cm），同期 VL53 稳定 867mm。
 *   根因：HC-SR04 锥角宽(±15°)，车一动就打到地面/侧物产生假回波，叠加电机 EMI。
 *   原实现取 min(VL53,HC) 且【单次读数即触发】，正好被这些毛刺打中 ——
 *   一跳到 9cm 就锁死前进，操作者推两次解锁后又被下一个毛刺锁上，
 *   表现为行进中反复顿挫，也就是用户反馈的"卡顿"。
 *   现策略：
 *     1) VL53 有效时【以 VL53 为准】—— 窄视场、抗 EMI、毫米级精度；
 *        仅当 VL53 超量程(前方 1.2m 内无目标)时才回退到超声波。
 *     2) 无论用哪个源，都要连续 OBS_CONFIRM_N 次判近才真正锁定，单次毛刺不算数。 */
#define OBS_CONFIRM_N         3U    /* 连续确认次数（17ms/次 → 约 51ms 才锁） */

/* 云台开机建图扫描步长(us/10ms循环)：全行程 2000us / 8 ≈ 2.5s 单程 */
#define SWEEP2_STEP         8

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/*
 * 遥控心跳超时 (Heartbeat Timeout)
 * ================================
 * MotionControlTask 每 17ms 运行一次。
 * 超时阈值 (单位：MotionControlTask 周期 = 17ms)：
 *   HB_ACTIVE   0~5    (~85ms)   正常控制
 *   HB_HOLD     6~30   (~510ms)  保持最后指令（容忍偶发丢帧）
 *   超过        →                安全停车
 */
#define HB_ACTIVE_MAX     5    // 正常控制窗口 (~85ms)
#define HB_HOLD_MAX      30    // 保持窗口 (~510ms)，之后安全停车
#define BTDBG_POLL_TICKS   59  // 59 × 17ms ≈ 1 秒，诊断输出周期

#ifndef THERMAL_TASK_ENABLED
#define THERMAL_TASK_ENABLED 1     /* MLX90640 原始帧转发（USART1 专线） */
#endif

/* ★ CAN 链路开关（实施方案 §3.6.6）：1=上行遥测/心跳同时走 CAN（与 USART3 并行，
   便于 A/B 对比与 candump 验证）；0=仅 USART3。命令 RX 两条链路始终都收，
   谁连着谁生效（见 can_link.c 的 0x201 分发与 usart.c 的 ISR）。 */
#ifndef LINK_USE_CAN
#define LINK_USE_CAN 1
#endif

/* ===== 电机速度闭环 PID 默认参数 =====================================
 *  下列宏仅是【上电默认值】——运行时可经 0x60 PID 参数帧在线修改
 *  （LongLook 链路调试页 → 龙芯 → F4），调好后把最终值写回这里固化。
 *  MOTION_PID_DEFAULT_CLOSED: 0=开环直通(出厂默认) 1=上电即闭环
 *  MOTION_PID_MAX_DELTA: 全速(±1000)每 17ms 编码器脉冲增量，须实车标定
 *    （用调试页"开环 PWM=1000"测试读 meas 稳态值）。 */
/* ★ 2026-07-21 台架实测整定完成（驱动轮悬空，编码器改脉冲+方向解码后）：
 *   系统增益 K ≈ 1.1 counts/(17ms·PWM单位)，来自开环标定 PWM=1000 -> 1098 counts。
 *   Kp=1.0：K·Kp≈1.1 接近单位增益，响应快且不超调（Kp=2.0 会超调 2.2 倍而震荡）。
 *   Ki=4.0：约 260ms 消除静差。实测 target=500 -> meas 稳定 495~504（静差 ≈ ±1%），
 *           左右轮高度对称，无超调。
 *   Kd=0  ：位置式 PID 已用 derivative-on-measurement，速度环加 D 只放大噪声。
 *   MaxDelta=1100：开环 PWM=1000 单轮满速实测 1098 counts/17ms。 */
#define MOTION_PID_KP        1.0f
#define MOTION_PID_KI        4.0f
#define MOTION_PID_KD        0.0f
#define MOTION_PID_MAX_DELTA 1100.0f  /* 实测标定值(原 300 为占位) */
#define MOTION_PID_DT        0.017f   /* 控制周期(s)，与 osDelay(17) 一致 */
/* ★ 2026-07-23：上电即闭环。
 *   开环直通给两个电机【相同 PWM】，但两侧电机/减速箱/轮胎摩擦不可能完全一致，
 *   实际转速必然有偏差 —— 表现为遥控直行时跑偏。而编码器虽然一直在读（dL/dR），
 *   开环分支下只用于遥测显示，等于摆设。
 *   置 1 后走常规闭环分支：命令速度映射为目标脉冲增量，由 PID 用编码器实测增量
 *   闭环校正各轮 PWM，两轮转速自动对齐，编码器真正进入控制回路。
 *   参数用上面 2026-07-21 台架整定并固化的值（静差 ≈ ±1%，无超调）。 */
#define MOTION_PID_DEFAULT_CLOSED 1
#define PID_TUNING_ENABLED   0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

/* USART3 发送互斥锁：TelemetryTask / EnvTask 共用同一 TX，
   用带优先级继承的互斥锁串行化，避免帧交错损坏。 */
osMutexId_t g_usart3_mtx = NULL;
static const osMutexAttr_t usart3_mtx_attr = {
  .name = "usart3Tx",
  .attr_bits = osMutexPrioInherit,
};

/* 加锁发送一帧到 USART3（互斥锁未建成时退化为直接发送） */
static void USART3_SendFrameSync(const uint8_t *f, uint8_t len)
{
  if (g_usart3_mtx) osMutexAcquire(g_usart3_mtx, osWaitForever);
  USART3_SendFrame(f, len);
  if (g_usart3_mtx) osMutexRelease(g_usart3_mtx);
}

/* USART6 debug port (COM26 on the bench): accept the same binary control/PID
   frames as USART3 so PID can be tuned directly through the local USB-serial
   adapter, without Loongson/LongLook in the loop. */
#define DBG_RX_STATE_IDLE 0U
#define DBG_RX_STATE_RECV 1U
static uint8_t dbg_rx_state = DBG_RX_STATE_IDLE;
static uint8_t dbg_rx_frame[FRAME_PIDPARAM_LENGTH];
static uint8_t dbg_rx_idx = 0;
static uint8_t dbg_rx_expect = FRAME_LENGTH;

static void DebugUart_ProcessRxByte(uint8_t byte)
{
  switch (dbg_rx_state)
  {
  case DBG_RX_STATE_IDLE:
    if (byte == FRAME_HEADER)
    {
      dbg_rx_frame[0] = byte;
      dbg_rx_idx = 1;
      dbg_rx_expect = FRAME_LENGTH;
      dbg_rx_state = DBG_RX_STATE_RECV;
    }
    break;

  case DBG_RX_STATE_RECV:
    if (dbg_rx_idx == 1U)
    {
      if      (byte == FRAME_PIDPARAM_MARKER) dbg_rx_expect = FRAME_PIDPARAM_LENGTH;
      else if (byte == FRAME_PIDTEST_MARKER)  dbg_rx_expect = FRAME_PIDTEST_LENGTH;
      else                                    dbg_rx_expect = FRAME_LENGTH;
    }
    if (dbg_rx_expect == FRAME_LENGTH && byte == FRAME_HEADER && dbg_rx_idx < 6U)
    {
      dbg_rx_frame[0] = byte;
      dbg_rx_idx = 1;
      g_rx_header_err++;
      break;
    }
    dbg_rx_frame[dbg_rx_idx++] = byte;
    if (dbg_rx_idx >= dbg_rx_expect)
    {
      uint8_t raw[FRAME_PIDPARAM_LENGTH];
      for (uint8_t i = 0; i < dbg_rx_expect; ++i) raw[i] = dbg_rx_frame[i];

      if (dbg_rx_expect == FRAME_PIDPARAM_LENGTH)
      {
#if PID_TUNING_ENABLED
        if (Protocol_CalculateChecksum(raw, FRAME_PIDPARAM_LENGTH - 1U) == raw[FRAME_PIDPARAM_LENGTH - 1U] &&
            Protocol_UnpackPidParam(raw, &g_pid_param_rx))
        {
          g_pid_param_ready = 1;
          g_rx_pid_frame_cnt++;
          LOG("[DDBG] PID param frame accepted via USART6");
        }
        else g_rx_crc_err++;
#endif
      }
      else if (dbg_rx_expect == FRAME_PIDTEST_LENGTH)
      {
#if PID_TUNING_ENABLED
        if (Protocol_CalculateChecksum(raw, FRAME_PIDTEST_LENGTH - 1U) == raw[FRAME_PIDTEST_LENGTH - 1U] &&
            Protocol_UnpackPidTest(raw, &g_pid_test_rx))
        {
          g_pid_test_ready = 1;
          g_rx_pid_frame_cnt++;
          LOG("[DDBG] PID test frame accepted via USART6");
        }
        else g_rx_crc_err++;
#endif
      }
      else if ((raw[5] & TELEMETRY_FLAG) == 0U)
      {
        ControlData tmp;
        if (Protocol_UnpackCommandFrame(raw, &tmp))
        {
          g_remote_data = tmp;
          g_remote_ready = 1;
          g_rx_frame_cnt++;
          LOG("[DDBG] cmd via USART6 spd=%d steer=%d mode=%u", tmp.speed, tmp.steering, tmp.mode);
        }
        else g_rx_crc_err++;
      }

      dbg_rx_state = DBG_RX_STATE_IDLE;
      dbg_rx_idx = 0;
    }
    break;
  }
}

static void DebugUart_PollRx(void)
{
  while (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_RXNE))
  {
    DebugUart_ProcessRxByte((uint8_t)(huart6.Instance->DR & 0xFFU));
  }
  if (__HAL_UART_GET_FLAG(&huart6, UART_FLAG_ORE))
  {
    __HAL_UART_CLEAR_OREFLAG(&huart6);
    dbg_rx_state = DBG_RX_STATE_IDLE;
    dbg_rx_idx = 0;
  }
}

/* 链路激活标志：由 MotionControlTask 维护（1=SBC/手柄已连接）。
   Telemetry/Env/Thermal 据此决定是否上报，避免链路空闲时占满 USART3。 */
volatile uint8_t g_remote_active = 0;

/* ---- RangingTask 发布的激光测距（EnvTask/运动决策消费，32 位读写在 M4 上原子） ---- */
volatile uint16_t g_vl53_mm      = VL53_OUT_OF_RANGE;
volatile uint32_t g_vl53_ok_cnt  = 0;    /* 有效样本计数（诊断链路活性） */
volatile uint8_t  g_line_bits    = 0;    /* 循迹位域(L/C/R/junction/lost)，RangingTask 发布 */
volatile uint8_t  g_obs_lock     = 0;    /* 遥控避障锁状态，MotionControlTask 发布，EnvTask 上报 */

/* ---- 巡检事件/状态（MotionControlTask 产生 → TelemetryTask 经 CAN 0x481 上报）---- */
volatile uint8_t  g_patrol_state     = 0;   /* PatrolStateT */
volatile uint8_t  g_patrol_event     = 0;   /* PatrolEventT，待上报 */
volatile uint8_t  g_patrol_point_seq = 0;   /* 路口计数 */
volatile uint8_t  g_patrol_evt_ready = 0;   /* 1=有新事件待发 */

/* ---- MotionControlTask 发布的运动快照（TelemetryTask 消费） ---- */
volatile int16_t g_mot_speed      = 0;   /* 当前命令速度 */
volatile int16_t g_mot_steer_tele = 0;   /* 舵机回读转向(-1000~1000) */
volatile uint8_t g_mot_mode       = MODE_MANUAL;

/* ---- 运行时 PID 参数/状态（MotionControlTask 独占写；tele 快照 volatile 供读） ---- */
static PidParams g_pid = {
  MOTION_PID_KP, MOTION_PID_KI, MOTION_PID_KD,
  MOTION_PID_MAX_DELTA, MOTION_PID_DEFAULT_CLOSED
};
/* PID 遥测快照（TelemetryTask 打包 0x62 帧） */
volatile uint8_t g_pt_flags = 0;   /* bit0=闭环 bit1=测试中 bit4-5=测试模式 */
volatile int16_t g_pt_targL = 0, g_pt_measL = 0, g_pt_outL = 0;
volatile int16_t g_pt_targR = 0, g_pt_measR = 0, g_pt_outR = 0;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for MotionControlTa */
osThreadId_t MotionControlTaHandle;
const osThreadAttr_t MotionControlTa_attributes = {
  .name = "MotionControlTa",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityRealtime,
};
/* Definitions for ScanTask */
osThreadId_t ScanTaskHandle;
const osThreadAttr_t ScanTask_attributes = {
  .name = "ScanTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for RangingTask */
osThreadId_t RangingTaskHandle;
const osThreadAttr_t RangingTask_attributes = {
  .name = "RangingTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal1,
};
/* Definitions for TelemetryTask */
osThreadId_t TelemetryTaskHandle;
const osThreadAttr_t TelemetryTask_attributes = {
  .name = "TelemetryTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for EnvTask */
osThreadId_t EnvTaskHandle;
const osThreadAttr_t EnvTask_attributes = {
  .name = "EnvTask",
  .stack_size = 512 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for ThermalTask */
osThreadId_t ThermalTaskHandle;
const osThreadAttr_t ThermalTask_attributes = {
  .name = "ThermalTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartMotionControlTask(void *argument);
void StartScanTask(void *argument);
void Ranging(void *argument);
void Telemetry(void *argument);
void Env(void *argument);
void Thermal(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  g_usart3_mtx = osMutexNew(&usart3_mtx_attr);
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of MotionControlTa */
  MotionControlTaHandle = osThreadNew(StartMotionControlTask, NULL, &MotionControlTa_attributes);

  /* creation of ScanTask */
  ScanTaskHandle = osThreadNew(StartScanTask, NULL, &ScanTask_attributes);

  /* creation of RangingTask */
  RangingTaskHandle = osThreadNew(Ranging, NULL, &RangingTask_attributes);

  /* creation of TelemetryTask */
  TelemetryTaskHandle = osThreadNew(Telemetry, NULL, &TelemetryTask_attributes);

  /* creation of EnvTask */
  EnvTaskHandle = osThreadNew(Env, NULL, &EnvTask_attributes);

  /* creation of ThermalTask */
  ThermalTaskHandle = osThreadNew(Thermal, NULL, &ThermalTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* 旧 EnvSensorTask/ThermalTask(USER CODE 版) 已迁入 CubeMX 任务表的
     EnvTask/ThermalTask（上方生成区创建），此处不再重复创建。 */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartMotionControlTask */
/**
* @brief Function implementing the MotionControlTa thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartMotionControlTask */
void StartMotionControlTask(void *argument)
{
  /* USER CODE BEGIN StartMotionControlTask */
  DUART_Trace("MotionControlTask started (Realtime priority)");

  // 启动 USART3 接收中断
  MX_USART3_StartRx();
  BTDbg_Init();
  LOG("[TASK] MotionControlTask started (Realtime). Steering self-check begins.");

  #define MOTOR_TEST_SPEED  500

  int16_t current_speed = 0;
  uint8_t current_mode = MODE_MANUAL;

  // 心跳超时跟踪
  uint32_t s_remote_age   = 0;
  uint8_t  remote_active  = 0;           // 0=纯本地模式, 1=遥控已激活

  // 差速转向：保存从遥控帧中解析的原始转向值 (-1000~+1000)
  int16_t s_raw_steering  = 0;
  int16_t s_last_left_spd  = 0;
  int16_t s_last_right_spd = 0;

  // 遥控避障锁状态（见文件头 OBS_LOCK_* 宏说明）
  uint8_t  s_obs_lock       = 0;   /* 1=锁定中，前进被强制归零 */
  uint8_t  s_obs_near_cnt   = 0;   /* 连续判近次数，达 OBS_CONFIRM_N 才锁（抗毛刺） */
  uint8_t  s_obs_unlock_cnt = 0;   /* 已累计的前进意图边沿次数 */
  uint32_t s_obs_last_intent_ms = 0; /* 最近一次前进意图时刻，用于超时清零 */
  int16_t  s_obs_prev_cmd_spd = 0; /* 上一帧遥控速度，用于检测 <=0 -> >0 上升沿 */

  // 舵机自检状态机：0=INIT,1=→LEFT,2=WAIT_L,3=→RIGHT,4=WAIT_R,5=→CENTER,6=DONE
  uint8_t  sc_state = 0;
  uint8_t  sc_wait  = 0;

  uint32_t dbg_tick = 0;
  uint32_t bt_dbg_tick = 0;
  int8_t   last_btn_cmd = 0;

  // ---- 速度闭环 PID（运行时参数 g_pid，0x60 帧在线可调） ----
  PID_t pid_left, pid_right;
  PID_Init(&pid_left,  g_pid.kp, g_pid.ki, g_pid.kd, -MAX_SPEED, MAX_SPEED);
  PID_Init(&pid_right, g_pid.kp, g_pid.ki, g_pid.kd, -MAX_SPEED, MAX_SPEED);
  int32_t pid_prev_cnt1 = Encoder1_GetCount();
  int32_t pid_prev_cnt2 = Encoder2_GetCount();

  // ---- PID 测试激励状态（0x61 帧驱动；到时自动停，安全上限见 protocol.h） ----
  uint8_t  test_mode   = PID_TEST_OFF;
  int16_t  test_L = 0, test_R = 0;
  uint32_t test_expire = 0;

  for(;;)
  {
    // ===== 0. 看门狗：本任务打存活点 + 聚合校验后喂狗（唯一喂狗点） =====
    /* 改造前是无条件 IWDG->KR=0xAAAA，只能发现整块 MCU 死掉；
       现由 WDG_Service 校验【所有】受监护任务都在截止期内打过点才喂狗，
       任一任务卡死或数据异常累计超限即停止喂狗，约 5s 后 IWDG 复位。 */
    WDG_Kick(WDG_TASK_MOTION);
    WDG_Service();

    // ===== 自检阶段（仅启动时执行一次；期间不动电机） =====
    if (sc_state < 6)
    {
      uint8_t arrived = (Servo_RampTick() == 0);
      switch (sc_state)
      {
      case 0:
        Servo_SetSmoothAngle(SERVO_ANGLE_MAX, SERVO_RAMP_FAST_STEP);
        LOG("[SELF-CHECK 1/3] Steering -> LEFT  +%d deg", SERVO_ANGLE_MAX);
        sc_state = 1;
        break;
      case 1:
        if (arrived) { sc_wait = 8; sc_state = 2; }
        break;
      case 2:
        if (--sc_wait == 0) {
          Servo_SetSmoothAngle(SERVO_ANGLE_MIN, SERVO_RAMP_FAST_STEP);
          LOG("[SELF-CHECK 2/3] Steering -> RIGHT %d deg", SERVO_ANGLE_MIN);
          sc_state = 3;
        }
        break;
      case 3:
        if (arrived) { sc_wait = 8; sc_state = 4; }
        break;
      case 4:
        if (--sc_wait == 0) {
          Servo_SetSmoothAngle(0, SERVO_RAMP_FAST_STEP);
          LOG("[SELF-CHECK 3/3] Steering -> CENTER (reset)");
          sc_state = 5;
        }
        break;
      case 5:
        if (arrived) {
          sc_state = 6;
          LOG("[SELF-CHECK] DONE. Steering centered, pulse=%u us", Servo_GetCurrentPulse());
          LOG("[READY] Buttons: PA11=Forward PA12=Backward. Ctrl RX on USART3.");
        }
        break;
      }
      osDelay(17);
      continue;
    }

    // ===== 正常控制阶段 ============================================

    Servo_RampTick();
    DebugUart_PollRx();
    USART3_PollRx();
    USART3_EnsureRxActive();

    // ===== 调试心跳（~3s）=====
    dbg_tick++;
    if (dbg_tick >= 176)
    {
      dbg_tick = 0;
      LOG("DBG: rx_frms=%lu pid_frms=%lu err(crc=%lu,hdr=%lu) spd=%d steer=%d "
          "L=%d R=%d age=%lu%s pid(closed=%u kp=%d.%03d maxD=%d) test=%u",
          g_rx_frame_cnt, g_rx_pid_frame_cnt, g_rx_crc_err, g_rx_header_err,
          current_speed, s_raw_steering,
          s_last_left_spd, s_last_right_spd, s_remote_age,
          (remote_active ? " BT" : " LOC"),
          g_pid.closed_loop, (int)g_pid.kp, (int)(g_pid.kp*1000)%1000,
          (int)g_pid.max_delta, test_mode);
      /* 编码器实时核查：开环给正 PWM 时 c1/c2 应递增、dL/dR 应为正；
         若某轮为负=该轮编码器相位反(CubeMX 交换该 TIM 两通道 Polarity)；
         若恒为 0=该轮 A/B 未接或引脚错(TIM3 PA6/PB5, TIM4 PD12/PD13)。 */
      LOG("[ENC] c1=%ld c2=%ld dL=%d dR=%d",
          (long)Encoder1_GetCount(), (long)Encoder2_GetCount(),
          Encoder1_GetDelta(), Encoder2_GetDelta());
      /* 外部时钟模式：CNT 只增不减；DIR 电平决定符号。
         判读：转轮子时 CNT 应单调递增；换向时 DIR 应翻转(0<->1)。 */
      LOG("[ENC_RAW] TIM3_CNT=%u DIR1=%u | TIM4_CNT=%u DIR2=%u",
          (unsigned)TIM3->CNT,
          (unsigned)HAL_GPIO_ReadPin(ENC1_DIR_GPIO_Port, ENC1_DIR_Pin),
          (unsigned)TIM4->CNT,
          (unsigned)HAL_GPIO_ReadPin(ENC2_DIR_GPIO_Port, ENC2_DIR_Pin));
      /* ★ PID 跟踪诊断（2026-07-23 加配重落地后加）：
         targ=目标脉冲增量(由命令速度×MaxDelta映射)，meas=编码器实测增量，out=PWM。
         判读：
           · |out| 长期贴 1000 而 meas 远低于 targ  => 驱动能力不足(负载太重/供电电压太低)
                                                      或 MaxDelta 标高了，PID 一直饱和 → 卡顿
           · meas/targ 稳定在某个比值 k            => 把 MOTION_PID_MAX_DELTA 乘以 k 重标定
           · meas 在 0 附近但 out 不小             => 死区未越过，抬高 MOTOR_MIN_DUTY */
      LOG("[PIDTRK] L targ=%d meas=%d out=%d | R targ=%d meas=%d out=%d | closed=%u",
          g_pt_targL, g_pt_measL, g_pt_outL,
          g_pt_targR, g_pt_measR, g_pt_outR, g_pid.closed_loop);
      /* CAN 链路活性判读：
           rxcmd 涨 = 能收 0x201 命令        tx 涨 = 有帧投递进邮箱
           tx≈drop  = 邮箱恒满 → 帧根本发不出去（对端没 ACK）
           LEC      = ★最后错误码，直接指认物理层病因：
                      ACK    → 没人回 ACK（对端没接/没上电/CANH-CANL 反/RS 未接地/无终端）
                      BitRec/BitDom → 发的和读回的不一致（收发器坏/接线错/总线被拉死）
                      Stuff/Form/CRC → 波特率或采样点不匹配
           BOFF     = 当前是否处于 bus-off（ESR.BOFF 权威现态，非累积计数） */
      LOG("[CAN] rxcmd=%lu rxoth=%lu tx=%lu drop=%lu err=%lu boff=%lu TEC=%lu REC=%lu LEC=%lu(%s) BOFF=%u",
          g_can_rx_cmd_cnt, g_can_rx_other_cnt, g_can_tx_cnt, g_can_tx_drop,
          g_can_err_cnt, g_can_busoff_cnt, CANLink_GetTEC(), CANLink_GetREC(),
          CANLink_GetLEC(), CANLink_LECStr(), CANLink_IsBusOff());
      LOG("[CAN_RECOVER] cnt=%lu", g_can_recover_cnt);
    }
    bt_dbg_tick++;
    if (bt_dbg_tick >= BTDBG_POLL_TICKS)
    {
      bt_dbg_tick = 0;
      BTDbg_Poll(remote_active, s_remote_age, (uint8_t)huart3.RxState);
    }

    // ===== 0.5 PID 在线参数 / 测试命令消费（ISR 写标志，本任务消费） =====
    if (g_pid_param_ready)
    {
      taskENTER_CRITICAL();
      PidParams np = g_pid_param_rx;
      g_pid_param_ready = 0;
      taskEXIT_CRITICAL();
      g_pid = np;
      PID_SetGains(&pid_left,  g_pid.kp, g_pid.ki, g_pid.kd);
      PID_SetGains(&pid_right, g_pid.kp, g_pid.ki, g_pid.kd);
      PID_Reset(&pid_left);            /* 换参清积分：阶跃响应可复现 */
      PID_Reset(&pid_right);
      LOG("[PID] param: kp=%d.%03d ki=%d.%03d kd=%d.%03d maxD=%d closed=%u",
          (int)g_pid.kp, (int)(g_pid.kp*1000)%1000,
          (int)g_pid.ki, (int)(g_pid.ki*1000)%1000,
          (int)g_pid.kd, (int)(g_pid.kd*1000)%1000,
          (int)g_pid.max_delta, g_pid.closed_loop);
    }
    if (g_pid_test_ready)
    {
      taskENTER_CRITICAL();
      PidTestCmd tc = g_pid_test_rx;
      g_pid_test_ready = 0;
      taskEXIT_CRITICAL();
      if (tc.mode == PID_TEST_OFF)
      {
        test_mode = PID_TEST_OFF;
        PID_Reset(&pid_left); PID_Reset(&pid_right);
        Motor1_Set(0); Motor2_Set(0);
        LOG("[PID] test STOP");
      }
      else
      {
        test_mode   = tc.mode;
        test_L      = tc.left;
        test_R      = tc.right;
        test_expire = osKernelGetTickCount() + tc.duration_ms;
        PID_Reset(&pid_left); PID_Reset(&pid_right);
        LOG("[PID] test start: mode=%u L=%d R=%d dur=%ums",
            tc.mode, tc.left, tc.right, tc.duration_ms);
      }
    }
    /* 测试到时自动停（安全）；链路断开也停 */
    if (test_mode != PID_TEST_OFF &&
        (osKernelGetTickCount() >= test_expire))
    {
      test_mode = PID_TEST_OFF;
      PID_Reset(&pid_left); PID_Reset(&pid_right);
      Motor1_Set(0); Motor2_Set(0);
      LOG("[PID] test expired -> stop");
    }

    // ===== 1. 遥控数据消费（优先级高于本地按键） =====
    if (g_remote_ready)
    {
      taskENTER_CRITICAL();
      ControlData remote = g_remote_data;
      g_remote_ready = 0;
      taskEXIT_CRITICAL();

      s_remote_age = 0;
      remote_active = 1;

      current_speed = remote.speed;
      current_mode  = remote.mode;
      s_raw_steering = remote.steering;

      if (current_speed > MAX_SPEED)  current_speed = MAX_SPEED;
      if (current_speed < -MAX_SPEED) current_speed = -MAX_SPEED;

      int16_t target_angle = (int16_t)(-(int32_t)remote.steering * 90 / 1000);
      if (target_angle < SERVO_ANGLE_MIN) target_angle = SERVO_ANGLE_MIN;
      if (target_angle > SERVO_ANGLE_MAX) target_angle = SERVO_ANGLE_MAX;
      Servo_SetSmoothAngle(target_angle, RAMP_REMOTE_STEP);
    }
    else if (!remote_active)
    {
      // 本地按键（PA11 前进 / PA12 后退，内部下拉，按下=1）
      uint8_t btn_forward  = HAL_GPIO_ReadPin(BTN_FWD_GPIO_Port, BTN_FWD_Pin);
      uint8_t btn_backward = HAL_GPIO_ReadPin(BTN_BACK_GPIO_Port, BTN_BACK_Pin);

      int8_t btn_cmd;
      if      (btn_forward == 1 && btn_backward == 0) { current_speed =  MOTOR_TEST_SPEED; btn_cmd = 1; }
      else if (btn_backward == 1 && btn_forward == 0) { current_speed = -MOTOR_TEST_SPEED; btn_cmd = -1; }
      else                                            { current_speed = 0; btn_cmd = 0; }

      if (btn_cmd != last_btn_cmd)
      {
        last_btn_cmd = btn_cmd;
        if (btn_cmd == 1)       LOG("[BTN] FWD -> speed=%d", MOTOR_TEST_SPEED);
        else if (btn_cmd == -1) LOG("[BTN] BACK -> speed=%d", -MOTOR_TEST_SPEED);
        else                    LOG("[BTN] released -> STOP");
      }
    }

    // ===== 1.3 遥控避障锁（仅遥控模式；循迹模式由 patrol.c 自己的停障逻辑负责）=====
    /* 距离融合：VL53(mm) 与超声波(cm) 取【更近的有效值】——两者视场互补，
       任一发现近障都应停车，宁可保守。无效值(0/超量程)不参与比较。 */
    if (current_mode == MODE_MANUAL)
    {
      uint16_t vl53   = g_vl53_mm;
      uint32_t hc_cm  = HC_GetDistance_cm();
      uint16_t near_mm = 0xFFFFU;                     /* 0xFFFF = 前方净空 */
      /* ★ 优先信任激光：VL53 在量程内就只看它，不让超声波的假回波参与判定。
         只有 VL53 超量程（前方 1.2m 内确实没东西）时才回退到超声波兜底。 */
      if (vl53 != VL53_OUT_OF_RANGE && vl53 > 0U)
        near_mm = vl53;
      else if (hc_cm > 0U)
        near_mm = (uint16_t)(hc_cm * 10U);

      uint32_t now_ms = HAL_GetTick();

      /* (1) 进入锁定：需连续 OBS_CONFIRM_N 次判近，单次毛刺不触发 */
      if (near_mm <= OBS_LOCK_ON_MM)
      {
        if (s_obs_near_cnt < 0xFFU) s_obs_near_cnt++;
        if (!s_obs_lock && s_obs_near_cnt >= OBS_CONFIRM_N)
        {
          s_obs_lock = 1;
          s_obs_unlock_cnt = 0;
          /* LOG 走 115200 裸串口，只用 ASCII —— 中文会乱码成 "3?????????" */
          LOG("[OBS] LOCK on: front=%umm (<=%umm, confirmed x%u) -> forward blocked",
              near_mm, (unsigned)OBS_LOCK_ON_MM, (unsigned)s_obs_near_cnt);
        }
        if (s_obs_lock) s_obs_unlock_cnt = 0;         /* 障碍还在，任何确认都作废 */
      }
      else
      {
        s_obs_near_cnt = 0;                           /* 一旦读到不近就清确认计数 */
      }

      /* (2) 前进意图边沿检测：speed 从 <=0 跳到 >0 记一次 */
      if (s_obs_lock)
      {
        if (s_obs_prev_cmd_spd <= 0 && current_speed > 0)
        {
          /* 超时窗口：两次确认间隔过久则重新计数，避免"很久以前推过一次"被算进来 */
          if (s_obs_last_intent_ms != 0U &&
              (now_ms - s_obs_last_intent_ms) > OBS_UNLOCK_WINDOW_MS)
            s_obs_unlock_cnt = 0;
          s_obs_last_intent_ms = now_ms;

          /* 只有障碍已退到迟滞阈值之外，确认才计数——贴着障碍推摇杆不算数 */
          if (near_mm > OBS_LOCK_OFF_MM)
          {
            s_obs_unlock_cnt++;
            LOG("[OBS] unlock intent %u/%u (front=%umm)",
                s_obs_unlock_cnt, (unsigned)OBS_UNLOCK_NEED, near_mm);
            if (s_obs_unlock_cnt >= OBS_UNLOCK_NEED)
            {
              s_obs_lock = 0;
              s_obs_unlock_cnt = 0;
              LOG("[OBS] LOCK released by operator confirm");
            }
          }
          else
          {
            /* 限流：贴着障碍持续推摇杆时这条会每 17ms 打一次，几秒就刷屏几十条，
               把 [DIST]/[PIDTRK] 等真正有用的信息冲掉。每 500ms 最多打一条。 */
            static uint32_t s_lastRejLogMs = 0;
            if ((now_ms - s_lastRejLogMs) > 500U)
            {
              s_lastRejLogMs = now_ms;
              LOG("[OBS] unlock rejected: obstacle still near (front=%umm, need >%umm)",
                  near_mm, (unsigned)OBS_LOCK_OFF_MM);
            }
          }
        }
      }
      s_obs_prev_cmd_spd = current_speed;

      /* (3) 锁定生效：前进归零，后退放行（倒车脱困） */
      if (s_obs_lock && current_speed > 0)
        current_speed = 0;
    }
    else
    {
      /* 切出遥控模式：清锁，避免残留状态影响下次进遥控 */
      s_obs_lock = 0;
      s_obs_near_cnt = 0;
      s_obs_unlock_cnt = 0;
      s_obs_prev_cmd_spd = 0;
    }
    g_obs_lock = s_obs_lock;      /* 发布给 EnvTask 打 ENV_FLAG_OBS_LOCK 上报 */

    // ===== 1.4 龙芯下发的巡检命令（CAN 0x301 sub=0x01/0x02）=====
    if (g_patrol_cmd_ready)
    {
      uint8_t c = g_patrol_cmd_rx; g_patrol_cmd_ready = 0;
      switch (c) {
        case 0: Patrol_Stop();   LOG("[PATROL] cmd STOP");   break;
        case 1: Patrol_Start();  LOG("[PATROL] cmd START");  break;
        case 2: Patrol_Pause();  LOG("[PATROL] cmd PAUSE");  break;
        case 3: Patrol_Resume(); LOG("[PATROL] cmd RESUME"); break;
        default: break;
      }
    }
    if (g_patrol_act_ready)
    {
      uint8_t act = g_patrol_act_action; g_patrol_act_ready = 0;
      Patrol_ResumeFromPoint(act);       /* 到点放行：龙芯认完点给出下一步动作 */
      LOG("[PATROL] act point=%u action=%u", g_patrol_act_point, act);
    }

    // ===== 1.5 循迹模式：本地循线快环覆盖遥控值 =====
    /* 三模式（protocol.h）：
     *   MODE_REMOTE(0)      —— 不介入，用上面遥控消费得到的 speed/steering
     *   MODE_LINE_ONLY(1)   —— F4 本地 TCRT 循线
     *   MODE_LINE_VISION(3) —— 同样本地循线（保证断网可跑），龙芯视觉只作叠加约束
     * ★ 循线快环完全在 F4 本地闭合：即使 CAN/龙芯抖动或断开，巡线照常，
     *   不会因为链路问题冲出线外。龙芯只负责启停、认点、到点放行。 */
    if (current_mode == MODE_LINE_ONLY || current_mode == MODE_LINE_VISION)
    {
      if (!Patrol_IsActive()) Patrol_Start();     /* 切入循迹模式即自动启动 */

      int16_t p_spd = 0, p_str = 0;
      /* 前向距离：超声波(已中值滤波)，HC_NO_TARGET_CM 表示空旷 */
      uint16_t front = (uint16_t)HC_GetDistance_cm();
      if (front == 0u) front = HC_NO_TARGET_CM;

      PatrolEventT ev = Patrol_Update(&p_spd, &p_str, front);

      /* 台架联调开关(PATROL_BENCH_STEER_ONLY)：1=驱动轮不转、只动舵机。
         供电已拆分后默认恢复实车循迹；若需要悬空只验证转向，可临时改回 1。 */
#ifndef PATROL_BENCH_STEER_ONLY
#define PATROL_BENCH_STEER_ONLY 0
#endif
#if PATROL_BENCH_STEER_ONLY
      p_spd = 0;
#endif

      current_speed  = p_spd;
      s_raw_steering = p_str;

      /* 交给既有舵机管线（阿克曼：转向只由舵机执行） */
      int16_t ta = (int16_t)(-(int32_t)p_str * 90 / 1000);
      if (ta < SERVO_ANGLE_MIN) ta = SERVO_ANGLE_MIN;
      if (ta > SERVO_ANGLE_MAX) ta = SERVO_ANGLE_MAX;
      Servo_SetSmoothAngle(ta, RAMP_REMOTE_STEP);

      /* 事件上报龙芯（CAN 0x481 sub=0x01），由 TelemetryTask 取走 */
      if (ev != PATROL_EV_NONE) {
        g_patrol_event     = (uint8_t)ev;
        g_patrol_point_seq = Patrol_GetPointSeq();
        g_patrol_evt_ready = 1;
        LOG("[PATROL] event=%u seq=%u state=%u", ev, g_patrol_point_seq, Patrol_GetState());
      }
      g_patrol_state = (uint8_t)Patrol_GetState();
    }
    else if (Patrol_IsActive())
    {
      Patrol_Stop();                              /* 切出循迹模式即停止 */
      g_patrol_state = (uint8_t)Patrol_GetState();
      LOG("[PATROL] stopped (mode=%u)", current_mode);
    }

    // ===== 2. 阿克曼驱动速度合成 =====
    /* ★ 2026-07-21 阿克曼修正（实施方案 §2.6 / §3.1）：
     *   旧实现按【差速车】用 steering 合成 turn_factor，让左右轮一加一减。
     *   但本车是【阿克曼底盘】——转向完全由前轮舵机(PA0/TIM2_CH1)执行，
     *   驱动轮只负责推进。再叠加差速等于把同一个转向意图【施加两次】，
     *   会造成：转弯半径比舵机指令更小、内外轮与转向角不匹配而拖胎、
     *   直行时任何微小 steering 都让两轮速度不等而跑偏。
     *
     *   正解：两个驱动轮给【同一目标速度】，转向只由舵机负责。
     *   （严格阿克曼下内外轮应有微小速差 v_in/v_out = 1 ∓ (W/2)·tanδ/L，
     *     但本车轮距 W 小、巡检转弯半径大，该差值 <2%，且后轮多为差速器/
     *     独立驱动可自行吸收；引入它反而需要精确的 δ 反馈，得不偿失。
     *     故统一给同速——这也是绝大多数阿克曼小车的标准做法。） */
    #define CLAMP_SPD(v)  do { if ((v) > MAX_SPEED) (v) = MAX_SPEED; \
                                if ((v) < -MAX_SPEED) (v) = -MAX_SPEED; } while(0)

    int16_t left_speed  = current_speed;
    int16_t right_speed = current_speed;

    if (!remote_active)
    {
      s_last_left_spd = current_speed;
      s_last_right_spd = current_speed;
    }
    else if (s_remote_age <= HB_ACTIVE_MAX)
    {
      /* 阿克曼：两驱动轮同速，不做差速合成（s_raw_steering 只驱动舵机，见第 1 段） */
      CLAMP_SPD(left_speed);
      CLAMP_SPD(right_speed);
      s_last_left_spd  = left_speed;
      s_last_right_spd = right_speed;
    }
    else if (s_remote_age <= HB_HOLD_MAX)
    {
      left_speed  = s_last_left_spd;
      right_speed = s_last_right_spd;
    }
    else
    {
      left_speed  = 0;
      right_speed = 0;
      current_speed = 0;
      remote_active = 0;
      s_last_left_spd  = 0;
      s_last_right_spd = 0;
      s_raw_steering   = 0;
      Servo_SetSmoothAngle(SERVO_ANGLE_MID, RAMP_SWITCH_STEP);
      /* 链路断开时若有测试在跑 → 一并终止（安全） */
      if (test_mode != PID_TEST_OFF)
      {
        test_mode = PID_TEST_OFF;
        PID_Reset(&pid_left); PID_Reset(&pid_right);
        LOG("[PID] link lost -> test abort");
      }
    }

    // ===== 3. 编码器更新（本任务独占——修复旧版双任务竞态） =====
    Encoder_Update();
    int32_t c1 = Encoder1_GetCount();
    int32_t c2 = Encoder2_GetCount();
    int16_t dL = (int16_t)(c1 - pid_prev_cnt1);   /* 本周期实测脉冲增量 */
    int16_t dR = (int16_t)(c2 - pid_prev_cnt2);
    pid_prev_cnt1 = c1;
    pid_prev_cnt2 = c2;

    // ===== 4. 电机输出（测试激励 > 闭环 > 开环 三级） =====
    int16_t outL = 0, outR = 0;
    int16_t teleTargL = 0, teleTargR = 0;

    if (test_mode == PID_TEST_OPEN_LOOP)
    {
      /* 开环直给 PWM：方向自检 / MAX_DELTA 标定（看 meas 稳态值） */
      outL = test_L; outR = test_R;
      teleTargL = test_L; teleTargR = test_R;
      Motor1_Set(outL);
      Motor2_Set(outR);
    }
    else if (test_mode == PID_TEST_CLOSED)
    {
      /* 闭环阶跃：目标=编码器增量 counts/周期（Kp/Ki/Kd 整定） */
      teleTargL = test_L; teleTargR = test_R;
      if (test_L == 0) { PID_Reset(&pid_left);  outL = 0; }
      else outL = (int16_t)PID_Update(&pid_left,  (float)test_L, (float)dL, MOTION_PID_DT);
      if (test_R == 0) { PID_Reset(&pid_right); outR = 0; }
      else outR = (int16_t)PID_Update(&pid_right, (float)test_R, (float)dR, MOTION_PID_DT);
      Motor1_Set(outL);
      Motor2_Set(outR);
    }
    else if (g_pid.closed_loop)
    {
      /* 常规闭环：命令速度(-1000~1000) 映射为目标脉冲增量 */
      float targL = (float)left_speed  / (float)MAX_SPEED * g_pid.max_delta;
      float targR = (float)right_speed / (float)MAX_SPEED * g_pid.max_delta;
      teleTargL = (int16_t)targL; teleTargR = (int16_t)targR;
      if (left_speed == 0)  { PID_Reset(&pid_left);  outL = 0; }
      else outL = (int16_t)PID_Update(&pid_left,  targL, (float)dL, MOTION_PID_DT);
      if (right_speed == 0) { PID_Reset(&pid_right); outR = 0; }
      else outR = (int16_t)PID_Update(&pid_right, targR, (float)dR, MOTION_PID_DT);
      Motor1_Set(outL);
      Motor2_Set(outR);
    }
    else
    {
      /* 开环直通（出厂默认，已验证能动） */
      outL = left_speed; outR = right_speed;
      teleTargL = left_speed; teleTargR = right_speed;
      Motor1_Set(outL);
      Motor2_Set(outR);
    }

    // ===== 5. 发布快照（TelemetryTask 打包上报；本任务不再直接发串口） =====
    g_pt_flags = (uint8_t)((g_pid.closed_loop ? 0x01 : 0) |
                           (test_mode != PID_TEST_OFF ? 0x02 : 0) |
                           ((test_mode & 0x03) << 4));
    g_pt_targL = teleTargL; g_pt_measL = dL; g_pt_outL = outL;
    g_pt_targR = teleTargR; g_pt_measR = dR; g_pt_outR = outR;
    g_mot_speed      = current_speed;
    g_mot_steer_tele = -(int16_t)((int32_t)Servo_GetCurrentAngle() * 1000 / 90);
    g_mot_mode       = current_mode;

    if (remote_active && s_remote_age <= HB_HOLD_MAX + 100)
    {
      s_remote_age++;
    }
    g_remote_active = remote_active;

    osDelay(17);
  }
  /* USER CODE END StartMotionControlTask */
}

/* USER CODE BEGIN Header_StartScanTask */
/**
* @brief Function implementing the ScanTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartScanTask */
void StartScanTask(void *argument)
{
  /* USER CODE BEGIN StartScanTask */
  /* 云台三态扫描（《深度优化报告》§4.5）：
       ① BOOT_SCAN：上电扫一遍（MIN→MAX→回中），获取周边障碍初始态势；
       ② 居中静止：巡航期间云台锁定中位，HC 前视（不再持续来回扫）；
       ③ ON_DEMAND：激光近障触发的按需扫描——随运动状态机接入（后续里程碑）。
     HC/编码器/OLED 均已移出本任务（HC→RangingTask，编码器→MotionControl，OLED 移除）。 */
  LOG("[TASK] ScanTask: boot sweep once, then center & hold (on-demand scan later)");

  uint16_t pulse = SERVO2_MIN;
  int8_t   phase = 0;           /* 0=去MAX 1=回MIN 2=回中完成 */
  Servo2_SetPulse(pulse);
  osDelay(300);                 /* 等云台到位 */

  for(;;)
  {
    WDG_Kick(WDG_TASK_SCAN);   /* 只打存活点，实际喂狗由 MotionControlTask 聚合判定 */

    if (phase == 0)
    {
      pulse += SWEEP2_STEP;
      if (pulse >= SERVO2_MAX) { pulse = SERVO2_MAX; phase = 1; }
      Servo2_SetPulse(pulse);
      osDelay(10);
    }
    else if (phase == 1)
    {
      pulse -= SWEEP2_STEP;
      if (pulse <= SERVO2_MID) { pulse = SERVO2_MID; phase = 2;
        LOG("[SCAN] boot sweep done -> center hold (dist=%lucm)", HC_GetDistance_cm()); }
      Servo2_SetPulse(pulse);
      osDelay(10);
    }
    else
    {
      /* 居中静止：低频保持（后续 ON_DEMAND 扫描在此接收触发） */
      Servo2_SetPulse(SERVO2_MID);
      osDelay(100);
    }
  }
  /* USER CODE END StartScanTask */
}

/* USER CODE BEGIN Header_Ranging */
/**
* @brief Function implementing the RangingTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Ranging */
void Ranging(void *argument)
{
  /* USER CODE BEGIN Ranging */
  /* 高频测距采集（AboveNormal1，1ms 粒度）——《深度优化报告》M1/M3：
       · HC-SR04 状态机 1ms 推进（旧版在 ScanTask 10ms 粒度 + OLED 阻塞，触发
         时序被拖慢；现在 TRIG 10us/50us 相位仅耗 1~2ms，测距可达物理极限）；
       · VL53L0X 每 10ms 非阻塞读一次（连续测距模式，仅查就绪位，绝不忙等）。
     全程无打印、无 OLED、无阻塞大户——它只做采集和发布。 */
  LOG("[TASK] RangingTask: HC @1ms tick + VL53 try-read @10ms");

  uint32_t cyc = 0;
  uint32_t hc_last_ms = 0;   /* 上次发起 HC 测量的时刻，用于 ≥60ms 间隔限速 */
  /* VL53 超量程去抖：连续 VL53_MISS_N 次才判"真的没目标"，期间保持最后有效值 */
  #define VL53_MISS_N   5U   /* 5 次 × 10ms ≈ 50ms */
  uint8_t  s_vl53_miss = 0;
  for(;;)
  {
    WDG_Kick(WDG_TASK_RANGING);
    /* HC 状态机推进（触发时序 + 超时判定；边沿在 TIM2 中断捕获） */
    HC_ProcessTick();
    /* ★ HC-SR04 测量间隔 ≥60ms(HC_MEAS_INTERVAL_MS)：数据手册要求相邻测量≥60ms，
       否则上一次 ping 余响未散就再发，近距目标(<50cm)"回波时有时无"(pulse 在 0 与
       真值间跳)——这正是近距 [DIST] 间歇 0 的根因。原 back-to-back(~150-300Hz)过快。 */
    if (!HC_IsMeasuring() && (HAL_GetTick() - hc_last_ms >= HC_MEAS_INTERVAL_MS))
    {
      HC_StartMeasurement();
      hc_last_ms = HAL_GetTick();
    }

    /* 循迹 TCRT×3：1ms 采样 + 去抖(3拍=3ms 收敛)，发布位域供遥测/循线消费 */
    g_line_bits = LineTrack_PackBits(LineTrack_Read());

    /* VL53 非阻塞取样（~30ms 测量预算，10ms 轮询足够取走每个样本） */
    if ((++cyc % 10U) == 0U)
    {
      uint16_t mm;
      if (VL53_TryReadRange_mm(&mm))
      {
        if (mm != VL53_OUT_OF_RANGE)
        {
          /* 有效读数：立即发布并清失效计数 */
          g_vl53_mm = mm;
          s_vl53_miss = 0;
          g_vl53_ok_cnt++;
        }
        else
        {
          /* ★ 2026-07-23 超量程去抖：
             VL53L0X 在目标接近量程上限(~1.2m)或反射率低时，会【间歇】返回超量程
             哨兵 8190。实测约 1/3 的采样是 8190，而 g_vl53_ok_cnt 同期稳定增长
             （每秒 20~25 次有效读数）—— 说明传感器工作正常，只是边缘条件下不稳。
             旧实现把 8190 直接覆盖 g_vl53_mm，导致上位机与小屏看到的是
             "880mm / 无数据" 来回跳，也就是"拿不到稳定数据"。
             现改为：连续 VL53_MISS_N 次超量程才真的对外发布 8190，
             期间保持最后一个有效值。与 DHT11 的 s_dht_fail>=3 去抖同一思路。 */
          if (s_vl53_miss < 0xFFU) s_vl53_miss++;
          if (s_vl53_miss >= VL53_MISS_N) g_vl53_mm = VL53_OUT_OF_RANGE;
        }
      }
    }

    /* VL53 运行时自恢复：未就绪时每 ~2s 重试一次 init。上电后再补接线/补供电也能
       自动恢复，无需重烧。仅传感器缺失时才跑（正常工作此分支从不触发，零开销）。 */
    if (!VL53_IsPresent() && (cyc % 2000U == 0U))
    {
      /* ★ 2026-07-23：与 MLX 同理，先做总线级恢复。VL53L0X 挂死拉低 SDA 时，
         单纯重跑 VL53_Init 会在第一条 I2C 命令上就失败，永远恢复不了。
         每 4s 才做一次总线恢复（隔次），避免正常缺件时频繁扰动总线。 */
      static uint8_t s_vl53RecoverToggle = 0;
      if (s_vl53RecoverToggle ^= 1U) {
        uint8_t busOk = I2C2_BusRecover();
        if (!busOk) LOG("[VL53] I2C2 bus recover: SDA STILL LOW (check 3V3 supply / pull-ups)");
      }
      if (VL53_Init() == 0) LOG("[VL53] runtime recovery OK -> ranging live");
    }

    osDelay(1);
  }
  /* USER CODE END Ranging */
}

/* USER CODE BEGIN Header_Telemetry */
/**
* @brief Function implementing the TelemetryTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Telemetry */
void Telemetry(void *argument)
{
  /* USER CODE BEGIN Telemetry */
  /* 上报专职任务（Normal，20ms 基准节拍）——把串口 TX 从运动环剥离：
       · ~60ms：7B 遥测帧 + 0x5A 扩展帧（距离/编码器）
       · PID 遥测 0x62：测试进行中 20ms(50Hz) 高频；平时 200ms 低频
     所有 TX 走 USART3 互斥锁，与 EnvTask 的 0x5C 串行化。 */
  LOG("[TASK] TelemetryTask: tele@60ms + PID tele 50Hz(test)/5Hz(idle)");

  uint8_t  frame[FRAME_PIDTELE_LENGTH];   /* 27B，够放所有帧型 */
  uint8_t  exFrame[FRAME_EXT_LENGTH];
  ControlData cd;
  uint16_t seq = 0;
  uint32_t tick = 0;

  for(;;)
  {
    WDG_Kick(WDG_TASK_TELEMETRY);
    /* ★遥测与命令心跳解耦（2026-07-10）：传感器数据【无条件】持续上报，
       绝不因命令链路 ~510ms 无帧(g_remote_active=0)而停发。
       —— 这是"掉线后界面数据被冻结"的根治：命令链路的任何抖动(EMI/偶发丢帧/
          上位机卡顿)都不再导致上行数据黑屏；心跳只用于运动安全停车
          (见 MotionControlTask 的 HB_HOLD_MAX 分支)，不再门控上行数据。 */
    {
      tick++;

      /* ---- 7B 遥测 + 扩展帧（每 3 拍 = 60ms） ---- */
      if (tick % 3U == 0U)
      {
        cd.speed    = g_mot_speed;
        cd.steering = g_mot_steer_tele;
        cd.mode     = g_mot_mode;
        Protocol_PackFrame(&cd, frame);
        USART3_SendFrameSync(frame, FRAME_LENGTH);

        uint16_t dist_cm = (uint16_t)HC_GetDistance_cm();
        uint8_t exLen = Protocol_PackTelemetryEx(
            g_mot_speed, g_mot_steer_tele, g_mot_mode,
            dist_cm, Encoder1_GetCount(), Encoder2_GetCount(), exFrame);
        USART3_SendFrameSync(exFrame, exLen);

#if LINK_USE_CAN
        /* CAN 上行（非阻塞，无锁）：0x181 核心遥测 + 0x281 编码器。
           大端定点，各 ≤8B（实施方案 §3.5.1）。
           ★ dist 用 HC_NO_TARGET_CM(0xFFFF) 哨兵表示"无目标/超量程"，与真实近距
             0cm 彻底区分——避免消费端把"前方空旷"误读成"贴脸 0cm 障碍"。 */
        uint16_t can_dist = (dist_cm == 0u) ? HC_NO_TARGET_CM : dist_cm;
        CANLink_SendTeleCore(g_mot_speed, g_mot_steer_tele, g_mot_mode, can_dist, g_line_bits);
        CANLink_SendTeleEnc(Encoder1_GetCount(), Encoder2_GetCount());
#endif
      }

#if LINK_USE_CAN
      /* ---- 0x701 节点心跳 @1Hz（每 50 拍 = 1s）：供龙芯判 F4 存活 ---- */
      CANLink_Service();
      if (tick % 50U == 0U)
      {
        CANLink_SendHeartbeat(CAN_NODE_STATE_OPERATIONAL);
      }

      /* ---- 巡检事件 0x481 sub=0x01：事件驱动，产生即发（到点/丢线/受阻）---- */
      if (g_patrol_evt_ready)
      {
        uint8_t ev = g_patrol_event, sq = g_patrol_point_seq, st = g_patrol_state;
        g_patrol_evt_ready = 0;
        CANLink_SendPatrolEvent(ev, sq, st);
      }

#if CAN_LOOPBACK_TEST
      /* ★ P1 自环自证：每 3 拍(60ms) 自发自收一帧 0x301 → 看 [CAN] 的 rxoth 涨不涨。
         静默自环下 CANTX 保持隐性，不碰外部总线，也不需要 ACK。 */
      if (tick % 3U == 0U)
      {
        CANLink_LoopbackSelfTest();
      }
#endif
#endif

      /* ---- PID 遥测 0x62：测试中每拍(20ms=50Hz)，平时每 10 拍(200ms) ---- */
      uint8_t testActive = (g_pt_flags & 0x02U) != 0U;
      if (testActive || (tick % 10U == 0U))
      {
        uint8_t len = Protocol_PackPidTele(
            seq++, g_pt_flags,
            g_pt_targL, g_pt_measL, g_pt_outL,
            g_pt_targR, g_pt_measR, g_pt_outR,
            Encoder1_GetCount(), Encoder2_GetCount(), frame);
        USART3_SendFrameSync(frame, len);
#if LINK_USE_CAN
        CANLink_SendPidTele((uint16_t)(seq - 1U), g_pt_flags,
            g_pt_targL, g_pt_measL, g_pt_outL,
            g_pt_targR, g_pt_measR, g_pt_outR,
            Encoder1_GetCount(), Encoder2_GetCount());
#endif
      }
    }
    osDelay(20);
  }
  /* USER CODE END Telemetry */
}

/* USER CODE BEGIN Header_Env */
/**
* @brief Function implementing the EnvTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Env */
void Env(void *argument)
{
  /* USER CODE BEGIN Env */
  /* 慢速环境/安全任务（BelowNormal，100ms）：MQ2/火焰/DHT11/蜂鸣 + 0x5C 上报。
     VL53 已迁 RangingTask（本任务读 g_vl53_mm 快照）——DHT11 阻塞读(4~25ms)
     不再拖累激光采样（修复《深度优化报告》问题 P3）。 */

  static int8_t  s_temp   = 0;
  static uint8_t s_humi   = 0;
  static uint8_t s_dht_ok = 0;
  static uint8_t s_dht_fail = 0;   /* 连续失败计数：去抖，偶发失败不丢显示 */
  /* ★ 2026-07-23：最后一次成功读取的时刻。
     原来只有"连续失败 3 次"这一个判据，但如果失败与成功交替出现
     （偶发成功把 s_dht_fail 清零），就永远达不到 3 次，
     即使实际刷新率已经低到几十秒一次，上位机看到的仍是 dhtOk=1 的陈旧值。
     加一个绝对新鲜度判据：超过 DHT_STALE_MS 没有成功读到，一律判无效。 */
  #define DHT_STALE_MS   15000U    /* 采样周期 2s，15s 没成功 = 真的坏了 */
  static uint32_t s_dht_ok_ms = 0;

  /* 障碍双重验证阈值：VL53(mm) 与 超声波(cm) 同时近才确认 */
  #define ENV_OBST_TH_MM   300U   /* 30cm */
  #define ENV_OBST_TH_CM    30U

  uint32_t cycle = 0;

  /* DHT11 上电需 ~1s 稳定 */
  osDelay(1000);

  for (;;)
  {
    WDG_Kick(WDG_TASK_ENV);
    /* ---- 1. 采集（10Hz；激光值来自 RangingTask 快照） ---- */
    Flame_Tick();
    uint16_t gas_raw   = MQ2_ReadRaw();
    uint8_t  gas_alarm = MQ2_IsAlarm();
    uint8_t  flame     = Flame_IsDetected();

    uint16_t vl53_mm = g_vl53_mm;                    /* RangingTask 发布 */
    uint8_t  vl53_ok = (vl53_mm != VL53_OUT_OF_RANGE);

    /* ---- DHT11：每 2 秒读一次（★手册要求采样间隔≥2s，原 1s 太快必频繁失败，
       是"温湿度只刷新一次后冻结"的根因）+ 失败重试 + 连续失败去抖 ---- */
    if ((cycle % 20U) == 0U)
    {
      int8_t  t = s_temp;
      uint8_t h = s_humi;
      uint8_t ok = 0;
      for (uint8_t retry = 0; retry < 3U && !ok; retry++)   /* 失败重试最多3次 */
      {
        ok = DHT11_Read(&t, &h);
        if (!ok) osDelay(30);                                /* 重试前小憩，让单总线复位 */
      }
      if (ok)
      {
        s_temp = t; s_humi = h;     /* 更新最后有效值 */
        s_dht_fail = 0;
        s_dht_ok = 1;
        s_dht_ok_ms = HAL_GetTick();
      }
      else
      {
        /* 单次失败不立刻判无效：保持上次值继续显示（DHT11 偶发失败很常见）。
           连续 3 次(≈6s)都失败才判传感器真失效 → 上位机才显示"--"。 */
        if (s_dht_fail < 250U) s_dht_fail++;
        if (s_dht_fail >= 3U) s_dht_ok = 0;
      }
      /* 绝对新鲜度兜底：成功/失败交替时 s_dht_fail 会被反复清零而永远到不了 3，
         此时上位机会一直看到 dhtOk=1 的陈旧值。超过 DHT_STALE_MS 无成功即判无效。 */
      if (s_dht_ok_ms != 0U && (HAL_GetTick() - s_dht_ok_ms) > DHT_STALE_MS)
        s_dht_ok = 0;
    }

    /* ---- 2. 障碍双重验证 ---- */
    uint16_t hc_cm = (uint16_t)HC_GetDistance_cm();
    uint8_t obstacle = (vl53_ok  && vl53_mm < ENV_OBST_TH_MM) &&
                       (hc_cm != 0U && hc_cm < ENV_OBST_TH_CM);

    /* ---- 3. 报警等级 + 蜂鸣 + PD2 联动 ---- */
    uint8_t       alarm;
    BuzzerPattern pat;
    if (flame || gas_alarm)      { alarm = ALARM_LV_FIRE; pat = BUZZER_ALARM; }
    else if (obstacle)           { alarm = ALARM_LV_WARN; pat = BUZZER_WARN;  }
    else                         { alarm = ALARM_LV_NONE; pat = BUZZER_OFF;   }

    Buzzer_SetPattern(pat);
    Buzzer_Tick(100);
    HAL_GPIO_WritePin(ALARM_OUT_GPIO_Port, ALARM_OUT_Pin,
                      (alarm == ALARM_LV_FIRE) ? GPIO_PIN_SET : GPIO_PIN_RESET);

    /* ---- 4. flags ---- */
    uint8_t flags = 0;
    if (gas_alarm)           flags |= ENV_FLAG_GAS;
    if (flame)               flags |= ENV_FLAG_FLAME;
    if (s_dht_ok)            flags |= ENV_FLAG_DHT_OK;
    if (vl53_ok)             flags |= ENV_FLAG_VL53_OK;
    if (VL53_IsPresent())    flags |= ENV_FLAG_VL53_PRESENT;   /* 在位≠有效：死值可辨 */
    if (obstacle)            flags |= ENV_FLAG_OBSTACLE;
    if (Buzzer_IsSounding()) flags |= ENV_FLAG_BUZZER;
    if (g_obs_lock)          flags |= ENV_FLAG_OBS_LOCK;   /* 遥控避障锁生效中 */

    /* ---- 5. 2Hz 上报 0x5C（与命令心跳解耦：无条件持续上报，不掉线） ---- */
    if ((cycle % 5U) == 0U)
    {
      uint8_t envFrame[FRAME_ENV_LENGTH];
      uint8_t envLen = Protocol_PackEnvFrame(
          gas_raw, vl53_mm, s_temp, s_humi, flags, alarm, envFrame);
      USART3_SendFrameSync(envFrame, envLen);

#if LINK_USE_CAN
      /* CAN 上行 0x381 环境帧（大端定点 8B，实施方案 §3.5.1） */
      CANLink_SendEnv(gas_raw, vl53_mm, s_temp, s_humi, flags, alarm);
#endif
    }

    /* 诊断日志(~1s一次) */
    if ((cycle % 10U) == 0U)
    {
      LOG("[DIST] ultra(HC)=%ucm pulse=%uus | laser(VL53)=%umm ok=%u okcnt=%lu | remote=%u",
          hc_cm, (unsigned)HC_GetLastPulse_us(), vl53_mm, vl53_ok,
          g_vl53_ok_cnt, g_remote_active);
      LOG("[ENV]  gas=%u T=%dC H=%u%% dhtOk=%u fail=%u flame=%u obst=%u",
          gas_raw, (int)s_temp, s_humi, s_dht_ok, s_dht_fail, flame, obstacle);
      /* 循迹核查：L/C/R=压线(1)/离线(0)，J=路口 X=丢线。压/离黑线时应即时翻转 */
      { uint8_t lb = g_line_bits;
        LOG("[LINE] L=%u C=%u R=%u  J=%u X=%u  (bits=0x%02X)",
            (lb & LINE_BIT_L) ? 1u : 0u, (lb & LINE_BIT_C) ? 1u : 0u,
            (lb & LINE_BIT_R) ? 1u : 0u, (lb & LINE_BIT_JUNCTION) ? 1u : 0u,
            (lb & LINE_BIT_LOST) ? 1u : 0u, lb); }
#if LINETRACK_USE_ADC
      /* ★ 阈值标定用：白底放一次、黑线放一次，各记三路读数，
         按 linetrack.h 里的说明定 LINE_ADC_ON_TH / OFF_TH。
         同时打 MQ2 通道，便于确认 DMA 扫描确实在刷新（四个值都不动=DMA 停了）。 */
      { uint16_t rl, rc, rr;
        LineTrack_GetRaw(&rl, &rc, &rr);
        LOG("[LINE_ADC] L=%4u(%u~%u) C=%4u(%u~%u) R=%4u(%u~%u) | mq2=%4u dma=%u",
            rl, (unsigned)LINE_ADC_OFF_TH_L, (unsigned)LINE_ADC_ON_TH_L,
            rc, (unsigned)LINE_ADC_OFF_TH_C, (unsigned)LINE_ADC_ON_TH_C,
            rr, (unsigned)LINE_ADC_OFF_TH_R, (unsigned)LINE_ADC_ON_TH_R,
            (unsigned)g_adc_dma[ADC_CH_MQ2], ADC_IsStreaming()); }
#endif
    }

    cycle++;
    osDelay(100);
  }
  /* USER CODE END Env */
}

/* USER CODE BEGIN Header_Thermal */
/**
* @brief Function implementing the ThermalTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Thermal */
void Thermal(void *argument)
{
  /* USER CODE BEGIN Thermal */
#if THERMAL_TASK_ENABLED
  /* 热成像原始帧转发（Low，~2Hz，USART1 专线）：F4 只读原始帧不解算，
     0x5E EEPROM(上电一次) + 0x5D 原始帧 → 龙芯 Melexis 解算。 */
  /* ★ 2026-07-23 重大修复：上电初始化失败【不再永久挂起任务】。
   *   旧实现是 `if (rc != 0) { for(;;) osDelay(1000); }` —— 只要上电那一刻
   *   MLX90640 没响应（供电未稳、I2C 总线残留在上次掉电时的死锁态），
   *   ThermalTask 就永久挂死：此后既没有任何热成像数据，也没有任何 [THERM]
   *   日志，主循环里的 I2C 总线恢复逻辑更是永远没机会运行。
   *   现象就是"必须整机断电重来、碰运气才能让热成像活"。
   *   现改为无限重试：每 2s 一次，每次先做 I2C3 总线级恢复再 init，
   *   成功即进入正常转发主循环。传感器晚就绪 / 事后补供电都能自愈。 */
  {
    int rc = Thermal_Init();
    uint32_t bootRetry = 0;
    while (rc != 0)
    {
      bootRetry++;
      /* 前几次只重试 init（可能只是上电时序没稳），之后每次都带总线恢复 */
      if (bootRetry >= 2U) {
        uint8_t busOk = I2C3_BusRecover();
        if ((bootRetry % 15U) == 2U)   /* 每 ~30s 打一条，避免刷屏 */
          LOG("[THERM] init retry #%lu: I2C3 recover(%s, cnt=%lu) rc=%d",
              (unsigned long)bootRetry, busOk ? "SDA released" : "SDA STILL LOW",
              (unsigned long)I2C_RecoverCount(), rc);
      } else {
        LOG("[THERM] init FAILED rc=%d -> MLX90640 无响应(查 I2C3 PA8/PC9 与 3.3V)，每 2s 重试", rc);
      }
      osDelay(2000);
      rc = Thermal_Init();
    }
    if (bootRetry)
      LOG("[THERM] init OK after %lu retries", (unsigned long)bootRetry);
  }
  LOG("[THERM] init OK -> raw-forward: 0x5E EEPROM(once)+0x5D frames via USART1");

  uint8_t  frame[FRAME_MLXCHUNK_MAXLEN];
  uint8_t  eeSent = 0;
  uint32_t loopCnt = 0, framesSent = 0;
  uint8_t  hushLogged = 0;
  uint16_t failStreak = 0;   /* 连续原始帧读取失败计数：坏态自恢复触发 */
  for (;;)
  {
    /* ★PID 在线整定期间暂停热成像转发：热成像帧大、与 50Hz PID 遥测抢龙芯→上位机
       的 TCP 带宽会导致掉线。g_pt_flags bit1=测试激励进行中(MotionControlTask 维护)。
       测试期间把链路让给 PID 曲线；测试结束自动恢复。 */
    uint8_t pidTesting = (g_pt_flags & 0x02U) != 0U;
    if (pidTesting && !hushLogged) { LOG("[THERM] PID test active -> thermal TX paused"); hushLogged = 1; }
    if (!pidTesting) hushLogged = 0;

    /* 与命令心跳解耦：热成像走 USART1 专线，只在 PID 在线整定时让出带宽，
       其余时刻持续转发（不再因 g_remote_active=0 而停发→龙芯热成像不再冻结）。 */
    if (!pidTesting)
    {
      if (!eeSent)
      {
        const uint16_t *ee = Thermal_GetEE();
        for (uint8_t idx = 0; (int)idx * MLX_CHUNK_WORDS < MLX_EE_WORDS; idx++)
        {
          int start = (int)idx * MLX_CHUNK_WORDS;
          int rem   = MLX_EE_WORDS - start;
          uint8_t cnt = (rem >= MLX_CHUNK_WORDS) ? MLX_CHUNK_WORDS : (uint8_t)rem;
          uint8_t len = Protocol_PackMlxChunk(FRAME_EEPROM_MARKER, idx, &ee[start], cnt, frame);
          HAL_UART_Transmit(&huart1, frame, len, 50);
          osDelay(2);
        }
        eeSent = 1;
        LOG("[THERM] EEPROM forwarded");
      }

      int rf = Thermal_ReadRawSubpage();
      if (rf == 0)
      {
        failStreak = 0;
        const uint16_t *raw = Thermal_GetRawFrame();
        for (uint8_t idx = 0; (int)idx * MLX_CHUNK_WORDS < MLX_FRAME_WORDS; idx++)
        {
          int start = (int)idx * MLX_CHUNK_WORDS;
          int rem   = MLX_FRAME_WORDS - start;
          uint8_t cnt = (rem >= MLX_CHUNK_WORDS) ? MLX_CHUNK_WORDS : (uint8_t)rem;
          uint8_t len = Protocol_PackMlxChunk(FRAME_RAWTHERM_MARKER, idx, &raw[start], cnt, frame);
          HAL_UART_Transmit(&huart1, frame, len, 50);
          osDelay(2);
        }
        framesSent++;
      }
      else
      {
        /* MLX 连续读失败=传感器掉坏态(供电/EMI/地弹后停刷)。就绪位轮询现已超时
           返回(见 MLX90640_API.c 修复)，不再永久挂死；此处连续 ~2s(10×200ms)
           失败即重初始化 MLX 恢复，USART1(PA9) 热成像输出可自愈，不需重启 F4。 */
        if (++failStreak >= 10U)
        {
          /* ★ 2026-07-23：先做【总线级】恢复再重初始化。
             MLX90640 在供电波动/EMI 下会在传输中途挂死并把 SDA 一直拉低，
             此时重发配置命令必然继续失败 —— 这正是日志里 "re-init FAILED"
             反复出现却永远好不了的原因（MLX90640_I2CInit() 本身还是空函数）。
             I2C3_BusRecover() 手动打 9 个 SCL 时钟逼从机释放 SDA，
             补 STOP 时序，再软复位并重初始化 I2C3 外设。 */
          uint8_t busOk = I2C3_BusRecover();
          LOG("[THERM] raw read fail x%u -> I2C3 bus recover(%s, cnt=%lu) + re-init",
              failStreak, busOk ? "SDA released" : "SDA STILL LOW",
              (unsigned long)I2C_RecoverCount());
          if (Thermal_Init() == 0) { eeSent = 0; failStreak = 0; LOG("[THERM] re-init OK, EEPROM will resend"); }
          else                     { failStreak = 0; osDelay(1000); LOG("[THERM] re-init FAILED, retry later"); }
        }
      }
      if ((++loopCnt % 10U) == 0U)
        LOG("[THERM] raw-forward remote=%u frames_sent=%lu", g_remote_active, framesSent);
      /* 周期重发 EEPROM(每 ~150 拍≈30s)：中途才连上/重连的龙芯也能取到标定参数解算，
         否则重连后热成像永远解不出(EEPROM 只在上电发过一次)。 */
      if ((loopCnt % 150U) == 0U) eeSent = 0;
    }
    osDelay(200);   /* ~2Hz/子页 */
  }
#else
  for (;;) { osDelay(1000); }
#endif
  /* USER CODE END Thermal */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

