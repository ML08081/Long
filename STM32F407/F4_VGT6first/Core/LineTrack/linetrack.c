/**
  ******************************************************************************
  * @file    linetrack.c
  * @brief   3×TCRT5000 循迹传感器驱动实现
  ******************************************************************************
  */
#include "linetrack.h"

static uint8_t s_stab[3] = {0, 0, 0};   /* 稳定值 L/C/R（1=压线） */
static uint8_t s_cnt[3]  = {0, 0, 0};   /* 去抖计数 */

#if LINETRACK_USE_ADC
static uint16_t s_raw[3] = {0, 0, 0};   /* 最近一次原始读数，供标定日志 */
#endif

void LineTrack_Init(void)
{
    for (int i = 0; i < 3; i++) { s_stab[i] = 0; s_cnt[i] = 0; }
#if LINETRACK_USE_ADC
    for (int i = 0; i < 3; i++) s_raw[i] = 0;
#endif
}

#if LINETRACK_USE_ADC

void LineTrack_GetRaw(uint16_t *l, uint16_t *c, uint16_t *r)
{
    if (l) *l = s_raw[0];
    if (c) *c = s_raw[1];
    if (r) *r = s_raw[2];
}

/**
 * 单路模拟量判定（双阈值迟滞 + 去抖）。
 *
 * 迟滞的意义：只有明确越过 ON 阈值才认为压线，明确越过 OFF 阈值才认为离线，
 * 两阈值之间是死区 —— 读数在死区里晃动时保持原状态不变。这样即使探头离地近、
 * 基线整体抬高导致读数在某个中间值附近抖，也不会来回翻转。
 * 数字量方案只有一个翻转点，抖动必然穿越它，靠加大去抖次数只能变慢、不能变稳。
 */
/* 每路独立阈值表，下标 0=L 1=C 2=R，与 s_stab/s_raw 一致 */
static const uint16_t kOnTh[3]  = { LINE_ADC_ON_TH_L,  LINE_ADC_ON_TH_C,  LINE_ADC_ON_TH_R  };
static const uint16_t kOffTh[3] = { LINE_ADC_OFF_TH_L, LINE_ADC_OFF_TH_C, LINE_ADC_OFF_TH_R };

static uint8_t read_one_adc(uint8_t idx, uint16_t rawIn)
{
    /* 统一极性：内部一律按"值越大 = 越像压在黑线上"处理 */
#if LINE_ADC_INVERTED
    uint16_t v = (rawIn >= 4095U) ? 0U : (uint16_t)(4095U - rawIn);
#else
    uint16_t v = rawIn;
#endif

    uint8_t want = s_stab[idx];          /* 默认维持原状态（死区内即走这条） */
    if (v >= kOnTh[idx])       want = 1U;   /* 明确压线 */
    else if (v <= kOffTh[idx]) want = 0U;   /* 明确离线 */

    if (want != s_stab[idx]) {
        if (++s_cnt[idx] >= LINE_ADC_DEBOUNCE_N) { s_stab[idx] = want; s_cnt[idx] = 0; }
    } else {
        s_cnt[idx] = 0;
    }
    return s_stab[idx];
}

LineState LineTrack_Read(void)
{
    /* 直接读 DMA 缓冲：ADC1 在后台以约 8.8kHz 循环扫描，这里零阻塞。 */
    s_raw[0] = (uint16_t)g_adc_dma[ADC_CH_LINE_L];
    s_raw[1] = (uint16_t)g_adc_dma[ADC_CH_LINE_C];
    s_raw[2] = (uint16_t)g_adc_dma[ADC_CH_LINE_R];

    LineState s;
    s.l = read_one_adc(0, s_raw[0]);
    s.c = read_one_adc(1, s_raw[1]);
    s.r = read_one_adc(2, s_raw[2]);
    return s;
}

#else  /* ---- 数字量 DO 回退路径 ---- */

static uint8_t read_one(GPIO_TypeDef *port, uint16_t pin, uint8_t idx)
{
    uint8_t on = (HAL_GPIO_ReadPin(port, pin) == LINE_ON_LEVEL) ? 1u : 0u;
    if (on != s_stab[idx]) {
        /* 非对称迟滞：0->1(认为压线) 用严格门限，1->0(认为离线) 用宽松门限 */
        uint8_t need = on ? DEBOUNCE_ON_N : DEBOUNCE_OFF_N;
        if (++s_cnt[idx] >= need) { s_stab[idx] = on; s_cnt[idx] = 0; }
    } else {
        s_cnt[idx] = 0;
    }
    return s_stab[idx];
}

LineState LineTrack_Read(void)
{
    LineState s;
    s.l = read_one(LINE_l_GPIO_Port, LINE_l_Pin, 0);
    s.c = read_one(LINE_C_GPIO_Port, LINE_C_Pin, 1);
    s.r = read_one(LINE_R_GPIO_Port, LINE_R_Pin, 2);
    return s;
}

#endif /* LINETRACK_USE_ADC */

uint8_t LineTrack_IsJunction(LineState s) { return (s.l && s.c && s.r); }
uint8_t LineTrack_IsLost(LineState s)     { return (!s.l && !s.c && !s.r); }

/* 线位置误差：越负=线在左需向左修；越正=向右修 */
int8_t LineTrack_Error(LineState s)
{
    if (s.c && !s.l && !s.r) return 0;      /* 正中 */
    if (s.l && !s.r)         return -2;     /* 明显偏左 */
    if (s.r && !s.l)         return  2;     /* 明显偏右 */
    if (s.l && s.c)          return -1;     /* 略偏左 */
    if (s.r && s.c)          return  1;     /* 略偏右 */
    return 0;                                /* 全亮=路口 / 全灭=丢线，交状态机处理 */
}

uint8_t LineTrack_PackBits(LineState s)
{
    uint8_t b = 0;
    if (s.l) b |= LINE_BIT_L;
    if (s.c) b |= LINE_BIT_C;
    if (s.r) b |= LINE_BIT_R;
    if (LineTrack_IsJunction(s)) b |= LINE_BIT_JUNCTION;
    if (LineTrack_IsLost(s))     b |= LINE_BIT_LOST;
    return b;
}
