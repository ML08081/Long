/**
 * thermal.c — MLX90640-D55 热成像高层实现
 *
 * 温度计算全部走 Melexis 官方 API（MLX90640_API.c），本文件只做编排。
 * 大数组均为文件级静态，避免占用任务栈。
 */
#include "thermal.h"
#include "MLX90640_API.h"
#include "MLX90640_I2C_Driver.h"


#define MLX_ADDR        0x33     /* MLX90640 默认 7-bit I2C 地址 */
#define MLX_EMISSIVITY  0.95f    /* 发射率（一般物体 ~0.95） */
#define MLX_REFRESH     0x02     /* 刷新率码：0x02 = 2Hz（100kHz I2C 下稳妥） */

static paramsMLX90640 s_params;
static uint16_t s_ee[832];
static uint16_t s_frame[834];
static float    s_to[THERMAL_PIXELS];
static int16_t  s_buf[THERMAL_PIXELS];
static uint8_t  s_ready = 0;
static float    s_ta    = 0.0f;

int Thermal_Init(void)
{
    MLX90640_I2CInit();
    MLX90640_SetRefreshRate(MLX_ADDR, MLX_REFRESH);
    MLX90640_SetChessMode(MLX_ADDR);
    if (MLX90640_DumpEE(MLX_ADDR, s_ee) != 0)              return -1;
    if (MLX90640_ExtractParameters(s_ee, &s_params) != 0)  return -2;
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
const uint16_t* Thermal_GetEE(void)       { return s_ee; }     /* 832 字, Thermal_Init 已 DumpEE */
const uint16_t* Thermal_GetRawFrame(void) { return s_frame; }  /* 834 字, 最近一个子页原始数据 */

int Thermal_ReadRawSubpage(void)
{
    /* 只读一个子页的原始数据(834字)，不做温度解算——省下 M4 的 CalculateTo 开销 */
    if (MLX90640_GetFrameData(MLX_ADDR, s_frame) < 0) return -1;
    return 0;
}
