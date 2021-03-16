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



#ifndef I2C_LIGHTSENSOR_HEADER
#define I2C_LIGHTSENSOR_HEADER
#include <stdint.h>
#include "flash_map.h"

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
typedef enum
{
	Si1133_power_state_unknown = 0x00,
	Si1133_power_state_sleep   = 0x01, //low power mode, with I2C enabled ~125nA
	Si1133_power_state_suspend = 0x02, //conversion/setup                 ~550nA
	Si1133_power_state_running = 0x03, //Active, and taking measurements  ~4.5mA
}Si1133_power_state_e;

typedef struct
{
	uint32_t                  visible_reading;
	uint32_t                  infra_reading  ;
}Si1133_reading_t;
 
 /********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void init_light_sensor(void);
void continious_read_light_level(void);
Si1133_reading_t read_light_level(void);
Si1133_power_state_e get_sensor_power_state(void);

void light_sensor_uplink(void);

void light_sensor_cli(int argc, char *argv[]);
void light_sensor_cli_component(int argc, char *argv[]);
light_sensor_config_component_t get_light_sensor_config_values( void );
void set_light_sensor_config_values(light_sensor_config_component_t config);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //I2C_LIGHTSENSOR_HEADER


