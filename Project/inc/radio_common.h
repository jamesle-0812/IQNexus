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

#ifndef RADIO_COMMON_HEADER
#define RADIO_COMMON_HEADER
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "global.h"
#include "packets.h"
 
/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
uint8_t fourBit_battery_calculation(void);
void Print_radio_information       (void);
void radio_init                    (void);
void sendStartupPacket             (void);
transmit_status_e Uplink           (uint8_t payload[], uint8_t size);
bool default_downlink              (uint8_t *buffer, uint8_t size);
bool radio_joined                  (void);
void save_radio_config_page        (void);
void load_radio_config_page        (void);


/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/



#endif //RADIO_COMMON_HEADER
