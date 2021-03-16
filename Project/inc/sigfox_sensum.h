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

#ifndef SIGFOX_SENSUM_HEADER
#define SIGFOX_SENSUM_HEADER
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "global.h"

#define MIN_HOURS_TO_REJOIN 1

#define MAX_RX_DATA 8 //maximum downlink of 8 bytes, dictated by sigfox

#define PACKED __attribute__((packed, aligned(1)))
/********************************************************************
 *Global Typedefs                                                   *
 ********************************************************************/
 
/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void sf_hardware_init  (void);
void sf_hardware_deinit(void);
bool sigfox_sleep      (void);
bool sigfox_wake       (void);
bool sf_print_info     (void);
bool sf_send           (uint8_t* data, int length);
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/



#endif //SIGFOX_SENSUM_HEADER
