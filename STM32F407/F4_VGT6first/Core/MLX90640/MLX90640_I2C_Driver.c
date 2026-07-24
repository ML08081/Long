/**
 * MLX90640_I2C_Driver.c — MLX90640 平台 I2C 层（基于 STM32 HAL, hi2c3）
 *
 * MLX90640 采用 16-bit 寄存器地址 + 16-bit 大端数据。
 * 这里用 HAL_I2C_Mem_Read/Write（内部自动做 重复起始），
 * 并在读回后把大端字节序转换为主机字节序（STM32 为小端）。
 */
#include "MLX90640_I2C_Driver.h"
#include "i2c.h"      /* 声明 hi2c3 */

#define MLX_I2C_HANDLE   hi2c3
#define MLX_I2C_TIMEOUT  500U     /* ms；整帧 1664B @100kHz 约 150ms，留足余量 */

void MLX90640_I2CInit(void)
{
    /* I2C3 已由 CubeMX 的 MX_I2C3_Init() 初始化，这里无需重复配置。 */
}

int MLX90640_I2CGeneralReset(void)
{
    uint8_t cmd = 0x06;               /* General Call Reset */
    HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(&MLX_I2C_HANDLE, 0x00, &cmd, 1, MLX_I2C_TIMEOUT);
    return (st == HAL_OK) ? 0 : -1;
}

/**
 * 读 nMemAddressRead 个 16-bit 字（大端）到 data[]。
 * 直接读入 data 的字节缓冲区再原地做字节交换，避免额外大数组。
 */
int MLX90640_I2CRead(uint8_t slaveAddr, uint16_t startAddress,
                     uint16_t nMemAddressRead, uint16_t *data)
{
    uint16_t nBytes = (uint16_t)(nMemAddressRead * 2U);
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&MLX_I2C_HANDLE,
                                            (uint16_t)(slaveAddr << 1),
                                            startAddress, I2C_MEMADD_SIZE_16BIT,
                                            (uint8_t *)data, nBytes, MLX_I2C_TIMEOUT);
    if (st != HAL_OK) return -1;

    /* 大端(片上) -> 主机(小端) */
    for (uint16_t i = 0; i < nMemAddressRead; i++) {
        uint8_t *p = (uint8_t *)&data[i];
        data[i] = (uint16_t)((p[0] << 8) | p[1]);
    }
    return 0;
}

int MLX90640_I2CWrite(uint8_t slaveAddr, uint16_t writeAddress, uint16_t data)
{
    uint8_t buf[2];
    buf[0] = (uint8_t)(data >> 8);
    buf[1] = (uint8_t)(data & 0xFF);

    HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&MLX_I2C_HANDLE,
                                             (uint16_t)(slaveAddr << 1),
                                             writeAddress, I2C_MEMADD_SIZE_16BIT,
                                             buf, 2, MLX_I2C_TIMEOUT);
    if (st != HAL_OK) return -1;

    /* 回读校验（与官方驱动一致，写配置寄存器时确保生效） */
    uint16_t check = 0;
    if (MLX90640_I2CRead(slaveAddr, writeAddress, 1, &check) != 0) return -1;
    if (check != data) return -2;
    return 0;
}

void MLX90640_I2CFreqSet(int freq)
{
    /* I2C 频率由 CubeMX 固定（100kHz），此处不动态调整。 */
    (void)freq;
}
