"""Compile actual motor/encoder drivers with hardware registers mocked."""
from pathlib import Path
import os, subprocess, tempfile, sys
f4=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='motor-encoder-') as td:
    p=Path(td)
    (p/'stm32f4xx_hal.h').write_text('''#pragma once
#include <stdint.h>
typedef struct { uint32_t CNT,CCR[4],PSC; } TIM_TypeDef;
typedef struct { TIM_TypeDef *Instance; } TIM_HandleTypeDef;
typedef int GPIO_PinState;
#define GPIO_PIN_SET 1
#define GPIO_PIN_RESET 0
#define TIM_CHANNEL_1 0
#define TIM_CHANNEL_2 1
#define TIM_CHANNEL_3 2
#define TIM_CHANNEL_4 3
extern TIM_TypeDef test_tim1,test_tim3,test_tim4;
extern TIM_HandleTypeDef htim1,htim3,htim4;
#define TIM3 (&test_tim3)
#define TIM4 (&test_tim4)
#define __HAL_TIM_SET_COMPARE(h,c,v) ((h)->Instance->CCR[c]=(v))
#define __HAL_TIM_SET_PRESCALER(h,v) ((h)->Instance->PSC=(v))
int HAL_TIM_Base_Start(TIM_HandleTypeDef*);
GPIO_PinState HAL_GPIO_ReadPin(int,int);
''')
    (p/'main.h').write_text('''#pragma once
#define ENC1_DIR_GPIO_Port 1
#define ENC2_DIR_GPIO_Port 2
#define ENC1_DIR_Pin 5
#define ENC2_DIR_Pin 13
''')
    (p/'tim.h').write_text('#include "stm32f4xx_hal.h"\n')
    (p/'test.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include "motor.h"
#include "encoder.h"
TIM_TypeDef test_tim1={0},test_tim3={0},test_tim4={0};
TIM_HandleTypeDef htim1={&test_tim1},htim3={&test_tim3},htim4={&test_tim4};
int dir1=0,dir2=1;
int HAL_TIM_Base_Start(TIM_HandleTypeDef*h){(void)h;return 0;}
GPIO_PinState HAL_GPIO_ReadPin(int port,int pin){(void)pin;return port==1?dir1:dir2;}
int main(void){
  Motor_Init();Motor1_Set(1);Motor2_Set(1);
  assert(test_tim1.CCR[0]==1 && test_tim1.CCR[2]==1);
  for(int n=0;n<100;n++){Motor1_Set(1000);Motor2_Set(1000);}
  assert(test_tim1.CCR[0]==500 && test_tim1.CCR[2]==500);
  Motor1_Set(0);Motor2_Set(0);assert(!test_tim1.CCR[0] && !test_tim1.CCR[2]);
  Motor1_Set(-1);Motor2_Set(-1);
  assert(test_tim1.CCR[0]==1 && test_tim1.CCR[2]==1);
  assert(test_tim1.CCR[1]==1000 && test_tim1.CCR[3]==1000);
  test_tim3.CNT=65530;test_tim4.CNT=65530;Encoder_Init();
  test_tim3.CNT=4;test_tim4.CNT=4;Encoder_UpdateElapsed(17000);
  assert(Encoder_IsSampleValid() && Encoder1_GetDelta()==10 && Encoder2_GetDelta()==10);
  dir1=1;dir2=0;test_tim3.CNT+=20;test_tim4.CNT+=20;Encoder_UpdateElapsed(17000);
  assert(Encoder1_GetDelta()==-20 && Encoder2_GetDelta()==-20);
  assert(Encoder1_GetCount()==-10 && Encoder2_GetCount()==-10);
  dir1=0;dir2=1;test_tim3.CNT+=2000;test_tim4.CNT+=2000;Encoder_UpdateElapsed(34000);
  assert(Encoder_IsSampleValid() && Encoder1_GetDelta()==2000 && Encoder2_GetDelta()==2000);
  int32_t previous=Encoder1_GetCount();
  test_tim3.CNT+=2000;test_tim4.CNT+=2000;Encoder_UpdateElapsed(17000);
  assert(!Encoder_IsSampleValid() && Encoder1_GetDelta()==0 && Encoder2_GetDelta()==0);
  assert(Encoder1_GetCount()==previous);
  test_tim3.CNT+=5;test_tim4.CNT+=5;Encoder_UpdateElapsed(17000);
  assert(Encoder_IsSampleValid() && Encoder1_GetDelta()==5 && Encoder2_GetDelta()==5);
  previous=Encoder1_GetCount();test_tim3.CNT+=5;test_tim4.CNT+=5;Encoder_UpdateElapsed(150000);
  assert(!Encoder_IsSampleValid() && Encoder1_GetCount()==previous);
  puts("PASS: small PWM, hard duty cap, immediate stop, reverse PH, encoder wrap/direction/actual interval/rejected sample");
}
''')
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-I'+str(p),
                    '-I'+str(f4/'Core/Motor'),'-I'+str(f4/'Core/Encoder'),
                    str(p/'test.c'),str(f4/'Core/Motor/motor.c'),str(f4/'Core/Encoder/encoder.c'),
                    '-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
