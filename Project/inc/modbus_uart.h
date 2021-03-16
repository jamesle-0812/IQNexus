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

#ifndef LPUART_DEBUG_HEADER_PUBLIC
#define LPUART_DEBUG_HEADER_PUBLIC
#include <stdint.h>
#include <stdarg.h>
#include "global.h"

typedef enum
{
	modbus_error_no_support = -1,
	modbus_error_crc        = -2,
	modbus_error_count      = -3,
}modbus_error_e;

typedef struct
{
	int read;
	int write;
}modbus_transaction_result_t;

//This register is special, because not only is it used in function calls, but it is also
//mapped onto the external flash, so you will need a very good reason to change the layout/size of
//this struct, and will need to thoroughly check the storing and loading to flash.

//6 bytes large * 16 = 96 bytes, which is larger than a single page of flash.
//we are storing this on two config pages in flash, specifically page 1 and 2 (with 0 still for generic config);
typedef struct
{
	uint8_t  slaveID;
	uint8_t  function_code;
	uint16_t start_Register;
	uint16_t register_count;
}PACKED modbus_register_t;

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void     modbus_init( void );
void     modbus_tamper_init(void);
int      modbus_writeRegisters(uint8_t dev_addr, uint16_t memory_address, uint16_t register_count, uint8_t *transmit_buffer, uint8_t data_bytes_count);
int      modbus_readRegisters(uint8_t dev_addr, uint16_t memory_address, uint16_t count, uint16_t *receive_buffer);
uint16_t modbus_calculateCrc(uint8_t* data, int data_length);
uint8_t  modbus_getChar(uint8_t *storage_loc);
void     modbus_sendChar(uint8_t toSend);
void     modbus_enable_pins( void );
void     modbus_disable_pins( void );
void     modbus_disable_rx_pin(void);
void     modbus_disable_tx_pin(void);
void     modbus_enable_rx_pin(void);
void     modbus_enable_tx_pin(void);

modbus_transaction_result_t modbus_transaction(modbus_register_t modbus_register, uint16_t *read_data, uint16_t *write_data, uint16_t read_limit, uint16_t write_limit);


/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/


#endif //LPUART_DEBUG_HEADER_PUBLIC
