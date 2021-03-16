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
#include "flash_map.h"

#include "lora.h"

#include "modbus_generic.h"
#include "counter.h"
#include "global.h"
#include "radio_common.h"

//remember to read this from the data page at startup
static uint32_t data_write_count = 0;
static uint16_t current_data_page = 0;

static uint32_t bad_blocks[8] = {0};

/********************************************************************
 *Functions                                                         *
 ********************************************************************/
void seek_data_page(void);

int is_page_marked_bad(uint16_t page)
{
	return (bad_blocks[current_data_page / 32] & 1<<(current_data_page % 32));
}

void mark_page_bad(uint16_t page)
{
	bad_blocks[page / 32] = bad_blocks[page / 32] ^ 1<<(page % 32);
}

int flash_count_bad_pages()
{
	int i=0;
	int j=0;
	int bad_count =  0;
	
	for(i=0;i<8;i++)
	{
		for(j=0;j<32;j++)
		{
			if(bad_blocks[i] & (1<<j))
			{
				bad_count++;
			}
		}
	}
	
	return bad_count;
}

void save_global_config_page(void)
{
	int i;
	uint8_t *appeui = lora_config_appeui_get();
	uint8_t *appkey = lora_config_appkey_get();
	config_page_base_layout_t config_data = {0};
	
	//rejoin interval
	config_data.members.lora_hours_rejoin = lora_hours_until_rejoin;
	
	//we need the device mode
	config_data.members.device_mode = device_mode;
	
	//wakeup interval
	config_data.members.wakeup_interval_ms = transmit_interval_ms;
	config_data.members.uplink_wakeups = wakeups_per_uplink;
	
	
	for(i=0;i<8;i++)
	{
		config_data.members.appeui[i] = appeui[i];
	}
	
	for(i=0;i<16;i++)
	{
		config_data.members.appkey[i] = appkey[i];
	}
	
	//bad blocks
	for(i=0;i<8;i++)
	{
		config_data.members.bad_block_marker[i] = bad_blocks[i];
	}	

	//then we write to the page
	flash_writeblock(config_page, config_data.raw_bytes, PAGE_SIZE);
}

void save_extra_config_page( uint8_t mode_specific[static PAGE_SIZE], uint8_t page)
{
	if(page > config_page && page < data_page_start)
	{
		flash_writeblock(page, mode_specific, PAGE_SIZE);
	}
	else
	{
		Debug_printf("Save extra config: page out of range\r\n");
	}
}


void save_config( void )
{
	save_global_config_page();
	save_radio_config_page();
	device.save_config();
}

void load_gloabl_config_page()
{
	uint8_t new_appeui[8] = LORAWAN_APPLICATION_EUI;
	uint8_t new_appkey[16] = LORAWAN_APPLICATION_KEY;
	int retry = 3;
	int i;
	
	device_mode = DEVICE_UNINITIALISED;
	
	do
	{
		retry --;
		
		config_page_base_layout_t config_data = {0};
		
		flash_readblock(config_page, config_data.raw_bytes, PAGE_SIZE);
		//now write out to system variables
		//device mode, check that valid data was received
		if(config_data.members.device_mode <= get_number_device_modes())
		{
			
			//hours before rejoining LoRa network
			lora_hours_until_rejoin = config_data.members.lora_hours_rejoin;
			if(lora_hours_until_rejoin < MIN_HOURS_TO_REJOIN)
			{
				lora_hours_until_rejoin = MIN_HOURS_TO_REJOIN;
			}
			

			device_mode = config_data.members.device_mode;
			update_device_behaviour();


			//wakeup interval, default to 1m, if a value less than one minute is read
			transmit_interval_ms = config_data.members.wakeup_interval_ms;
			if(transmit_interval_ms < 60000)
			{
				transmit_interval_ms = 60000;
			}
			
			wakeups_per_uplink = config_data.members.uplink_wakeups;
			if(wakeups_per_uplink == 0)
			{
				wakeups_per_uplink = 1;
			}
			
			lora_config_appeui_set(new_appeui);
			lora_config_appkey_set(new_appkey);
			//if the configs are all 0, load the defaults
			for(i=0;i<8;i++)
			{
				if(config_data.members.appeui[i])
				{
					lora_config_appeui_set(config_data.members.appeui);
				}
			}
			for(i=0;i<16;i++)
			{
				if(config_data.members.appkey[i])
				{
					lora_config_appkey_set(config_data.members.appkey);
				}
			}
			for(i=0;i<8;i++)
			{
				bad_blocks[i] = config_data.members.bad_block_marker[i];
			}
		}
			
		
	}while(device_mode == DEVICE_UNINITIALISED && retry > 0);
}

void load_extra_config_page( uint8_t mode_specific[static PAGE_SIZE], uint8_t page)
{
	if(page > config_page && page < data_page_start)
	{
		flash_readblock(page, mode_specific, PAGE_SIZE);
	}
	else
	{
		Debug_printf("Load extra config: page out of range\r\n");
	}
}

void load_config()
{
	load_gloabl_config_page();
	load_radio_config_page();
	device.load_config();
}


void save_data_page(uint8_t data[static DATA_SIZE])
{
	data_page_base_layout_t data_page = {0};
	data_page_base_layout_t confirm_data_page = {0};
	uint8_t matched = 0;
	uint8_t i;
	
	//handle rolling over when we get near uint32 max writes
	//this is so we do not have issues finding the newest entry
	//as it depends on the newest entry having a higher data_write_count
	//than the entries before it.
	//uint32_max is 4,294,967,295
	if(data_write_count >= 4000000000)
	{
		//technically this is ~300,000 writes early.
		//however as far as wear on the device goes, this should be an extra page
		//write every 4,000,000, which should not be too impactful.
		
		//erase the flash, to null out all entries
		flash_erase_all();
		//reset the data write count, bring it far away from uint32 max
		data_write_count = 1;
		//reset the current data page, so we start writing from the beginning agian.
		current_data_page = 0;
		//write the configuration back into flash, so it is not lost
		save_config();
		//now write the data into flash, as origionally intended.
	}
	
	if(current_data_page < data_page_start)
	{
		seek_data_page();
	}
	
	while(matched == 0)
	{
		//when we write the data page, we increment the write count, and page
		//ensure that we don't try to write to a known bad page
		do
		{
			current_data_page ++;
		}while(is_page_marked_bad(current_data_page));
		
		data_write_count ++;
		
		if(current_data_page > data_page_end)
		{
			current_data_page = data_page_start;
		}
		
		data_page.members.write_count = data_write_count;

		//populate the array to save to flash
		for(i=0;i<DATA_SIZE;i++)
		{
			data_page.members.mode_specific[i] = data[i];
		}
		
		
		flash_writeblock(current_data_page, data_page.raw_bytes, PAGE_SIZE);
		
		//wait for the requisite 5ms for the write
		HW_RTC_DelayMs(5);
		
		//read the block back, to ensure that the write was successful
		flash_readblock(current_data_page, confirm_data_page.raw_bytes, PAGE_SIZE);
		
		matched = 1;
		//now do a comparison
		for(i=0;i<PAGE_SIZE;i++)
		{
			if(confirm_data_page.raw_bytes[i] != data_page.raw_bytes[i])
			{
				//the read does not match the write
				//we have a bad block!
				matched = 0;
				//mark the current data page as bad
				mark_page_bad(current_data_page);
				save_config();
			}
		}
	}
}



void load_data_page(uint8_t data[static DATA_SIZE])
{
	uint8_t i;
	data_page_base_layout_t data_page = {0};
	
	if(current_data_page < data_page_start)
	{
		seek_data_page();
	}
	//this will always be pointed at the last page written, and therefore will not
	//need to check if the current page is bad.
	
	
	//when we read the data page, we do not have to increment the page or write count
	flash_readblock(current_data_page, data_page.raw_bytes, PAGE_SIZE);
	
	//now assign to the data array, so the caller can process the data
	for(i=0; i<DATA_SIZE; i++)
	{
		data[i] = data_page.members.mode_specific[i];
	}
}

void seek_data_page(void)
{
 data_page_base_layout_t read_page = {0};
 config_page_base_layout_t config_data = {0};
 int i;
 
 //we will use increment to determine how many pages we have skipped since the last good page.
 int increment = 0;
 
 //first we need the map of bad pages, so we can ignore them
 flash_readblock(config_page, config_data.raw_bytes, PAGE_SIZE);
 
 	for(i=0;i<8;i++)
	{
		bad_blocks[i] = config_data.members.bad_block_marker[i];
	}
 
 for(current_data_page = data_page_start; current_data_page <data_page_end;current_data_page++)
 {
	 increment = 1;
	 //skip over known bad pages
		while(is_page_marked_bad(current_data_page))
		{
				current_data_page++;
				increment++;
		}
		
		
		flash_readblock(current_data_page, read_page.raw_bytes, PAGE_SIZE);
		 if(data_write_count < read_page.members.write_count)
		 {
				data_write_count = read_page.members.write_count;
		 }
		 //if we find a page with a lower count, we have reached the
		 //end of the search
		 else
		 {
			 current_data_page -=increment;
			 break;
		 }
 }
 
 //at this point, we should have the correct write count, and data page, and therefore can start writing to the flash again
}


void dump_flash(int page)
{
	uint8_t raw_bytes[PAGE_SIZE] = {0};
	int i;
	int j;
	
	//all
	if(page < 255)
	{
		flash_readblock(page,raw_bytes,PAGE_SIZE);
		
		Debug_printf("Page %03d:\r\n",page);
		//lines of 8 bytes
		for(j=0;j<64;j++)
		{
			if(j%8==0)
			{
				await_uart_tx();
				Debug_printf("\r\n%02d:\t", j);
			}
			Debug_printf("%02X ", raw_bytes[j]);
		}
		
		Debug_printf("\r\n\r\n\r\n");
	}
	else
	{

		
		for(i=0; i<255; i++)
		{
			flash_readblock(i,raw_bytes,PAGE_SIZE);
			
			Debug_printf("Page %03d:\r\n",i);
			//lines of 8 bytes
			for(j=0;j<64;j++)
			{
				if(j%8==0)
				{
					await_uart_tx();
					Debug_printf("\r\n%02d:\t", j);
				}
				Debug_printf("%02X ", raw_bytes[j]);
			}
			
			Debug_printf("\r\n\r\n\r\n");
		}
	}
}


void cli_flash_help()
{
	Debug_printf("Usage: flash [save|load] [config|data]\r\n");
	await_uart_tx();
	Debug_printf("Usage: flash erase confirm\r\n");
	await_uart_tx();
	Debug_printf("\tErases all blocks of the flash\r\n");
	await_uart_tx();
	Debug_printf("Usage: flash show\r\n");
	await_uart_tx();
	Debug_printf("\tShows flash info\r\n");
	await_uart_tx();
	Debug_printf("Usage: flash dump [x]\r\n");
	await_uart_tx();
	Debug_printf("\tDumps flash pages to cli\r\n");
	await_uart_tx();
	Debug_printf("\t\tIf [x] is 'all', will dump all pages\r\n");
	await_uart_tx();
	Debug_printf("\t\tOtherwise dumps page [x]\r\n");
	await_uart_tx();
}

void cli_flash_implementation( int argc, char *argv[])
{	
	int x;
	if(argc == 1)
	{		
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("Bad Pages   :%d\r\n", flash_count_bad_pages());
			Debug_printf("Current Page:%d\r\n", current_data_page);
			Debug_printf("Write Count :%d\r\n", data_write_count);
			return;
		}
		
	}
	
	
	if(argc == 2)
	{
				//save
		if(!strcmp(argv[0],"save"))
		{
			if(!strcmp(argv[1],"config"))
			{
				save_config();
				Debug_printf("Config Saved\r\n");
				return;
			}
			if(!strcmp(argv[1],"data"))
			{
				device.save_data();
				Debug_printf("Data Saved\r\n");
				return;
			}
		}
		//load
		if(!strcmp(argv[0],"load"))
		{
			if(!strcmp(argv[1],"config"))
			{
				load_config();
				Debug_printf("Config Loaded\r\n");
				return;
			}
			if(!strcmp(argv[1],"data"))
			{
				device.load_data();
				Debug_printf("Data Loaded\r\n");
				return;
			}
		}
			//erase
		if(!strcmp(argv[0],"erase"))
		{
			if(!strcmp(argv[1], "confirm"))
			{
				Debug_printf("Erasing Flash\r\n");
				flash_erase_all();
				//reset the data page, so we can load to it correctly after the write
				current_data_page = 0;
				data_write_count = 0;
				return;
			}
		}
		
				
		if(!strcmp(argv[0], "dump"))
		{
			if(!strcmp(argv[1], "all"))
			{
				dump_flash(255);
				return;
			}
			
			x = atoi(argv[1]);
			
			if(x < 255)
			{
				Debug_printf("Page %03d:\r\n",x);
				dump_flash(x);
				return;
			}
		}
	}
	
	//if we get to the end without ececuting a command, print help
	cli_flash_help();
	return;
}


