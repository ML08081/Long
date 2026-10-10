"""Compile actual bus recovery / VL53 sources against failure-injecting HAL mocks."""
from pathlib import Path
import subprocess, sys, tempfile

f4=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='i2c-recovery-') as td:
    p=Path(td)
    (p/'stm32f4xx_hal.h').write_text(r'''#pragma once
#include <stdint.h>
typedef enum { HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT } HAL_StatusTypeDef;
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { uint32_t CR1; } I2C_TypeDef;
typedef struct { I2C_TypeDef *Instance; } I2C_HandleTypeDef;
typedef struct { int bus; } GPIO_TypeDef;
typedef struct { uint32_t Pin,Mode,Pull,Speed,Alternate; } GPIO_InitTypeDef;
extern I2C_TypeDef regs[2];
extern GPIO_TypeDef ports[3];
#define I2C2 (&regs[0])
#define I2C3 (&regs[1])
#define GPIOA (&ports[0])
#define GPIOB (&ports[1])
#define GPIOC (&ports[2])
#define GPIO_PIN_8 256
#define GPIO_PIN_9 512
#define GPIO_PIN_10 1024
#define GPIO_PIN_11 2048
#define GPIO_MODE_OUTPUT_OD 1
#define GPIO_MODE_AF_OD 2
#define GPIO_PULLUP 1
#define GPIO_SPEED_FREQ_LOW 1
#define GPIO_AF4_I2C2 4
#define GPIO_AF4_I2C3 4
#define __NOP() ((void)0)
void clock_on(int);
void force_reset(int);
void release_reset(int);
#define __HAL_RCC_I2C2_CLK_ENABLE() clock_on(0)
#define __HAL_RCC_I2C3_CLK_ENABLE() clock_on(1)
#define __HAL_RCC_I2C2_FORCE_RESET() force_reset(0)
#define __HAL_RCC_I2C3_FORCE_RESET() force_reset(1)
#define __HAL_RCC_I2C2_RELEASE_RESET() release_reset(0)
#define __HAL_RCC_I2C3_RELEASE_RESET() release_reset(1)
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef*);
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef*,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef*,uint16_t,uint8_t*,uint16_t,uint32_t);
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*,uint16_t,uint32_t,uint32_t);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*,uint16_t);
uint32_t HAL_GetTick(void);
void HAL_Delay(uint32_t);
''')
    (p/'main.h').write_text('#include "stm32f4xx_hal.h"\n#define VL53_XSHUT_GPIO_Port GPIOB\n#define VL53_XSHUT_Pin 8192\n')
    (p/'i2c.h').write_text('#include "stm32f4xx_hal.h"\nextern I2C_HandleTypeDef hi2c2,hi2c3;\nvoid MX_I2C2_Init(void);\nvoid MX_I2C3_Init(void);\n')
    (p/'watchdog.h').write_text('#define WDG_FAULT_SENSOR 1\nvoid WDG_ReportFault(int);\n')
    source=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "main.h"
#include "i2c.h"
I2C_TypeDef regs[2]; GPIO_TypeDef ports[3];
I2C_HandleTypeDef hi2c2={I2C2},hi2c3={I2C3};
static int clocks[2]={1,1}, resets[2], released[2], deinits[2], inits[2], fault;
static int active=-1, stuck_scl, stuck_sda, calls, fail_call, reg, ready=7;
static uint32_t ms;
uint32_t HAL_GetTick(void){return ms;}
void HAL_Delay(uint32_t delay){ms+=delay;}
HAL_StatusTypeDef HAL_I2C_DeInit(I2C_HandleTypeDef*h){active=h->Instance==I2C2?0:1;clocks[active]=0;deinits[active]++;return HAL_OK;}
void clock_on(int bus){assert(bus==active && deinits[bus]);clocks[bus]=1;}
void force_reset(int bus){assert(bus==active && clocks[bus]);resets[bus]++;}
void release_reset(int bus){assert(resets[bus] && clocks[bus]);released[bus]++;}
void MX_I2C2_Init(void){assert(released[0] && clocks[0]);inits[0]++;active=-1;}
void MX_I2C3_Init(void){assert(released[1] && clocks[1]);inits[1]++;active=-1;}
void HAL_GPIO_Init(GPIO_TypeDef*port,GPIO_InitTypeDef*g){(void)port;if(g->Mode==GPIO_MODE_OUTPUT_OD)assert(active>=0 && released[active]);}
void HAL_GPIO_WritePin(GPIO_TypeDef*p,uint16_t pin,GPIO_PinState s){(void)p;(void)pin;(void)s;}
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef*p,uint16_t pin){(void)p;return ((pin==GPIO_PIN_8 && stuck_scl)||(pin==GPIO_PIN_9 && stuck_sda))?GPIO_PIN_RESET:GPIO_PIN_SET;}
void WDG_ReportFault(int code){assert(code==1);fault++;}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef*h,uint16_t addr,uint8_t*b,uint16_t n,uint32_t timeout){
 (void)addr;(void)timeout;assert(h==&hi2c2);calls++;if(calls==fail_call)return HAL_TIMEOUT;if(n==1)reg=b[0];return HAL_OK;}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef*h,uint16_t addr,uint8_t*b,uint16_t n,uint32_t timeout){
 (void)addr;(void)timeout;assert(h==&hi2c2);calls++;if(calls==fail_call)return HAL_TIMEOUT;memset(b,0,n);if(reg==0x13)b[0]=ready; if(reg==0x1e && n==2){b[0]=1;b[1]=244;}return HAL_OK;}
HAL_StatusTypeDef HAL_I2C_IsDeviceReady(I2C_HandleTypeDef*h,uint16_t a,uint32_t n,uint32_t t){(void)h;(void)a;(void)n;(void)t;return HAL_OK;}
'''
    # Include the complete real sources; file-static driver state remains accessible to
    # the harness for placing the sensor in a normal running state before fault injection.
    source+='#include "'+(f4/'Core/I2CFix/i2c_recover.c').as_posix()+'"\n'
    source+='#include "'+(f4/'Core/VL53L0X/vl53l0x.c').as_posix()+'"\n'
    source+=r'''
static void running(void){s_present=1;s_continuous=1;s_io_error=0;s_last_range_ms=ms;calls=0;fail_call=0;ready=7;}
int main(void){
 assert(I2C3_BusRecover()==1);assert(inits[1]==1 && resets[1]==1 && !resets[0]);
 assert(I2C2_BusRecover()==1);assert(inits[0]==1 && resets[0]==1 && deinits[0]==1);
 stuck_scl=1;assert(!I2C3_BusRecover());assert(fault==1);stuck_scl=0;
 stuck_sda=1;assert(!I2C3_BusRecover());assert(fault==2);stuck_sda=0;
 assert(I2C_RecoverCount()==4);
 uint16_t mm=1234;running();assert(VL53_TryReadRange_mm(&mm) && mm==500);
 for(int fail=1;fail<=5;fail++){
   running();fail_call=fail;mm=1234;assert(!VL53_TryReadRange_mm(&mm));
   assert(mm==1234 && !VL53_IsPresent() && calls==fail);
   assert(!VL53_TryReadRange_mm(&mm) && calls==fail);
 }
 running();ready=0;ms+=251;assert(!VL53_TryReadRange_mm(&mm) && !VL53_IsPresent());
 running();fail_call=1;s_continuous=0;VL53_StartContinuous();assert(!s_continuous && !s_present && calls==1);
 running();fail_call=1;assert(VL53_Init()<0 && calls==1 && !s_present);
 puts("PASS: RCC reset after clock-off, bus isolation, stuck lines, VL53 TX/RX/clear failures, stale-ready recovery and retry suppression");
}
'''
    (p/'test.c').write_text(source)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(p),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
