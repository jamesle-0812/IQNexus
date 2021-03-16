/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
                                           
	Description: Device driver for the internal watchdog
	note that the internal watchdog is being used instead of the independant watchdog.
	This is because the independant watchdog would be active during sleep, and would reseet
	the chip during a long sleep.

	Maintainer: Shea Gosnell

*/


#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"

#include "_watchdog.h"
#include "watchdog.h"

volatile static WWDG_CR_t     *REG_Watchdog_CR    = WWDG_CR_ADDR;
volatile static WWDG_CFR_t    *REG_Watchdog_CFR   = WWDG_CFR_ADDR;
//volatile static WWDG_SR_t     *REG_Watchdog_SR    = WWDG_SR_ADDR;
volatile static RCC_APB1ENR_t *REG_Watchdog_CLKEN = WWDG_CLK_EN_ADDR;
volatile static RCC_CSR_t     *REG_Watchdog_reset = WWDG_RESET_ADDR;

void init_watchdog        ( void )
{
	
	REG_Watchdog_CR->T = 0x40 | WATCHDOG_RESET_VALUE;
	
	//We don't want to fire the interrupt
	REG_Watchdog_CFR->EWI = 0;
	
	
	//divide clock by 8, for slowest reset clock
	REG_Watchdog_CFR->WDGTB = 3;
	
	//disable the window
	REG_Watchdog_CFR->W = WATCHDOG_WINDOW_VALUE;
	
	//start the clock
	REG_Watchdog_CLKEN->WWDG = 1;
	
	//finally, set the WDGA bit to enable the watchdog
	REG_Watchdog_CR->WDGA = 1;
}

void force_mcu_reset_via_watchdog( void )
{
	//set the WDGA bit to enable the watchdog
	REG_Watchdog_CR->WDGA = 1;
	//then clear the msb of the counter to force the reset
	REG_Watchdog_CR->T = 0;
}

int watchdog_reset_occured(void)
{
	if(REG_Watchdog_reset->WWDG_RSTF)
	{
		return 1;
	}
	return 0;
}

void clear_reset_source(void)
{
	REG_Watchdog_reset->RMVF = 1;
}

void reset_watchdog       ( void )
{
	REG_Watchdog_CR->T = 0x40 | WATCHDOG_RESET_VALUE;
}


//This IRQ will fire about 1ms before the watchdog expires.
void WWDG_IRQHandler( void )
{
	//this is here in case we decide to use the interrupt at some
	//point in the future
}


