/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	Interface header for the SHT20 Temp/humidity sensor

	Maintainer: Shea Gosnell


*/



#ifndef SHT20HEADER
#define SHT20HEADER
#include <stdint.h>
#include "lora_sensum.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/

 /********************************************************************
 *Public Function Prototypes                                         *
 ********************************************************************/
int16_t sht20GetTemperature( void );
uint16_t sht20GetHumidity( void );
void sht20_uplink( void );
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //SHT20HEADER


