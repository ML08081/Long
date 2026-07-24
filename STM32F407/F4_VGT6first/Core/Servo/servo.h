#ifndef __SERVO_H
#define __SERVO_H

#include "stm32f4xx_hal.h"

/*============================================================================
 * 转向舵机控制模块
 *
 * 硬件：TIM2_CH1 (PA0)，50Hz PWM，1us 分辨率
 *
 * === 线性化说明 ===
 *
 * 舵机 → 连杆 → 车轮的传递函数不是线性的（典型 S 形特性）：
 *   中心区域（-30°~+30°）：机械增益高 → 小的脉宽变化产生大的车轮转角
 *   边缘区域（>±60°）   ：机械增益低 → 大的脉宽变化只产生小的车轮转角
 *
 * 补偿方式（Servo_AngleToPulse）：
 *   先做角度 → 脉宽的线性映射，再叠加上凸二次修正曲线。
 *   在中心区域主动减少脉宽变化量，在边缘区域主动增加脉宽变化量，
 *   使得"每度角度变化"对应的实际车轮转角基本一致。
 *
 * === 平滑过渡说明 ===
 *
 * Servo_SetSmoothPulse() / Servo_SetSmoothAngle() 设置目标位置，
 * Servo_RampTick() 在每个控制周期（建议 17ms）中步进一步，
 * 实现可控速度的平滑过渡，避免急转导致的抖动和非线性感知。
 *============================================================================*/

/*---------- 默认脉宽参数（单位：us，依赖 TIM2=1MHz/1us-per-tick） ----------*/
#define SERVO_PULSE_MID        1500U      // 中位（0°）——物理验证值，勿动
#define SERVO_PULSE_LEFT_MAX   1711U      // 左极限——物理验证值，勿动（约 2.28us/°）
#define SERVO_PULSE_RIGHT_MIN  1300U      // 右极限——物理验证值，勿动

/*---------- 角度参数（单位：度） ----------*/
#define SERVO_ANGLE_MIN        -90        // 最右
#define SERVO_ANGLE_MID         0         // 直行
#define SERVO_ANGLE_MAX        90         // 最左

/*---------- 平滑过渡参数 ----------*/
/* ★ 2026-07-23 转向丝滑改造：
 *   旧实现每 Tick 走【固定】步长 = 匀速直线运动，起步和到位都是速度突变，
 *   加上满行程仅 411us、15us/步只有 27 个离散位置，观感明显"一格一格"卡顿。
 *   现改为【比例步长】：step = |误差| / SERVO_RAMP_DIV，再由 MIN/MAX 夹逼。
 *   远离目标时按上限快速接近，越接近步长越小 => 指数式渐近，自带减速，
 *   到位平顺无过冲；靠近目标时步长可细到 1us，分辨率也比原来高得多。
 *   下面的 *_STEP 现在的语义是【每 Tick 步长上限】，不再是固定步长。 */
#define SERVO_RAMP_DIV           4        // 比例除数：步长 = |误差|/4
#define SERVO_RAMP_MIN_STEP      1        // 步长下限（us）——保证最终必定收敛到目标
#define SERVO_RAMP_DEFAULT_STEP  8        // 默认每 Tick 步长上限（us），温和过渡
#define SERVO_RAMP_FAST_STEP    25        // 快速步长上限（us），用于遥控快速响应
                                          // 411us 满行程：上限段 ~17 Tick，之后自动减速收尾

/**
 * @brief  舵机自检（阻塞）：左极限 → 右极限 → 回中
 * @param  step_us  每步脉宽变化量（us），推荐 5~10
 * @param  delay_ms 每步间隔（ms），推荐 15~30
 * @retval 1 = 自检完成
 *
 * @note 这是一个阻塞函数！请在 main() 初始化阶段（osKernelStart 之前）
 *       或单独的初始化任务中调用。自检期间会占用 CPU 直到完成。
 *
 * 自检流程：
 *   1. 从中位 → 左极限 (SERVO_ANGLE_MAX, +90°)
 *   2. 从左极限 → 右极限 (SERVO_ANGLE_MIN, -90°)
 *   3. 从右极限 → 回中 (0°)
 *   4. 输出结果日志
 */
uint8_t Servo_SelfCheck(uint16_t step_us, uint16_t delay_ms);

/*============================================================================
 * API
 *============================================================================*/

/**
 * @brief  初始化舵机（TIM2_CH1 PWM 启动，默认中位）
 */
void Servo_Init(void);

/**
 * @brief  直接设置脉宽（立即生效，不经过平滑）
 * @param  pulse_us  1300(SERVO_PULSE_RIGHT_MIN) ~ 1711(SERVO_PULSE_LEFT_MAX)
 */
void Servo_SetPulse(uint16_t pulse_us);

/**
 * @brief  设置目标角度（立即生效，不经过平滑）
 * @param  angle_deg  -90 ~ +90，负=右转，0=直行，正=左转
 */
void Servo_SetAngle(int16_t angle_deg);

/**
 * @brief  平滑过渡到目标脉宽
 * @param  target_pulse  目标脉宽（自动限幅）
 * @param  ramp_step     每 Tick 最大步进（us），0=使用默认值
 */
void Servo_SetSmoothPulse(uint16_t target_pulse, uint16_t ramp_step);

/**
 * @brief  平滑过渡到目标角度
 * @param  target_angle  目标角度（自动限幅）
 * @param  ramp_step     每 Tick 最大步进（us），0=使用默认值
 */
void Servo_SetSmoothAngle(int16_t target_angle, uint16_t ramp_step);

/**
 * @brief  平滑过渡 Tick 函数——每控制周期调用一次
 * @retval 1 = 仍在运动中，0 = 已到达目标位置
 *
 * 在 MotionControlTask 的主循环中调用（建议 ~17ms 周期）。
 * 每次按 ramp_step 步进一次，直到追上目标值。
 */
uint8_t Servo_RampTick(void);

/**
 * @brief  3 点校准
 * @param  pulse_mid    中位脉宽（默认 1500us）
 * @param  pulse_left   左极限脉宽（默认 1711us）
 * @param  pulse_right  右极限脉宽（默认 1300us）
 *
 * 如果你的舵机安装后左右极限或中位有偏差，用此函数修正。
 * 建议在 main() 初始化时调用一次。
 */
void Servo_Calibrate(uint16_t pulse_mid, uint16_t pulse_left, uint16_t pulse_right);

/**
 * @brief  角度 → 脉宽转换（含线性化补偿曲线）
 */
uint16_t Servo_AngleToPulse(int16_t angle_deg);

/**
 * @brief  脉宽 → 角度转换（逆映射，含逆向补偿）
 */
int16_t Servo_PulseToAngle(uint16_t pulse_us);

/**
 * @brief  获取当前实际脉宽（正在输出的值）
 */
uint16_t Servo_GetCurrentPulse(void);

/**
 * @brief  获取当前目标脉宽
 */
uint16_t Servo_GetTargetPulse(void);

/**
 * @brief  获取当前实际角度
 */
int16_t Servo_GetCurrentAngle(void);

/*---------- 向后兼容宏 ----------*/
#define Servo_Set(p)           Servo_SetPulse(p)
#define Servo_GoStraight()     Servo_SetAngle(0)
#define Servo_TurnLeft()       Servo_SetAngle(SERVO_ANGLE_MAX)
#define Servo_TurnRight()      Servo_SetAngle(SERVO_ANGLE_MIN)

#endif /* __SERVO_H */
