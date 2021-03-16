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

#include "watchdog.h"
#include "delays.h"
#include "hw_rtc.h"												
#include "debug_uart.h"		
#include "timeServer.h"

void timer_dummy_event()
{
	//intentionally blank
}

void start_timeout_timer(TimerEvent_t* timer, uint32_t timeout_ms)
{
	//ensure that this timer is not still running
	TimerStop(timer);
	TimerInit(timer, &timer_dummy_event);
	TimerStop(timer);
	TimerSetValue(timer, timeout_ms);
	TimerStart(timer);
}

void stop_timeout_timer(TimerEvent_t* timer)
{
	TimerStop(timer);
}

bool timer_expired(TimerEvent_t* timer)
{
	return !TimerExists(timer);
}

void delay_timeout_ms(uint32_t delay_ms)
{
	static TimerEvent_t delay_timer;
	static TimerEvent_t sleep_timer;
	
	start_timeout_timer(&delay_timer,delay_ms);
	reset_watchdog();
	while(!timer_expired(&delay_timer))
	{
		start_timeout_timer(&sleep_timer, 100);
		sleep_until_interrupt();
		reset_watchdog();
	}
	//ensure that the timer is stopped and removed from the list before going out of scope
	TimerStop(&delay_timer);
	TimerStop(&sleep_timer);
}

//void delay_timeout_ms(uint32_t delay_ms)
//{
//	static TimerEvent_t delay_timer;
//	
//	start_timeout_timer(&delay_timer,delay_ms);
//	
//	while(!timer_expired(&delay_timer))
//	{
//		reset_watchdog();
//	}
//	
//	//ensure that the timer is stopped and removed from the list before going out of scope
//	TimerStop(&delay_timer);
//}

void delayWithFeedback(int numberOfPeriods, int feedbackPeriodMs)
{
	int i = 0;
	for(i=0;i<numberOfPeriods;i++)
	{
		delay_timeout_ms(feedbackPeriodMs);
		Debug_printf(".");
	}
}

void delay_us(int duration)
{
	while(duration--)
	{
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
		__asm("nop");
	}
}

