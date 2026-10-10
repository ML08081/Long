/**
  ******************************************************************************
  * @file    i2c_recover.c
  * @brief   I2C 总线级死锁恢复实现（2026-07-23）—— 原理见 i2c_recover.h
  ******************************************************************************
  */
#include "i2c_recover.h"
#include "main.h"
#include "i2c.h"        /* hi2c2 / hi2c3 + MX_I2C2_Init / MX_I2C3_Init */
#include "watchdog.h"   /* 恢复失败上报异常：连续救不回来说明是硬件层问题 */

static volatile uint32_t s_recoverCnt = 0;

uint32_t I2C_RecoverCount(void) { return s_recoverCnt; }

/* 半个时钟周期。100kHz 标准模式下半周期 5us，这里给 10us 留足余量
   （恢复时序不追求速度，宁慢勿错）。 */
static void half_bit_delay(void)
{
    /* 168MHz 下约 10us。用忙等而非 osDelay：本函数可能在临界路径调用，
       且 10us 级别用 RTOS 延时反而不准。 */
    for (volatile uint32_t i = 0; i < 560U; i++) { __NOP(); }
}

/**
 * 通用总线恢复。
 * @param sclPort/sclPin  SCL 引脚
 * @param sdaPort/sdaPin  SDA 引脚
 * @param af              这两脚原本的复用功能号（恢复后要还原）
 * @return 1=SDA 已被释放为高，0=仍被从机拉低（恢复失败）
 */
static uint8_t bus_recover(GPIO_TypeDef *sclPort, uint16_t sclPin,
                           GPIO_TypeDef *sdaPort, uint16_t sdaPin,
                           uint8_t af)
{
    GPIO_InitTypeDef g = {0};

    /* ---- 1. 把 SCL/SDA 从复用切成 GPIO 开漏输出（带上拉，模拟 I2C 电平） ---- */
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;

    /* 先预置 ODR 为释放，避免切换为输出时产生意外低脉冲。 */
    HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(sdaPort, sdaPin, GPIO_PIN_SET);
    g.Pin = sclPin;  HAL_GPIO_Init(sclPort, &g);
    g.Pin = sdaPin;  HAL_GPIO_Init(sdaPort, &g);
    half_bit_delay();

    /* ---- 2. 打 9 个时钟，逼从机把卡住的那一位发完并释放 SDA ---- */
    for (int i = 0; i < 9; i++) {
        if (HAL_GPIO_ReadPin(sdaPort, sdaPin) == GPIO_PIN_SET)
            break;                                  /* SDA 已释放，提前收工 */
        HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_RESET);
        half_bit_delay();
        HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_SET);
        half_bit_delay();
    }

    /* ---- 3. 补一个 STOP：SCL 高期间 SDA 由低变高 ---- */
    HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_RESET);
    half_bit_delay();
    HAL_GPIO_WritePin(sdaPort, sdaPin, GPIO_PIN_RESET);
    half_bit_delay();
    HAL_GPIO_WritePin(sclPort, sclPin, GPIO_PIN_SET);
    half_bit_delay();
    HAL_GPIO_WritePin(sdaPort, sdaPin, GPIO_PIN_SET);
    half_bit_delay();

    uint8_t released = (HAL_GPIO_ReadPin(sclPort, sclPin) == GPIO_PIN_SET &&
                        HAL_GPIO_ReadPin(sdaPort, sdaPin) == GPIO_PIN_SET) ? 1U : 0U;

    /* 打完 9 个时钟 + STOP 后 SDA 仍被拉低 = 从机彻底失联（多半是掉电/短路），
       软件已无能为力，上报看门狗；累计到阈值让 IWDG 复位整机再试。
       成功释放则不计异常（偶发总线抖动属正常，不该触发复位）。 */
    if (!released) WDG_ReportFault(WDG_FAULT_SENSOR);

    /* ---- 4. 还原为 I2C 复用功能 ---- */
    g.Mode      = GPIO_MODE_AF_OD;
    g.Pull      = GPIO_PULLUP;
    g.Speed     = GPIO_SPEED_FREQ_LOW;
    g.Alternate = af;

    g.Pin = sclPin;  HAL_GPIO_Init(sclPort, &g);
    g.Pin = sdaPin;  HAL_GPIO_Init(sdaPort, &g);

    s_recoverCnt++;
    return released;
}

/* MSP DeInit 会关闭外设时钟。此后写 CR1.SWRST 不生效，必须使用 RCC 复位。
   在 GPIO 总线恢复之前停用外设，防止外设和 GPIO 同时驱动总线。 */
static void periph_reset(I2C_HandleTypeDef *h)
{
    HAL_I2C_DeInit(h);
    if (h->Instance == I2C2) {
        __HAL_RCC_I2C2_CLK_ENABLE();
        __HAL_RCC_I2C2_FORCE_RESET();
        __NOP(); __NOP();
        __HAL_RCC_I2C2_RELEASE_RESET();
    } else if (h->Instance == I2C3) {
        __HAL_RCC_I2C3_CLK_ENABLE();
        __HAL_RCC_I2C3_FORCE_RESET();
        __NOP(); __NOP();
        __HAL_RCC_I2C3_RELEASE_RESET();
    }
}

uint8_t I2C2_BusRecover(void)
{
    /* VL53L0X: SCL=PB10, SDA=PB11, AF4 = I2C2 */
    periph_reset(&hi2c2);
    uint8_t ok = bus_recover(GPIOB, GPIO_PIN_10, GPIOB, GPIO_PIN_11, GPIO_AF4_I2C2);
    MX_I2C2_Init();
    return ok;
}

uint8_t I2C3_BusRecover(void)
{
    /* MLX90640: SCL=PA8, SDA=PC9, AF4 = I2C3 */
    periph_reset(&hi2c3);
    uint8_t ok = bus_recover(GPIOA, GPIO_PIN_8, GPIOC, GPIO_PIN_9, GPIO_AF4_I2C3);
    MX_I2C3_Init();
    return ok;
}
