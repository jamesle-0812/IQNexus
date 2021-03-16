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



#ifndef DS18B20HEADER
#define DS18B20HEADER
#include <stdint.h>
#include "lora_sensum.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/

 /********************************************************************
 *Public Function Prototypes                                         *
 ********************************************************************/
void ds18b20_uplink              (void);
void ds18b20_init                (void);
void multi_ds18b20_init          (void);
void ds18b20_onWakeup            (void);
void multi_ds18b20_onWakeup      (void);
void ds18b20_cli_threshold       (int argc, char *argv[]); 
void ds18b20_cli_device          (int argc, char *argv[]);
void ds18b20_load_config         (void);
void ds18b20_save_config         (void);
void six_ds18b20_load_config     (void);
void six_ds18b20_save_config     (void);
void ds18b20_onDownlink          (uint8_t *buffer, uint8_t size);
void ds18b20_mulit_on_each_wakeup(void);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //DS18B20HEADER


