/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	Interface header for the SHT20 Temp/humidity sensor
   Created by: James Le
   Date: Jun 8th, 2020
	Maintainer: Shea Gosnell


*/



#ifndef SPS30HEADER
#define SPS30HEADER
#include <stdint.h>
#include "lora_sensum.h"
#include "sht30.h"


/********************************************************************
 *Public Definitions                                                *
 ********************************************************************/
/* 
 values of measurement is saved in big-endian, 
 which is different with little-endian in memory layout.
 e.g. 
 int 1 (big-endian)
 LSB					 MSB
 +----+----+----+----+
 |0x01|0x00|0x00|0x00|
 +----+----+----+----+
 
 int 1 (little-endian)
 MSB					 LSB
 +----+----+----+----+
 |0x00|0x00|0x00|0x01|
 +----+----+----+----+
 */
/* Swap -endians (big-endian <-> little-endian), apply to 32-bit variables only */
#define SWAP_ENDIAN(x)	(((x>>24)&0xff) | ((x>>8)&0xff00) | ((x<<8)&0xff0000) | ((x<<24)&0xff000000))
// 					       move byte 3->0	 move byte 2->1	  move byte 1->2		   move byte 0->3

/* 
#define ABS(X)  ((X>=0)? X : -(x))
#define ROUND(X)  (X>=0)? (int) (X + 0.5) : (int)-(ABS(X) +0.5)
 */

/********************************************************************
 * Public Function Prototypes                                       *
 ********************************************************************/
void sps30_uplink( void );
void sps30_onWakeup( void );
void sps30_save_config(void);
void sps30_load_config(void);
void sps30_cli_threshold(int argc, char *argv[]);
void sps30_onDownlink(uint8_t *buffer, uint8_t size);

 
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/

typedef struct
{
   float MassCon_PM1p0;
   float MassCon_PM2p5;
   float MassCon_PM4p0;
   float MassCon_PM10;
   float NumCon_PM0p5;
   float NumCon_PM1p0;
   float NumCon_PM2p5;
   float NumCon_PM4p0;
   float NumCon_PM10;
   float size;
   uint8_t  status;
}sps30_reading_sensor;

#endif //SPS30HEADER
