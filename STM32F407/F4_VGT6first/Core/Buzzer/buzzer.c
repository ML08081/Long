#include "buzzer.h"
#include "main.h"     // Buzzer_Pin / Buzzer_GPIO_Port

/* 低电平触发：安全/静音状态必须输出高电平。 */
#define BUZZER_ON_LEVEL   GPIO_PIN_RESET
#define BUZZER_OFF_LEVEL  GPIO_PIN_SET

#define BUZZER_OBSTACLE_PERIOD_MS  1200U
#define BUZZER_GAS_PERIOD_MS       1600U
#define BUZZER_FLAME_PERIOD_MS      800U
#define BUZZER_FAULT_PERIOD_MS     4200U

static volatile BuzzerPattern s_requested_pattern = BUZZER_OFF;
static BuzzerPattern s_active_pattern = BUZZER_OFF;
static uint16_t s_elapsed_ms = 0;   /* 当前节奏周期内已累计毫秒 */
static uint8_t  s_on         = 0;    /* 当前电平：1=鸣，0=静 */

static void buzzer_write(uint8_t on)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, on ? BUZZER_ON_LEVEL : BUZZER_OFF_LEVEL);
    s_on = on ? 1 : 0;
}

static uint16_t buzzer_period_ms(BuzzerPattern p)
{
    switch (p)
    {
    case BUZZER_WARN:
    case BUZZER_OBSTACLE:
        return BUZZER_OBSTACLE_PERIOD_MS;
    case BUZZER_GAS:
        return BUZZER_GAS_PERIOD_MS;
    case BUZZER_ALARM:
    case BUZZER_FLAME:
        return BUZZER_FLAME_PERIOD_MS;
    case BUZZER_FAULT:
        return BUZZER_FAULT_PERIOD_MS;
    default:
        return 0U;
    }
}

static uint8_t buzzer_pattern_on(BuzzerPattern p, uint16_t phase_ms)
{
    switch (p)
    {
    case BUZZER_WARN:
    case BUZZER_OBSTACLE:
        /* 障碍/警告：300ms 短响 + 900ms 静音，持续慢速循环。 */
        return (phase_ms < 300U) ? 1U : 0U;
    case BUZZER_GAS:
        /* 气体：200ms 双短响，中间停 200ms，随后长停 1000ms。 */
        return ((phase_ms < 200U) || (phase_ms >= 400U && phase_ms < 600U)) ? 1U : 0U;
    case BUZZER_ALARM:
    case BUZZER_FLAME:
        /* 火焰/严重报警：600ms 长响 + 200ms 短停，快速持续循环。 */
        return (phase_ms < 600U) ? 1U : 0U;
    case BUZZER_FAULT:
        /* 系统故障：双长响（1000/1000ms），随后 1700ms 静音。 */
        return ((phase_ms < 1000U) ||
                (phase_ms >= 1250U && phase_ms < 2250U)) ? 1U : 0U;
    default:
        return 0U;
    }
}

void Buzzer_Init(void)
{
    /* PB8 已由 MX_GPIO_Init 配为推挽输出，这里仅确保初始静音 */
    s_requested_pattern = BUZZER_OFF;
    s_active_pattern    = BUZZER_OFF;
    s_elapsed_ms = 0;
    buzzer_write(0);
}

void Buzzer_On(void)  { buzzer_write(1); }
void Buzzer_Off(void) { buzzer_write(0); }

void Buzzer_SetPattern(BuzzerPattern p)
{
    s_requested_pattern = p;
}

void Buzzer_Tick(uint16_t tick_ms)
{
    uint16_t period;

    /* 仅快速节拍任务修改活动状态，环境采集阻塞不会打断报警节奏。 */
    if (s_active_pattern != s_requested_pattern)
    {
        s_active_pattern = s_requested_pattern;
        s_elapsed_ms = 0U;
    }

    switch (s_active_pattern)
    {
    case BUZZER_OFF:
        if (s_on) buzzer_write(0);
        return;
    case BUZZER_SOLID:
        if (!s_on) buzzer_write(1);
        return;
    default:
        break;
    }

    period = buzzer_period_ms(s_active_pattern);
    if (period == 0U)
    {
        buzzer_write(0);
        return;
    }

    /* 先输出当前相位，模式切换后的第一拍立即鸣响。 */
    buzzer_write(buzzer_pattern_on(s_active_pattern, s_elapsed_ms));

    s_elapsed_ms += tick_ms;
    while (s_elapsed_ms >= period)
    {
        s_elapsed_ms -= period;
    }
}

uint8_t Buzzer_IsSounding(void)
{
    return s_on;
}
