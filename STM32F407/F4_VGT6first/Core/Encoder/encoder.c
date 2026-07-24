#include "encoder.h"
#include "main.h"     /* ENC1_DIR_Pin/Port, ENC2_DIR_Pin/Port（CubeMX 生成） */

/*============================================================================
 *  编码器驱动实现 —— 龙邱 Mini512Z (SD512)「步进脉冲 + 方向」型
 *
 *  ★ 2026-07-21 重大修正：本编码器【不是】AB 相正交输出！
 *    手册《Mini编码器.pdf》§4.2 明确：
 *      PIN3 = LSB  步进脉冲输出 (CMOS)
 *      PIN4 = Dir  旋转方向输出 (CMOS)
 *    而 256/1024 线型才是 AB 相正交。
 *
 *  原先把 TIM3/TIM4 配成 Encoder Mode TI12（正交解码）是错的：
 *    正交模式在 TI1 的上升沿和下降沿【都】计数且方向相反，
 *    当 TI2(方向脚) 电平恒定时，一个完整脉冲净计数 = (+1) + (-1) = 0，
 *    于是无论轮子转多快，CNT 都只在原地 ±1 抖动
 *    —— 这正是台架实测 measL/measR 恒为 -1/0/+1 的根因。
 *
 *  现方案：TIM3/TIM4 = External Clock Mode 1 (TI1FP1, 上升沿)
 *    · TIMx->CNT 由 LSB 脉冲驱动，【只增不减】（无符号）
 *    · 方向由 ENCx_DIR 引脚电平决定，软件合成带符号增量
 *
 *  接线（2026-07-21 实测确认，注意左右与变量名相反，见下）：
 *    编码器1 = TIM3：LSB->PA6(TIM3_CH1)  Dir->PB5 (ENC1_DIR)   —— 实际装在【右】轮
 *    编码器2 = TIM4：LSB->PD12(TIM4_CH1) Dir->PD13(ENC2_DIR)   —— 实际装在【左】轮
 *============================================================================*/

/* ---- 方向极性：DIR 引脚为该电平时判定为「正转」(增量取正) ----
   ★ 2026-07-21 台架实测标定（PWM=+400 单轮开环）：
       Motor1 正转 -> DIR1 = 1  => ENC1 取 GPIO_PIN_SET   为正转
       Motor2 正转 -> DIR2 = 0  => ENC2 取 GPIO_PIN_RESET 为正转
     两者相反是因为左右电机/编码器镜像安装，属正常。
     统一后：给正 PWM 时 dL、dR 均为正，满足闭环 PID「target 与 measured 同号」的前提。
   ★ 只改这里，不要去 PID 或电机代码里加负号。 */
#define ENC1_DIR_FORWARD_LEVEL   GPIO_PIN_SET
#define ENC2_DIR_FORWARD_LEVEL   GPIO_PIN_RESET

/* 单次采样增量上限保护
   实测(2026-07-21)：开环 PWM=1000 满速 = 1098 counts/17ms，即物理上限约 1100。
   取 1500 作阈值（1.36 倍余量）。超过必属异常——实测曾见某拍 measL 突跳到
   2025/2963，是控制周期被拉长后累积了多拍增量所致；若原样喂给 PID，会被误判
   为「速度暴增」而输出 -1000 急刹，造成电机顿挫。
   处理：超限时【保持上一拍增量】而非置 0（置 0 会被误判为「堵转」同样有害）。 */
#define ENC_DELTA_SANE_MAX       1500u

/* 上一次采样的硬件计数器值（用于计算增量） */
static uint16_t s_prev_cnt1 = 0;
static uint16_t s_prev_cnt2 = 0;

/* 有符号累计脉冲数 */
static volatile int32_t s_pos1 = 0;
static volatile int32_t s_pos2 = 0;

/* 上一次采样区间的脉冲增量（有符号） */
static volatile int16_t s_delta1 = 0;
static volatile int16_t s_delta2 = 0;

/*============================================================================*/

/**
 * @brief  初始化编码器
 *         - 启动 TIM3 / TIM4 计数器（外部时钟模式，由 LSB 脉冲驱动）
 *         - 记录初始 CNT 值
 *         - 清零累计位置
 *
 *  注意：外部时钟模式用 HAL_TIM_Base_Start（不是 HAL_TIM_Encoder_Start）。
 *  计数时钟来自 TI1FP1(LSB 脚)，CEN 置位后每个上升沿 CNT++。
 */
void Encoder_Init(void)
{
    /* 启动计数器；时钟源为外部 LSB 脉冲（CubeMX: SlaveMode=External Clock Mode 1） */
    HAL_TIM_Base_Start(&htim3);
    HAL_TIM_Base_Start(&htim4);

    /* 记录初始计数值 */
    s_prev_cnt1 = TIM3->CNT;
    s_prev_cnt2 = TIM4->CNT;

    /* 清零位置 */
    s_pos1 = 0;
    s_pos2 = 0;
    s_delta1 = 0;
    s_delta2 = 0;
}

/**
 * @brief  更新编码器计数值（脉冲+方向解码）
 *
 *  外部时钟模式下 TIMx->CNT 【只增不减】，方向不在计数里，必须另读 DIR 脚：
 *    1) raw = (uint16_t)(cur - prev)  —— 无符号增量，天然处理 16-bit 回绕
 *    2) 读 ENCx_DIR 电平决定符号
 *    3) delta = ±raw
 *  由调用者（MotionControlTask）每 17ms 调用一次。
 */
void Encoder_Update(void)
{
    /* ---- 编码器1 (TIM3, LSB=PA6 / DIR=PB5) ---- */
    {
        uint16_t cur = TIM3->CNT;
        uint16_t raw = (uint16_t)(cur - s_prev_cnt1);   /* 无符号增量(只增)，自动处理回绕 */
        s_prev_cnt1  = cur;

        int16_t delta;
        if (raw > ENC_DELTA_SANE_MAX) {
            delta = s_delta1;                           /* 采样异常：保持上一拍，避免 PID 误判急刹 */
        } else {
            delta = (HAL_GPIO_ReadPin(ENC1_DIR_GPIO_Port, ENC1_DIR_Pin)
                     == ENC1_DIR_FORWARD_LEVEL)
                    ?  (int16_t)raw
                    : -(int16_t)raw;
        }

        s_pos1  += delta;
        s_delta1 = delta;
    }

    /* ---- 编码器2 (TIM4, LSB=PD12 / DIR=PD13) ---- */
    {
        uint16_t cur = TIM4->CNT;
        uint16_t raw = (uint16_t)(cur - s_prev_cnt2);
        s_prev_cnt2  = cur;

        int16_t delta;
        if (raw > ENC_DELTA_SANE_MAX) {
            delta = s_delta2;                           /* 采样异常：保持上一拍 */
        } else {
            delta = (HAL_GPIO_ReadPin(ENC2_DIR_GPIO_Port, ENC2_DIR_Pin)
                     == ENC2_DIR_FORWARD_LEVEL)
                    ?  (int16_t)raw
                    : -(int16_t)raw;
        }

        s_pos2  += delta;
        s_delta2 = delta;
    }
}

/**
 * @brief  获取编码器1 累计有符号脉冲数
 */
int32_t Encoder1_GetCount(void)
{
    return s_pos1;
}

/**
 * @brief  获取编码器2 累计有符号脉冲数
 */
int32_t Encoder2_GetCount(void)
{
    return s_pos2;
}

/**
 * @brief  获取编码器1 上一次采样区间的脉冲增量
 */
int16_t Encoder1_GetDelta(void)
{
    return s_delta1;
}

/**
 * @brief  获取编码器2 上一次采样区间的脉冲增量
 */
int16_t Encoder2_GetDelta(void)
{
    return s_delta2;
}

/**
 * @brief  复位两个编码器的累计计数值
 */
void Encoder_Reset(void)
{
    s_pos1 = 0;
    s_pos2 = 0;
    s_delta1 = 0;
    s_delta2 = 0;
    s_prev_cnt1 = TIM3->CNT;
    s_prev_cnt2 = TIM4->CNT;
}
