/*
 *		____					____
 *  / ____|           / ____|                
 * | (___   ___ _ __ | (___  _   _ _ __ ___  
 *  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
 *  ____) |  __/ | | |____) | |_| | | | | | |
 * |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
 * 
 * 
 * Description : Device driver for the LoRa chip
 * Created by  : James Le
 * Date        : 13/07/2020
 * Maintainer  : Alvaro
 * 
 */


/********************************************************************
 *	Header files		                                               *
 ********************************************************************/
#include <stdint.h>
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "global.h"
#include "debug_uart.h"
#include "flash_map.h"
#include "radio_common.h"

#include "stm32l0xx_ll_dac.h"
#include "stm32l0xx_ll_comp.h"
#include "stm32l0xx_ll_lptim.h"
#include "stm32l0xx_ll_tim.h"
#include "stm32l0xx_ll_cortex.h"

#include "lc_sensor.h"
#include "delays.h"
#include "watchdog.h"

#ifdef DISABLE_LC_SENSOR_DEBUG
	#define Debug_printf(...)
#endif

#define 	FULL_SCALE_12BITS				(__LL_DAC_DIGITAL_SCALE(LL_DAC_RESOLUTION_12B))
#define 	VMID_SCALE						(FULL_SCALE_12BITS/2)
#define 	VCOMP_SCALE						(FULL_SCALE_12BITS*2/3)

#ifdef	ELSTER_WATER_METER
	#define	MAX_PULSES_ELSTER
	#define	METAL_DETECTED_ELSTER
#elif		ITRON_WATER_METER
	#define	MAX_PULSES_ITRON
	#define	METAL_DETECTED_ITRON
#endif


typedef struct
{
	volatile uint16_t counter_before;
	volatile uint16_t counter_after;
	volatile FlagStatus status;
} lc_sensor;

lc_sensor	lc1;
uint32_t timer_clk = 0;

/*!
 *	@brief	Initialize GPIO for LC sensor
 * @param	None
 * @return	None
 * @note		LC1 	-> PB7	->	COMP2_INP (Additional functions)
 * 			LC2 	-> PA3	-> COMP2_INP (Additional functions)
 */
void LC_GPIO_Init(void){

	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);		//	Enable GPIO Clock
	LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);		//	Enable GPIO Clock
	// LL_GPIO_InitTypeDef	GPIO_InitStruct;

	/****************************** Configure PA3 as LC2 input ******************************/
	LL_GPIO_SetPinMode(LC2_PORT, LC2_PIN, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinSpeed(LC2_PORT, LC2_PIN, LL_GPIO_SPEED_FREQ_HIGH);
	LL_GPIO_SetPinPull(LC2_PORT, LC2_PIN, LL_GPIO_PULL_NO);
	/********************************* Finish Configuration *********************************/
	
	/****************************** Configure PB7 as LC1 input ******************************/
	LL_GPIO_SetPinMode(LC1_PORT, LC1_PIN, LL_GPIO_MODE_ANALOG);
	LL_GPIO_SetPinSpeed(LC1_PORT, LC1_PIN, LL_GPIO_SPEED_FREQ_HIGH);
	LL_GPIO_SetPinPull(LC1_PORT, LC1_PIN, LL_GPIO_PULL_NO);
	/********************************* Finish Configuration *********************************/

	/* Pre-set output for LC pins for fast switches */
	LL_GPIO_SetOutputPin(LC2_PORT, LC2_PIN);
	LL_GPIO_SetOutputPin(LC1_PORT, LC1_PIN);
}

/*!
 *	@brief	Initialize voltage references of Vmid & Vcomp
 * @param	None
 * @return	None
 * @note		Vmid 	-> PA5
 * 			Vcomp	-> COMP2_INM (PA4, internal)
 */
void LC_DAC_Init(void){
	
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_DAC1);	// Enable DAC clock

	/* Set Vcomp output for the selected DAC channel */
	LL_DAC_SetOutputBuffer(DAC, VCOMP_DAC_CHANNEL, LL_DAC_OUTPUT_BUFFER_ENABLE);

	/* Set Vmid output for the selected DAC channel */
	LL_DAC_SetOutputBuffer(DAC, VMID_DAC_CHANNEL, LL_DAC_OUTPUT_BUFFER_ENABLE);

	/* Set the data to be loaded in the data holding register */
	// LL_DAC_ConvertDualData12RightAligned(DAC, VCOMP_SCALE, VMID_SCALE);
	LL_DAC_ConvertDualData12RightAligned(DAC, VMID_SCALE, VCOMP_SCALE);
	// LL_DAC_ConvertData12RightAligned(DAC, VCOMP_DAC_CHANNEL, VCOMP_SCALE);

	/* Configure DAC output in analog mode for Vmid */
	LL_GPIO_SetPinMode(GPIOA, VMID_GPIO_PIN, LL_GPIO_MODE_ANALOG);	
	/* Configure DAC output in analog mode for Vcomp */
	LL_GPIO_SetPinMode(GPIOA, VCOMP_GPIO_PIN, LL_GPIO_MODE_ANALOG);
}

/*!
 *	@brief	Initialize LPTM1 settings to accquires pulses from LC sensor
 *				The LPTIM will generate a pulse every time it triggered by COMP2 output
 *	@param	None
 * @return	None 
 */
void LC_LPTIM1_Init(void){

	// LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSI);	// Unnecessary
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1); 		// Enable LPTIM1 clock
	/* 
	LL_LPTIM_InitTypeDef LPTIM_InitStruct;

	LPTIM_InitStruct.ClockSource	= LL_LPTIM_CLK_SOURCE_EXTERNAL;
	LPTIM_InitStruct.Polarity		= LL_LPTIM_OUTPUT_POLARITY_REGULAR;
	LPTIM_InitStruct.Prescaler		= LL_LPTIM_PRESCALER_DIV1;
	//LPTIM_InitStruct.Waveform		= LL_LPTIM_OUTPUT_WAVEFORM_SETONCE;

	LL_LPTIM_Init(LPTIM1, &LPTIM_InitStruct);
	 */

	/* Use External clock source for LPTIM1, not internal clocks */
	LL_LPTIM_SetClockSource(LPTIM1, LL_LPTIM_CLK_SOURCE_EXTERNAL);
	LL_LPTIM_ConfigClock(LPTIM1, LL_LPTIM_CLK_FILTER_NONE, LL_LPTIM_CLK_POLARITY_RISING);
	/* No division */
	LL_LPTIM_SetPrescaler(LPTIM1, LL_LPTIM_PRESCALER_DIV1);
	/* Output polarity: high */
	LL_LPTIM_SetPolarity(LPTIM1, LL_LPTIM_OUTPUT_POLARITY_REGULAR);
	/* Registers are immediately updated after any write access */
	LL_LPTIM_SetUpdateMode(LPTIM1, LL_LPTIM_UPDATE_MODE_IMMEDIATE);
	/* Counter source: External event (from COMP2 output) */
	LL_LPTIM_SetCounterMode(LPTIM1, LL_LPTIM_COUNTER_MODE_EXTERNAL);

	LL_LPTIM_ConfigTrigger(LPTIM1, LL_LPTIM_TRIG_SOURCE_COMP2, 
											 LL_LPTIM_TRIG_FILTER_NONE, 
											 LL_LPTIM_TRIG_POLARITY_RISING);
	
	LL_LPTIM_Enable(LPTIM1);
	/* Settings configured ONLY when LPTIM enabled */
	LL_LPTIM_SetAutoReload(LPTIM1, 0xFFFF);
	/* Start counting in continous mode */
	LL_LPTIM_StartCounter(LPTIM1, LL_LPTIM_OPERATING_MODE_CONTINUOUS);
}

/*! 
 *	@brief	Initialize COMP2 settings to handle inputs from LC sensor & Vcomp
 *	@param	None
 *	@return 	None
 */
void LC_COMP2_Init(void){

	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SYSCFG);	// Enable COMP clock
	/* 
	LL_COMP_InitTypeDef COMP2_InitStruct;

	COMP2_InitStruct.InputMinus 		= LL_COMP_INPUT_MINUS_DAC1_CH1;
	COMP2_InitStruct.InputPlus			= LL_COMP_INPUT_PLUS_IO5;	// for LC1
	// COMP2_InitStruct.InputPlus		= LL_COMP_INPUT_PLUS_IO1;	// for LC2
	COMP2_InitStruct.OutputPolarity 	= LL_COMP_OUTPUTPOL_NONINVERTED;
	COMP2_InitStruct.PowerMode			= LL_COMP_POWERMODE_MEDIUMSPEED;

	LL_COMP_Init(COMP2, &COMP2_InitStruct);
	 */
	LL_COMP_SetPowerMode(COMP2, LL_COMP_POWERMODE_MEDIUMSPEED);
	LL_COMP_SetOutputPolarity(COMP2, LL_COMP_OUTPUTPOL_NONINVERTED);
	LL_COMP_ConfigInputs(COMP2, LL_COMP_INPUT_MINUS_DAC1_CH2, LL_COMP_INPUT_PLUS_IO5);

	// LL_COMP_SetInputPlus(COMP2, LL_COMP_INPUT_PLUS_IO1);

	/* Mapping COMP2 output to LPTIM1 to count pulses */
	LL_COMP_SetOutputLPTIM(COMP2, LL_COMP_OUTPUT_LPTIM1_IN1_COMP2);

#ifndef DISABLE_LC_SENSOR_DEBUG
	/************************ Debug: Configure PA2 as COMP2 output ***********************/
	LL_GPIO_SetPinMode(COMP2_PORT, COPM2_PIN, LL_GPIO_MODE_ALTERNATE);
	LL_GPIO_SetPinOutputType(COMP2_PORT, COPM2_PIN, LL_GPIO_OUTPUT_PUSHPULL);
	LL_GPIO_SetPinPull(COMP2_PORT, COPM2_PIN, LL_GPIO_PULL_NO);
	LL_GPIO_SetPinSpeed(COMP2_PORT, COPM2_PIN, LL_GPIO_SPEED_FREQ_HIGH);

	LL_GPIO_SetAFPin_0_7(COMP2_PORT, COPM2_PIN, LL_GPIO_AF_7);
#endif
}

void LC_TIM7_Init(void){

	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM7);
	/* AHB is not prescaled */
	timer_clk = (LL_RCC_GetAPB1Prescaler() >> RCC_CFGR_PPRE1_Pos);
	if ((timer_clk >> 2) != 0){
		timer_clk = (timer_clk & 0x3) + 1;
		timer_clk = SystemCoreClock/pow(2, timer_clk);
		timer_clk *= 2;
	}

	LL_TIM_SetPrescaler(TIM7, __LL_TIM_CALC_PSC(timer_clk, 1000000));
	//LL_TIM_SetAutoReload(TIM7, 1000);

	/* Automatically disable the timer at the next update event UEV */
	LL_TIM_SetOnePulseMode(TIM7, LL_TIM_ONEPULSEMODE_SINGLE);

	LL_TIM_GenerateEvent_UPDATE(TIM7);

	LL_TIM_EnableIT_UPDATE(TIM7);
	LL_TIM_ClearFlag_UPDATE(TIM7);

	NVIC_ClearPendingIRQ(TIM7_IRQn);
	NVIC_EnableIRQ(TIM7_IRQn);
}

void LC_RTC_WakeUpTimer_Init(void){

	LL_RTC_DisableWriteProtection(RTC);

	LL_RTC_WAKEUP_Disable(RTC);// 
	LL_RTC_ClearFlag_WUT(RTC);	// Clear Wake up flag

	while(!LL_RTC_IsActiveFlag_WUTW(RTC));	// Wait until Wakeup timer configuration update allowed

	#ifndef	DISABLE_LC_SENSOR_DEBUG
	// Execute interrupt every 7s
	LL_RTC_WAKEUP_SetClock(RTC, /* LL_RTC_WAKEUPCLOCK_DIV_16 */ LL_RTC_WAKEUPCLOCK_DIV_2);

	LL_RTC_WAKEUP_SetAutoReload(RTC, /* 14336 */ 512);
	#else
	/*********************************************************************
	 * RTC using LSE oscillator (32.768kHz), target 32Hz output (31.25ms)*
	 * Wakeup Timer Base = 2 / 32.768kHz = 61.03515625 us						*
	 * Auto Reload = 31.25ms / 61.03515625 us = 512								*
	 *********************************************************************/
	LL_RTC_WAKEUP_SetClock(RTC, LL_RTC_WAKEUPCLOCK_DIV_2);

	LL_RTC_WAKEUP_SetAutoReload(RTC, 512);
	#endif

	LL_RTC_EnableIT_WUT(RTC);
	// LL_RTC_WAKEUP_Enable(RTC);

	LL_RTC_EnableWriteProtection(RTC);

	LL_EXTI_EnableIT_0_31(LL_EXTI_LINE_20);
	LL_EXTI_EnableRisingTrig_0_31(LL_EXTI_LINE_20);
}

/*!
 *	@brief	Initialize settings for LC sensors
 *	@param	None
 *	@return 	None
 */
void lc_sensor_init(void){

	// LC_LPM();
	// LC_TIM7_Init();
	LC_RTC_WakeUpTimer_Init();
	
	LC_GPIO_Init();
	LC_LPTIM1_Init();
	LC_COMP2_Init();
	LC_DAC_Init();

}

/*!
 *	@brief	Enable DAC with a delay to make sure it's stable
 *				Forked from Examples_LL library
 * @param	LL_DAC_CHANNEL_1 or
 * 			LL_DAC_CHANNEL_2
 * @return	None
 */
void LC_DAC_Enable(uint32_t mask){

	// __IO uint32_t wait_loop_index = 0;
	/* Enable DAC channel */
	LL_DAC_Enable(DAC, mask);

	/* Delay for DAC channel voltage settling time from DAC channel startup.    */
	/* Compute number of CPU cycles to wait for, from delay in us.              */
	/* Note: Variable divided by 2 to compensate partially                      */
	/*       CPU processing cycles (depends on compilation optimization).       */
	/* Note: If system core clock frequency is below 200kHz, wait time          */
	/*       is only a few CPU processing cycles.                               */
	// delay_us(LL_DAC_DELAY_STARTUP_VOLTAGE_SETTLING_US);	// not working	
	/* 
	wait_loop_index = ((LL_DAC_DELAY_STARTUP_VOLTAGE_SETTLING_US * (SystemCoreClock / (100000 * 2))) / 10);
	while (wait_loop_index != 0)
	{
		wait_loop_index--;
	}
	 */
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
}
/* 
void delay_us_TIM7(uint16_t us){

	// T = (TIM_PSC + 1)(TIM_ARR + 1)/TIM_CLK
	WRITE_REG(TIM7->ARR, __LL_TIM_CALC_ARR(timer_clk, 3, (uint16_t)(1/((float)(us*1e-6)))));
	// SET_BIT(TIM7->EGR, TIM_EGR_UG);	// Reinitialize CNT of the timer
	
	CLEAR_BIT(TIM7->SR, TIM_SR_UIF); 
	NVIC_ClearPendingIRQ(TIM7_IRQn);
	// CLEAR_BIT(SCB->SCR, SCB_SCR_SLEEPDEEP_Msk);// Clear SLEEPDEEP bit of Cortex System Control Register
		
	// SET_BIT(RCC->CFGR, RCC_CFGR_HPRE_DIV64);	// Decrease the HCLK clock during sleep
	SET_BIT(TIM7->CR1, TIM_CR1_CEN);	// Enable TIM7

	while (!READ_BIT(TIM7->SR, TIM_SR_UIF));
	
	// SET_BIT(RCC->CFGR, RCC_CFGR_HPRE_DIV1);// Go back to full speed
}

void TIM7_IRQHandler(){
	CLEAR_BIT(TIM7->SR, TIM_SR_UIF);
}
 */
void lc_sensor_measurement(void){

	reset_watchdog();
	
	/* Enable Vcomp for Comparator 2 */
	// LL_DAC_Enable(DAC, VCOMP_DAC_CHANNEL);
	LL_DAC_Enable(DAC, VMID_DAC_CHANNEL);

	// LC_DAC_Enable(VMID_DAC_CHANNEL);
	LC_DAC_Enable(VCOMP_DAC_CHANNEL);

	/* Enable Comparator 2 */
	//LL_COMP_Enable(COMP2);
	SET_BIT(COMP2->CSR, COMP_CSR_COMP2EN);
	/* Choose LC1 as input of COMP2 */
	MODIFY_REG(COMP2->CSR, COMP_CSR_COMP2INPSEL, COMP_CSR_COMP2INPSEL_2);
	
	// counter_before = READ_REG(LPTIM1->CNT);	// reading counter before a measurement

	/* switch to output pushpull to start oscillation */
	MODIFY_REG(LC1_PORT->MODER, GPIO_MODER_MODE7, GPIO_MODER_MODE7_0);

	/* Minimum width of the excitation pulse, refer to section 2.4.1 of AN4636 */
	// __DSB();// DSB instruction is one clock cycle. Need to do measurements to get an excitation of 2us
	
	/* back to analog input */
	SET_BIT(LC1_PORT->MODER, GPIO_MODER_MODE7_1);
	
	LL_DAC_Disable(DAC, VMID_DAC_CHANNEL);
	// CLEAR_BIT(DAC->CR, DAC_CR_EN1);
	
	/* DAC COMP threshold can be stopped very soon (internal S/H) */
	LL_DAC_Disable(DAC, VCOMP_DAC_CHANNEL);
	// CLEAR_BIT(DAC->CR, DAC_CR_EN2);

	// /* Wait for capture time, refer to section 3.2.3 of AN4636 */
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();
	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();	__DSB();

	// counter_after = READ_REG(LPTIM1->CNT) - counter;	// get the pulses in the measurement
	
	/* Disable COMP2 */
	// LL_COMP_Disable(COMP2);
	CLEAR_BIT(COMP2->CSR, COMP_CSR_COMP2EN);

	/* LC2 measurement */
	// CLEAR_BIT(COMP2->CSR, COMP_CSR_COMP2INPSEL); Choose LC2 as input of COMP2

	// MODIFY_REG(LC2_PORT->MODER, GPIO_MODER_MODE3, GPIO_MODER_MODE3_0);
	// __DSB();
	// SET_BIT(LC2_PORT->MODER, GPIO_MODER_MODE3_1);

	/* 
	uint8_t pulses_read;
	if (lc1.counter_after < lc1.counter_before)
	{
		pulses_read = (uint8_t)(lc1.counter_after + (0xFFFF - lc1.counter_before));
	} else {
		pulses_read = (uint8_t)(lc1.counter_after - lc1.counter_before);
	}

#ifdef	ELSTER_WATER_METER
	if (pulses_read <= METAL_DETECTED_ELSTER){
		lc1.status = SET;
	} else {
		lc1.status = RESET;
	}
#elif		ITRON_WATER_METER
	if (pulses_read <= METAL_DETECTED_ITRON){
		lc1.status = SET;
	} else {
		lc1.status = RESET;
	}
#endif
	 */
}

void lc_sensor_uplink( void )
{

	Debug_printf("Current counter value is %d\r\n", READ_REG(LPTIM1->CNT));

	static bool test = true;
	if (test != false){
		LL_RTC_DisableWriteProtection(RTC);
		LL_RTC_WAKEUP_Enable(RTC);
		LL_RTC_EnableWriteProtection(RTC);
		test = false;
	}

	/* 
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	Uplink(payload.payload, LC_SENSOR_SIZE);
	 */
}

void lc_sensor_onWakeup( void )
{
	if(wakeup_count % wakeups_per_uplink == 0)
	{
		Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
		lc_sensor_uplink();
		return;
	}

}



