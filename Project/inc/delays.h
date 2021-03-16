/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Device driver for the LoRa chip

	Maintainer: Shea Gosnell

*/

#ifndef DELAYS_HEADER
#define DELAYS_HEADER
#include <stdint.h>
#include <stdbool.h>
#include "lora_sensum.h"
#include "timeServer.h"

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void delay_timeout_ms(uint32_t delay_ms);
void delayWithFeedback(int numberOfPeriods, int feedbackPeriodMs);
bool timer_expired(TimerEvent_t* timer);
void start_timeout_timer(TimerEvent_t* timer, uint32_t timeout_ms);
void stop_timeout_timer(TimerEvent_t* timer);

void delay_us(int duration);

#endif //DELAYS_HEADER
