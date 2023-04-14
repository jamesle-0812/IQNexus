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
 * Maintainer  : Shea Gosnell 
 * 
 */



#ifndef LC_SENSOR_HEADER
#define LC_SENSOR_HEADER


#include <stdint.h>
#include "stm32l0xx_ll_rcc.h"
#include "cmsis_armcc.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/
#define	ELSTER_WATER_METER
// #define	ITRON_WATER_METER

#define	VMID_GPIO_PORT		GPIOA
#define	VMID_GPIO_PIN		LL_GPIO_PIN_4
#define	VMID_DAC_CHANNEL	LL_DAC_CHANNEL_1

#define	VCOMP_GPIO_PORT	GPIOA
#define	VCOMP_GPIO_PIN		LL_GPIO_PIN_5
#define	VCOMP_DAC_CHANNEL	LL_DAC_CHANNEL_2

#define	COMP2_PORT			GPIOA
#define 	COPM2_PIN			LL_GPIO_PIN_2

#define	LC2_PORT				GPIOA
#define	LC2_PIN				LL_GPIO_PIN_3

#define	LC1_PORT				GPIOB
#define	LC1_PIN				LL_GPIO_PIN_7

#define	FLIP_BIT(REG, BIT)     ((REG) ^= (BIT))

#define	__DELAY_US(__TIME__)	\
	{\
		WRITE_REG(TIM7->ARR, __TIME__ - 1);	\
		MODIFY_REG(RCC->CFGR, RCC_CFGR_HPRE, RCC_CFGR_HPRE_3 | RCC_CFGR_HPRE_2);\
		SET_BIT(TIM7->CR1, TIM_CR1_CEN);	\
		__WFI();									\
		CLEAR_BIT(RCC->CFGR, RCC_CFGR_HPRE);	\
	}

/********************************************************************
 *Public Function Prototypes                                        *
 ********************************************************************/
void lc_sensor_init(void);
void lc_sensor_uplink(void);
void lc_sensor_onWakeup(void);
void lc_sensor_measurement(void);
/**
  * @brief  Macros used to start oscillations on LC sensor with minimum time + x cycles.
  * @param  group: specifies the IO group.
  * @param  output_pp: specifies register value to set IO in output PP state
  * @param  analog: specifies register value to set IO in analog state
  * @param  texcit: specifies number of cycles to loop
  * @retval None
  */
/* 
__attribute__((optimize("-O0"))) __STATIC_INLINE void __LC_EXCIT(uint32_t group, uint32_t output_pp, uint32_t analog, uint32_t texcit)
{
	__DSB();
	__ISB();
	__ASM volatile(\
	               "push {r0-r3}\n"								\
	               "str %1, [%0]\n" 								\
	               "1:\n" 											\
	               "subs %3, #1\n" 								\
	               "bne 1b\n" 										\
	               "str %2, [%0]\n"								\
	               "pop {r0-r3}\n"								\
	               :: "r" (group), 								\
	               "r" (output_pp), 								\
	               "r" (analog), 									\
	               "r" (texcit): "r0", "r1", "r2", "r3");
}
 */

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/

#endif //LC_SENSOR_HEADER


