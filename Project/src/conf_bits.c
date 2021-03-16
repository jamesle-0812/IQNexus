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
#include "_conf_bits.h"
#include "conf_bits.h"

#include "debug_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "global.h"
#include "watchdog.h"
#include "delays.h"

static volatile uint32_t* opt_key   = (uint32_t*)FLASH_OPTKEYR_ADDR;
static volatile uint32_t* opt_reg   = (uint32_t*)FLASH_OPTBYTES_ADDR;
static volatile uint32_t* opt_r_reg = (uint32_t*)FLASH_OPTR_ADDR;
static volatile uint32_t* sr_reg    = (uint32_t*)FLASH_SR_ADDR;
static volatile uint32_t* pecr_reg  = (uint32_t*)FLASH_PECR_ADDR;
static volatile uint32_t* pecr_key  = (uint32_t*)FLASH_PKEYR_ADDR;


#define wait_sr_not_busy() while(*sr_reg & FLASH_SR_BUSY){;}

void disable_fw_readout()
{
	static uint32_t temp;
	
	if((*opt_reg & FLASH_OPTBYTES_RDP_LEVEL1)!= 0xFF)
	{
		//Entire process taken from near page 120 of the STM32L0 programming manual
		
		//ensure that the flash is not busy
		wait_sr_not_busy();
		
		//ensure that the PECR is unlocked
		if((*pecr_reg & FLASH_PECR_PELOCK_BIT))
		{
			*pecr_key = FLASH_PKEYR_UNLOCK_1;
			*pecr_key = FLASH_PKEYR_UNLOCK_2;
		}
		
		//wait for the flash to not be busy
		wait_sr_not_busy();
		
		if((*pecr_reg & FLASH_PECR_OPTLOCK_BIT))
		{
			//unlock the Opt-bytes register
			*opt_key = FLASH_OPTKEYR_UNLOCK_1;
			*opt_key = FLASH_OPTKEYR_UNLOCK_2;
		}
		
		//wait for the flash to not be busy
		wait_sr_not_busy();
		
		//read the registers lower 16 bits, and get ready to write
		temp = FLASH_RDP_LELVEL_1;
		
		//wait for the flash to not be busy
		wait_sr_not_busy();
		
		*pecr_reg |= FLASH_PECR_FIX_BIT;
		
		//wait for the flash to not be busy
		wait_sr_not_busy();
		
		//write the new value to the OPT register
		*opt_r_reg = temp;
		
		//wait for the flash to not be busy
		wait_sr_not_busy();
		
		//indicate the end of the transaction
		if (*sr_reg & FLASH_SR_END)
		{
			*sr_reg |= FLASH_SR_END;
		}
		
		//set the launch bit, indicating that the values should be reloaded.
		*pecr_reg |= FLASH_PECR_OBL_LAUNCH_BIT;
		
		while(1)
		{
			Debug_printf("Power off Device\r\n");
			reset_watchdog();
			delay_timeout_ms(100);
		}
		
	}
	
	
}

