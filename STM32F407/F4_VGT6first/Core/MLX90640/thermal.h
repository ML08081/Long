/**
 * thermal.h — MLX90640-D55 热成像高层模块（32x24 红外阵列）
 *
 * 依赖 Melexis 官方库 MLX90640_API.c/.h + 本工程平台层 MLX90640_I2C_Driver.c。
 * 输出：768 个 int16 温度值，单位 0.01°C，行主序（行 0..23，每行 32 列）。
 */
#ifndef __THERMAL_H
#define __THERMAL_H

#include <stdint.h>

#define THERMAL_COLS    32
#define THERMAL_ROWS    24
#define THERMAL_PIXELS  (THERMAL_COLS * THERMAL_ROWS)   /* 768 */

/* 初始化：读 EEPROM + 提取标定参数 + 设刷新率/棋盘模式。0=成功，负=失败 */
int  Thermal_Init(void);

/* 读一整幅（两个子页）并计算温度。0=成功，负=失败。约 300~400ms @100kHz/2Hz */
int  Thermal_ReadFrame(void);

/* 最近一帧温度缓冲（768 个 int16，0.01°C，行主序）。仅在 Thermal_IsReady() 为真后有效 */
const int16_t* Thermal_GetBuffer(void);

/* 是否至少成功读到过一帧 */
uint8_t Thermal_IsReady(void);

/* 最近环境温度 Ta（°C，调试用） */
float   Thermal_GetTa(void);

/* ---- 原始帧转发模式（解算下放龙芯）---- */
/* EEPROM 标定数据(832 字)，Thermal_Init 里已 DumpEE 填充，上电转发给龙芯一次 */
const uint16_t* Thermal_GetEE(void);
/* 只读一个子页原始数据(不解算)。0=成功，负=失败 */
int  Thermal_ReadRawSubpage(void);
/* 最近一次读到的原始帧(834 字) */
const uint16_t* Thermal_GetRawFrame(void);

#endif /* __THERMAL_H */
