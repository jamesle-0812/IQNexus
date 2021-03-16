/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Device driver for the 4-20mA (and generic ADC) input

	Maintainer: Shea Gosnell

*/

#ifndef ADC_HEADER
#define ADC_HEADER
#include <stdint.h>
#include <stdbool.h>

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/


/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
uint16_t AdcReadCompensate(uint32_t Channel);
void init_three_adc( void );
void three_adc_send( void );
uint16_t read_adc( void );
void init_adc( void );

#endif //ADC_HEADER
