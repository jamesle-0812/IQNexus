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

#ifndef WATCHDOG_HEADER_PRIVATE
#define WATCHDOG_HEADER_PRIVATE
#include <stdint.h>
#include <stdbool.h>


/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
  #define WWDG_BASE_ADDR	  0x40002C00

 #define WWDG_CR_ADDR     (WWDG_CR_t    *)0x40002C00
 #define WWDG_CFR_ADDR    (WWDG_CFR_t   *)0x40002C04
 #define WWDG_SR_ADDR     (WWDG_SR_t    *)0x40002C08
 
 #define WWDG_CLK_EN_ADDR (RCC_APB1ENR_t*)0x40021038
 #define WWDG_RESET_ADDR  (RCC_CSR_t    *)0x40021050

/********************************************************************
 *Register Bitfields                                                *
 ********************************************************************/

//Control Register 1
// LSB FIRST
typedef struct
{
	uint32_t
		T			  :7,	//Value of the watchdog counter
		WDGA    :1, //Activation bit, once set can only be cleared by device reset
		Res1		:24;	//Reserved, must be kept at reset value
}WWDG_CR_t; //at offset 0x00

//Configuration register
typedef struct
{
	uint32_t
		W			  :7,  //7-bit window. Clearing the watchdog before it reaches this level will trigger a reset
		WDGTB   :2,  //Timer base
		EWI     :1,  //Early-wakeup interrupt
		Res1    :22; //Reserved, must be kept at reset value
}WWDG_CFR_t; //at offset 0x04



//status register
typedef struct
{
	uint32_t
		EWF   :1,  //early wakeup interrupt flag
		Res1  :31; //Reserved, must be kept at reset value
}WWDG_SR_t;

typedef struct
{
	uint32_t
	Res1    :11,
	WWDG    :1,
	Res2    :20;
}RCC_APB1ENR_t;

typedef struct
{
	uint32_t
	res3      :23,
	RMVF			:1,
	FW_RSTF   :1,
	OBL_RSTF  :1,
	PIN_RSTF  :1,
	POR_RSTF  :1,
	SFT_RSTF  :1,
	IWDG_RSTF :1,
	WWDG_RSTF :1,
	LPWR_RSTF :1;
}RCC_CSR_t;


#endif //WATCHDOG_HEADER_PRIVATE
