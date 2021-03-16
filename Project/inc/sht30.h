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



#ifndef SHT30HEADER
#define SHT30HEADER
#include <stdint.h>
#include "lora_sensum.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/
typedef struct
{
	int16_t T1;
	int16_t T2;
	uint16_t H1;
	uint16_t H2;
}sht30Reading_t;
 /********************************************************************
 *Public Function Prototypes                                         *
 ********************************************************************/
sht30Reading_t sht30GetReading( void );
void sht30_uplink( void );
void sht30_onWakeup( void );
void sht30_save_config(void);
void sht30_load_config(void);
void sht30_cli_threshold(int argc, char *argv[]);
void sht32_onDownlink(uint8_t *buffer, uint8_t size);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //SHT30HEADER


