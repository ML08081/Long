/**
  ******************************************************************************
  * @file    linetrack.h
  * @brief   3×TCRT5000 循迹传感器驱动（当前安装：PC3=车体左 / PC1=中 / PC0=车体右）
  *          见《定点巡检实现方案.md》§3 与《实施方案_CAN与PCB.md》§2.1。
  *
  *  纯传感器层：读 3 路探头 + 去抖，产出线位置误差 / 路口 / 丢线判定，
  *  以及打包给 CAN TeleCore(byte7) 的位域。循线控制逻辑不在这里（在 patrol.c）。
  *
  *  ★ 2026-07-23：由数字量 DO 升级为模拟量 AO（ADC）。
  *    原因：探头离地过近时反射基线整体抬高，DO 的翻转点由模块【板载电位器】
  *    决定，软件无从调整，表现为"有时到地面就识别、有时又完全不识别"。
  *    改读 AO 后阈值写在本文件里，随时可调、可复现，且能用双阈值迟滞
  *    （压线阈值与离线阈值分开、中间留死区）从根本上抗抖动 —— 这是
  *    数字量方案无论怎么加软件去抖都做不到的。
  ******************************************************************************
  */
#ifndef __LINETRACK_H
#define __LINETRACK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"    /* HAL */
#include "adc.h"     /* g_adc_dma[] / ADC_CH_LINE_* */

/* ===== 采集方式开关 =========================================================
 *  1 = 模拟量 AO 走 ADC（当前方案，需 CubeMX 把 PC0/PC1/PC3 配成 ADC1_IN10/11/13）
 *  0 = 数字量 DO 走 GPIO（旧方案，回退用；需把接线换回 DO 且 CubeMX 配回 GPIO_Input）
 *  ★ 切换本开关必须【同时】改接线与 CubeMX 配置，只改宏是不生效的。 */
#define LINETRACK_USE_ADC   1

/* 接插件的 L/R 标号与当前探头安装方向相反。统一在采集入口纠正，
 * 原始值、标定、CAN 位域和纠偏均使用朝车头方向看的车体左/中/右。
 * 不改变遥控舵机方向或路口左/右转命令的含义。 */
#define LINE_SWAP_LR        1

/* 一次实际采样间隔；多个任务重复读取不会加快去抖。 */
#define LINE_SAMPLE_MS      5U

#if LINETRACK_USE_ADC

/* ===== 模拟量阈值（12 位 ADC，量程 0~4095）===================================
 *  默认使用下方独立范围学习；手动回退阈值必须先取得现场两点读数。
 *
 *  标定步骤（烧录后看 F4 串口日志 COM26 @115200 的 [LINE_ADC] 行）：
 *    1. 小车放【白底】上，记三路读数 —— 这是"离线"基线
 *    2. 小车放【黑线】上，记三路读数 —— 这是"压线"基线
 *    3. 取两者之间留出迟滞带：
 *         LINE_ADC_ON_TH  = 底色 + (线路-底色) × 60%（偏线路侧，严格判压线）
 *         LINE_ADC_OFF_TH = 底色 + (线路-底色) × 40%（偏底色侧，及时判离线）
 *    4. 两个阈值之间就是死区，读数在死区内时保持上一状态不变 —— 抗抖动核心
 *
 *  极性说明：TCRT5000 常见接法是【黑线(反射弱) → AO 电压高 → ADC 值大】。
 *  若你的模块相反（压线时读数反而变小），把 LINE_ADC_INVERTED 置 1 即可，
 *  不要去交换上面两个阈值的大小关系。 */
#define LINE_ADC_INVERTED    0        /* 0=压线时 ADC 值更大；1=压线时 ADC 值更小 */

/* 默认按每路自身观察到的黑/白范围标定；停车后手动横扫线路，确保每个
 * 探头都经过底色和线路。未获得足够对比度时不判压线，巡检保持停车。
 * 只扩展范围、不遗忘已见过的底色，避免长时间丢线后把底色重新当成线。
 * 有现场两点读数时调用 LineTrack_SetCalibration，可固定三路标定与极性。
 * AUTO=0 时才使用下方手动阈值；不能把占位值当作已完成现场标定。 */
#define LINE_ADC_AUTO_CALIBRATE 1
#define LINE_ADC_MIN_SPAN       400U
#define LINE_ADC_ENDPOINT_N     3U
#define LINE_ADC_ENDPOINT_MARGIN 20U

/* ★ 每路【独立】阈值。
 *   旧日志有 L=2790 / C=1065 / R=2140；缺少对应的黑/白两组表面说明，
 *   不能仅按这一组数值直接填写六个阈值。三个探头的读数可能离散很大，
 *   来源是元件离散、LED 驱动电流差异、以及各探头实际离地高度的微小不同。
 *   用一套全局阈值必然顾此失彼 —— 这正是"一落地就全亮"的直接原因：
 *   全局阈值对读数偏高的那一路来说太低，白底也被判成黑线。
 *   模拟量方案的价值就在这里：每路单独标定即可，数字量时代没这个选项。
 *
 *   下面仅为 AUTO=0 的手动回退占位值；默认 AUTO=1 不用它们判线。 */
#define LINE_ADC_ON_TH_L   2500U      /* 左：判定压线 */
#define LINE_ADC_OFF_TH_L  1800U      /* 左：判定离线（必须 < ON） */
#define LINE_ADC_ON_TH_C   2500U      /* 中 */
#define LINE_ADC_OFF_TH_C  1800U
#define LINE_ADC_ON_TH_R   2500U      /* 右 */
#define LINE_ADC_OFF_TH_R  1800U

/* 去抖：连续 N 次越过同一阈值才翻转。迟滞已经消掉大部分抖动，
   这里不必像数字量方案那样做到 6 次，2 次即可，响应更快。 */
#define LINE_ADC_DEBOUNCE_N  2

/* 取最近一次三路 ADC 原始读数，供标定日志打印使用 */
void LineTrack_GetRaw(uint16_t *l, uint16_t *c, uint16_t *r);
uint8_t LineTrack_SetCalibration(const uint16_t background[3], const uint16_t line[3]);
/* 阈值使用极性统一后的坐标（高值=线路）；反向通道需用4095-原始值比较。 */
void LineTrack_GetThresholds(uint16_t off[3], uint16_t on[3], uint8_t *readyMask);

#else  /* 数字量 DO 回退路径 */

/* ★ 压在黑线上时 DO 的电平：上电用模块电位器把"压线/离线"翻转调清晰后，
   实测记录并确认此宏。多数 TCRT5000 模块：压黑线(反射弱)→DO=低(RESET)。 */
#define LINE_ON_LEVEL   GPIO_PIN_RESET
#define DEBOUNCE_ON_N    6
#define DEBOUNCE_OFF_N   3

#endif /* LINETRACK_USE_ADC */

typedef struct { uint8_t l, c, r; } LineState;   /* 1=该探头压在黑线上 */

/* ---- CAN TeleCore(byte7) / 遥测位域 ---- */
#define LINE_BIT_L         0x01u
#define LINE_BIT_C         0x02u
#define LINE_BIT_R         0x04u
#define LINE_BIT_JUNCTION  0x08u   /* 三路全压线 = 路口/定点标志 */
#define LINE_BIT_LOST      0x10u   /* 三路全脱线 = 丢线 */

void      LineTrack_Init(void);            /* 清去抖状态 */
LineState LineTrack_Read(void);            /* 固定5ms采样、滤波/迟滞；重复读取返回同一状态 */
uint8_t   LineTrack_IsCalibrated(void);     /* 三路均有足够黑/白对比度 */
uint8_t   LineTrack_IsJunction(LineState s);/* 三路全压线 */
uint8_t   LineTrack_IsLost(LineState s);   /* 三路全脱线 */
int8_t    LineTrack_Error(LineState s);    /* 线位置误差: -2..+2（负=线偏左，需向左修） */
uint8_t   LineTrack_PackBits(LineState s); /* 打包成 CAN TeleCore byte7 位域 */

#ifdef __cplusplus
}
#endif

#endif /* __LINETRACK_H */
