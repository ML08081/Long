#ifndef __BUZZER_H
#define __BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 有源蜂鸣器驱动（报警提示）
 *
 * 引脚：PB8（GPIO 推挽输出，高电平鸣响）—— main.h: Buzzer_Pin / Buzzer_GPIO_Port
 * 说明：PB8 未接定时器通道，按“有源蜂鸣器”处理（通电即响，无需 PWM 音调）。
 *       若为无源蜂鸣器，请改接 TIM 通道后单独实现音调驱动。
 *
 * 报警模式由上层（EnvSensorTask）每周期调用 Buzzer_SetPattern() 设置，
 * Buzzer_Tick() 在固定节拍（建议 ~10Hz）推进闪鸣状态机，无阻塞。
 *============================================================================*/

typedef enum {
    BUZZER_OFF   = 0,   // 静音
    BUZZER_WARN  = 1,   // 警告：慢速间歇鸣（~1Hz）
    BUZZER_ALARM = 2,   // 报警：快速间歇鸣（~5Hz）
    BUZZER_SOLID = 3,   // 长鸣
} BuzzerPattern;

/* 初始化：PB8 输出，初始静音 */
void Buzzer_Init(void);

/* 立即开/关（底层直接控制，供自检或简单场景用） */
void Buzzer_On(void);
void Buzzer_Off(void);

/* 设置鸣响模式（下一拍 Buzzer_Tick 生效） */
void Buzzer_SetPattern(BuzzerPattern p);

/* 状态机推进一拍；tick_ms = 调用周期（毫秒），用于换算闪鸣节奏 */
void Buzzer_Tick(uint16_t tick_ms);

/* 当前是否正在鸣响（供上报 flags 用） */
uint8_t Buzzer_IsSounding(void);

#ifdef __cplusplus
}
#endif

#endif /* __BUZZER_H */
