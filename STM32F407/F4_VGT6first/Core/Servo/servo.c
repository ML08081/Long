#include "servo.h"
#include "tim.h"

/*============================================================================
 * 线性化原理
 *
 * 实际测量典型舵机转向机构后得到以下观察：
 *   角度    脉宽    车轮转角
 *   -90°    1300    右极限
 *   -45°    1405    ~右45°
 *     0°    1500    直行
 *   +45°    1595    ~左45°
 *   +90°    1711    左极限
 *
 * 如果做简单的线性映射（1711-1300)/180 = 2.28us/°，
 * 中心区域实际车轮转角会偏大，边缘区域偏小。
 *
 * 补偿策略：二次凸曲线修正
 *   在线性映射结果上叠加 compensation = K × (1 - (angle/90)²)
 *   中心点 (0°) 修正量最大 (-K)，边缘 (±90°) 修正量为 0
 *   修正量 K 通过 COMPENSATION_K 调整
 *============================================================================*/

/*---------- 校准参数 ----------*/
static uint16_t s_pulse_mid   = SERVO_PULSE_MID;
static uint16_t s_pulse_left  = SERVO_PULSE_LEFT_MAX;
static uint16_t s_pulse_right = SERVO_PULSE_RIGHT_MIN;

/*---------- 平滑过渡状态 ----------*/
static volatile uint16_t s_cur_pulse = SERVO_PULSE_MID;   // 当前 PWM 输出值
static volatile uint16_t s_tar_pulse = SERVO_PULSE_MID;   // 目标值
static volatile uint16_t s_ramp_step = SERVO_RAMP_DEFAULT_STEP;

/*---------- 线性化补偿强度 ----------*/
// K 值越大，中心区域修正越强（"转向手感"越平缓）
// 范围 15~30，默认 22（约为脉宽范围 411us 的 5.3%）
#define COMPENSATION_K  22

/*============================================================================*/

void Servo_Init(void)
{
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, s_pulse_mid);
    s_cur_pulse = s_pulse_mid;
    s_tar_pulse = s_pulse_mid;
}

/*============================================================================*/

void Servo_SetPulse(uint16_t pulse_us)
{
    /* 限幅 */
    if (pulse_us < s_pulse_right) pulse_us = s_pulse_right;
    if (pulse_us > s_pulse_left)  pulse_us = s_pulse_left;

    /* 立即输出 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse_us);
    s_cur_pulse = pulse_us;
    s_tar_pulse = pulse_us;
}

/*============================================================================*/

void Servo_SetSmoothPulse(uint16_t target_pulse, uint16_t ramp_step)
{
    /* 限幅 */
    if (target_pulse < s_pulse_right) target_pulse = s_pulse_right;
    if (target_pulse > s_pulse_left)  target_pulse = s_pulse_left;

    s_tar_pulse = target_pulse;
    if (ramp_step > 0) s_ramp_step = ramp_step;
}

/*============================================================================*/

uint8_t Servo_RampTick(void)
{
    if (s_cur_pulse == s_tar_pulse)
        return 0;   /* 已到达 */

    int32_t  diff  = (int32_t)s_tar_pulse - (int32_t)s_cur_pulse;
    uint32_t adiff = (diff > 0) ? (uint32_t)diff : (uint32_t)(-diff);

    /* ★ 比例步长（详见 servo.h 的改造说明）：
     *   step = |误差| / SERVO_RAMP_DIV，再夹到 [MIN_STEP, s_ramp_step]。
     *   误差大 -> 走上限（快速接近）；误差小 -> 步长自动变小（平顺减速到位）。
     *   s_ramp_step 由调用方给定，语义已从"固定步长"变为"步长上限"。 */
    uint32_t step = adiff / SERVO_RAMP_DIV;
    if (step < SERVO_RAMP_MIN_STEP) step = SERVO_RAMP_MIN_STEP;
    if (step > s_ramp_step)         step = s_ramp_step;

    if (diff > 0)
    {
        /* 向左转（增大脉宽） */
        if (adiff <= step)
            s_cur_pulse = s_tar_pulse;
        else
            s_cur_pulse += (uint16_t)step;
    }
    else
    {
        /* 向右转（减小脉宽） */
        if (adiff <= step)
            s_cur_pulse = s_tar_pulse;
        else
            s_cur_pulse -= (uint16_t)step;
    }

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, s_cur_pulse);
    return (s_cur_pulse != s_tar_pulse) ? 1 : 0;
}

/*============================================================================*/

uint16_t Servo_AngleToPulse(int16_t angle_deg)
{
    /* 限幅 */
    if (angle_deg < SERVO_ANGLE_MIN) angle_deg = SERVO_ANGLE_MIN;
    if (angle_deg > SERVO_ANGLE_MAX) angle_deg = SERVO_ANGLE_MAX;

    /*---- 第一步：纯线性映射 ----*/
    int32_t range_pulse = (int32_t)s_pulse_left - (int32_t)s_pulse_right;   // 411
    int32_t range_angle = (int32_t)SERVO_ANGLE_MAX - (int32_t)SERVO_ANGLE_MIN; // 180
    int32_t offset_angle = (int32_t)angle_deg - (int32_t)SERVO_ANGLE_MIN;   // 0~180

    int32_t linear = (int32_t)s_pulse_right
                   + (offset_angle * range_pulse) / range_angle;

    /*---- 第二步：非线性补偿修正 ----*/
    /*
     *  补偿公式：
     *    final = linear - K × (1 - (angle_deg / 90°)²)
     *
     *  推导：
     *    令 t = |angle_deg| / 90  (0~1)
     *    correction = K × (1 - t²)
     *    中心点 (t=0)  correction = K    → 减去最大修正量
     *    边缘   (t=1)  correction = 0    → 不修正
     *
     *  正负号说明：
     *    右侧 (angle_deg < 0)：修正量为正值 → 脉宽增加（抵消机械放大）
     *    左侧 (angle_deg > 0)：修正量为负值 → 脉宽减小（对称处理）
     *   实际上修正量方向与角度方向相同。
     */

    /* 计算 |angle_deg| / 90 的平方，使用 Q16 定点数避免浮点 */
    int32_t abs_angle = (angle_deg < 0) ? -angle_deg : angle_deg;
    /* abs_angle² / 90²  × 65536 */
    int32_t t_sq_q16 = ((int32_t)abs_angle * (int32_t)abs_angle * 65536)
                      / ((int32_t)90 * 90);

    /* correction = K × (1 - t²)，Q16 运算 */
    int32_t correction = (COMPENSATION_K * (65536 - t_sq_q16)) / 65536;

    /*
     * 修正方向：
     *   右半侧 (angle < 0)：correction 为正 → 向右修正自然为增加脉宽
     *   左半侧 (angle > 0)：correction 为正 → 向左修正自然为减小脉宽
     *
     *   但我们的补偿是要"中心区域少走，边缘多走"，所以：
     *   右半侧：线性结果偏大 → 减去修正量
     *   左半侧：线性结果偏小 → 加上修正量
     *
     *   更直观的理解：补偿曲线是上凸的（∩形），使得靠近中位时变化平缓
     */
    int32_t result;
    if (angle_deg < 0)
        result = linear - correction;   /* 右半侧：减修正量 */
    else
        result = linear + correction;   /* 左半侧：加修正量 */

    /* 最终限幅 */
    if (result < (int32_t)s_pulse_right) result = s_pulse_right;
    if (result > (int32_t)s_pulse_left)  result = s_pulse_left;

    return (uint16_t)result;
}

/*============================================================================*/

int16_t Servo_PulseToAngle(uint16_t pulse_us)
{
    /* 限幅 */
    if (pulse_us < s_pulse_right) pulse_us = s_pulse_right;
    if (pulse_us > s_pulse_left)  pulse_us = s_pulse_left;

    /* 逆线性映射（不尝试逆补偿，直接反映射） */
    int32_t range_pulse  = (int32_t)s_pulse_left - (int32_t)s_pulse_right;
    int32_t offset_pulse = (int32_t)pulse_us - (int32_t)s_pulse_right;
    int32_t range_angle  = (int32_t)SERVO_ANGLE_MAX - (int32_t)SERVO_ANGLE_MIN;

    int32_t angle = (int32_t)SERVO_ANGLE_MIN
                  + (offset_pulse * range_angle) / range_pulse;

    return (int16_t)angle;
}

/*============================================================================*/

uint8_t Servo_SelfCheck(uint16_t step_us, uint16_t delay_ms)
{
    uint16_t pulse;

    /* 0. 如果当前不在中位，先回中 */
    if (s_cur_pulse != s_pulse_mid)
    {
        pulse = s_cur_pulse;
        while (pulse != s_pulse_mid)
        {
            if (pulse < s_pulse_mid)
            {
                pulse += step_us;
                if (pulse > s_pulse_mid) pulse = s_pulse_mid;
            }
            else
            {
                if (pulse <= step_us) pulse = 0;
                else pulse -= step_us;
                if (pulse < s_pulse_mid) pulse = s_pulse_mid;
            }
            Servo_SetPulse(pulse);
            HAL_Delay(delay_ms);
        }
        HAL_Delay(delay_ms * 3);
    }

    /* 1. 从中位 → 左极限 (SERVO_ANGLE_MAX, +90°, pulse=s_pulse_left) */
    pulse = s_cur_pulse;
    while (pulse < s_pulse_left)
    {
        pulse += step_us;
        if (pulse > s_pulse_left) pulse = s_pulse_left;
        Servo_SetPulse(pulse);
        HAL_Delay(delay_ms);
    }
    HAL_Delay(delay_ms * 5);   // 极限位置多停留一会

    /* 2. 从左极限 → 右极限 (SERVO_ANGLE_MIN, -90°, pulse=s_pulse_right) */
    while (pulse > s_pulse_right)
    {
        if (pulse <= step_us) pulse = 0;
        else pulse -= step_us;
        if (pulse < s_pulse_right) pulse = s_pulse_right;
        Servo_SetPulse(pulse);
        HAL_Delay(delay_ms);
    }
    HAL_Delay(delay_ms * 5);

    /* 3. 从右极限 → 回中 (0°, pulse=s_pulse_mid) */
    while (pulse < s_pulse_mid)
    {
        pulse += step_us;
        if (pulse > s_pulse_mid) pulse = s_pulse_mid;
        Servo_SetPulse(pulse);
        HAL_Delay(delay_ms);
    }

    s_tar_pulse = s_pulse_mid;   // 同步目标值，防止 RampTick 下次误动

    return 1;
}

/*============================================================================*/

void Servo_SetAngle(int16_t angle_deg)
{
    Servo_SetPulse(Servo_AngleToPulse(angle_deg));
}

void Servo_SetSmoothAngle(int16_t target_angle, uint16_t ramp_step)
{
    Servo_SetSmoothPulse(Servo_AngleToPulse(target_angle), ramp_step);
}

/*============================================================================*/

void Servo_Calibrate(uint16_t pulse_mid, uint16_t pulse_left, uint16_t pulse_right)
{
    s_pulse_mid   = pulse_mid;
    s_pulse_left  = pulse_left;
    s_pulse_right = pulse_right;

    /* 校准后自动限制当前值在新范围内 */
    if (s_cur_pulse > s_pulse_left)  s_cur_pulse = s_pulse_left;
    if (s_cur_pulse < s_pulse_right) s_cur_pulse = s_pulse_right;
    if (s_tar_pulse > s_pulse_left)  s_tar_pulse = s_pulse_left;
    if (s_tar_pulse < s_pulse_right) s_tar_pulse = s_pulse_right;
}

/*============================================================================*/
/* 查询接口                                                                    */
/*============================================================================*/

uint16_t Servo_GetCurrentPulse(void) { return s_cur_pulse; }
uint16_t Servo_GetTargetPulse(void)  { return s_tar_pulse;  }
int16_t  Servo_GetCurrentAngle(void) { return Servo_PulseToAngle(s_cur_pulse); }
