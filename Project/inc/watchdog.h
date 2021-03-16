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

#ifndef WATCHDOG_HEADER
#define WATCHDOG_HEADER
#include <stdint.h>
#include <stdbool.h>

#define WATCHDOG_RESET_VALUE 62 //*2 ms 63*2ms is the maximum (*2 is a rough estimate)
#define WATCHDOG_WINDOW_VALUE (0x40|63)

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void init_watchdog                ( void );
void reset_watchdog               ( void );
void force_mcu_reset_via_watchdog ( void );
int  watchdog_reset_occured       ( void );
void clear_reset_source           ( void );

#endif //WATCHDOG_HEADER


