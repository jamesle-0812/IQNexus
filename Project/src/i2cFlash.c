/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	I2C register level driver for LoRa only implementation project
								Implements I2C flash external periphheral.

	Maintainer: Shea Gosnell


*/


#include "i2c1.h"
#include "i2cFlash.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#include "debug_uart.h"
#include "global.h"
#include "delays.h"

#include "watchdog.h"

#ifdef DISABLE_FLASH_DEBUG
	#define Debug_printf(...) 
#endif

#define FLASH_I2C_ADDR	0x50

#define NUM_PAGES 256

/********************************************************************
 *Functions                                                         *
 ********************************************************************/
 
void flash_writeblock(uint16_t block, uint8_t *data, uint8_t data_size)
{
	//write to the specified block address
	uint16_t block_to_write = block;
	//Array to store data and address.
	uint8_t block_data[66];
	uint8_t i,j;
	
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot continue.
		return; //without attempting to write
	}
	
	Debug_printf("Populating Packet\r\n");
	await_uart_tx();
	
	//shift 6 bits left, to get the block address, instead of a byte address
	block_to_write = block_to_write << 6;
	

	
	//break the block data into 8-bit bytes to send as the memory address
	block_data[0] = block_to_write >> 8;
	block_data[1] = block_to_write & 0x00FF;
	
	Debug_printf("Addr: 0x%02x%02x\r\n", block_data[0], block_data[1]);
	await_uart_tx();
	//write out the data to the terminal for debugging
	Debug_printf("Data:\r\n00:\t");
	await_uart_tx();
	i=0;
	//lines of 8 bytes
	for(j=0;j<data_size;j++)
	{
		block_data[j+2] = data[j];
		if(i==8)
		{
			await_uart_tx();
			Debug_printf("\r\n%02d:\t", j);
			i=0;
		}
		Debug_printf("%02X ", data[j]);
		i++;
	}
	
	Debug_printf("\r\n");
	

	Debug_printf("Writing to Flash\r\n");
	await_uart_tx();
	//write the block to flash via I2C
	//TODO: Return a status to let the caller know that the write was (un)successful
	i2c1_send(FLASH_I2C_ADDR, block_data, 2+data_size, 1);
	
}
void flash_readblock(uint16_t block, uint8_t *data, uint8_t data_size)
{
	//read from the specified address
	uint16_t block_to_read = block;
	//only need two bytes for the block address
	uint8_t block_addr[2];
	uint8_t i,j;
	
	reset_watchdog();
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot continue with I2C comms
		return; //without modifying the data
	}
		//shift 6 bits left, to get the block address, instead of a byte address
	block_to_read = block_to_read << 6;
	//break the block data into 8-bit bytes to send as the memory address
	block_addr[0] = block_to_read >> 8;
	block_addr[1] = block_to_read & 0x00FF;
	
	Debug_printf("Reading from Flash\r\n");
	await_uart_tx();
	//request the stored data from the flash
	i2c1_send(FLASH_I2C_ADDR, block_addr, 2, 0);
	i2c1_receive(FLASH_I2C_ADDR, data, data_size,1);
	
	//print out the read values, to test that they match the written values

	//write out the data to the terminal for debugging
	Debug_printf("Data:\r\n00:\t");
	await_uart_tx();
	
	i=0;
	//lines of 8 bytes
	for(j=0;j<data_size;j++)
	{
		
		if(i==8)
		{
			await_uart_tx();
			Debug_printf("\r\n%02d:\t", j);
			i=0;
		}
		Debug_printf("%02X ", data[j]);
		i++;
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
}


void flash_erase_all()
{
	uint16_t block = 0;
	uint8_t data[64] = {0};
	
	//write all blocks to 00.
	//from here, we can set bits to 'write' to the blocks
	for(block = 0; block < NUM_PAGES; block++)
	{
		flash_writeblock(block, data, 64);
		delay_timeout_ms(10);
	}
}

