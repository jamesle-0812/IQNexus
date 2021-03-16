/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Physical pins definitions for the project

	Maintainer: Shea Gosnell

*/

#ifndef PINS_HEADER
#define PINS_HEADER

#include "global.h"

//PH15 does not exist on the STM32L07XXX
#define PORT_NOT_MAPPED         GPIOH
#define PIN_NOT_MAPPED          LL_GPIO_PIN_15
#define ADC_NOT_MAPPED          LL_ADC_CHANNEL_VREFINT

#ifdef HW_1_0
	#include "pins_1_0.h"
#endif

#ifdef HW_1_1
	#include "pins_1_1.h"
#endif

#ifdef HW_1_2
	#include "pins_1_2.h"
#endif

#ifdef HW_1_3
	#include "pins_1_3.h"
#endif	

#endif //PINS_HEADER		
