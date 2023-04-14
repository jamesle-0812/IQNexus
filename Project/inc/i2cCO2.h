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



#ifndef I2C_CO2_HEADER
#define I2C_CO2_HEADER
#include <stdint.h>
#include "lora_sensum.h"

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
typedef struct
{
	uint16_t value;
	uint8_t
		co2_error_restore_fail :1,
		co2_error_start_fail   :1,
		co2_error_timeout_rise :1,
		co2_error_timeout_fall :1,
		co2_error_read_fail    :1,
		co2_error_backup_fail  :1,
		co2_error              :1,
		RESERVED               :1;
}CO2_reading_t;
 
 /********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void C02_init( void );
void legacy_C02_init( void );
void co2_uplink( void );
void legacy_co2_uplink( void );
void CO2_hourly_interrupt( void );
void co2_cli_device(int argc, char *argv[]);
void legacy_co2_cli_device(int argc, char *argv[]);
void co2_onDownlink(uint8_t *buffer, uint8_t size);
void legacy_co2_onDownlink(uint8_t *buffer, uint8_t size);
void co2_save_config( void );
void co2_load_config( void );
CO2_reading_t CO2_read( void );
 
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //I2C_CO2_HEADER


