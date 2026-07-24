#include "dht11.h"
#include "main.h"   // DHT11_Pin / DHT11_GPIO_Port

/*---------------------- DWT 微秒延时 ----------------------*/
static void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

static void dwt_delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - start) < ticks) { }
}

/* 等待引脚变为指定电平，超时(us)返回 0；成功返回等待掉的微秒数(>=1) */
static uint32_t dht_wait_level(GPIO_PinState level, uint32_t timeout_us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t limit = timeout_us * (SystemCoreClock / 1000000U);
    while (HAL_GPIO_ReadPin(DHT11_GPIO_Port, DHT11_Pin) != level)
    {
        if ((DWT->CYCCNT - start) > limit) return 0;
    }
    uint32_t el = (DWT->CYCCNT - start) / (SystemCoreClock / 1000000U);
    return el ? el : 1;
}

/*---------------------- 引脚方向切换 ----------------------*/
static void dht_pin_output(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin   = DHT11_Pin;
    g.Mode  = GPIO_MODE_OUTPUT_OD;   // 开漏，配合外部/内部上拉
    g.Pull  = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_GPIO_Port, &g);
}

static void dht_pin_input(void)
{
    GPIO_InitTypeDef g = {0};
    g.Pin  = DHT11_Pin;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT11_GPIO_Port, &g);
}

/*---------------------- API ----------------------*/
void DHT11_Init(void)
{
    dwt_init();
    dht_pin_output();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);  // 空闲拉高
}

/* 时序敏感段：释放总线 -> 等响应 -> 读 40 bit，全程约 4~5ms。
   ★必须在关中断下执行：本函数靠 DWT 计数器以 40us 阈值区分 bit 0/1，
     一旦被 SysTick / CAN / 串口中断抢占，高电平宽度就会测长，校验必然失败。
     这正是"温湿度恒为 0、重试 3 次全败"的软件根因。
   DWT->CYCCNT 是硬件计数器，关中断期间照常递增，不影响本函数计时。 */
static uint8_t dht_read_bits(uint8_t *data)
{
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_SET);
    dwt_delay_us(30);

    /* ---- 等待 DHT11 响应：80us 低 + 80us 高 ---- */
    dht_pin_input();
    if (dht_wait_level(GPIO_PIN_RESET, 100) == 0) return 0;  // 未拉低 → 无响应
    if (dht_wait_level(GPIO_PIN_SET,   100) == 0) return 0;  // 响应低结束
    if (dht_wait_level(GPIO_PIN_RESET, 100) == 0) return 0;  // 响应高结束，进入数据

    /* ---- 读 40 bit：每 bit 以 50us 低起始，随后高电平长短决定 0/1 ---- */
    for (int i = 0; i < 40; i++)
    {
        /* 等待本 bit 的高电平开始（50us 低结束） */
        if (dht_wait_level(GPIO_PIN_SET, 100) == 0) return 0;
        /* 测量高电平持续时间：>40us 记为 1，否则 0 */
        uint32_t hi = dht_wait_level(GPIO_PIN_RESET, 120);
        if (hi == 0) return 0;
        data[i / 8] <<= 1;
        if (hi > 40) data[i / 8] |= 1;
    }
    return 1;
}

uint8_t DHT11_Read(int8_t *temp, uint8_t *humi)
{
    uint8_t data[5] = {0};

    /* ---- 1. 主机起始信号：拉低 >=18ms ----
       这 18ms 只要求"够长"，不要求精确，且 HAL_Delay 依赖 SysTick 中断，
       因此必须放在关中断之外。 */
    dht_pin_output();
    HAL_GPIO_WritePin(DHT11_GPIO_Port, DHT11_Pin, GPIO_PIN_RESET);
    HAL_Delay(20);                 // >=18ms

    /* ---- 2. 时序敏感段整体关中断 ----
       用 PRIMASK 存取而非无条件 __enable_irq()，避免在已处于临界区的调用点
       被本函数提前打开中断。USART1(热成像)走 HAL_UART_Transmit 阻塞轮询发送、
       CAN RX 有 3 级 FIFO，屏蔽这 ~5ms 不会丢数据。 */
    uint32_t prim = __get_PRIMASK();
    __disable_irq();
    uint8_t ok = dht_read_bits(data);
    if (!prim) __enable_irq();

    if (!ok) return 0;

    /* ---- 3. 校验：前 4 字节之和低 8 位 == 第 5 字节 ---- */
    uint8_t sum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (sum != data[4]) return 0;

    /* data[0]=湿度整数, [2]=温度整数（DHT11 小数位恒 0） */
    if (humi) *humi = data[0];
    if (temp) *temp = (int8_t)data[2];
    return 1;
}
