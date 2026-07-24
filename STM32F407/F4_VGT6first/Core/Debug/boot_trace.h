#ifndef __BOOT_TRACE_H
#define __BOOT_TRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"

/*============================================================================
 * 启动跟踪日志 (USART3 直接寄存器访问)
 *
 * 用途：
 *   在系统启动初期输出诊断信息，精确定位卡死位置。
 *   使用 USART3 直接寄存器操作，不依赖 HAL 中断状态。
 *
 * 前提：
 *   调用任何 DUART_* 函数前，MX_USART3_UART_Init() 必须已执行
 *   （USART3 时钟已使能，GPIO 已配置）。
 *
 * 特点：
 *   - 完全轮询，不使能中断
 *   - 不修改 USART3 的 RX 配置
 *   - 可与后续蓝牙协议栈共存
 *============================================================================*/

/**
 * @brief  通过 USART3 输出一个字符（轮询方式）
 * @param  c  要输出的字符
 */
void DUART_PutChar(char c);

/**
 * @brief  通过 USART3 输出字符串
 * @param  s  以 '\0' 结尾的字符串
 */
void DUART_Print(const char *s);

/**
 * @brief  通过 USART3 输出无符号整数（十进制）
 * @param  val  要输出的值
 */
void DUART_PrintDec(uint32_t val);

/**
 * @brief  通过 USART3 输出无符号整数（十六进制，带 "0x" 前缀）
 * @param  val  要输出的值
 */
void DUART_PrintHex(uint32_t val);

/**
 * @brief  带时间戳和换行的跟踪宏
 *         用法：BOOT_TRACE("Hello");  →  "[tick] Hello\r\n"
 */
void DUART_Trace(const char *msg);

/**
 * @brief  输出 32 位寄存器名和值（调试用）
 *         用法：DUART_ShowReg("SR", USART3->SR);
 *         →  "SR = 0xXXXX\r\n"
 */
void DUART_ShowReg(const char *name, uint32_t val);

#ifdef __cplusplus
}
#endif

#endif /* __BOOT_TRACE_H */
