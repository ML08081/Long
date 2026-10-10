"""Compile real line sensing/patrol sources with synthetic ADC/HAL input.

These tests verify recognition and safety logic, not ground calibration.
"""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

f4 = Path(sys.argv[1]).resolve()
compiler = os.environ.get("CC") or shutil.which("cc") or shutil.which("gcc")
if not compiler:
    candidate = Path("F:/QT/Tools/mingw1310_64/bin/gcc.exe")
    if candidate.is_file():
        compiler = str(candidate)
if not compiler:
    raise SystemExit("Host C compiler required")
with tempfile.TemporaryDirectory(prefix="linetrack-test-") as td:
    p = Path(td)
    (p / "stm32f4xx_hal.h").write_text(r'''#pragma once
#include <stdint.h>
extern uint32_t test_ms, test_irq;
static inline uint32_t HAL_GetTick(void){return test_ms;}
static inline uint32_t __get_PRIMASK(void){return test_irq;}
static inline void __disable_irq(void){test_irq=1;}
static inline void __enable_irq(void){test_irq=0;}
''')
    (p / "main.h").write_text('#include "stm32f4xx_hal.h"\n')
    (p / "adc.h").write_text(r'''#pragma once
#include <stdint.h>
#define ADC_CH_LINE_L 1
#define ADC_CH_LINE_C 2
#define ADC_CH_LINE_R 3
extern volatile uint16_t g_adc_dma[4];
uint8_t ADC_IsStreaming(void);
''')
    (p / "hc.h").write_text('#define HC_NO_TARGET_CM 65535U\n')
    (p / "cmsis_os.h").write_text('')
    (p / "test.c").write_text(r'''
#include <assert.h>
#include <stdio.h>
#include "linetrack.h"
#include "patrol.h"
uint32_t test_ms, test_irq;
volatile uint16_t g_adc_dma[4];
static uint8_t streaming=1;
static uint16_t cap=500;
uint8_t ADC_IsStreaming(void){return streaming;}
uint16_t Runtime_Speed(void){return cap;}
/* Deliberately unequal synthetic channels; not hardware measurements. */
static const uint16_t bg[3]={2790,1065,2140}, line[3]={3900,2600,3400};
static LineState sample(unsigned bits, unsigned cycles){
  for(int i=0;i<3;i++)g_adc_dma[i+1]=(bits&(1U<<i))?line[i]:bg[i];
  LineState s={0};
  while(cycles--){test_ms+=5;s=LineTrack_Read();assert(!test_irq);}
  return s;
}
static PatrolEventT tick(unsigned bits,uint32_t elapsed,int16_t *v,int16_t *a){
  sample(bits,8);test_ms+=elapsed;return Patrol_Update(v,a,65535);
}
int main(void){
  LineTrack_Init();
  /* Even an initial DMA impulse must not permanently seed a false endpoint. */
  for(int i=1;i<=3;i++)g_adc_dma[i]=4095;
  test_ms+=5;LineTrack_Read();
  sample(0,20);assert(!LineTrack_IsCalibrated());
  Patrol_Init();Patrol_Start();int16_t v=99,a=99;
  Patrol_Update(&v,&a,65535);assert(v==0&&a==0);
  /* A single ADC impulse cannot teach a black endpoint. */
  g_adc_dma[1]=4095;test_ms+=5;LineTrack_Read();sample(0,8);
  assert(!LineTrack_IsCalibrated());
  /* Each channel must actually see both surfaces before autonomy starts. */
  sample(1,8);sample(0,8);assert(!LineTrack_IsCalibrated());
  sample(2,8);sample(0,8);assert(!LineTrack_IsCalibrated());
  sample(4,8);sample(0,8);assert(LineTrack_IsCalibrated());
  uint16_t off[3],on[3];uint8_t ready;
  LineTrack_GetThresholds(off,on,&ready);assert(ready==7);
  for(int i=0;i<3;i++){
    assert(off[i]==bg[i]+(line[i]-bg[i])*40U/100U);
    assert(on[i]==bg[i]+(line[i]-bg[i])*60U/100U);
  }
  /* No decay: a long spell on background must remain lost. */
  sample(0,1000);assert(LineTrack_IsLost(LineTrack_Read()));
  streaming=0;LineTrack_Read();assert(!LineTrack_IsCalibrated());
  streaming=1;sample(0,8);assert(LineTrack_IsCalibrated());
  LineTrack_GetThresholds(off,on,&ready);
  for(int i=0;i<3;i++)assert(on[i]==bg[i]+(line[i]-bg[i])*60U/100U);
  const int8_t errors[8]={0,-2,0,-1,2,0,1,0};
  for(unsigned bits=0;bits<8;bits++){
    LineState s=sample(bits,8);
    assert((LineTrack_PackBits(s)&7U)==bits);
    assert(LineTrack_Error(s)==errors[bits]);
    assert(LineTrack_IsLost(s)==(bits==0));
    assert(LineTrack_IsJunction(s)==(bits==7));
  }
  /* Repeated consumers within one tick do not advance median/debounce. */
  sample(0,8);g_adc_dma[1]=line[0];test_ms+=5;
  for(int n=0;n<100;n++)assert(!LineTrack_Read().l);
  test_ms+=5;assert(!LineTrack_Read().l);
  test_ms+=5;assert(LineTrack_Read().l);
  /* Hysteresis holds either state in the dead band. */
  g_adc_dma[1]=(uint16_t)((off[0]+on[0])/2U);
  for(int n=0;n<8;n++){test_ms+=5;assert(LineTrack_Read().l);}
  sample(0,8);g_adc_dma[1]=(uint16_t)((off[0]+on[0])/2U);
  for(int n=0;n<8;n++){test_ms+=5;assert(!LineTrack_Read().l);}
  /* Invalid two-point calibration is rejected atomically. */
  uint16_t bad[3]={bg[0]+1,line[1],line[2]};
  assert(!LineTrack_SetCalibration(bg,bad));assert(LineTrack_IsCalibrated());
  /* Each channel's polarity can differ, without rewiring. */
  uint16_t reversed[3]={1000,2600,500};
  assert(LineTrack_SetCalibration(bg,reversed));
  for(int i=0;i<3;i++)g_adc_dma[i+1]=reversed[i];
  for(int n=0;n<8;n++){test_ms+=5;LineTrack_Read();}
  assert(LineTrack_IsJunction(LineTrack_Read()));sample(0,8);
  assert(LineTrack_IsLost(LineTrack_Read()));
  assert(LineTrack_SetCalibration(bg,line));sample(0,8);
  /* Saturated/stopped ADC cannot retain motion permission. */
  sample(2,8);streaming=0;assert(!LineTrack_IsCalibrated());
  assert(LineTrack_IsLost(LineTrack_Read()));
  Patrol_Start();Patrol_Update(&v,&a,65535);assert(v==0&&a==0);
  streaming=1;sample(0,8);
  /* Never saw a line: do not drive blindly or report an initial full-bright junction. */
  Patrol_Start();tick(0,0,&v,&a);assert(v==0);
  tick(7,0,&v,&a);assert(v==0&&Patrol_GetPointSeq()==0);
  tick(2,0,&v,&a);assert(v==cap&&a==0&&LineTrack_IsCalibrated());
  tick(3,0,&v,&a);assert(a==-350);
  tick(1,0,&v,&a);assert(a==-700);
  /* Short loss remembers correction, and real elapsed time controls stopping. */
  tick(0,0,&v,&a);assert(v<=220&&a==-700);
  test_ms+=799;Patrol_Update(&v,&a,65535);assert(v>0);
  test_ms+=1;assert(Patrol_Update(&v,&a,65535)==PATROL_EV_LINE_LOST);
  assert(v==0&&Patrol_GetState()==PATROL_ST_LOST);
  tick(7,0,&v,&a);assert(Patrol_GetState()==PATROL_ST_LOST);
  tick(2,0,&v,&a);assert(Patrol_GetState()==PATROL_ST_FOLLOW&&v==0);
  Patrol_Update(&v,&a,65535);assert(v==cap);
  assert(tick(7,0,&v,&a)==PATROL_EV_NONE);
  test_ms+=49;assert(Patrol_Update(&v,&a,65535)==PATROL_EV_NONE);
  test_ms+=1;assert(Patrol_Update(&v,&a,65535)==PATROL_EV_ARRIVE);
  assert(v==0&&Patrol_GetPointSeq()==1);
  Patrol_ResumeFromPoint(PATROL_ACT_LEFT);
  test_ms+=3000;assert(Patrol_Update(&v,&a,65535)==PATROL_EV_LINE_LOST);
  assert(v==0&&Patrol_GetState()==PATROL_ST_LOST);
  /* Unsigned sampling deadlines also work across tick wrap. */
  LineTrack_Init();assert(LineTrack_SetCalibration(bg,line));test_ms=0xfffffff0U;
  assert(LineTrack_IsLost(sample(0,8)));assert(sample(2,8).c);
  puts("PASS: real ADC line/patrol sources: per-channel learning, hysteresis, median impulses, polarity, fixed sampling, all patterns, uncalibrated/stopped-ADC gating, junction confirmation and bounded line/turn loss");
}
''')
    includes = [p, f4 / "Core/LineTrack", f4 / "Core/Patrol"]
    cmd = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror"]
    cmd += ["-I" + str(folder) for folder in includes]
    binary = p / ("test.exe" if os.name == "nt" else "test")
    cmd += [str(p / "test.c"), str(f4 / "Core/LineTrack/linetrack.c"),
            str(f4 / "Core/Patrol/patrol.c"), "-o", str(binary)]
    subprocess.run(cmd, check=True)
    subprocess.run([str(binary)], check=True)
