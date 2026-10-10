#ifndef RUNTIME_CONTROL_H
#define RUNTIME_CONTROL_H
#include "control_policy.h"
/* CAN 301: sub05 preview/flags/seq/EEPROM-request, sub06 gear/seq.
 * UART AA 63 08: preview,flags,gear,seq,EEPROM-request,0,0,0,XOR.
 * Status CAN481 sub04 + sub05; UART AA6410 uses the same 16 bytes.
 * Receiver only copies validated configuration; no I2C or motor action in ISR. */
void Runtime_ThermalRequest(uint8_t mode, uint8_t flags, uint8_t seq, uint8_t ee, uint8_t sampleSeq);
void Runtime_SpeedRequest(uint8_t gear, uint8_t seq);
void Runtime_UartRequest(const uint8_t *frame);
uint16_t Runtime_Speed(void);
uint8_t Runtime_RemoteFlags(uint32_t now);
void Runtime_LegacyVision(uint8_t alarm);
void Runtime_Env(uint8_t flags, int8_t temp, uint16_t sampleSeq, uint32_t sampleMs);
uint16_t Runtime_ThermalReason(uint32_t now);
uint8_t Runtime_TakeEepromRequest(void);
void Runtime_SetThermalState(uint8_t state);
void Runtime_Status(uint8_t *out16);
#endif
