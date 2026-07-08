/*
 * MLX90640_I2C_Driver.c — 龙芯侧空实现（stub）
 * 见头文件说明：龙芯只用 Melexis 纯计算 API，这些 I2C 接口不会被调用。
 */
#include "MLX90640_I2C_Driver.h"

void MLX90640_I2CInit(void) {}

int MLX90640_I2CGeneralReset(void) { return -1; }

int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                     uint16_t nMemAddressRead, uint16_t *data) {
    (void)slaveAddr; (void)startAddress; (void)nMemAddressRead; (void)data;
    return -1;   /* 龙芯不做 I2C */
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data) {
    (void)slaveAddr; (void)writeAddress; (void)data;
    return -1;
}

void MLX90640_I2CFreqSet(int freq) { (void)freq; }
