#include "servo2.h"
#include "tim.h"     // htim8

/*============================================================================
 *  超声波云台舵机驱动实现 —— TIM8_CH3 硬件 PWM (PC8 / YT_PWM)
 *
 *  (2026-07-07 CubeMX 重配后)：由原 TIM2 中断软件 PWM 迁移为 TIM8 硬件 PWM。
 *  占空比由定时器硬件生成，不占中断、无抖动、不受 FreeRTOS 抢占影响。
 *
 *  脉宽以 us 为单位直接写入 CCR3（要求 TIM8 Prescaler=167 → 1MHz、
 *  ARR=19999 → 50Hz；PC8=TIM8_CH3 为普通输出通道，用 HAL_TIM_PWM_Start）。
 *============================================================================*/

/*============================================================================*/

void Servo2_Init(void)
{
    /* 回中脉宽，随后启动硬件 PWM 输出（PC8 的 AF3 由 CubeMX MSP 配置） */
    __HAL_TIM_SET_COMPARE(&SERVO2_TIM, SERVO2_CHANNEL, SERVO2_MID);
    HAL_TIM_PWM_Start(&SERVO2_TIM, SERVO2_CHANNEL);
}

/*============================================================================*/

void Servo2_SetPulse(uint16_t pulse)
{
    if (pulse < SERVO2_MIN) pulse = SERVO2_MIN;
    if (pulse > SERVO2_MAX) pulse = SERVO2_MAX;
    /* 直接更新比较值，硬件在下一周期以新占空比输出（非阻塞、无抖动） */
    __HAL_TIM_SET_COMPARE(&SERVO2_TIM, SERVO2_CHANNEL, pulse);
}

/*============================================================================*/

void Servo2_SetAngle(uint16_t deg)
{
    if (deg > 180) deg = 180;
    /* 线性映射：500 + deg * (2500-500)/180 */
    Servo2_SetPulse(SERVO2_MIN + (deg * (SERVO2_MAX - SERVO2_MIN)) / 180);
}
