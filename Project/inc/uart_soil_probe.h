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

#ifndef SOIL_PROBE_HEADER
#define SOIL_PROBE_HEADER
#include <stdint.h>
#include <stdarg.h>
#include "modbus_uart.h"
#include "lora_sensum.h"
#include "global.h"

enum
{
	probe_success_ok = 0,
	probe_fail_crc,
	probe_fail_noprobe,
};

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void probe_sendData( void );
int probe_getData(uint8_t data[static 32]);
void probe_power_off( void );
void probe_power_on( void );
void probe_bleed_off( void );
void probe_bleed_on( void );
void probe_init(void);
test_status_e test_probe(void);

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/


#endif //SOIL_PROBE_HEADER
