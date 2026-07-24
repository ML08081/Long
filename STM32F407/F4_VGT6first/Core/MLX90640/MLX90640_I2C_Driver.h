/**
 * MLX90640_I2C_Driver.h — Melexis MLX90640 官方 API 的 STM32 平台 I2C 层
 *
 * 对接本工程 hi2c3 (PC9=SDA / PA8=SCL)。函数签名与 Melexis 官方
 * mlx90640-library 的 MLX90640_I2C_Driver.h 完全一致，供 MLX90640_API.c 调用。
 *   官方库: https://github.com/melexis/mlx90640-library (Apache-2.0)
 */
#ifndef _MLX90640_I2C_DRIVER_H_
#define _MLX90640_I2C_DRIVER_H_

#include <stdint.h>

void MLX90640_I2CInit(void);
int  MLX90640_I2CGeneralReset(void);
int  MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress, uint16_t nMemAddressRead, uint16_t *data);
int  MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data);
void MLX90640_I2CFreqSet(int freq);

#endif
