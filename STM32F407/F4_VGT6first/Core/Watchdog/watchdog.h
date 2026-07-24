/**
  ******************************************************************************
  * @file    watchdog.h
  * @brief   IWDG 任务级聚合喂狗（2026-07-23）
  *
  *  背景：IWDG 已在 CubeMX 配好（Prescaler=64, Reload=2499, LSI≈32kHz
  *        => 超时 ≈ 64*2500/32000 = 5.0s），但原实现是在 MotionControlTask 与
  *        ScanTask 里【无条件】写 KR=0xAAAA。这只能发现"整块 MCU 死掉"，
  *        任何单个任务卡死、或某任务陷在异常数据里空转，看门狗都发现不了
  *        —— 因为总有别的任务在喂。
  *
  *  本模块把喂狗改为【聚合判定】：
  *    · 每个受监护任务在自己的循环里调 WDG_Kick(WDG_TASK_xxx) 打存活标记；
  *    · 唯一喂狗点 WDG_Service() 检查【所有】受监护任务都在各自的截止期内
  *      打过标记，才真正写 KR；任一任务超时就【停止喂狗】，约 5s 后 IWDG 复位。
  *    · 另有 WDG_ReportFault() 供业务层主动上报数据异常，累计到阈值同样
  *      停止喂狗触发复位。
  *
  *  ★ 设计边界（重要）：
  *    "龙芯命令断流"【不】计入看门狗。F4 的循迹快环设计上就要能断网自主运行
  *    （见开发文档 §1.3），命令断流已有 HB_HOLD_MAX(~510ms) 安全停车兜底，
  *    再叠加复位只会让断网时反复重启。若确需该行为，打开 WDG_RESET_ON_LINK_LOSS。
  ******************************************************************************
  */
#ifndef __WATCHDOG_H
#define __WATCHDOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* 受监护任务枚举。新增任务时在此加一项并在其循环里调 WDG_Kick。 */
typedef enum {
    WDG_TASK_MOTION = 0,   /* MotionControlTask 17ms  —— 运动闭环，最关键 */
    WDG_TASK_RANGING,      /* RangingTask      1ms    —— HC/VL53 采集 */
    WDG_TASK_TELEMETRY,    /* TelemetryTask    20ms   —— 上行遥测 */
    WDG_TASK_ENV,          /* EnvTask          100ms  —— 环境采集(含阻塞 DHT11) */
    WDG_TASK_SCAN,         /* ScanTask         10~100ms —— 云台扫描 */
    WDG_TASK_COUNT
} WdgTaskId;

/* 数据异常上报的来源分类（仅用于日志定位，不影响判定逻辑） */
typedef enum {
    WDG_FAULT_CAN_DEAD = 0,   /* CAN 控制器反复自恢复失败 */
    WDG_FAULT_SENSOR,         /* 传感器数据持续非法 */
    WDG_FAULT_OTHER,
} WdgFaultKind;

/* 是否把"龙芯命令断流"也当成复位条件（默认关，理由见文件头） */
#define WDG_RESET_ON_LINK_LOSS   0

/* 连续多少次异常上报后判定为不可恢复、停止喂狗 */
#define WDG_FAULT_LIMIT          20U

void WDG_Init(void);                       /* 清标记；须在 osKernelStart 前调用 */
void WDG_Kick(WdgTaskId id);               /* 任务存活打点 */
void WDG_ReportFault(WdgFaultKind kind);   /* 业务层上报数据异常 */
void WDG_ClearFault(void);                 /* 异常已恢复，清计数 */
void WDG_Service(void);                    /* 唯一喂狗点，由 MotionControlTask 周期调用 */
uint8_t WDG_LastBlameTask(void);           /* 最近一次阻止喂狗的任务号，供日志 */

#ifdef __cplusplus
}
#endif

#endif /* __WATCHDOG_H */
