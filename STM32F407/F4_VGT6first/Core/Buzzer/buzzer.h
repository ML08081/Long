#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 有源蜂鸣器驱动（报警提示）
 *
 * 引脚：PB8（GPIO 开漏输出、无内部上下拉，低电平鸣响）—— main.h: Buzzer_Pin / Buzzer_GPIO_Port
 * 静音时写 SET 释放控制线，使用模块自身上拉；不可用推挽强驱高电平。
 * 说明：PB8 未接定时器通道，按“有源蜂鸣器”处理（通电即响，无需 PWM 音调）。
 *       当前通过不同鸣响节奏区分告警类型；若为无源蜂鸣器，请改接 TIM 通道后单独实现音调驱动。
 *
 * 报警模式由上层（EnvSensorTask）调用 Buzzer_SetPattern() 设置，
 * Buzzer_Tick() 由独立 20ms 快速节拍推进闪鸣状态机，不受传感器读取阻塞影响。
 *============================================================================*/

typedef enum {
    BUZZER_OFF      = 0,   // 静音：PB8 开漏释放，由模块上拉
    BUZZER_WARN     = 1,   // 通用警告：短响慢停，持续循环
    BUZZER_ALARM    = 2,   // 通用严重报警：长响短停，快速循环
    BUZZER_SOLID    = 3,   // 长鸣
    BUZZER_OBSTACLE = 4,   // 障碍：300ms 响 / 900ms 停
    BUZZER_GAS      = 5,   // 气体：双 200ms 短响 / 1000ms 停
    BUZZER_FLAME    = 6,   // 火焰：600ms 响 / 200ms 停
    BUZZER_FAULT    = 7,   // 系统故障：双 1000ms 长响 / 1700ms 停
} BuzzerPattern;

/* HAL_Init 后立即调用，尽早结束复位期间 PB8 输入悬空状态。 */
void Buzzer_EarlySilence(void);

/* 初始化：PB8 开漏输出，初始释放静音 */
void Buzzer_Init(void);

/* 立即开/关（底层直接控制，供自检或简单场景用） */
void Buzzer_On(void);
void Buzzer_Off(void);

/* 设置鸣响模式（最多 20ms 后生效，并持续循环到模式改变） */
void Buzzer_SetPattern(BuzzerPattern p);

/* 状态机推进一拍；tick_ms = 调用周期（毫秒），用于换算闪鸣节奏 */
void Buzzer_Tick(uint16_t tick_ms);

/* 当前是否正在鸣响（供上报 flags 用） */
uint8_t Buzzer_IsSounding(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
