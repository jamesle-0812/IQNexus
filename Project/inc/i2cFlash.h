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



#ifndef I2C_FLASH_HEADER
#define I2C_FLASH_HEADER
#include <stdint.h>

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
 
 /********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void flash_writeblock(uint16_t block, uint8_t *data, uint8_t data_size);
void flash_readblock(uint16_t block, uint8_t *data, uint8_t data_size);
void flash_erase_all(void);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //I2C_FLASH_HEADER


