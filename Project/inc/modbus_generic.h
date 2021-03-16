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

#ifndef DISTANCE_HEADER
#define DISTANCE_HEADER
#include <stdint.h>
#include <stdarg.h>
#include "modbus_uart.h"
#include "lora_sensum.h"

#define PACKED __attribute__((packed, aligned(1)))

/********************************************************************
 *Defined Types                                                     *
 ********************************************************************/

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
uint16_t Distance_getDistance( void );
void cli_modbus_implementation(int argc, char* argv[]);
void cli_modbus_help( void );
void genric_modbus_uplink( void );
void genericModbus_onDownlink(uint8_t *buffer, uint8_t size);
void save_generic_modbus_config(void);
void load_generic_modbus_config(void);
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/
extern modbus_register_t modbus_upload_regs[];

#endif //DISTANCE_HEADER
