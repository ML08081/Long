#include "bt_debug.h"
#include "main.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define RAW_RING_SIZE  64

static volatile uint8_t  s_raw_ring[RAW_RING_SIZE];
static volatile uint16_t s_raw_head = 0;
static volatile uint16_t s_raw_tail = 0;

static volatile uint32_t s_restart_ore = 0;
static volatile uint32_t s_restart_it  = 0;
static volatile uint32_t s_restart_ens = 0;

static volatile uint8_t  s_last_ok[7];
static volatile uint8_t  s_last_bad[7];
static volatile uint8_t  s_last_bad_calc = 0;
static volatile uint8_t  s_pending_ok  = 0;
static volatile uint8_t  s_pending_bad = 0;
static volatile uint8_t  s_pending_tele = 0;
static volatile uint8_t  s_last_tele[7];

static uint32_t s_last_byte_cnt  = 0;
static uint32_t s_last_frame_cnt  = 0;
static uint32_t s_zero_byte_warn   = 0;

void BTDbg_Init(void)
{
    s_raw_head = s_raw_tail = 0;
    s_restart_ore = s_restart_it = s_restart_ens = 0;
    s_pending_ok = s_pending_bad = s_pending_tele = 0;
    s_last_byte_cnt = s_last_frame_cnt = 0;
    s_zero_byte_warn = 0;
    LOG("BTDBG: USART2 debug ready 115200 PA2/PA3");
    LOG("BTDBG: watch USART3 PB10/PB11 BT UART here");
}

void BTDbg_OnRxByte(uint8_t byte)
{
    uint16_t next = (uint16_t)((s_raw_head + 1U) % RAW_RING_SIZE);
    if (next != s_raw_tail) {
        s_raw_ring[s_raw_head] = byte;
        s_raw_head = next;
    }
    (void)byte;
}

void BTDbg_OnGoodFrame(const uint8_t *frame, int16_t speed, int16_t steering, uint8_t mode)
{
    for (uint8_t i = 0; i < 7; i++) {
        s_last_ok[i] = frame[i];
    }
    s_pending_ok = 1;
    (void)speed;
    (void)steering;
    (void)mode;
}

void BTDbg_OnBadFrame(const uint8_t *frame, uint8_t calc_crc)
{
    for (uint8_t i = 0; i < 7; i++) {
        s_last_bad[i] = frame[i];
    }
    s_last_bad_calc = calc_crc;
    s_pending_bad = 1;
}

void BTDbg_OnTelemetryReject(const uint8_t *frame)
{
    for (uint8_t i = 0; i < 7; i++) {
        s_last_tele[i] = frame[i];
    }
    s_pending_tele = 1;
}

void BTDbg_OnRxRestart(uint8_t reason)
{
    switch (reason) {
    case 0: s_restart_ore++; break;
    case 1: s_restart_it++;  break;
    default: s_restart_ens++; break;
    }
}

#if BTDBG_ENABLE
static void log_frame_hex(const char *tag, const volatile uint8_t *f)
{
    LOG("BTDBG %s: %02X %02X %02X %02X %02X %02X %02X",
        tag, f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
}

static void log_raw_ring(void)
{
    char buf[RAW_RING_SIZE * 3 + 8];
    int  pos = 0;
    uint16_t tail = s_raw_tail;
    uint16_t head = s_raw_head;
    int count = 0;

    pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos, "raw:");
    while (tail != head && count < 32 && pos < (int)sizeof(buf) - 4) {
        pos += snprintf(buf + pos, sizeof(buf) - (size_t)pos,
                        " %02X", s_raw_ring[tail]);
        tail = (uint16_t)((tail + 1U) % RAW_RING_SIZE);
        count++;
    }
    LOG("BTDBG %s", buf);
}

static void log_zero_byte_help(void)
{
    s_zero_byte_warn++;
    LOG("BTDBG WARN: USART3 got 0 bytes this second (total still %lu)",
        g_rx_byte_cnt);
    LOG("BTDBG => NO UART data on PB11. Software RX is OK (hal_rx=BUSY).");
    LOG("BTDBG CHECKLIST:");
    LOG("BTDBG  1) BT module TX wire -> STM32 PB11 (RX)");
    LOG("BTDBG  2) BT module GND -> STM32 GND");
    LOG("BTDBG  3) BT module VCC powered, LED on");
    LOG("BTDBG  4) PC paired AND connected to THIS module");
    LOG("BTDBG  5) JDY BLE: use PC app BLE tab | HC-05: use COM port tab");
    LOG("BTDBG  6) BT module UART baud = 115200 (AT+BAUD8 for JDY)");
    if (s_zero_byte_warn >= 5U) {
        LOG("BTDBG !!! 5s zero bytes: almost certainly WIRING or PC not linked");
    }
}
#endif /* BTDBG_ENABLE */

void BTDbg_Poll(uint8_t remote_active, uint32_t remote_age, uint8_t u3_rx_hal_state)
{
#if !BTDBG_ENABLE
    /* 默认关闭（原因见 bt_debug.h）。计数器仍在 ISR 里累加，需要时把
       BTDBG_ENABLE 改回 1 就能恢复完整输出，无需改动调用点。 */
    (void)remote_active; (void)remote_age; (void)u3_rx_hal_state;
    return;
#else
    uint32_t bytes = g_rx_byte_cnt;
    uint32_t delta_bytes = bytes - s_last_byte_cnt;
    uint32_t delta_frames = g_rx_frame_cnt - s_last_frame_cnt;
    s_last_byte_cnt = bytes;
    s_last_frame_cnt = g_rx_frame_cnt;

    LOG("=== BTDBG tick=%lu ===", HAL_GetTick());
    LOG("BTDBG RX: +%luB period total=%lu ok=%lu poll=%lu",
        delta_bytes, bytes, g_rx_frame_cnt, g_rx_poll_cnt);
    LOG("BTDBG ERR: crc=%lu tele=%lu hdr=%lu +ok=%lu",
        g_rx_crc_err, g_rx_tele_reject, g_rx_header_err, delta_frames);
    LOG("BTDBG STATE: fsm=%u hal_rx=%u(34=BUSY) ready=%u active=%u age=%lu",
        USART3_GetFsmState(), u3_rx_hal_state,
        (unsigned)g_remote_ready, remote_active, remote_age);
    LOG("BTDBG RESTART: ore=%lu it=%lu ensure=%lu",
        s_restart_ore, s_restart_it, s_restart_ens);

    USART3_LogHwStatus();

    if (delta_bytes == 0) {
        log_zero_byte_help();
    } else {
        s_zero_byte_warn = 0;
    }

    if (s_pending_ok) {
        s_pending_ok = 0;
        log_frame_hex("OK_CMD", s_last_ok);
    }
    if (s_pending_bad) {
        s_pending_bad = 0;
        log_frame_hex("BAD_CRC", s_last_bad);
        LOG("BTDBG BAD_CRC calc=%02X recv=%02X", s_last_bad_calc, s_last_bad[6]);
    }
    if (s_pending_tele) {
        s_pending_tele = 0;
        log_frame_hex("SKIP_TELE", s_last_tele);
    }

    if (delta_bytes > 0) {
        log_raw_ring();
    }
#endif /* BTDBG_ENABLE */
}
