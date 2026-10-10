"""Host-side regression of actual F4 runtime, patrol and UART parser sources."""
from pathlib import Path
import subprocess, tempfile, sys
f4=Path(sys.argv[1])
with tempfile.TemporaryDirectory(prefix='patrol-f4-test-') as td:
 t=Path(td)
 (t/'stm32f4xx_hal.h').write_text('''#pragma once
#include <stdint.h>
extern uint32_t test_ms;
static inline uint32_t HAL_GetTick(void){return test_ms;}
static inline uint32_t __get_PRIMASK(void){return 0;}
static inline void __disable_irq(void){}
static inline void __enable_irq(void){}
''')
 (t/'linetrack.h').write_text('''#pragma once
#include <stdint.h>
typedef struct{uint8_t l,c,r;} LineState;
void LineTrack_Init(void);
LineState LineTrack_Read(void);
uint8_t LineTrack_IsJunction(LineState);
uint8_t LineTrack_IsLost(LineState);
int8_t LineTrack_Error(LineState);
uint8_t LineTrack_IsCalibrated(void);
''')
 (t/'hc.h').write_text('#define HC_NO_TARGET_CM 65535U\n')
 (t/'cmsis_os.h').write_text('')
 uart=(f4/'Core/Src/usart.c').read_text()
 parser=uart[uart.index('#define RX_STATE_IDLE'):uart.index('static void USART3_RestartRx(void)')]
 # Hardware scaffolding is mocked; the state machine under test is extracted verbatim.
 (t/'uart.inc').write_text(parser)
 (t/'test.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "protocol.h"
#include "runtime_control.h"
#include "patrol.h"
#include "linetrack.h"
#define PID_TUNING_ENABLED 0
uint32_t test_ms=0;
void BTDbg_OnRxByte(uint8_t b){(void)b;}
void BTDbg_OnTelemetryReject(const uint8_t *p){(void)p;}
void BTDbg_OnGoodFrame(const uint8_t*p,int a,int b,int c){(void)p;(void)a;(void)b;(void)c;}
void BTDbg_OnBadFrame(const uint8_t*p,int a){(void)p;(void)a;}
#include "uart.inc"
static LineState line={0,1,0};
void LineTrack_Init(void){}
uint8_t LineTrack_IsCalibrated(void){return 1;}
LineState LineTrack_Read(void){return line;}
uint8_t LineTrack_IsJunction(LineState s){return s.l && s.c && s.r;}
uint8_t LineTrack_IsLost(LineState s){return !(s.l || s.c || s.r);}
int8_t LineTrack_Error(LineState s){return s.c?0:s.l?-2:2;}
static void feed(uint8_t *f,int n){for(int i=0;i<n;i++){test_ms++;USART3_ProcessRxByte(f[i]);}}
int main(void){
  (void)rx_byte;
  assert(Runtime_Speed()==500);
  uint8_t line_status[16]; Runtime_Status(line_status); assert(line_status[15]==1);
  Runtime_SetLineReady(1); Runtime_Status(line_status); assert(line_status[15]==0);
  Runtime_SetLineReady(0); Runtime_Status(line_status); assert(line_status[15]==1);
  ThermalPolicy p={0};
  assert(Control_Thermal(&p,0,TH_AUTO,0,0,0,1)==TH_BOOT);
  assert(Control_Thermal(&p,179999,TH_AUTO,0,0,0,1)==TH_BOOT);
  assert(Control_Thermal(&p,180000,TH_AUTO,0,0,0,1)==0);
  assert(Control_Thermal(&p,0xFFFFFFFE,TH_AUTO,0,0,0,1)==0);
  assert(Control_Thermal(&p,10,TH_AUTO,0,0,0,1)==0);
  for(int a=TH_GAS;a<=TH_HOT;a*=2){
    ThermalPolicy q={1,0,0,0,0};
    assert(Control_Thermal(&q,200000,TH_OFF,a,0,0,1)&a);
    assert(Control_Thermal(&q,229999,TH_OFF,0,0,0,0)&TH_VERIFY);
    assert(Control_Thermal(&q,260000,TH_OFF,0,0,0,0)&TH_VERIFY);
    assert(Control_Thermal(&q,260001,TH_OFF,0,0,0,1)==TH_HOLD);
    assert(Control_Thermal(&q,290000,TH_OFF,0,0,0,0)==TH_HOLD);
    assert(Control_Thermal(&q,290001,TH_OFF,0,0,0,0)==0);
  }
  ThermalPolicy q={1,0,0,0,0};
  assert(Control_Thermal(&q,200000,TH_OFF,0,1,45,1)&TH_AMBIENT);
  assert(Control_Thermal(&q,200100,TH_OFF,0,1,43,1)&TH_AMBIENT);
  assert(!(Control_Thermal(&q,200200,TH_OFF,0,1,42,1)&TH_AMBIENT));
  assert(Control_Thermal(&q,240200,TH_OFF,0,0,80,1)==0);
  q=(ThermalPolicy){1,0,0,0,0};
  Control_Thermal(&q,0xFFFFFFF0,TH_OFF,TH_GAS,0,0,1);
  assert(Control_Thermal(&q,10,TH_OFF,0,0,0,1)==TH_HOLD);
  assert(Control_Thermal(&q,30010,TH_OFF,0,0,0,0)==0);
  assert(Control_Ramp(0,1000,100)==50);
  assert(Control_Ramp(500,0,100)==0);
  assert(Control_Ramp(500,-500,100)==0);
  assert(Control_Ramp(500,100,100)==100);
  assert(Control_Ramp(-500,-1000,100)==-550);
  for(int gear=0;gear<=19;gear++){
    Runtime_SpeedRequest(gear,1); int cap=100+100*gear;
    assert(Runtime_Speed()==cap);
    for(int speed=-2000;speed<=2000;speed++) assert(Control_Clamp(speed,cap)<=cap && Control_Clamp(speed,cap)>=-cap);
    Patrol_Init(); Patrol_Start(); line=(LineState){0,1,0};
    int16_t speed,steer; Patrol_Update(&speed,&steer,65535); assert(speed==cap);
    Patrol_Update(&speed,&steer,26); assert(speed>=0 && speed<cap);
    line=(LineState){0,0,0}; Patrol_Update(&speed,&steer,65535); assert(speed<=cap && speed<=220);
  }
  Runtime_SpeedRequest(20,2); assert(Runtime_Speed()==2000);
  Runtime_SpeedRequest(255,2); assert(Runtime_Speed()==2000);
  Patrol_Pause(); Patrol_Update(&(int16_t){0},&(int16_t){0},1);
  Patrol_Update(&(int16_t){0},&(int16_t){0},65535); assert(Patrol_GetState()==PATROL_ST_HOLD);
  test_ms=300000; Runtime_ThermalRequest(TH_ON,0,4,1,0);
  assert(Runtime_ThermalReason(test_ms)&TH_MANUAL);
  assert(Runtime_TakeEepromRequest()==1 && Runtime_TakeEepromRequest()==0);
  test_ms+=5000; assert(Runtime_ThermalReason(test_ms)==0);
  uint8_t cfg[12]={0xAA,0x63,8,TH_OFF,1,0,7,0,0,0,0,0};
  cfg[11]=Protocol_CalculateChecksum(cfg,11); feed(cfg,12);
  assert(Runtime_Speed()==100);
  assert(Runtime_ThermalReason(test_ms)&TH_VFIRE);
  uint8_t st[16]; Runtime_Status(st); assert(st[1]<=3 && st[6]==0 && st[7]==7);
  cfg[5]=19; cfg[11]^=1; feed(cfg,12); assert(Runtime_Speed()==100);
  cfg[11]=Protocol_CalculateChecksum(cfg,11); feed(cfg,12); assert(Runtime_Speed()==2000);
  cfg[5]=20; cfg[11]=Protocol_CalculateChecksum(cfg,11); feed(cfg,12); assert(Runtime_Speed()==2000);
  test_ms+=40000; Runtime_LegacyVision(2);
  assert(Runtime_ThermalReason(test_ms)&TH_VFIRE);
  test_ms+=3001; assert(!(Runtime_ThermalReason(test_ms)&TH_VFIRE));
  assert(Runtime_ThermalReason(test_ms)&TH_VERIFY);
  Runtime_ThermalRequest(TH_OFF,8,8,0,1);
  assert(Runtime_ThermalReason(test_ms)==TH_HOLD);
  test_ms+=30000; assert(Runtime_ThermalReason(test_ms)==0);
  test_ms+=100;
  for(int mode=0;mode<4;mode++){
    uint8_t cmd[7]={0xAA,0,0xAA,0,0xAA,(uint8_t)mode,0};
    cmd[6]=Protocol_CalculateChecksum(cmd,6); g_remote_ready=0; feed(cmd,7);
    assert(g_remote_ready && g_remote_data.speed==170 && g_remote_data.steering==170 && g_remote_data.mode==mode);
    cmd[5]|=0x80; cmd[6]=Protocol_CalculateChecksum(cmd,6);g_remote_ready=0;feed(cmd,7);assert(!g_remote_ready);
  }
  uint8_t invalid[7]={0xAA,0,100,0,0,4,0};
  invalid[6]=Protocol_CalculateChecksum(invalid,6); g_remote_ready=0;feed(invalid,7);assert(!g_remote_ready);
  puts("PASS: F4 20 gears, ramp/stop/reversal, patrol safety, thermal startup/override/hold/wrap, lease and real UART FSM");
}
''')
 cmd=['cc','-std=c11','-Wall','-Wextra','-I'+str(t),'-I'+str(f4/'Core/Protocol'),'-I'+str(f4/'Core/Patrol'),str(t/'test.c'),str(f4/'Core/Protocol/protocol.c'),str(f4/'Core/Patrol/runtime_control.c'),str(f4/'Core/Patrol/patrol.c'),'-o',str(t/'test')]
 subprocess.run(cmd,check=True)
 subprocess.run([str(t/'test')],check=True)
