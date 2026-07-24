#include "hc.h"
#include "gpio.h"    // HAL_GPIO_Init / ReadPin / WritePin
#include "tim.h"     // htim2

/*============================================================================
 * HC-SR04 驱动实现
 *
 * TIM2 配置回顾 (来自 MX_TIM2_Init)：
 *   Prescaler    = 83      → APB1定时器 84 MHz / 84 = 1 MHz → 1 tick = 1 us
 *                            (SYSCLK=168MHz 下 APB1 Timer=84MHz；勿改回 71，否则
 *                             测距偏高 16.7%、舵机中位偏移过满舵)
 *   Period (ARR) = 19999   → 20 ms (50 Hz，用于舵机 PWM)
 *   CounterMode  = UP
 *
 * TIM2_CH2 输入捕获 (本文件在 HC_Init 中配置)：
 *   CC2S = 01   → IC2 映射到 TI2 (PB3, AF1)
 *   IC2PSC = 00 → 无预分频，每个边沿捕获一次
 *   IC2F = 000  → 无滤波
 *
 * 非阻塞模式使用 TIM2_CH2 捕获中断 (CC2IE)：
 *   TIM2_IRQHandler → HC_TIM2_IRQHandler 处理上升/下降沿捕获
 *   HC_ProcessTick() 在 ScanTask 中轮询，管理 TRIG 时序和超时
 *============================================================================*/

/* 上次测量的原始脉冲宽度 (us) */
static volatile uint32_t s_last_pulse_us = 0;

/* 非阻塞状态机 */
#define HC_STATE_IDLE       0
#define HC_STATE_TRIG_HIGH  1   // TRIG 高电平中
#define HC_STATE_TRIG_LOW   2   // TRIG 刚变低，等待 ECHO
#define HC_STATE_WAIT_RISE  3   // 等待 ECHO 上升沿 (中断处理)
#define HC_STATE_WAIT_FALL  4   // 等待 ECHO 下降沿 (中断处理)
#define HC_STATE_DONE       5   // 完成

static volatile uint8_t  s_hc_state       = HC_STATE_IDLE;
static volatile uint32_t s_hc_tick_start  = 0;   // 当前阶段起始 32 位 us 时间戳
static volatile uint32_t s_rising_us      = 0;   // 上升沿 32 位 us 时间戳

/*============================================================================*/

/*---------- TIM2 溢出计数器 (与 stm32f4xx_it.c 共享) ----------*/
static volatile uint32_t s_tim2_ovf = 0;

void HC_OnTimerOverflow(void)
{
    s_tim2_ovf++;
}

uint32_t HC_GetUs(void)
{
    /* ARM Cortex-M4 上 uint32_t 读写是原子的，无需关中断 */
    return s_tim2_ovf * HC_TIM2_PERIOD_US + TIM2->CNT;
}

/* 边沿时间戳（仅供捕获中断用）：读【硬件锁存的 CCR2】而非 CNT。
   —— 这是超声波抖动的根因修复(《深度优化报告》P2)：读 CNT 拿到的是
      "边沿时刻 + 中断响应延迟"，TIM2_IRQn 优先级最低时延迟抖动可达数十 us
      (1us≈0.17cm)；读 CCR2 拿到的是边沿到来那一刻硬件锁存的计数，延迟归零。
   wrap 校正：TIM2_IRQHandler 先处理 update(ovf++)、后处理 CC2。若边沿发生在
      wrap 之前，进入本函数时 CCR2 会大于当前 CNT —— 说明该边沿属上一周期，
      而 ovf 已在本 IRQ 前段被 +1，故回退 1 个周期。 */
static inline uint32_t HC_CaptureUs(void)
{
    uint32_t ovf = s_tim2_ovf;
    uint32_t cap = TIM2->CCR2;
    uint32_t now = TIM2->CNT;
    if (cap > now) ovf--;                       /* 边沿之后已 wrap，ovf 超前 → 回退 */
    return ovf * HC_TIM2_PERIOD_US + cap;
}

/* ---- 5 点中值滤波：剔除超声波飞点(斜面/软物/多径回波)与残余回绕野值。
       中值优于均值——均值会被单个飞点拖偏，中值直接把少数飞点排除在外。
       原始 0(超时/无目标)也入缓冲：多数为 0 → 中值 0 → 正确判"无目标"。 ---- */
#define HC_MEDIAN_N   5
static uint32_t          s_med_buf[HC_MEDIAN_N] = {0};
static uint8_t           s_med_idx = 0;
static uint8_t           s_med_cnt = 0;         /* 已填充样本数(预热) */
static volatile uint32_t s_filt_pulse_us = 0;   /* 中值后脉宽，供 GetDistance 使用 */

static void HC_PushSample(uint32_t raw_pulse)
{
    s_med_buf[s_med_idx] = raw_pulse;
    s_med_idx = (uint8_t)((s_med_idx + 1u) % HC_MEDIAN_N);
    if (s_med_cnt < HC_MEDIAN_N) s_med_cnt++;

    uint32_t tmp[HC_MEDIAN_N];
    uint8_t  n = s_med_cnt;
    for (uint8_t i = 0; i < n; i++) tmp[i] = s_med_buf[i];
    for (uint8_t i = 1; i < n; i++) {           /* 插入排序，N=5 开销可忽略 */
        uint32_t v = tmp[i];
        int8_t   j = (int8_t)i - 1;
        while (j >= 0 && tmp[j] > v) { tmp[j + 1] = tmp[j]; j--; }
        tmp[j + 1] = v;
    }
    s_filt_pulse_us = tmp[n / 2];                /* 中位数 */
}

/*============================================================================*/

/**
 * @brief  配置 TIM2_CH2 输入捕获
 *
 * 通过直接寄存器操作配置，避免与 HAL 的 PWM 状态机冲突。
 * 仅修改 CC2 相关位，完全不影响 CH1 (PWM 输出)。
 */
static void HC_ConfigCapture(void)
{
    /* ---- CCMR1: CC2 输入模式 ---- */
    TIM2->CCMR1 &= ~TIM_CCMR1_CC2S;      /* 清零 CC2S[1:0] */
    TIM2->CCMR1 |= TIM_CCMR1_CC2S_0;     /* CC2S = 01 : IC2 映射到 TI2 (PB3) */

    TIM2->CCMR1 &= ~TIM_CCMR1_IC2PSC;    /* 无捕获预分频 */
    TIM2->CCMR1 &= ~TIM_CCMR1_IC2F;      /* 无滤波 */

    /* ---- CCER: 上升沿捕获 ---- */
    TIM2->CCER &= ~(TIM_CCER_CC2P | TIM_CCER_CC2NP);  /* 00 = 上升沿 */
    TIM2->CCER |= TIM_CCER_CC2E;          /* 使能 CH2 捕获 */
}

/*============================================================================*/

/**
 * @brief  切换 TIM2_CH2 捕获极性
 * @param  falling  0=上升沿, 1=下降沿
 */
static void HC_SetPolarity(uint8_t falling)
{
    TIM2->CCER &= ~TIM_CCER_CC2E;         /* 先禁用通道 */
    if (falling)
        TIM2->CCER |= TIM_CCER_CC2P;      /* CC2P=1 → 下降沿 */
    else
        TIM2->CCER &= ~TIM_CCER_CC2P;     /* CC2P=0 → 上升沿 */
    TIM2->CCER |= TIM_CCER_CC2E;          /* 重新使能 */
}

/*============================================================================*/
/*                          Public API                                        */
/*============================================================================*/

void HC_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    /* ---- TRIG (PB0) : 推挽输出，初始低电平 ---- */
    gpio.Pin       = HC_TRIG_PIN;
    gpio.Mode      = GPIO_MODE_OUTPUT_PP;
    gpio.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio.Pull      = GPIO_NOPULL;
    HAL_GPIO_Init(HC_TRIG_PORT, &gpio);
    HAL_GPIO_WritePin(HC_TRIG_PORT, HC_TRIG_PIN, GPIO_PIN_RESET);

    /* ---- ECHO (PB3) : 复用功能输入 (TIM2_CH2, AF1) ---- */
    /* 输入捕获必须将引脚置于 AF 模式，否则定时器看不到外部电平。
       PB3 默认是 JTDO/SWO；本工程调试用 SWD(PA13/PA14)，PB3 可自由复用。 */
    gpio.Pin       = HC_ECHO_PIN;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(HC_ECHO_PORT, &gpio);

    /* ---- TIM2_CH2 输入捕获配置 ---- */
    HC_ConfigCapture();

    /* 使能 TIM2_CH2 捕获中断 (CC2IE)，用于非阻塞边缘检测 */
    TIM2->DIER |= TIM_DIER_CC2IE;
    HAL_NVIC_SetPriority(TIM2_IRQn, 15, 0);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);

    /* 使能 TIM2 更新中断 (UIE)，用于 32 位溢出计数器 */
    TIM2->SR &= ~TIM_SR_UIF;           /* 清除可能的悬挂标志 */
    TIM2->DIER |= TIM_DIER_UIE;        /* 使能更新中断 */
    s_tim2_ovf = 0;                    /* 清零溢出计数器 */

    /* 清零 */
    s_last_pulse_us = 0;
    for (int i = 0; i < HC_MEDIAN_N; i++) s_med_buf[i] = 0;  /* 复位中值滤波器 */
    s_med_idx = 0; s_med_cnt = 0; s_filt_pulse_us = 0;
    s_hc_state = HC_STATE_IDLE;
}

/*============================================================================*/

uint32_t HC_GetDistance_cm(void)
{
    uint32_t pulse = s_filt_pulse_us;   /* ★中值滤波后的脉宽(非单发原始值) */

    if (pulse == 0 || pulse > HC_MAX_VALID_US)
        return 0;   /* 超时 / 超出量程（CAN 侧映射为 HC_NO_TARGET_CM 哨兵） */

    /* 声速 343 m/s，往返折半：
     *   距离 (cm) = 声速 (cm/s) × 时间 (s) / 2
     *             = 34300 × (pulse_us / 1000000) / 2
     *             = pulse_us / 58.3
     *   整数运算： (pulse + 29) / 58 实现四舍五入 */
    return (pulse + 29) / 58;
}

/*============================================================================*/

uint32_t HC_GetDistance_mm(void)
{
    uint32_t pulse = s_filt_pulse_us;   /* ★中值滤波后的脉宽 */

    if (pulse == 0 || pulse > HC_MAX_VALID_US)
        return 0;

    /* 距离 (mm) = pulse_us / 5.83 ≈ pulse_us × 10 / 58 */
    return (pulse * 10 + 29) / 58;
}

/*============================================================================*/

uint32_t HC_GetLastPulse_us(void)
{
    return s_last_pulse_us;
}

/*============================================================================*/
/*              非阻塞状态机 API (中断驱动)                                    */
/*============================================================================*/

/**
 * @brief  TIM2_CH2 捕获中断处理 (由 stm32f4xx_it.c TIM2_IRQHandler 调用)
 *
 *  在中断上下文中处理上升/下降沿捕获，不受应用层阻塞影响。
 *  这样即使 Servo2_Set() 阻塞 20ms，也不会错过 ECHO 边沿。
 */
void HC_TIM2_IRQHandler(void)
{
    /* 清除 CC2 中断标志 */
    TIM2->SR &= ~TIM_SR_CC2IF;

    switch (s_hc_state)
    {
    case HC_STATE_WAIT_RISE:
        /* 捕获到上升沿 → 保存 32 位时间戳，切换为下降沿捕获，并进入等待下降沿状态。
           ★ 修复(2026-07-07)：原代码漏了状态转换，导致永远停在 WAIT_RISE，
             下降沿被当成上升沿反复覆盖 s_rising_us，脉宽永远算不出 → HC_ProcessTick
             必然超时把 s_last_pulse_us 置 0（现象：pulse=0us 恒成立，测距无数据）。 */
        s_rising_us = HC_CaptureUs();       /* ★读 CCR2(硬件锁存)，非 CNT，去中断延迟抖动 */
        HC_SetPolarity(1);
        s_hc_tick_start = s_rising_us;      /* 下降沿超时窗口从上升沿起算 */
        s_hc_state = HC_STATE_WAIT_FALL;
        break;

    case HC_STATE_WAIT_FALL:
    {
        /* 捕获到下降沿 → 计算脉宽 (32 位差，无回绕问题) */
        uint32_t falling_us = HC_CaptureUs();   /* ★读 CCR2(硬件锁存)，非 CNT */
        s_last_pulse_us = falling_us - s_rising_us;

        /* 恢复上升沿捕获 */
        HC_SetPolarity(0);
        s_hc_state = HC_STATE_DONE;
        break;
    }
    default:
        /* 未在等待状态时收到的边沿 → 忽略，恢复上升沿 */
        HC_SetPolarity(0);
        break;
    }
}

/*============================================================================*/

void HC_StartMeasurement(void)
{
    if (s_hc_state != HC_STATE_IDLE)
        return;  /* 上一次测量未完成，忽略 */

    /* TRIG = 1 (高电平) */
    HAL_GPIO_WritePin(HC_TRIG_PORT, HC_TRIG_PIN, GPIO_PIN_SET);
    s_hc_tick_start = HC_GetUs();
    s_hc_state = HC_STATE_TRIG_HIGH;
}

/*============================================================================*/

uint8_t HC_IsMeasuring(void)
{
    return (s_hc_state != HC_STATE_IDLE) ? 1 : 0;
}

/*============================================================================*/

uint8_t HC_ProcessTick(void)
{
    uint32_t now_us = HC_GetUs();
    uint32_t elapsed;

    switch (s_hc_state)
    {
    case HC_STATE_IDLE:
        return 0;

    case HC_STATE_TRIG_HIGH:
        /* TRIG 高电平持续 10us 后拉低 */
        elapsed = now_us - s_hc_tick_start;
        if (elapsed >= 10)
        {
            HAL_GPIO_WritePin(HC_TRIG_PORT, HC_TRIG_PIN, GPIO_PIN_RESET);
            s_hc_tick_start = now_us;
            s_hc_state = HC_STATE_TRIG_LOW;
        }
        return 0;

    case HC_STATE_TRIG_LOW:
        /* 等待 50us 后使能捕获中断，开始等待 ECHO */
        elapsed = now_us - s_hc_tick_start;
        if (elapsed >= 50)
        {
            /* 配置上升沿捕获 + 清标志 */
            HC_SetPolarity(0);
            TIM2->SR &= ~TIM_SR_CC2IF;
            s_hc_tick_start = now_us;
            s_hc_state = HC_STATE_WAIT_RISE;
        }
        return 0;

    case HC_STATE_WAIT_RISE:
        /* 等待 ECHO 上升沿 (由中断捕获)，超时判定 */
        elapsed = now_us - s_hc_tick_start;
        if (elapsed >= HC_TIMEOUT_RISE_US)
        {
            s_last_pulse_us = 0;
            s_hc_state = HC_STATE_DONE;
            return 1;
        }
        return 0;

    case HC_STATE_WAIT_FALL:
        /* 等待 ECHO 下降沿 (由中断捕获)，超时判定 */
        elapsed = now_us - s_hc_tick_start;
        if (elapsed >= HC_TIMEOUT_FALL_US)
        {
            s_last_pulse_us = 0;
            HC_SetPolarity(0);  /* 恢复上升沿 */
            s_hc_state = HC_STATE_DONE;
            return 1;
        }
        return 0;

    case HC_STATE_DONE:
        HC_PushSample(s_last_pulse_us);   /* ★本次原始脉宽入中值滤波(含超时的 0) */
        s_hc_state = HC_STATE_IDLE;
        return 1;

    default:
        s_hc_state = HC_STATE_IDLE;
        return 1;
    }
}
