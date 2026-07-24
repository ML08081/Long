/**
  ******************************************************************************
  * @file    watchdog.c
  * @brief   IWDG 任务级聚合喂狗实现（2026-07-23）
  ******************************************************************************
  */
#include "watchdog.h"
#include "main.h"          /* HAL_GetTick / IWDG */

/* 各任务的存活截止期（ms）：取任务标称周期的若干倍，留足调度抖动余量。
   必须【全部】小于 IWDG 超时(5s)，否则起不到提前发现的作用。
   EnvTask 放宽到 2s：它含阻塞式 DHT11 读（重试 3 次 + 每次 30ms 间隔），
   正常一轮就可能耗近 100ms，且 2s 才做一次采样。 */
static const uint32_t kDeadlineMs[WDG_TASK_COUNT] = {
    [WDG_TASK_MOTION]    =  500U,   /* 17ms 周期 -> 500ms 判死 */
    [WDG_TASK_RANGING]   =  500U,   /* 1ms  周期 */
    [WDG_TASK_TELEMETRY] = 1000U,   /* 20ms 周期 */
    [WDG_TASK_ENV]       = 2000U,   /* 100ms 周期，含阻塞 DHT11 */
    [WDG_TASK_SCAN]      = 2000U,   /* 10~100ms，云台扫描有长停留 */
};

static volatile uint32_t s_lastKick[WDG_TASK_COUNT];
static volatile uint8_t  s_started[WDG_TASK_COUNT];   /* 该任务是否已打过至少一次点 */
static volatile uint32_t s_faultCnt = 0;
static volatile uint8_t  s_blame    = 0xFF;
static uint32_t          s_bootMs   = 0;

/* ★ 启动宽限期（必须够长，否则会把正常启动误判成故障并陷入复位循环）：
 *   WDG_Init 在 osKernelStart【之前】调用，而它之后 main 里还要跑完整个 BRINGUP：
 *   VL53L0X 最多 3 次重试、失败时对 0x08~0x77 共 112 个地址做 I2C 全总线扫描
 *   （每个 3ms 超时 ≈ 340ms）、以及大量 115200 串口 LOG。之后调度器才启动，
 *   各任务自身还有上电延时（EnvTask osDelay(1000)、ScanTask osDelay(300)）。
 *   整个过程可达数秒，远超单个任务 2s 的截止期。
 *   ——2026-07-23 首版漏了这一点，导致 F4 上电即被自己的看门狗打进无限重启。 */
#define WDG_BOOT_GRACE_MS   15000U

void WDG_Init(void)
{
    s_bootMs = HAL_GetTick();
    for (int i = 0; i < WDG_TASK_COUNT; i++) {
        s_lastKick[i] = s_bootMs;
        s_started[i]  = 0;
    }
    s_faultCnt = 0;
    s_blame    = 0xFF;
}

void WDG_Kick(WdgTaskId id)
{
    if ((int)id < WDG_TASK_COUNT) {
        s_lastKick[id] = HAL_GetTick();
        s_started[id]  = 1;
    }
}

void WDG_ReportFault(WdgFaultKind kind)
{
    (void)kind;
    if (s_faultCnt < 0xFFFFFFFFU) s_faultCnt++;
}

void WDG_ClearFault(void)
{
    s_faultCnt = 0;
}

uint8_t WDG_LastBlameTask(void)
{
    return s_blame;
}

void WDG_Service(void)
{
    uint32_t now = HAL_GetTick();

    /* (0) 启动宽限期内无条件喂狗 —— BRINGUP 与各任务上电延时都在这段里，
     *     此时任务尚未开始打点，任何超时判定都是误判。 */
    if ((now - s_bootMs) < WDG_BOOT_GRACE_MS) {
        s_blame = 0xFF;
        IWDG->KR = 0xAAAA;
        return;
    }

    /* (1) 数据异常累计超限 -> 不喂狗，等 IWDG 复位 */
    if (s_faultCnt >= WDG_FAULT_LIMIT) {
        s_blame = 0xFE;          /* 0xFE 表示"因数据异常"而非某个任务 */
        return;
    }

    /* (2) 逐任务校验。宽限期已过，此时仍未打过点的任务 = 创建失败或从未运行，
     *     同样判为故障（宽限期 15s 远大于所有任务的上电延时 + 周期）。
     *     时间比较用无符号差值，天然处理 HAL_GetTick 的 32 位回绕（约 49.7 天）。 */
    for (int i = 0; i < WDG_TASK_COUNT; i++) {
        if (!s_started[i] || (now - s_lastKick[i]) > kDeadlineMs[i]) {
            s_blame = (uint8_t)i;
            return;
        }
    }

    /* (3) 全员健康 -> 喂狗 */
    s_blame = 0xFF;
    IWDG->KR = 0xAAAA;
}
