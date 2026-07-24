#ifndef BT_DEBUG_H
#define BT_DEBUG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ★ 2026-07-23：默认关闭。
 *   这套诊断是当年排查"USART3 蓝牙遥控收不到数据"时加的，每秒往日志口打 17 行。
 *   现在主链路已迁到 CAN，USART3 只作回退且平时不接蓝牙模块，于是它会永远
 *   刷 "NO UART data on PB11 / 检查接线" 这类【对当前架构完全错误】的提示，
 *   把 [THERM]/[ENV]/[OBS] 等真正有用的日志整段淹没（实测 20 秒日志里
 *   有效信息占比不到 15%），排障时反而看不见东西。
 *   需要重新排查 USART3 蓝牙链路时，把本宏改回 1 即可。 */
#ifndef BTDBG_ENABLE
#define BTDBG_ENABLE   0
#endif

void BTDbg_Init(void);

/* ISR 安全：仅写环形缓冲/计数，不打印 */
void BTDbg_OnRxByte(uint8_t byte);
void BTDbg_OnGoodFrame(const uint8_t *frame, int16_t speed, int16_t steering, uint8_t mode);
void BTDbg_OnBadFrame(const uint8_t *frame, uint8_t calc_crc);
void BTDbg_OnTelemetryReject(const uint8_t *frame);
void BTDbg_OnRxRestart(uint8_t reason); /* 0=ORE/error_cb 1=Receive_IT_fail 2=EnsureRx */

/* 任务上下文：通过 USART2(LOG) 输出诊断信息 */
void BTDbg_Poll(uint8_t remote_active, uint32_t remote_age, uint8_t u3_rx_hal_state);

#ifdef __cplusplus
}
#endif

#endif /* BT_DEBUG_H */
