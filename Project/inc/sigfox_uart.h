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

#ifndef UART_SIGFOX_HEADER_PUBLIC
#define UART_SIGFOX_HEADER_PUBLIC
#include <stdint.h>
#include <stdarg.h>


/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void sigfox_usart_init      (void);
void disable_sigfox_usart   (void);
void sigfox_send            (const char *format, ... );
void sigfox_AddToWriteBuffer(char* message, int length);
void await_sigfox_uart_tx   (void);
int  sigfox_isCharToSend    (void);
int  Sigfox_getRxDataLength (void);
char Sigfox_getChar         (void);
int  sigfox_readLine        (char* input, int max_length, int timeout_ms);
void USART4_5_IRQHandler    (void);

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/

#endif //UART_SIGFOX_HEADER_PUBLIC
