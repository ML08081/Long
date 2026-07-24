/**
 * vl53l0x.c — VL53L0X 单次测距精简驱动（hi2c2）
 *
 * 移植自 ST VL53L0X API 的核心流程（等价于社区常用的 Pololu 精简实现）：
 *   DataInit(2v8) → 读 SPAD 信息 → 加载默认调优表 → 配置中断 →
 *   设置测量时间预算 → VHV/相位参考校正 → 单次测距读取。
 *
 * 仅依赖 hi2c2；超时/无回波返回 VL53_OUT_OF_RANGE。
 */
#include "vl53l0x.h"
#include "i2c.h"    // hi2c2

/*========================= 寄存器定义 =========================*/
#define REG_SYSRANGE_START                          0x00
#define REG_SYSTEM_SEQUENCE_CONFIG                  0x01
#define REG_SYSTEM_INTERMEASUREMENT_PERIOD          0x04
#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO            0x0A
#define REG_GPIO_HV_MUX_ACTIVE_HIGH                 0x84
#define REG_SYSTEM_INTERRUPT_CLEAR                  0x0B
#define REG_RESULT_INTERRUPT_STATUS                 0x13
#define REG_RESULT_RANGE_STATUS                     0x14
#define REG_MSRC_CONFIG_CONTROL                     0x60
#define REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT 0x44
#define REG_PRE_RANGE_CONFIG_VCSEL_PERIOD           0x50
#define REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD         0x70
#define REG_SYSTEM_HISTOGRAM_BIN                    0x81
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET        0x4F
#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD     0x4E
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT       0xB6
#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0        0xB0
#define REG_MSRC_CONFIG_TIMEOUT_MACROP              0x46
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI    0x71
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_LO    0x72
#define REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI      0x51
#define REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_LO      0x52
#define REG_IDENTIFICATION_MODEL_ID                 0xC0

static uint8_t  s_present = 0;
static uint8_t  s_continuous = 0;
static uint8_t  s_stop_variable = 0;
static uint32_t s_meas_timing_budget_us = 0;

/*========================= I2C 基础读写 =========================*/
static HAL_StatusTypeDef wr8(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, buf, 2, 10);
}
static uint8_t rd8(uint8_t reg)
{
    uint8_t v = 0;
    HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, &reg, 1, 10);
    HAL_I2C_Master_Receive(&hi2c2, VL53_I2C_ADDR_8BIT, &v, 1, 10);
    return v;
}
static HAL_StatusTypeDef wr16(uint8_t reg, uint16_t val)
{
    uint8_t buf[3] = { reg, (uint8_t)(val >> 8), (uint8_t)(val & 0xFF) };
    return HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, buf, 3, 10);
}
static uint16_t rd16(uint8_t reg)
{
    uint8_t v[2] = {0};
    HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, &reg, 1, 10);
    HAL_I2C_Master_Receive(&hi2c2, VL53_I2C_ADDR_8BIT, v, 2, 10);
    return (uint16_t)((v[0] << 8) | v[1]);
}
static HAL_StatusTypeDef wr_multi(uint8_t reg, const uint8_t *src, uint8_t n)
{
    uint8_t buf[8];
    if (n > 7) n = 7;
    buf[0] = reg;
    for (uint8_t i = 0; i < n; i++) buf[1 + i] = src[i];
    return HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, buf, (uint16_t)(n + 1), 20);
}
static void rd_multi(uint8_t reg, uint8_t *dst, uint8_t n)
{
    HAL_I2C_Master_Transmit(&hi2c2, VL53_I2C_ADDR_8BIT, &reg, 1, 10);
    HAL_I2C_Master_Receive(&hi2c2, VL53_I2C_ADDR_8BIT, dst, n, 20);
}

/*========================= 超时编解码宏 =========================*/
#define decodeVcselPeriod(x)   (((x) + 1) << 1)
#define calcMacroPeriod(vcsel) ((((uint32_t)2304 * (vcsel) * 1655) + 500) / 1000)

static uint16_t decodeTimeout(uint16_t val)
{
    return (uint16_t)((val & 0xFF) << (uint16_t)(val >> 8)) + 1;
}
static uint16_t encodeTimeout(uint32_t timeout_mclks)
{
    uint32_t ls = 0, ms = 0;
    if (timeout_mclks > 0) {
        ls = timeout_mclks - 1;
        while ((ls & 0xFFFFFF00) > 0) { ls >>= 1; ms++; }
        return (uint16_t)((ms << 8) | (ls & 0xFF));
    }
    return 0;
}
static uint32_t timeoutMclksToMicroseconds(uint16_t timeout_mclks, uint8_t vcsel_period_pclks)
{
    uint32_t macro_ns = calcMacroPeriod(vcsel_period_pclks);
    return ((timeout_mclks * macro_ns) + (macro_ns / 2)) / 1000;
}
static uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_us, uint8_t vcsel_period_pclks)
{
    uint32_t macro_ns = calcMacroPeriod(vcsel_period_pclks);
    return ((timeout_us * 1000) + (macro_ns / 2)) / macro_ns;
}

/*========================= 序列超时 =========================*/
typedef struct {
    uint8_t  tcc, msrc, dss, pre_range, final_range;
} SeqStepEnables;
typedef struct {
    uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;
    uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
    uint32_t msrc_dss_tcc_us, pre_range_us, final_range_us;
} SeqStepTimeouts;

static void getSequenceStepEnables(SeqStepEnables *e)
{
    uint8_t seq = rd8(REG_SYSTEM_SEQUENCE_CONFIG);
    e->tcc         = (seq >> 4) & 0x1;
    e->dss         = (seq >> 3) & 0x1;
    e->msrc        = (seq >> 2) & 0x1;
    e->pre_range   = (seq >> 6) & 0x1;
    e->final_range = (seq >> 7) & 0x1;
}
static uint8_t getVcselPulsePeriodPre(void)  { return decodeVcselPeriod(rd8(REG_PRE_RANGE_CONFIG_VCSEL_PERIOD)); }
static uint8_t getVcselPulsePeriodFinal(void){ return decodeVcselPeriod(rd8(REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD)); }

static void getSequenceStepTimeouts(const SeqStepEnables *e, SeqStepTimeouts *t)
{
    t->pre_range_vcsel_period_pclks = getVcselPulsePeriodPre();
    t->msrc_dss_tcc_mclks = rd8(REG_MSRC_CONFIG_TIMEOUT_MACROP) + 1;
    t->msrc_dss_tcc_us = timeoutMclksToMicroseconds(t->msrc_dss_tcc_mclks, t->pre_range_vcsel_period_pclks);

    t->pre_range_mclks = decodeTimeout(rd16(REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
    t->pre_range_us = timeoutMclksToMicroseconds(t->pre_range_mclks, t->pre_range_vcsel_period_pclks);

    t->final_range_vcsel_period_pclks = getVcselPulsePeriodFinal();
    t->final_range_mclks = decodeTimeout(rd16(REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));
    if (e->pre_range) t->final_range_mclks -= t->pre_range_mclks;
    t->final_range_us = timeoutMclksToMicroseconds(t->final_range_mclks, t->final_range_vcsel_period_pclks);
}

static uint32_t getMeasurementTimingBudget(void)
{
    SeqStepEnables e; SeqStepTimeouts t;
    uint32_t budget = 1910 + 960;  /* StartOverhead + EndOverhead */
    getSequenceStepEnables(&e);
    getSequenceStepTimeouts(&e, &t);
    if (e.tcc)  budget += t.msrc_dss_tcc_us + 590;
    if (e.dss)  budget += 2 * (t.msrc_dss_tcc_us + 690);
    else if (e.msrc) budget += t.msrc_dss_tcc_us + 660;
    if (e.pre_range)   budget += t.pre_range_us + 660;
    if (e.final_range) budget += t.final_range_us + 550;
    s_meas_timing_budget_us = budget;
    return budget;
}

static uint8_t setMeasurementTimingBudget(uint32_t budget_us)
{
    SeqStepEnables e; SeqStepTimeouts t;
    uint32_t used = 1320 + 960;
    const uint32_t MinBudget = 20000;
    if (budget_us < MinBudget) return 0;

    getSequenceStepEnables(&e);
    getSequenceStepTimeouts(&e, &t);
    if (e.tcc)  used += t.msrc_dss_tcc_us + 590;
    if (e.dss)  used += 2 * (t.msrc_dss_tcc_us + 690);
    else if (e.msrc) used += t.msrc_dss_tcc_us + 660;
    if (e.pre_range) used += t.pre_range_us + 660;

    if (e.final_range)
    {
        used += 550;
        if (used > budget_us) return 0;  /* 预算不足 */
        uint32_t final_range_timeout_us = budget_us - used;
        uint32_t final_range_timeout_mclks =
            timeoutMicrosecondsToMclks(final_range_timeout_us, t.final_range_vcsel_period_pclks);
        if (e.pre_range) final_range_timeout_mclks += t.pre_range_mclks;
        wr16(REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI, encodeTimeout(final_range_timeout_mclks));
        s_meas_timing_budget_us = budget_us;
    }
    return 1;
}

/*========================= SPAD 信息 =========================*/
static uint8_t getSpadInfo(uint8_t *count, uint8_t *type_is_aperture)
{
    uint8_t tmp;
    wr8(0x80, 0x01); wr8(0xFF, 0x01); wr8(0x00, 0x00);
    wr8(0xFF, 0x06);
    wr8(0x83, rd8(0x83) | 0x04);
    wr8(0xFF, 0x07); wr8(0x81, 0x01);
    wr8(0x80, 0x01);
    wr8(0x94, 0x6b); wr8(0x83, 0x00);

    uint32_t start = HAL_GetTick();
    while (rd8(0x83) == 0x00) { if (HAL_GetTick() - start > 50) return 0; }
    wr8(0x83, 0x01);
    tmp = rd8(0x92);
    *count = tmp & 0x7f;
    *type_is_aperture = (tmp >> 7) & 0x01;

    wr8(0x81, 0x00); wr8(0xFF, 0x06);
    wr8(0x83, rd8(0x83) & ~0x04);
    wr8(0xFF, 0x01); wr8(0x00, 0x01);
    wr8(0xFF, 0x00); wr8(0x80, 0x00);
    return 1;
}

/*========================= 参考校正 =========================*/
static uint8_t performSingleRefCalibration(uint8_t vhv_init_byte)
{
    wr8(REG_SYSRANGE_START, 0x01 | vhv_init_byte);
    uint32_t start = HAL_GetTick();
    while ((rd8(REG_RESULT_INTERRUPT_STATUS) & 0x07) == 0)
    {
        if (HAL_GetTick() - start > 100) return 0;
    }
    wr8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    wr8(REG_SYSRANGE_START, 0x00);
    return 1;
}

/*========================= 初始化 =========================*/
int VL53_Init(void)
{
    s_present = 0;

    /* ---- 上电：XSHUT 拉高，等待启动 ---- */
    HAL_GPIO_WritePin(VL53_XSHUT_GPIO_Port, VL53_XSHUT_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(VL53_XSHUT_GPIO_Port, VL53_XSHUT_Pin, GPIO_PIN_SET);
    HAL_Delay(5);

    /* 探测：型号 ID 应为 0xEE */
    if (rd8(REG_IDENTIFICATION_MODEL_ID) != 0xEE) return -1;

    /* ---- DataInit：2.8V 模式 + 标准 I2C ---- */
    wr8(0x88, 0x00);
    wr8(0x80, 0x01); wr8(0xFF, 0x01); wr8(0x00, 0x00);
    s_stop_variable = rd8(0x91);
    wr8(0x00, 0x01); wr8(0xFF, 0x00); wr8(0x80, 0x00);

    /* 关闭 SIGNAL_RATE_MSRC / SIGNAL_RATE_PRE_RANGE 限制检查 */
    wr8(REG_MSRC_CONFIG_CONTROL, rd8(REG_MSRC_CONFIG_CONTROL) | 0x12);
    /* 设置最终测距信号速率下限 0.25 MCPS（Q9.7: 0.25*(1<<7)=32） */
    wr16(REG_FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32);
    wr8(REG_SYSTEM_SEQUENCE_CONFIG, 0xFF);

    /* ---- StaticInit：SPAD 管理 ---- */
    uint8_t spad_count = 0, spad_type_is_aperture = 0;
    if (!getSpadInfo(&spad_count, &spad_type_is_aperture)) return -2;

    uint8_t ref_spad_map[6];
    rd_multi(REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    wr8(0xFF, 0x01);
    wr8(REG_DYNAMIC_SPAD_REF_EN_START_OFFSET, 0x00);
    wr8(REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD, 0x2C);
    wr8(0xFF, 0x00);
    wr8(REG_GLOBAL_CONFIG_REF_EN_START_SELECT, 0xB4);

    uint8_t first_spad = spad_type_is_aperture ? 12 : 0;
    uint8_t spads_enabled = 0;
    for (uint8_t i = 0; i < 48; i++)
    {
        if (i < first_spad || spads_enabled == spad_count)
            ref_spad_map[i / 8] &= ~(1 << (i % 8));
        else if ((ref_spad_map[i / 8] >> (i % 8)) & 0x1)
            spads_enabled++;
    }
    wr_multi(REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    /* ---- 默认调优设置表 ---- */
    static const uint8_t tuning[] = {
        0xFF,0x01, 0x00,0x00, 0xFF,0x00, 0x09,0x00, 0x10,0x00, 0x11,0x00,
        0x24,0x01, 0x25,0xFF, 0x75,0x00, 0xFF,0x01, 0x4E,0x2C, 0x48,0x00,
        0x30,0x20, 0xFF,0x00, 0x30,0x09, 0x54,0x00, 0x31,0x04, 0x32,0x03,
        0x40,0x83, 0x46,0x25, 0x60,0x00, 0x27,0x00, 0x50,0x06, 0x51,0x00,
        0x52,0x96, 0x56,0x08, 0x57,0x30, 0x61,0x00, 0x62,0x00, 0x64,0x00,
        0x65,0x00, 0x66,0xA0, 0xFF,0x01, 0x22,0x32, 0x47,0x14, 0x49,0xFF,
        0x4A,0x00, 0xFF,0x00, 0x7A,0x0A, 0x7B,0x00, 0x78,0x21, 0xFF,0x01,
        0x23,0x34, 0x42,0x00, 0x44,0xFF, 0x45,0x26, 0x46,0x05, 0x40,0x40,
        0x0E,0x06, 0x20,0x1A, 0x43,0x40, 0xFF,0x00, 0x34,0x03, 0x35,0x44,
        0xFF,0x01, 0x31,0x04, 0x4B,0x09, 0x4C,0x05, 0x4D,0x04, 0xFF,0x00,
        0x44,0x00, 0x45,0x20, 0x47,0x08, 0x48,0x28, 0x67,0x00, 0x70,0x04,
        0x71,0x01, 0x72,0xFE, 0x76,0x00, 0x77,0x00, 0xFF,0x01, 0x0D,0x01,
        0xFF,0x00, 0x80,0x01, 0x01,0xF8, 0xFF,0x01, 0x8E,0x01, 0x00,0x01,
        0xFF,0x00, 0x80,0x00
    };
    for (uint16_t i = 0; i < sizeof(tuning); i += 2) wr8(tuning[i], tuning[i + 1]);

    /* ---- 中断配置：新样本就绪，GPIO 低电平有效 ---- */
    wr8(REG_SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    wr8(REG_GPIO_HV_MUX_ACTIVE_HIGH, rd8(REG_GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10);
    wr8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);

    /* ---- 测量时间预算（默认约 33ms） ---- */
    (void)getMeasurementTimingBudget();
    wr8(REG_SYSTEM_SEQUENCE_CONFIG, 0xE8);
    setMeasurementTimingBudget(33000);

    /* ---- 参考校正：VHV + 相位 ---- */
    wr8(REG_SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (!performSingleRefCalibration(0x40)) return -3;
    wr8(REG_SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (!performSingleRefCalibration(0x00)) return -4;
    wr8(REG_SYSTEM_SEQUENCE_CONFIG, 0xE8);

    s_present = 1;

    /* 初始化完成后进入连续测距(back-to-back)模式 */
    VL53_StartContinuous();
    return 0;
}

uint8_t VL53_IsPresent(void) { return s_present; }

/* 总线诊断：把笼统的 init 失败拆成可定位的三态。
   返回 0=总线上无任何 ACK（器件缺失/接线错/未供电/XSHUT 拉低/地址不符）
        1=有 ACK 且 Model ID=0xEE（芯片正常，失败在后续 init 流程）
        2=有 ACK 但 Model ID 不对（地址撞了别的器件 / 芯片异常）
   raw_id 回填读到的 ID 字节，便于日志判读。 */
uint8_t VL53_BusProbe(uint8_t *raw_id)
{
    /* 先拉高 XSHUT 并等待启动——否则芯片处于关断态，探测必然无 ACK(假阴性)。
       这样 id 读数才可信：真无 ACK = 接线/供电/上拉问题，而非 XSHUT 时序。 */
    HAL_GPIO_WritePin(VL53_XSHUT_GPIO_Port, VL53_XSHUT_Pin, GPIO_PIN_SET);
    HAL_Delay(10);
    if (HAL_I2C_IsDeviceReady(&hi2c2, VL53_I2C_ADDR_8BIT, 3, 20) != HAL_OK) {
        if (raw_id) *raw_id = 0;
        return 0;
    }
    uint8_t id = rd8(REG_IDENTIFICATION_MODEL_ID);
    if (raw_id) *raw_id = id;
    return (id == 0xEE) ? 1u : 2u;
}

/*========================= 连续测距(back-to-back) =========================*/
void VL53_StartContinuous(void)
{
    if (!s_present) return;

    /* 恢复 stop_variable（与单次测距前置序列一致） */
    wr8(0x80, 0x01); wr8(0xFF, 0x01); wr8(0x00, 0x00);
    wr8(0x91, s_stop_variable);
    wr8(0x00, 0x01); wr8(0xFF, 0x00); wr8(0x80, 0x00);

    /* SYSRANGE_START = 0x02：连续背靠背模式（inter-measurement period=0）。
       传感器此后持续测量，读取端只需取最新结果并清中断，无需每次重启测量。 */
    wr8(REG_SYSRANGE_START, 0x02);
    s_continuous = 1;
}

/*========================= 读取测距结果 =========================*/
uint16_t VL53_ReadRange_mm(void)
{
    if (!s_present) return VL53_OUT_OF_RANGE;

    /* 若尚未进入连续模式（异常情况），补一次启动 */
    if (!s_continuous) VL53_StartContinuous();

    /* 等待数据就绪（连续模式下几乎总是已就绪，几乎不阻塞） */
    uint32_t start = HAL_GetTick();
    while ((rd8(REG_RESULT_INTERRUPT_STATUS) & 0x07) == 0)
    {
        if (HAL_GetTick() - start > 100) return VL53_OUT_OF_RANGE;
    }

    uint16_t range = rd16(REG_RESULT_RANGE_STATUS + 10);  /* 0x1E */
    wr8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);   /* 清中断，允许下一帧结果 */

    /* VL53L0X 无回波时返回 8190/8191，统一映射到 OUT_OF_RANGE */
    if (range >= 8190) return VL53_OUT_OF_RANGE;
    return range;
}

uint8_t VL53_TryReadRange_mm(uint16_t *out_mm)
{
    if (!s_present) return 0;
    if (!s_continuous) VL53_StartContinuous();

    /* 只查一次就绪位（单次 I2C 读 <1ms），未就绪立即返回——不忙等 */
    if ((rd8(REG_RESULT_INTERRUPT_STATUS) & 0x07) == 0)
        return 0;

    uint16_t range = rd16(REG_RESULT_RANGE_STATUS + 10);  /* 0x1E */
    wr8(REG_SYSTEM_INTERRUPT_CLEAR, 0x01);
    *out_mm = (range >= 8190) ? VL53_OUT_OF_RANGE : range;
    return 1;
}
