"""Exercise the real active-low buzzer driver and safe startup ordering."""
from pathlib import Path
import subprocess
import sys
import tempfile

root = Path(sys.argv[1]).resolve()
main = (root/'Core/Src/main.c').read_text()
assert main.index('HAL_Init();') < main.index('Buzzer_EarlySilence();') < main.index('SystemClock_Config();')
with tempfile.TemporaryDirectory(prefix='patrol-buzzer-') as directory:
    out = Path(directory)
    (out/'stm32f4xx_hal.h').write_text(r'''#pragma once
#include <stdint.h>
typedef enum { GPIO_PIN_RESET, GPIO_PIN_SET } GPIO_PinState;
typedef struct { int unused; } GPIO_TypeDef;
typedef struct { uint32_t Pin,Mode,Pull,Speed; } GPIO_InitTypeDef;
extern GPIO_TypeDef port;
#define GPIOB (&port)
#define GPIO_MODE_OUTPUT_OD 2
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
void clock_enable(void);
#define __HAL_RCC_GPIOB_CLK_ENABLE() clock_enable()
void HAL_GPIO_WritePin(GPIO_TypeDef*,uint16_t,GPIO_PinState);
void HAL_GPIO_Init(GPIO_TypeDef*,GPIO_InitTypeDef*);
''')
    (out/'main.h').write_text('#include "stm32f4xx_hal.h"\n#define Buzzer_Pin 256\n#define Buzzer_GPIO_Port GPIOB\n')
    (out/'test.c').write_text(r'''
#include <assert.h>
#include <stdio.h>
#include "buzzer.h"
#include "main.h"
GPIO_TypeDef port;
static int clocked, configured, level=-1, low_writes;
void clock_enable(void) { clocked=1; }
void HAL_GPIO_WritePin(GPIO_TypeDef *p,uint16_t pin,GPIO_PinState value) {
    assert(clocked && p==GPIOB && pin==256);
    level=value; if(value==GPIO_PIN_RESET) ++low_writes;
}
void HAL_GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *gpio) {
    assert(p==GPIOB && gpio->Pin==256 && gpio->Mode==GPIO_MODE_OUTPUT_OD);
    assert(gpio->Pull==GPIO_NOPULL && level==GPIO_PIN_SET);
    configured=1;
}
int main(void) {
    Buzzer_EarlySilence();
    assert(configured && level==GPIO_PIN_SET && !low_writes);
    Buzzer_Init();
    for(int t=0;t<5000;t+=20) Buzzer_Tick(20);
    assert(level==GPIO_PIN_SET && !Buzzer_IsSounding() && !low_writes);
    Buzzer_SetPattern(BUZZER_GAS); Buzzer_Tick(20);
    assert(level==GPIO_PIN_RESET && Buzzer_IsSounding());
    for(int t=20;t<=200;t+=20) Buzzer_Tick(20);
    assert(level==GPIO_PIN_SET && !Buzzer_IsSounding());
    Buzzer_SetPattern(BUZZER_FLAME); Buzzer_Tick(20);
    assert(level==GPIO_PIN_RESET && Buzzer_IsSounding());
    Buzzer_SetPattern(BUZZER_OFF); Buzzer_Tick(20);
    assert(level==GPIO_PIN_SET && !Buzzer_IsSounding());
    Buzzer_On(); assert(level==GPIO_PIN_RESET && Buzzer_IsSounding());
    Buzzer_Off(); assert(level==GPIO_PIN_SET && !Buzzer_IsSounding());
    puts("PASS: early PB8 open-drain release without internal pulls, silent boot, active-low gas/flame alarms and OFF");
}
''')
    exe = out/'test'
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I'+str(out), '-I'+str(root/'Core/Buzzer'), str(root/'Core/Buzzer/buzzer.c'),
                    str(out/'test.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
