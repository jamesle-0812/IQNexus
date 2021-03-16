/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Device Configuration register bits
	
	This is primrarily used to ensure that the RDPROT bits are set to a value
	which ensures that the firmware cannot be read off the uC after programming
	

	Maintainer: Shea Gosnell


*/

#ifndef CONF_BITS_HEADER_PRIVATE
#define CONF_BITS_HEADER_PRIVATE
#include <stdint.h>
#include <stdarg.h>

#define FLASH_OPTKEYR_ADDR        0x40022014
#define FLASH_OPTKEYR_UNLOCK_1    0xFBEAD9C8
#define FLASH_OPTKEYR_UNLOCK_2    0x24252627

#define FLASH_OPTR_ADDR           0x1FF80000
#define FLASH_OPTBYTES_ADDR       0x4002201C
#define FLASH_OPTBYTES_RDP_LEVEL1 0x000000FF
#define FLASH_RDP_LELVEL_1        0xFF0000FF
#define FLASH_OPTBYTES_WPRMOD     0x00000100

#define FLASH_SR_ADDR             0x40022018
#define FLASH_SR_BUSY             0x01
#define FLASH_SR_END              0x04

#define FLASH_PECR_ADDR           0x40022004
#define FLASH_PECR_PELOCK_BIT     0x01
#define FLASH_PECR_OPTLOCK_BIT    0x04
#define FLASH_PECR_OBL_LAUNCH_BIT 0x400
#define FLASH_PECR_FIX_BIT        0x100

#define FLASH_PKEYR_ADDR          0x4002200C
#define FLASH_PKEYR_UNLOCK_1      0x89ABCDEF
#define FLASH_PKEYR_UNLOCK_2      0x02030405



#endif //CONF_BITS_HEADER_PRIVATE
