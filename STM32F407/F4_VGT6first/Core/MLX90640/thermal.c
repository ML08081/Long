/**
 * thermal.c — MLX90640-D55 热成像高层实现
 *
 * 温度计算全部走 Melexis 官方 API（MLX90640_API.c），本文件只做编排。
 * 大数组均为文件级静态，避免占用任务栈。
 */
#include "thermal.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"
#include "stm32f4xx_hal.h"
#include <string.h>


#define MLX_ADDR        0x33     /* MLX90640 默认 7-bit I2C 地址 */
#define MLX_EMISSIVITY  0.95f    /* 发射率（一般物体 ~0.95） */
#define MLX_REFRESH     0x02     /* 刷新率码：0x02 = 2Hz（100kHz I2C 下稳妥） */

static paramsMLX90640 s_params;
static uint16_t s_ee[832];
/* 重初始化期间先写候选区。MLX 供电/I2C 不稳时 DumpEE 可能成功返回但内容
   已损坏；只有参数提取和传感器配置全部成功后才覆盖当前有效标定。 */
static paramsMLX90640 s_params_candidate;
static uint16_t s_ee_candidate[832];
static uint16_t s_frame[834];
static float    s_to[THERMAL_PIXELS];
static int16_t  s_buf[THERMAL_PIXELS];
static uint8_t  s_ready = 0;
static uint8_t  s_calibration_valid = 0;
static uint32_t s_ee_expected_hash = 0;
static uint32_t s_ee_recover_count = 0;
static float    s_ta    = 0.0f;

static uint32_t Thermal_CalcEEFingerprint(const uint16_t *ee)
{
    uint32_t hash = 2166136261UL;
    for (int i = 0; i < 832; i++) {
        hash = (hash ^ (uint8_t)(ee[i] >> 8)) * 16777619UL;
        hash = (hash ^ (uint8_t)ee[i]) * 16777619UL;
    }
    return hash;
}

int Thermal_Init(void)
{
    MLX90640_I2CInit();
    /* 总线已释放但 MLX 内部状态机未恢复时，General Call Reset 比单纯重配
       STM32 I2C 外设更有效。I2C3 由 MLX 独占，不会影响其它传感器。 */
    if (MLX90640_I2CGeneralReset() != 0) return -1;
    HAL_Delay(20);

    /* EEPROM 是传感器出厂只读标定。一旦本次 F4 上电已经取得一份有效副本，
       后续 I2C 恢复只重配工作寄存器，绝不让瞬态坏读覆盖有效标定。 */
    if (!s_calibration_valid) {
        if (MLX90640_DumpEE(MLX_ADDR, s_ee_candidate) != 0) return -2;
        if (MLX90640_ExtractParameters(s_ee_candidate, &s_params_candidate) != 0)
            return -3;
    }
    if (MLX90640_SetRefreshRate(MLX_ADDR, MLX_REFRESH) != 0) return -4;
    if (MLX90640_SetChessMode(MLX_ADDR) != 0)              return -5;

    if (!s_calibration_valid) {
        /* 参数结构体体积较大，先完成它的拷贝，再把 EEPROM 作为
           最后一步提交，避免任何异常写入影响已提交的标定副本。 */
        s_params = s_params_candidate;
        memcpy(s_ee, s_ee_candidate, sizeof(s_ee));
        s_ee_expected_hash = Thermal_CalcEEFingerprint(s_ee_candidate);
        s_calibration_valid = 1;
    }
    s_ready = 1;
    return 0;
}

int Thermal_ReadFrame(void)
{
    /* 一整幅图像需要读两个子页 */
    for (int sub = 0; sub < 2; sub++) {
        if (MLX90640_GetFrameData(MLX_ADDR, s_frame) < 0) return -1;
        s_ta = MLX90640_GetTa(s_frame, &s_params);
        float tr = s_ta - 8.0f;   /* 反射温度经验取 Ta-8°C */
        MLX90640_CalculateTo(s_frame, &s_params, MLX_EMISSIVITY, tr, s_to);
    }

    /* float °C -> int16 0.01°C（限幅） */
    for (int i = 0; i < THERMAL_PIXELS; i++) {
        float v = s_to[i] * 100.0f;
        if (v >  32767.0f) v =  32767.0f;
        if (v < -32768.0f) v = -32768.0f;
        s_buf[i] = (int16_t)v;
    }
    s_ready = 1;
    return 0;
}

const int16_t* Thermal_GetBuffer(void) { return s_buf; }
uint8_t        Thermal_IsReady(void)   { return s_ready; }
float          Thermal_GetTa(void)     { return s_ta; }

/* ---- 原始帧转发模式（解算下放龙芯）：F4 只读原始数据，不再 CalculateTo ---- */
const uint16_t* Thermal_GetEE(void)
{
    /* EEPROM 是出厂只读数据。如果有效副本在运行中指纹变化，说明发生了
       内存越界/干扰；只从仍匹配启动指纹的候选副本恢复，不会盲目传输。 */
    if (s_calibration_valid && Thermal_CalcEEFingerprint(s_ee) != s_ee_expected_hash &&
        Thermal_CalcEEFingerprint(s_ee_candidate) == s_ee_expected_hash) {
        memcpy(s_ee, s_ee_candidate, sizeof(s_ee));
        s_ee_recover_count++;
    }
    return s_ee;
}
uint32_t Thermal_GetEEFingerprint(void)
{
    return Thermal_CalcEEFingerprint(Thermal_GetEE());
}
uint32_t Thermal_GetEERecoverCount(void) { return s_ee_recover_count; }
const uint16_t* Thermal_GetRawFrame(void) { return s_frame; }  /* 834 字, 最近一个子页原始数据 */

int Thermal_ReadRawSubpage(void)
{
    /* 只读一个子页的原始数据(834字)，不做温度解算——省下 M4 的 CalculateTo 开销 */
    if (MLX90640_GetFrameData(MLX_ADDR, s_frame) < 0) return -1;
    return 0;
}
