#ifndef CONTROL_POLICY_H
#define CONTROL_POLICY_H
#include <stdint.h>

/* Configuration gears only; feedback/PID outputs must not be quantized. */
#define CONTROL_SPEED_MAX_GEAR 14U
#define CONTROL_SPEED_MAX_LEVEL 1500
static inline uint16_t Control_Speed(uint8_t gear) { return 100U + 100U * gear; }
static inline int16_t Control_Clamp(int16_t v, uint16_t limit) {
    return v > (int16_t)limit ? (int16_t)limit : v < -(int16_t)limit ? -(int16_t)limit : v;
}
static inline int16_t Control_Ramp(int16_t old, int16_t target, uint32_t dt) {
    if ((old < 0 && target > 0) || (old > 0 && target < 0)) return 0;
    int32_t a = old < 0 ? -old : old, b = target < 0 ? -target : target;
    if (b <= a) return target; /* stop, obstacle and lower limit take effect immediately */
    uint32_t step = dt * 500U / 1000U;
    if (b > a + (int32_t)step) b = a + (int32_t)step;
    return (int16_t)(target < 0 ? -b : b);
}
enum { TH_AUTO=0, TH_ON=1, TH_OFF=2 };
enum { TH_STOPPED=0, TH_STARTING=1, TH_STREAMING=2, TH_FAULT=3 };
enum { TH_BOOT=1, TH_MANUAL=2, TH_GAS=4, TH_FLAME=8, TH_AMBIENT=16,
       TH_VFIRE=32, TH_VSMOKE=64, TH_HOT=128, TH_HOLD=256, TH_VERIFY=512 };
typedef struct {
    uint8_t bootDone, ambientHot, held;
    uint32_t holdUntil;
    uint8_t awaitingVerification;
} ThermalPolicy;
static inline uint16_t Control_Thermal(ThermalPolicy *p, uint32_t now,
    uint8_t preference, uint16_t alarms, uint8_t tempValid, int temp, uint8_t newCoolSample) {
    if (now >= 180000U) p->bootDone = 1; /* latched: tick wrap cannot restart preview */
    if (tempValid) {
        if (temp >= 45) p->ambientHot = 1;
        else if (temp <= 42) p->ambientHot = 0;
    } else p->ambientHot = 0;
    if (p->ambientHot) alarms |= TH_AMBIENT;
    if (alarms) { p->held = 1; p->awaitingVerification=1; p->holdUntil = now + 30000U; }
    else if(p->awaitingVerification) {
        if(newCoolSample) { p->awaitingVerification=0; p->holdUntil=now+30000U; }
    }
    else if (p->held && (int32_t)(now - p->holdUntil) >= 0) p->held = 0;
    uint16_t reason = alarms;
    if(p->awaitingVerification && !alarms) reason|=TH_VERIFY;
    if (!alarms && p->held) reason |= TH_HOLD;
    if (preference == TH_ON) reason |= TH_MANUAL;
    if (preference == TH_AUTO && !p->bootDone) reason |= TH_BOOT;
    return reason; /* alarms override manual OFF */
}
#endif
