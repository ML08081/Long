#include "mq2.h"
#include "adc.h"    // hadc1 / MX_ADC1_Init
#include "main.h"   // MQ2_DO_Pin / MQ2_DO_GPIO_Port

static uint16_t s_last_raw = 0;

void MQ2_Init(void)
{
    /* ADC1 已由 MX_ADC1_Init + ADC_StartDma() 转入 DMA 循环扫描；
       DO(PC4) 已在 MX_GPIO_Init 配为输入。这里无需再做预热采样。 */
    s_last_raw = 0;
}

uint16_t MQ2_ReadRaw(void)
{
    /* ★ 2026-07-23：改为直接读 DMA 缓冲。
       原实现每次调用都 Start → PollForConversion(10ms 超时) → Stop，
       在 4 通道扫描模式下已不适用（单次 Poll 只能拿到序列首个通道），
       而且反复启停 ADC 会打断其它通道的采样。
       现在 DMA 在后台以约 8.8kHz 循环刷新 g_adc_dma[]，这里只做一次原子读。 */
    s_last_raw = (uint16_t)g_adc_dma[ADC_CH_MQ2];
    return s_last_raw;
}

uint8_t MQ2_ReadDO(void)
{
    GPIO_PinState st = HAL_GPIO_ReadPin(MQ2_DO_GPIO_Port, MQ2_DO_Pin);
#if MQ2_DO_ACTIVE_LOW
    return (st == GPIO_PIN_RESET) ? 1 : 0;
#else
    return (st == GPIO_PIN_SET) ? 1 : 0;
#endif
}

uint8_t MQ2_IsAlarm(void)
{
    uint8_t byDO = MQ2_ReadDO();
    uint8_t byAO = (MQ2_ReadRaw() >= MQ2_ADC_ALARM_TH) ? 1 : 0;
    return (byDO || byAO) ? 1 : 0;
}
