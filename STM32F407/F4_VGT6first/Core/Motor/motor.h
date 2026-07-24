#ifndef __MOTOR_H
#define __MOTOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "main.h"


// 电机速度范围：-1000 ~ 1000
// 正数 = 正转，负数 = 反转，0 = 刹车
#define MAX_SPEED 1000

/* ===== PWM 载波频率（2026-07-23 降噪）=====================================
 *  TIM1 在 APB2，定时器时钟 168MHz。
 *    原配置 Prescaler=71, Period=999 -> 168M/72/1000 = 2.33kHz
 *    2.33kHz 正落在人耳最敏感的 1~4kHz 频段，这就是电机啸叫的直接来源。
 *  改为 Prescaler=7 -> 168M/8/1000 = 21kHz，超出人耳听觉上限（~20kHz）。
 *  附带好处：载波越高，电机电感对电流的平滑作用越强，转矩脉动更小、低速更稳。
 *  DRV8701 支持到 100kHz PWM，21kHz 完全在规格内。
 *  ★ 这里用运行时改预分频，CubeMX 重生成也不会丢；
 *    但建议同时把 CubeMX 里 TIM1 的 Prescaler 也改成 7，保持配置一致。 */
#define MOTOR_PWM_PRESCALER   7

/* ===== 死区补偿 =============================================================
 *  电机存在静摩擦与驱动器导通阈值：占空比低于某个值时只发热不转动。
 *  PID 在这个区间里会"看不到响应"而持续累积，表现为低速档完全推不动，
 *  一旦越过阈值又突然窜出去 —— 也就是"只有高速档才顺、低速卡死"。
 *  补偿办法：非零指令一律抬到至少 MOTOR_MIN_DUTY，让每一个指令都真的产生动作。
 *  ⚠️ MOTOR_MIN_DUTY 必须实测标定：从 0 缓慢加大 PWM，记下车轮（带配重、落地）
 *     刚好开始持续转动的那个值，再往上留 10% 余量。 */
#define MOTOR_MIN_DUTY      200     /* 最小有效占空比，需实测标定 */
#define MOTOR_DEADZONE       15     /* 小于此值视为停车，防止零位附近抖动 */

/* ===== 输出斜坡限幅 =========================================================
 *  前进/后退切换或急打方向时，PWM 从 +N 直接跳到 -N 会造成电流冲击、
 *  机械顿挫、以及电源轨瞬间塌陷（本项目供电本就紧张）。
 *  限制每个控制周期(17ms)的最大变化量，把突变摊成斜坡。
 *  300/周期 => 从 0 到满速约 6 个周期 ≈ 100ms，既跟手又不冲击。 */
#define MOTOR_SLEW_PER_TICK 300

// 函数声明
void Motor_Init(void);              /* 设置 PWM 载波频率，须在 MX_TIM1_Init 之后调用 */
void Motor1_Set(int16_t speed);
void Motor2_Set(int16_t speed);
void DRV8701_Enable(void);
void DRV8701_Sleep(void);
uint8_t DRV8701_ReadFault(void);

#ifdef __cplusplus
}
#endif

#endif