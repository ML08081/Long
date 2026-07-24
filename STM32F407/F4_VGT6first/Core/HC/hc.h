#ifndef __HC_H
#define __HC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * HC-SR04 超声波模块驱动
 *
 * 引脚映射：
 *   TRIG — PB0 (GPIO 推挽输出)
 *   ECHO — PB3 (TIM2_CH2 AF1，输入捕获)   ← 原 PA1 已让给 ADC1_IN1(MQ2)
 *
 * 原理：
 *   TIM2 已配置为 PWM 输出 (CH1 → 舵机)，Prescaler=71 → 1 tick = 1us。
 *   在 CH2 上叠加输入捕获模式，不影响 CH1 的 PWM 输出。
 *   CH1 和 CH2 共用同一个计数器，但模式配置完全独立。
 *
 * 测距流程：
 *   1. TRIG 输出 10us 高电平脉冲
 *   2. TIM2_CH2 输入捕获上升沿 → 记录 t_rise
 *   3. 切换为下降沿捕获 → 记录 t_fall
 *   4. 脉宽 = t_fall - t_rise (处理 16-bit 回绕)
 *   5. 距离(cm) = 脉宽(us) / 58
 *============================================================================*/

/*---------- 引脚定义 ----------*/
#define HC_TRIG_PORT   GPIOB
#define HC_TRIG_PIN    GPIO_PIN_0

#define HC_ECHO_PORT   GPIOB
#define HC_ECHO_PIN    GPIO_PIN_3

/*---------- 超时 (单位: timer tick = 1us) ----------*/
#define HC_TIMEOUT_RISE_US  30000   /* 等待上升沿超时 30ms (≈510cm) */
#define HC_TIMEOUT_FALL_US  25000   /* 等待下降沿超时 25ms (≈430cm) */
#define HC_MAX_VALID_US     23000   /* 有效回波上限 23ms (≈400cm) */

/*---------- CAN/遥测哨兵 ----------*/
/* dist_cm 无目标哨兵：前方空旷/超量程/无回波。与"真实近距 0cm"彻底区分——
   消费端(龙芯/避障)见此值即判"无障碍"，绝不能误读成"贴脸 0cm"。 */
#define HC_NO_TARGET_CM     0xFFFFu

/*---------- 测量节拍 ----------*/
/* 相邻测量最小间隔：HC-SR04 数据手册要求 ≥60ms，让上一次 ping 余响散尽。
   否则近距(<50cm)目标会"回波时有时无"(pulse 在 0 与真值间跳)。限速 ~15Hz。 */
#define HC_MEAS_INTERVAL_MS 60U

/*---------- TIM2 参数 ----------*/
#define HC_TIM2_PERIOD_US   20000U  /* TIM2 自动重载值+1 (ARR=19999) */

/*---------- 外部引用 ----------*/
extern TIM_HandleTypeDef htim2;   /* TIM2 由 CubeMX 管理 */

/*---------- API ----------*/

/**
 * @brief  初始化 HC-SR04
 *         - PB0 → TRIG 推挽输出
 *         - PA1 → TIM2_CH2 输入捕获 (上升沿)
 *         - 不影响 TIM2_CH1 的 PWM 输出
 */
void HC_Init(void);

/**
 * @brief  TIM2 更新溢出回调（由 stm32f4xx_it.c TIM2_IRQHandler 调用）
 *         每 20ms 递增一次，配合 TIM2->CNT 构成 32 位微秒级时间戳。
 */
void HC_OnTimerOverflow(void);

/**
 * @brief  获取 32 位微秒级时间戳（基于 TIM2，分辨率为 1us）
 * @retval 单调递增微秒数，可处理 TIM2 回绕，约 71 分钟循环一次。
 * @note   用于替代 (uint16_t)(TIM2->CNT - start) 这类不抗回绕的计算。
 */
uint32_t HC_GetUs(void);

/**
 * @brief  初始化 HC-SR04
 *         - PB0 → TRIG 推挽输出
 *         - PA1 → TIM2_CH2 输入捕获 (上升沿)
 *         - 不影响 TIM2_CH1 的 PWM 输出
 */
void HC_Init(void);

/**
 * @brief  启动一次非阻塞测距 (触发 TRIG 10us 脉冲)
 *         之后需在循环中调用 HC_ProcessTick() 驱动状态机。
 * @note   若上一次测量未完成，则忽略本次调用。
 */
void HC_StartMeasurement(void);

/**
 * @brief  非阻塞状态机推进 (在 ScanTask 等循环中高频调用)
 * @retval 0 = 测量进行中, 1 = 本次测量已完成
 *         完成后自动回到空闲状态，可通过 HC_GetDistance_cm() 获取结果
 */
uint8_t HC_ProcessTick(void);

/**
 * @brief  查询是否正在测量中
 * @retval 0 = 空闲, 1 = 测量进行中
 */
uint8_t HC_IsMeasuring(void);

/**
 * @brief  获取最后一次测量的距离 (厘米)
 * @retval 1 ~ 400 (cm)，超时或无效返回 0
 * @note   非阻塞，返回缓存值
 */
uint32_t HC_GetDistance_cm(void);

/**
 * @brief  获取最后一次测量的距离 (毫米)
 * @retval 10 ~ 4000 (mm)，超时或无效返回 0
 */
uint32_t HC_GetDistance_mm(void);

/**
 * @brief  TIM2_CH2 捕获中断处理 (由 stm32f4xx_it.c 调用)
 * @note   在中断中处理上升/下降沿捕获，不受 Servo2_Set 阻塞影响
 */
void HC_TIM2_IRQHandler(void);

/**
 * @brief  读取上次测量的原始脉冲宽度 (微秒)
 */
uint32_t HC_GetLastPulse_us(void);

#ifdef __cplusplus
}
#endif

#endif /* __HC_H */
