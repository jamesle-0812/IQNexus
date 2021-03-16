/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	

	Maintainer: Shea Gosnell


*/



#ifndef ONEWIRE_HEADER
#define ONEWIRE_HEADER
#include <stdint.h>
#include "lora_sensum.h"
#include "debug_uart.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/
#ifdef DISABLE_OWP_DEBUG
	#define dbg_owp(...)
#else
	#define dbg_owp(...) Debug_printf(__VA_ARGS__)
#endif
 /********************************************************************
 *Public Function Prototypes                                         *
 ********************************************************************/
void OWP_init(GPIO_TypeDef* port, uint16_t pin);
void OWP_reset_bus(void);
void OWP_write_byte(uint8_t byte);
uint8_t OWP_read_byte(void);
uint64_t OWP_getSingleID(void);
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //ONEWIRE_HEADER


