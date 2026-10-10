#include "runtime_control.h"
#include "stm32f4xx_hal.h"
#include "protocol.h"
#include <string.h>
static volatile uint8_t gear=4, preference=TH_AUTO, remoteFlags, configSeq, eeRequest;
static volatile uint8_t envFlags, thState=TH_STARTING;
static volatile int8_t envTemp;
static volatile uint16_t dhtSeq, reasons;
static volatile uint32_t requestMs, envMs, dhtMs;
static volatile uint8_t requestSeen;
static ThermalPolicy policy;
static volatile uint8_t thermalSampleSeq;
static uint8_t consumedSampleSeq;
static volatile uint8_t legacyVision, legacySeen;
static volatile uint32_t legacyVisionMs;
void Runtime_LegacyVision(uint8_t alarm) {
    legacyVision=alarm<=2U?alarm:0; legacyVisionMs=HAL_GetTick(); legacySeen=1;
}
uint16_t Runtime_Speed(void) { return Control_Speed(gear); }
void Runtime_ThermalRequest(uint8_t mode, uint8_t flags, uint8_t seq, uint8_t ee, uint8_t sampleSeq) {
    if (mode > TH_OFF || (flags & ~15U)) return;
    thermalSampleSeq=sampleSeq;
    preference=mode; remoteFlags=flags; configSeq=seq;
    requestMs=HAL_GetTick(); requestSeen=1;
    if (ee) eeRequest=1;
}
void Runtime_SpeedRequest(uint8_t value, uint8_t seq) {
    if (value > CONTROL_SPEED_MAX_GEAR) return;
    gear=value; configSeq=seq;
}
void Runtime_UartRequest(const uint8_t *f) {
    if (f[0]!=0xAA || f[1]!=0x63 || f[2]!=8 || f[3]>2 || f[5]>CONTROL_SPEED_MAX_GEAR || (f[4]&~15U)) return;
    if (Protocol_CalculateChecksum((uint8_t*)f,11)!=f[11]) return;
    Runtime_ThermalRequest(f[3],f[4],f[6],f[7],f[8]); Runtime_SpeedRequest(f[5],f[6]);
}
void Runtime_Env(uint8_t flags, int8_t temp, uint16_t seq, uint32_t sampleMs) {
    envTemp=temp; envFlags=flags; dhtSeq=seq; dhtMs=sampleMs; envMs=HAL_GetTick();
}
uint8_t Runtime_RemoteFlags(uint32_t now) {
    return requestSeen && now-requestMs<3000U ? remoteFlags : 0;
}
uint16_t Runtime_ThermalReason(uint32_t now) {
    uint16_t a=0;
    if (now-envMs<2000U) { if(envFlags&1U) a|=TH_GAS; if(envFlags&2U) a|=TH_FLAME; }
    if (requestSeen && now-requestMs<3000U) {
        if(remoteFlags&1U) a|=TH_VFIRE;
        if(remoteFlags&2U) a|=TH_VSMOKE;
        if(remoteFlags&4U) a|=TH_HOT;
    }
    if(legacySeen && now-legacyVisionMs<3000U) {
        if(legacyVision==2) a|=TH_VFIRE;
        else if(legacyVision==1) a|=TH_VSMOKE;
    }
    if (requestSeen && now-requestMs>=5000U) { preference=TH_AUTO; requestSeen=0; }
    uint8_t newCool=requestSeen && now-requestMs<3000U &&
        (remoteFlags&8U) && thermalSampleSeq!=consumedSampleSeq;
    consumedSampleSeq=thermalSampleSeq;
    reasons=Control_Thermal(&policy,now,preference,a,
        (envFlags&4U) && now-envMs<2000U && dhtSeq && now-dhtMs<7000U,envTemp,newCool);
    return reasons;
}
uint8_t Runtime_TakeEepromRequest(void) {
    uint32_t irq=__get_PRIMASK(); __disable_irq(); uint8_t r=eeRequest; eeRequest=0;
    if(!irq) __enable_irq();
    return r;
}
void Runtime_SetThermalState(uint8_t state) { thState=state; }
void Runtime_Status(uint8_t *p) {
    uint32_t now=HAL_GetTick(); uint32_t age=dhtSeq ? (now-dhtMs)/100U : 65535U;
    if(age>65535U) age=65535U;
    p[0]=4; p[1]=thState; p[2]=preference; p[3]=(uint8_t)(reasons>>8); p[4]=(uint8_t)reasons;
    p[5]=policy.bootDone ? 0 : (uint8_t)((180000U-now+999U)/1000U);
    p[6]=gear; p[7]=configSeq;
    p[8]=5; p[9]=(uint8_t)(dhtSeq>>8); p[10]=(uint8_t)dhtSeq;
    p[11]=(uint8_t)(age>>8); p[12]=(uint8_t)age;
    p[13]=envFlags; p[14]=2; p[15]=0; /* protocol version 2 */
}
