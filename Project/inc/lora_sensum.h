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

#ifndef LORA_SENSUM_HEADER
#define LORA_SENSUM_HEADER
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include "global.h"
#include "packets.h"

#define MIN_HOURS_TO_REJOIN 1
#define LORA_REJOIN_LOCKOUT_VALUE 1023


#if defined(REGION_US915)
	#define LORA_JOIN_ATTEMPT_MAX 5
#else
	#define LORA_JOIN_ATTEMPT_MAX 1
#endif
 
/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
transmit_status_e Lora_Uplink(uint8_t payload[], uint8_t size);
void print_lora_radio_information(void);
void lora_init( void );
void lora_manage_rejoin( void );
void lora_force_rejoin(void);
void lora_set_antenna_gain(int16_t gain);
int16_t lora_get_antenna_gain(void );
int process_lora_downlink(void);
void save_lora_config_page(void);
void load_lora_config_page(void);
void print_active_channels(void);
void remove_channels(uint8_t* list, uint8_t length);
void add_channels(uint8_t* list ,uint8_t length);
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/
//the reset value
extern uint16_t lora_hours_until_rejoin;
//this one is the actual down-counter
extern uint32_t hours_until_rejoin;


#endif //LORA_SENSUM_HEADER
