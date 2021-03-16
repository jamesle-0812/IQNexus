/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Device driver for the UMG604 Power meter.

	Maintainer: Shea Gosnell

*/

#ifndef SCL61D5_HEADER
#define SCL61D5_HEADER
#include <stdint.h>
#include <stdarg.h>
#include "modbus_uart.h"
#include "lora_sensum.h"

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void scl61d5_wakeup(void);
void scl61d5_uplink(void);

void scl61d5_save_config(void);
void scl61d5_load_config(void);

void scl61d5Downlink(uint8_t *buffer, uint8_t size);

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/


#endif //SCL61D5_HEADER
