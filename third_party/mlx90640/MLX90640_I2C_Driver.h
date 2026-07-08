/*
 * MLX90640_I2C_Driver.h — 龙芯侧空实现（stub）
 *
 * 龙芯不直接接 MLX90640；热成像原始帧由 F4 经串口转发过来，龙芯只调用
 * Melexis 官方 API 的纯计算函数（ExtractParameters / CalculateTo / GetTa）。
 * 这些 I2C 接口不会被调用，仅为满足 MLX90640_API.c 的链接符号而提供空桩。
 */
#ifndef _MLX90640_I2C_DRIVER_H_
#define _MLX90640_I2C_DRIVER_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MLX90640_I2CInit(void);
int  MLX90640_I2CGeneralReset(void);
int  MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data);
int  MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
void MLX90640_I2CFreqSet(int freq);

#ifdef __cplusplus
}
#endif

#endif
