#include "motor.h"
#include "tim.h"

/* 上一周期实际下发的占空比，用于斜坡限幅 */
static int16_t s_last_out1 = 0;
static int16_t s_last_out2 = 0;

/**
 * 设置 PWM 载波频率。必须在 MX_TIM1_Init() 之后调用。
 * 只改预分频，不动 Period（Period=999 保证 1000 级分辨率与 MAX_SPEED 对齐）。
 */
void Motor_Init(void)
{
	__HAL_TIM_SET_PRESCALER(&htim1, MOTOR_PWM_PRESCALER);
	s_last_out1 = 0;
	s_last_out2 = 0;
}

/**
 * 死区补偿 + 斜坡限幅。
 *  1) 死区补偿：非零指令抬到至少 MOTOR_MIN_DUTY，保证每个指令都真的产生动作；
 *     绝对值小于 MOTOR_DEADZONE 的当作停车，避免零位附近来回抖。
 *  2) 斜坡限幅：限制相对上一周期的变化量，把前进/后退切换这类突变摊成斜坡，
 *     消除电流冲击与机械顿挫。
 */
static int16_t shape_output(int16_t speed, int16_t *last)
{
	/* 限幅 */
	if(speed >  MAX_SPEED) speed =  MAX_SPEED;
	if(speed < -MAX_SPEED) speed = -MAX_SPEED;

	/* 死区补偿 */
	if(speed > -MOTOR_DEADZONE && speed < MOTOR_DEADZONE)
	{
		speed = 0;
	}
	else if(speed > 0 && speed < MOTOR_MIN_DUTY)
	{
		speed = MOTOR_MIN_DUTY;
	}
	else if(speed < 0 && speed > -MOTOR_MIN_DUTY)
	{
		speed = -MOTOR_MIN_DUTY;
	}

	/* 斜坡限幅（停车指令不限速，安全优先：要停就立刻停） */
	if(speed != 0)
	{
		int32_t diff = (int32_t)speed - (int32_t)(*last);
		if(diff >  MOTOR_SLEW_PER_TICK) speed = (int16_t)(*last + MOTOR_SLEW_PER_TICK);
		if(diff < -MOTOR_SLEW_PER_TICK) speed = (int16_t)(*last - MOTOR_SLEW_PER_TICK);
	}

	*last = speed;
	return speed;
}

/*********************************************************
  函数功能：设置电机1转速与方向
  入口参数：speed：-1000 ~ 1000
*********************************************************/
void Motor1_Set(int16_t speed)
{
	speed = shape_output(speed, &s_last_out1);

	if(speed > 0)
	{
		// 正转
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, 0);    // PH = 0
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, speed); // EN = 速度
	}
	else if(speed < 0)
	{
		// 反转
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, MAX_SPEED); // PH = 1
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, -speed);
	}
	else
	{
		// 刹车
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 0);
	}
}

/*********************************************************
  函数功能：设置电机2转速与方向
*********************************************************/
void Motor2_Set(int16_t speed)
{
	speed = shape_output(speed, &s_last_out2);

	if(speed > 0)
	{
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 0);
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, speed);
	}
	else if(speed < 0)
	{
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, MAX_SPEED);
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, -speed);
	}
	else
	{
		__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_3, 0);
	}
}

/*********************************************************
  函数功能：使能DRV8701驱动（唤醒）
  注：模块自带外部开关，PB0 不由软件控制，
      此处保留函数签名以供兼容调用。
*********************************************************/
void DRV8701_Enable(void)
{
	// 硬件外部开关已使能，无需软件控制
	// HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);
}

/*********************************************************
  函数功能：DRV8701进入休眠
  注：模块自带外部开关，PB0 不由软件控制。
*********************************************************/
void DRV8701_Sleep(void)
{
	// 硬件外部开关已使能，无需软件控制
	// HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}

/*********************************************************
  函数功能：读取DRV8701故障状态
  返回值：1=故障，0=正常
*********************************************************/
uint8_t DRV8701_ReadFault(void)
{
	// ⚠ PB1 已被 Servo2（云台舵机软件 PWM）占用，不可同时用于 DRV8701 故障读取。
	// 如需要故障检测功能，请将 nFAULT 接到其他空闲 GPIO 并更新此函数。
	// 当前实现始终返回 0（无故障），避免编译警告。
	return 0;
}