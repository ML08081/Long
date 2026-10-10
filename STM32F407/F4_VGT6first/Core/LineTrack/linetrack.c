/**
  ******************************************************************************
  * @file    linetrack.c
  * @brief   3×TCRT5000 循迹传感器驱动实现
  ******************************************************************************
  */
#include "linetrack.h"

static uint8_t s_stab[3] = {0, 0, 0};   /* 稳定值 L/C/R（1=压线） */
static uint8_t s_cnt[3]  = {0, 0, 0};   /* 去抖计数 */
static uint32_t s_sample_ms;
static uint8_t s_sample_seen;

#if LINETRACK_USE_ADC
static uint16_t s_raw[3] = {0, 0, 0};   /* 最近一次原始读数，供标定日志 */
static uint16_t s_history[3][3];
static uint8_t s_history_pos;
static uint8_t s_history_count;
static uint16_t s_on[3], s_off[3], s_low[3], s_high[3];
static uint32_t s_low_sum[3], s_high_sum[3];
static uint8_t s_low_cnt[3], s_high_cnt[3], s_inverted[3];
static uint8_t s_range_seeded[3];
static uint8_t s_ready_mask, s_manual;
static const uint16_t kOnTh[3]  = { LINE_ADC_ON_TH_L,  LINE_ADC_ON_TH_C,  LINE_ADC_ON_TH_R  };
static const uint16_t kOffTh[3] = { LINE_ADC_OFF_TH_L, LINE_ADC_OFF_TH_C, LINE_ADC_OFF_TH_R };
#endif

void LineTrack_Init(void)
{
    for (int i = 0; i < 3; i++) { s_stab[i] = 0; s_cnt[i] = 0; }
    s_sample_ms = 0; s_sample_seen = 0;
#if LINETRACK_USE_ADC
    for (int i = 0; i < 3; i++) s_raw[i] = 0;
    s_history_pos = 0; s_history_count = 0;
    s_ready_mask = LINE_ADC_AUTO_CALIBRATE ? 0U : 7U;
    s_manual = LINE_ADC_AUTO_CALIBRATE ? 0U : 1U;
    for (uint8_t i = 0; i < 3; i++) {
        s_on[i] = kOnTh[i]; s_off[i] = kOffTh[i];
        s_low[i] = s_high[i] = 0;
        s_low_cnt[i] = s_high_cnt[i] = 0;
        s_low_sum[i] = s_high_sum[i] = 0;
        s_range_seeded[i] = 0;
        s_inverted[i] = LINE_ADC_INVERTED;
    }
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
static void thresholds_from_range(uint8_t i)
{
    uint16_t span = (uint16_t)(s_high[i] - s_low[i]);
    if (span < LINE_ADC_MIN_SPAN) return;
    uint16_t base = s_inverted[i] ? (uint16_t)(4095U - s_high[i]) : s_low[i];
    s_off[i] = (uint16_t)(base + (uint32_t)span * 40U / 100U);
    s_on[i] = (uint16_t)(base + (uint32_t)span * 60U / 100U);
    s_ready_mask |= (uint8_t)(1U << i);
}

static void learn_range(uint8_t i, uint16_t value)
{
    if (s_manual) return;
    if (!s_range_seeded[i]) {
        s_low[i] = s_high[i] = value; s_range_seeded[i] = 1; return;
    }
    if ((uint32_t)value + LINE_ADC_ENDPOINT_MARGIN < s_low[i]) {
        s_low_sum[i] += value;
        if (++s_low_cnt[i] >= LINE_ADC_ENDPOINT_N) {
            s_low[i] = (uint16_t)(s_low_sum[i] / s_low_cnt[i]);
            s_low_sum[i] = 0; s_low_cnt[i] = 0;
        }
    } else { s_low_cnt[i] = 0; s_low_sum[i] = 0; }
    if ((uint32_t)value > (uint32_t)s_high[i] + LINE_ADC_ENDPOINT_MARGIN) {
        s_high_sum[i] += value;
        if (++s_high_cnt[i] >= LINE_ADC_ENDPOINT_N) {
            s_high[i] = (uint16_t)(s_high_sum[i] / s_high_cnt[i]);
            s_high_sum[i] = 0; s_high_cnt[i] = 0;
        }
    } else { s_high_cnt[i] = 0; s_high_sum[i] = 0; }
    thresholds_from_range(i);
}

uint8_t LineTrack_SetCalibration(const uint16_t background[3], const uint16_t line[3])
{
    if (!background || !line) return 0;
    for (uint8_t i = 0; i < 3; i++) {
        int32_t diff = (int32_t)line[i] - background[i];
        if (background[i] > 4095U || line[i] > 4095U ||
            (diff > -(int32_t)LINE_ADC_MIN_SPAN && diff < (int32_t)LINE_ADC_MIN_SPAN)) return 0;
    }
    uint32_t irq = __get_PRIMASK(); __disable_irq();
    for (uint8_t i = 0; i < 3; i++) {
        s_inverted[i] = line[i] < background[i];
        s_low[i] = s_inverted[i] ? line[i] : background[i];
        s_high[i] = s_inverted[i] ? background[i] : line[i];
        s_range_seeded[i] = 1;
        thresholds_from_range(i);
        s_stab[i] = 0; s_cnt[i] = 0;
    }
    s_manual = 1;
    if (!irq) __enable_irq();
    return 1;
}

void LineTrack_GetThresholds(uint16_t off[3], uint16_t on[3], uint8_t *readyMask)
{
    uint32_t irq = __get_PRIMASK(); __disable_irq();
    for (uint8_t i = 0; i < 3; i++) {
        if (off) off[i] = s_off[i];
        if (on) on[i] = s_on[i];
    }
    if (readyMask) *readyMask = s_ready_mask;
    if (!irq) __enable_irq();
}

static uint16_t median3(uint16_t a, uint16_t b, uint16_t c)
{
    if (a > b) { uint16_t t = a; a = b; b = t; }
    if (b > c) { uint16_t t = b; b = c; c = t; }
    return (a > b) ? a : b;
}

static uint8_t read_one_adc(uint8_t idx, uint16_t rawIn)
{
    /* 统一极性：内部一律按"值越大 = 越像压在黑线上"处理 */
    learn_range(idx, rawIn);
    if (!(s_ready_mask & (uint8_t)(1U << idx))) return 0;
    uint16_t v = s_inverted[idx] ? (uint16_t)(4095U - rawIn) : rawIn;

    uint8_t want = s_stab[idx];          /* 默认维持原状态（死区内即走这条） */
    if (v >= s_on[idx])       want = 1U;   /* 明确压线 */
    else if (v <= s_off[idx]) want = 0U;   /* 明确离线 */

    if (want != s_stab[idx]) {
        if (++s_cnt[idx] >= LINE_ADC_DEBOUNCE_N) { s_stab[idx] = want; s_cnt[idx] = 0; }
    } else {
        s_cnt[idx] = 0;
    }
    return s_stab[idx];
}

LineState LineTrack_Read(void)
{
    /* Ranging 与 Motion 都会读取；以固定时间采样，不能按调用次数去抖。
       很短的临界区防止高优先级 Motion 在三路更新中途抢占 Ranging。 */
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    uint32_t now = HAL_GetTick();
    if (!ADC_IsStreaming()) {
        for (uint8_t i = 0; i < 3; i++) { s_stab[i] = 0; s_cnt[i] = 0; }
        s_sample_seen = 0; s_history_count = 0;
    } else if (!s_sample_seen || now - s_sample_ms >= LINE_SAMPLE_MS) {
        s_raw[0] = (uint16_t)g_adc_dma[ADC_CH_LINE_L];
        s_raw[1] = (uint16_t)g_adc_dma[ADC_CH_LINE_C];
        s_raw[2] = (uint16_t)g_adc_dma[ADC_CH_LINE_R];
        if (s_history_count < 3U) s_history_count++;
        for (uint8_t i = 0; i < 3; i++) {
            if (!s_sample_seen) {
                for (uint8_t n = 0; n < 3; n++) s_history[i][n] = s_raw[i];
            }
            s_history[i][s_history_pos] = s_raw[i];
            /* 首个DMA值也可能是毛刺，必须已有3次真实样本才学习范围。 */
            if (s_history_count >= 3U)
                read_one_adc(i, median3(s_history[i][0], s_history[i][1], s_history[i][2]));
        }
        s_history_pos = (uint8_t)((s_history_pos + 1U) % 3U);
        s_sample_ms = now;
        s_sample_seen = 1;
    }
    LineState s = {s_stab[0], s_stab[1], s_stab[2]};
    if (!irq) __enable_irq();
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
    uint32_t irq = __get_PRIMASK();
    __disable_irq();
    uint32_t now = HAL_GetTick();
    if (!s_sample_seen || now - s_sample_ms >= LINE_SAMPLE_MS) {
        read_one(LINE_l_GPIO_Port, LINE_l_Pin, 0);
        read_one(LINE_C_GPIO_Port, LINE_C_Pin, 1);
        read_one(LINE_R_GPIO_Port, LINE_R_Pin, 2);
        s_sample_ms = now; s_sample_seen = 1;
    }
    LineState s = {s_stab[0], s_stab[1], s_stab[2]};
    if (!irq) __enable_irq();
    return s;
}

#endif /* LINETRACK_USE_ADC */

uint8_t LineTrack_IsCalibrated(void)
{
#if LINETRACK_USE_ADC
    return ADC_IsStreaming() && s_ready_mask == 7U;
#else
    return 1;
#endif
}

uint8_t LineTrack_IsJunction(LineState s) { return (s.l && s.c && s.r); }
uint8_t LineTrack_IsLost(LineState s)     { return (!s.l && !s.c && !s.r); }

/* 线位置误差：越负=线在左需向左修；越正=向右修 */
int8_t LineTrack_Error(LineState s)
{
    if (s.l && !s.r)         return s.c ? -1 : -2;
    if (s.r && !s.l)         return s.c ?  1 :  2;
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
