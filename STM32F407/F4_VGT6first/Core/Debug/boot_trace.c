#include "boot_trace.h"
#include "main.h"

/*============================================================================
 * USART3 直接寄存器访问诊断输出
 *
 * 寄存器映射 (STM32F4):
 *   USART3 基址 = 0x40004800
 *   SR  (偏移 0x00) — 状态寄存器
 *   DR  (偏移 0x04) — 数据寄存器
 *   CR1 (偏移 0x0C) — 控制寄存器 1
 *
 * 标志位:
 *   TXE (SR bit 7) — 发送数据寄存器空 → 可写入新数据
 *   TC  (SR bit 6) — 发送完成
 *============================================================================*/

/*============================================================================*/

void DUART_PutChar(char c)
{
    /* 等待 TXE 置位（发送寄存器空） */
    while (!(USART3->SR & USART_SR_TXE));

    /* 写入数据寄存器 */
    USART3->DR = (uint8_t)c;

    /* 换行符附加回车 */
    if (c == '\n')
    {
        while (!(USART3->SR & USART_SR_TXE));
        USART3->DR = '\r';
    }
}

/*============================================================================*/

void DUART_Print(const char *s)
{
    if (!s) return;
    while (*s)
    {
        DUART_PutChar(*s);
        s++;
    }
}

/*============================================================================*/

void DUART_PrintDec(uint32_t val)
{
    char buf[12];
    uint8_t i = 0;

    if (val == 0)
    {
        DUART_PutChar('0');
        return;
    }

    while (val > 0 && i < sizeof(buf) - 1)
    {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }

    /* 反转输出 */
    while (i > 0)
        DUART_PutChar(buf[--i]);
}

/*============================================================================*/

void DUART_PrintHex(uint32_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t started = 0;

    DUART_Print("0x");

    for (int8_t i = 28; i >= 0; i -= 4)
    {
        uint8_t nibble = (uint8_t)((val >> i) & 0x0F);
        if (nibble || started || i == 0)
        {
            DUART_PutChar(hex[nibble]);
            started = 1;
        }
    }
}

/*============================================================================*/

void DUART_Trace(const char *msg)
{
    DUART_PutChar('[');
    DUART_PrintDec(HAL_GetTick());
    DUART_Print("] ");
    DUART_Print(msg);
    DUART_Print("\r\n");
}

/*============================================================================*/

void DUART_ShowReg(const char *name, uint32_t val)
{
    DUART_Print(name);
    DUART_Print(" = 0x");
    /* 始终输出 8 位十六进制 */
    static const char hex[] = "0123456789ABCDEF";
    for (int8_t i = 28; i >= 0; i -= 4)
    {
        DUART_PutChar(hex[(val >> i) & 0x0F]);
    }
    DUART_Print("\r\n");
}
