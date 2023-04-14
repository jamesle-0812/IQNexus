/*
 *    ____              ____
 *  / ____|           / ____|                
 * | (___   ___ _ __ | (___  _   _ _ __ ___  
 *  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
 *  ____) |  __/ | | |____) | |_| | | | | | |
 * |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
 * 
 * 
 * Description : Device driver for the LoRa chip
 * Created by  : James Le
 * Date        : 29/06/2020
 * Maintainer  : Shea Gosnell 
 * 
 */

#ifndef BME680HEADER
#define BME680HEADER
#include <stdint.h>
#include "lora_sensum.h"

#include "../Middlewares/Third_Party/bsec/bsec_integration.h"

/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/
typedef struct
{
   uint8_t  chip_id;
   uint16_t bme_IAQ;
   uint8_t  bme_IAQ_Accuracy;
   uint16_t bme_Pressure;
   uint16_t bme_CO2;
}bme680Reading_t;
 /********************************************************************
 *Public Function Prototypes                                         *
 ********************************************************************/
bme680Reading_t bme680GetReading( void );
void bme680_uplink( void );
void bme680_bsec_run( void );
void bme680_bsec_init( void );
int64_t get_timestamp_us(void);
void bme680_cli_threshold(int argc, char *argv[]);
void bme680_save_config(void);
void bme680_load_config(void);
void bme680_onWakeup( void );
void bme680_onDownlink(uint8_t *buffer, uint8_t size);
void service_bme680(void);


 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //BME680HEADER

