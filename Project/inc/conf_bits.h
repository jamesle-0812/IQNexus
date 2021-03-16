/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: USART Register Level driver for LoRa only implementation project

	Maintainer: Shea Gosnell

	IF THIS IS BEING COMPILED FOR A DIFFERENT PLATFORM, SEVERAL THINGS CAN GO WRONG
	1) THE REGISTERS COULD BE DEFINED DIFFERENTLT - CHECK THE PROGRAMMING MANUAL
	2) THE ORDER OF THE BITFOElDS (MSB FIRST / LSB FIRST) COULD BE DIFfERENT - BE CAREFUL
	3) THE ADDRESSES OF THE REGIStERS COULD BE DIFFERENT, CHECK THE MANUAL

*/

#ifndef CONF_BITS_HEADER_PUBLIC
#define CONF_BITS_HEADER_PUBLIC
#include <stdint.h>
#include <stdarg.h>

void disable_fw_readout(void);

#endif //CONF_BITS_HEADER_PUBLIC
