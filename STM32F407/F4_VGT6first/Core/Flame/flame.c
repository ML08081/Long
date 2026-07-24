#include "flame.h"
#include "main.h"   // Flame_Pin / Flame_GPIO_Port

static uint8_t s_stable = 0;   /* 去抖后稳定状态 */
static uint8_t s_cnt    = 0;   /* 连续一致计数 */
static uint8_t s_last   = 0;   /* 上次瞬时读数 */

void Flame_Init(void)
{
    s_stable = 0;
    s_cnt    = 0;
    s_last   = Flame_ReadRaw();
}

uint8_t Flame_ReadRaw(void)
{
    GPIO_PinState st = HAL_GPIO_ReadPin(Flame_GPIO_Port, Flame_Pin);
#if FLAME_ACTIVE_LOW
    return (st == GPIO_PIN_RESET) ? 1 : 0;
#else
    return (st == GPIO_PIN_SET) ? 1 : 0;
#endif
}

void Flame_Tick(void)
{
    uint8_t cur = Flame_ReadRaw();
    if (cur == s_last)
    {
        if (s_cnt < FLAME_DEBOUNCE_N) s_cnt++;
        if (s_cnt >= FLAME_DEBOUNCE_N) s_stable = cur;
    }
    else
    {
        s_cnt = 0;
    }
    s_last = cur;
}

uint8_t Flame_IsDetected(void)
{
    return s_stable;
}
