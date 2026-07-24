#include "buzzer.h"
#include "main.h"     // Buzzer_Pin / Buzzer_GPIO_Port

/* 闪鸣半周期（毫秒）：ON/OFF 各持续此时长 */
#define BUZZER_WARN_HALF_MS   500   /* 慢闪 ~1Hz */
#define BUZZER_ALARM_HALF_MS  100   /* 快闪 ~5Hz */

static volatile BuzzerPattern s_pattern = BUZZER_OFF;
static uint16_t s_elapsed_ms = 0;   /* 当前半周期已累计毫秒 */
static uint8_t  s_on         = 0;    /* 当前电平：1=鸣，0=静 */

static void buzzer_write(uint8_t on)
{
    HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
    s_on = on ? 1 : 0;
}

void Buzzer_Init(void)
{
    /* PB8 已由 MX_GPIO_Init 配为推挽输出，这里仅确保初始静音 */
    s_pattern    = BUZZER_OFF;
    s_elapsed_ms = 0;
    buzzer_write(0);
}

void Buzzer_On(void)  { buzzer_write(1); }
void Buzzer_Off(void) { buzzer_write(0); }

void Buzzer_SetPattern(BuzzerPattern p)
{
    if (p != s_pattern)
    {
        s_pattern    = p;
        s_elapsed_ms = 0;
        /* 切换模式立即刷新电平，避免等待一个半周期 */
        if (p == BUZZER_OFF)        buzzer_write(0);
        else if (p == BUZZER_SOLID) buzzer_write(1);
        else                        buzzer_write(1);   /* 闪鸣从“鸣”开始 */
    }
}

void Buzzer_Tick(uint16_t tick_ms)
{
    uint16_t half;

    switch (s_pattern)
    {
    case BUZZER_OFF:
        if (s_on) buzzer_write(0);
        return;
    case BUZZER_SOLID:
        if (!s_on) buzzer_write(1);
        return;
    case BUZZER_WARN:
        half = BUZZER_WARN_HALF_MS;
        break;
    case BUZZER_ALARM:
    default:
        half = BUZZER_ALARM_HALF_MS;
        break;
    }

    s_elapsed_ms += tick_ms;
    if (s_elapsed_ms >= half)
    {
        s_elapsed_ms = 0;
        buzzer_write(s_on ? 0 : 1);   /* 翻转 */
    }
}

uint8_t Buzzer_IsSounding(void)
{
    return s_on;
}
