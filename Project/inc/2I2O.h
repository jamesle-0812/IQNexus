/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	I2C register level header for LoRa only implementation project
								Implements I2C flash external periphheral.

	Maintainer: Shea Gosnell


*/



#ifndef TWO_IO_HEADER
#define TWO_IO_HEADER
#include <stdint.h>
#include "lora_sensum.h"

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
 
 /********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
 void twoIO_init(void);
 void twoIO_uplink_state(void);
 void twoIO_onDownlink(uint8_t *buffer, uint8_t size);
 void twoIO_cli_threshold(int argc, char *argv[]);
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //TWO_IO_HEADER


