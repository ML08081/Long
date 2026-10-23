"""Exercise real F4 CAN recovery and watchdog with an unavailable peer."""
from pathlib import Path
import subprocess, sys, tempfile

root = Path(sys.argv[1]).resolve()
can = (root/'Core/CAN/can_link.c').read_text()
start = can.index('static uint8_t CANLink_NeedsRecovery(')
end = can.index('uint8_t CANLink_SendTeleCore(', start)
with tempfile.TemporaryDirectory(prefix='can-fault-isolation-') as directory:
    out = Path(directory)
    (out/'main.h').write_text(r'''#pragma once
#include <stdint.h>
extern uint32_t test_ms;
typedef struct { uint32_t KR; } TestWatchdog;
extern TestWatchdog watchdog;
#define IWDG (&watchdog)
static inline uint32_t HAL_GetTick(void) { return test_ms; }
''')
    (out/'can.inc').write_text(can[start:end])
    (out/'test.c').write_text(r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "main.h"
#include "watchdog.h"
uint32_t test_ms;
TestWatchdog watchdog;
typedef int HAL_CAN_StateTypeDef;
enum { HAL_CAN_STATE_LISTENING, HAL_CAN_STATE_ERROR, HAL_CAN_STATE_RESET, HAL_CAN_STATE_READY };
enum { HAL_OK, HAL_ERROR };
enum { CAN_IT_RX_FIFO0_MSG_PENDING=1, CAN_IT_ERROR=2, CAN_IT_BUSOFF=4 };
enum { CAN_ID_STD, CAN_RTR_DATA, DISABLE, CAN_TX_MAILBOX0 };
typedef struct { uint32_t StdId,ExtId,IDE,RTR,DLC,TransmitGlobalTime; } CAN_TxHeaderTypeDef;
static int hcan1, bus_off=1, start_failure, recovery_calls, queued;
static uint32_t g_can_recover_cnt, g_can_tx_cnt, g_can_tx_drop;
static uint8_t CANLink_IsBusOff(void) { return bus_off; }
static HAL_CAN_StateTypeDef HAL_CAN_GetState(void *p) { (void)p; return HAL_CAN_STATE_LISTENING; }
static void HAL_CAN_Stop(void *p) { (void)p; }
static void HAL_CAN_ResetError(void *p) { (void)p; }
static int HAL_CAN_Start(void *p) { (void)p; ++recovery_calls; return start_failure?HAL_ERROR:HAL_OK; }
static void HAL_CAN_ActivateNotification(void *p,int flags) { (void)p; (void)flags; }
static unsigned HAL_CAN_GetTxMailboxesFreeLevel(void *p) { (void)p; return 3; }
static void HAL_CAN_AbortTxRequest(void *p,int box) { (void)p; (void)box; }
static int HAL_CAN_AddTxMessage(void *p,CAN_TxHeaderTypeDef *header,uint8_t *data,uint32_t *box) {
    (void)p; (void)data; *box=0; assert(header->DLC<=8); ++queued; return HAL_OK;
}
#include "can.inc"
static void kick_all(void) { for(int i=0;i<WDG_TASK_COUNT;i++) WDG_Kick((WdgTaskId)i); }
int main(void) {
    WDG_Init();
    uint8_t data[8]={0};
    for(test_ms=0;test_ms<=120000;test_ms+=10) {
        kick_all(); watchdog.KR=0;
        assert(!CANLink_Send(0x701,data,1));
        WDG_Service(); assert(watchdog.KR==0xAAAA);
    }
    assert(recovery_calls==121 && queued==0);
    start_failure=1;
    for(;test_ms<150000;test_ms+=10) {
        kick_all(); watchdog.KR=0; CANLink_Service(); WDG_Service();
        assert(watchdog.KR==0xAAAA);
    }
    assert(recovery_calls==150);
    bus_off=0; start_failure=0;
    assert(CANLink_Send(0x701,data,1) && queued==1);
    /* Healthy CAN must not hide an internal task which no longer runs. */
    uint32_t begin=test_ms;
    for(;test_ms<begin+2500;test_ms+=10) {
        for(int task=0;task<WDG_TASK_COUNT;task++)
            if(task!=WDG_TASK_ENV) WDG_Kick((WdgTaskId)task);
        watchdog.KR=0; CANLink_Service(); WDG_Service();
    }
    assert(watchdog.KR==0 && WDG_LastBlameTask()==WDG_TASK_ENV);
    /* Existing non-CAN fault handling remains effective. */
    WDG_Init(); kick_all(); test_ms+=15001; kick_all(); bus_off=1;
    for(unsigned i=0;i<WDG_FAULT_LIMIT;i++) WDG_ReportFault(WDG_FAULT_SENSOR);
    watchdog.KR=0; WDG_Service(); assert(watchdog.KR==0);
    puts("PASS: missing CAN peer/start failures do not reset healthy tasks; recovery <=1/s; faulted CAN queues no TX; real task stalls and sensor faults still stop watchdog feeding");
}
''')
    exe=out/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
        '-I'+str(out),'-I'+str(root/'Core/Watchdog'), str(out/'test.c'),
        str(root/'Core/Watchdog/watchdog.c'),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
