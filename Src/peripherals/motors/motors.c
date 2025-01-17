/**************************************************************************/
/*!
    @file    motors.c
    @author  PLF (PACABOT)
    @date
    @version  0.0
 */
/**************************************************************************/
/* STM32 hal library declarations */
#include "stm32f4xx_hal.h"

/* General declarations */
#include "config/basetypes.h"
#include "config/config.h"
#include "config/errors.h"

#include "stdbool.h"
#include <arm_math.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

/* Peripheral declarations */
#include "peripherals/display/ssd1306.h"
#include "peripherals/display/smallfonts.h"
#include "peripherals/expander/pcf8574.h"

/* Middleware declarations */

/* Declarations for this module */
#include "peripherals/motors/motors.h"

extern TIM_HandleTypeDef MOTORS_TIMER;

motor left_motor =
{
		LEFT_MOTOR_IN1,
		LEFT_MOTOR_IN2
};

motor right_motor =
{
		RIGHT_MOTOR_IN1,
		RIGHT_MOTOR_IN2
};

void motorsInit(void)
{
	TIM_OC_InitTypeDef sConfigOC;
	TIM_ClockConfigTypeDef sClockSourceConfig;
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig;
	TIM_MasterConfigTypeDef sMasterConfig;
	uint32_t uwPrescalerValue = 0;

	/* Compute the prescaler value to have TIM7 counter clock equal to 10 KHz */
	uwPrescalerValue = (uint32_t) ((SystemCoreClock /2) / (MOTORS_FREQ* MOTORS_PERIOD));

	htim8.Instance = TIM8;
	htim8.Init.Prescaler = uwPrescalerValue;
	htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim8.Init.Period = MOTORS_PERIOD;
	htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim8.Init.RepetitionCounter = 0;
	HAL_TIM_Base_Init(&htim8);

	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig);

	HAL_TIM_PWM_Init(&htim8);

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_LOW;
	sConfigOC.OCNPolarity = TIM_OCPOLARITY_LOW;
	sConfigOC.OCFastMode = TIM_OCFAST_ENABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
	sConfigOC.OCNIdleState = TIM_OCIDLESTATE_SET;
	htim8.Instance->CR2 &= ~TIM_CR2_CCPC;
	HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_1);
	HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_2);
	HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_3);
	HAL_TIM_PWM_ConfigChannel(&htim8, &sConfigOC, TIM_CHANNEL_4);

	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_LOW;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	HAL_TIMEx_ConfigBreakDeadTime(&htim8, &sBreakDeadTimeConfig);

	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig);

	motorsSleepDriver(ON);
}

void motorsSleepDriver(int isOn)
{
	if (isOn == 1)
		HAL_GPIO_WritePin(GPIOA, MOTORS_STANDBY, RESET);
	else
		HAL_GPIO_WritePin(GPIOA, MOTORS_STANDBY, SET);

}

void motorSet(motor *mot, int duty, int isSlowDECAY)
{
	TIM_OC_InitTypeDef sConfigOC;

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	sConfigOC.OCNPolarity = TIM_OCNIDLESTATE_RESET;

	if (duty == 0)
	{
		// Free motor
		HAL_TIM_PWM_Stop(&MOTORS_TIMER, mot->IN1);
		HAL_TIM_PWM_Stop(&MOTORS_TIMER, mot->IN2);
		return;
	}

	// Limit duty cycle to its maximum value
	if (duty > MOTORS_PERIOD)
	{
		duty = MOTORS_PERIOD;
	}
	if (duty < -MOTORS_PERIOD)
	{
		duty = -MOTORS_PERIOD;
	}

	// Reverse left motor
	if (mot == &left_motor)
	{
		duty = (-1 * duty);
	}

	/* if:
	 * 	- Forward Fast DECAY_FAST
	 * 	or
	 * 	- Backward Fast DECAY_FAST
	 */
	if ((duty > 0) && (isSlowDECAY == 0))
	{
		duty = abs(duty);
		if (isSlowDECAY == 1)
		{
			sConfigOC.Pulse = MOTORS_PERIOD - duty;
		}
		else
		{
			sConfigOC.Pulse = duty;
		}
		// Send PWM on IN1
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN1);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN1);

		//Set IN2 to 0
		sConfigOC.Pulse = 0;
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN2);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN2);
	}
	else if ((duty < 0) && (isSlowDECAY == 0))
	{
		duty = abs(duty);
		if (isSlowDECAY == 1)
		{
			sConfigOC.Pulse = MOTORS_PERIOD - duty;
		}
		else
		{
			sConfigOC.Pulse = duty;
		}
		// Send PWM on IN2
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN2);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN2);

		// Set IN1 to 0
		sConfigOC.Pulse = 0;
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN1);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN1);
	}

	/* if:
	 * 	- Forward Slow DECAY_FAST
	 * 	or
	 * 	- Backward Slow DECAY_FAST
	 */
	if ((duty > 0) && (isSlowDECAY == 1))
	{
		duty = abs(duty);
		if (isSlowDECAY == 1)
		{
			sConfigOC.Pulse = MOTORS_PERIOD - duty;
		}
		else
		{
			sConfigOC.Pulse = duty;
		}
		// Send PWM on IN2
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN2);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN2);

		// Set IN1 to 1
		sConfigOC.Pulse = MOTORS_PERIOD;
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN1);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN1);
	}
	else if ((duty < 0) && (isSlowDECAY == 1))
	{
		duty = abs(duty);
		if (isSlowDECAY == 1)
		{
			sConfigOC.Pulse = MOTORS_PERIOD - duty;
		}
		else
		{
			sConfigOC.Pulse = duty;
		}
		// Send PWM on IN1
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN1);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN1);

		// Set IN2 to 1
		sConfigOC.Pulse = MOTORS_PERIOD;
		HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, mot->IN2);
		HAL_TIM_PWM_Start(&MOTORS_TIMER, mot->IN2);
	}
}

void motorsBrake(void)
{
	TIM_OC_InitTypeDef sConfigOC;

	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	sConfigOC.OCNPolarity = TIM_OCNIDLESTATE_RESET;
	sConfigOC.Pulse = MOTORS_PERIOD;
	HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, left_motor.IN1);
	HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, left_motor.IN2);
	HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, right_motor.IN1);
	HAL_TIM_PWM_ConfigChannel(&MOTORS_TIMER, &sConfigOC, right_motor.IN2);
	HAL_TIM_PWM_Start(&MOTORS_TIMER, left_motor.IN1);
	HAL_TIM_PWM_Start(&MOTORS_TIMER, left_motor.IN2);
	HAL_TIM_PWM_Start(&MOTORS_TIMER, right_motor.IN1);
	HAL_TIM_PWM_Start(&MOTORS_TIMER, right_motor.IN2);
}

void motorsTest(void)
{
	int i = 0;
	motorsInit();

	// Forward Fast (PWM on IN1, LOW on IN2)
	motorSet(&left_motor, 0, DECAY_FAST);
	motorSet(&right_motor, 0, DECAY_FAST);
	motorsSleepDriver(OFF);

	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD FAST DECAY 0->20%", &Font_5x8);
	ssd1306Refresh();
	for (i = 0; i < 150; i += 1)
	{
		motorSet(&left_motor, i, DECAY_FAST);
		motorSet(&right_motor, i, DECAY_FAST);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD FAST DECAY 20%", &Font_5x8);
	ssd1306Refresh();
	HAL_Delay(500);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD FAST DECAY 20->0%", &Font_5x8);
	ssd1306Refresh();
	for (i = 150; i > 0; i -= 1)
	{
		motorSet(&left_motor, i, DECAY_FAST);
		motorSet(&right_motor, i, DECAY_FAST);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BRAKE FAST DECAY 0%", &Font_5x8);
	ssd1306Refresh();
	motorsBrake();
	HAL_Delay(4000);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD FAST DECAY 0->20%", &Font_5x8);
	ssd1306Refresh();
	for (i = 0; i > -150; i -= 1)
	{
		motorSet(&left_motor, i, DECAY_FAST);
		motorSet(&right_motor, i, DECAY_FAST);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD FAST DECAY 20%", &Font_5x8);
	ssd1306Refresh();
	HAL_Delay(500);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD FAST DECAY 20->0%", &Font_5x8);
	ssd1306Refresh();
	for (i = -150; i < 0; i += 1)
	{
		motorSet(&left_motor, i, DECAY_FAST);
		motorSet(&right_motor, i, DECAY_FAST);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BRAKE FAST DECAY 0%", &Font_5x8);
	ssd1306Refresh();
	motorsBrake();
	// Slow decay
	HAL_Delay(1000);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD SLOW DECAY 0->20%", &Font_5x8);
	ssd1306Refresh();
	for (i = 0; i < 150; i += 1)
	{
		motorSet(&left_motor, i, DECAY_SLOW);
		motorSet(&right_motor, i, DECAY_SLOW);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD SLOW DECAY 20%", &Font_5x8);
	ssd1306Refresh();
	HAL_Delay(500);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "FWD SLOW DECAY 20->0%", &Font_5x8);
	ssd1306Refresh();
	for (i = 150; i > 0; i -= 1)
	{
		motorSet(&left_motor, i, DECAY_SLOW);
		motorSet(&right_motor, i, DECAY_SLOW);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BRAKE SLOW DECAY 0%", &Font_5x8);
	ssd1306Refresh();
	motorsBrake();
	HAL_Delay(1000);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD SLOW DECAY 0->20%", &Font_5x8);
	ssd1306Refresh();
	for (i = 0; i > -150; i -= 1)
	{
		motorSet(&left_motor, i, DECAY_SLOW);
		motorSet(&right_motor, i, DECAY_SLOW);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD SLOW DECAY 20%", &Font_5x8);
	ssd1306Refresh();
	HAL_Delay(500);
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BWD SLOW DECAY 20->0%", &Font_5x8);
	ssd1306Refresh();
	for (i = -150; i < 0; i += 1)
	{
		motorSet(&left_motor, i, DECAY_SLOW);
		motorSet(&right_motor, i, DECAY_SLOW);
		HAL_Delay(20);
	}
	ssd1306ClearScreen();
	ssd1306DrawString(1,  20,  "BRAK SLOW DECAY 0%", &Font_5x8);
	ssd1306Refresh();
	motorsBrake();

	motorsSleepDriver(ON);
}
