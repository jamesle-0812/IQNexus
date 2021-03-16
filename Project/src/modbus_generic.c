/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
                                           
	Description: Device driver for the UMG604 Power meter.

	Maintainer: Shea Gosnell

*/

#include "global.h"
#include "modbus_uart.h"
#include "watchdog.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "modbus_generic.h"
#include "../SHELL/app_cli.h"
#include <string.h>
#include "radio_common.h"
#include "adc.h"
#include "flash_map.h"


//SD-123 Support 16 modbus slots
#define NUM_MODBUS_UPLINK_SLOTS   16
#define MAX_READ_PER_TRANSACTION  64
#define MAX_WRITE_SLOTS           16

bool adc_enabled = false;
uint16_t          modbus_write_data[MAX_WRITE_SLOTS] = {0};
modbus_register_t modbus_upload_regs[NUM_MODBUS_UPLINK_SLOTS] = {0};

void modbus_init_adc( void )
{
	//configure the pin as an analog input
	//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	// ADC-input Configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	

	//Initialise the pin
	HW_GPIO_Init(MODBUS_ADC_PORT, MODBUS_ADC_PIN, &GPIO_InitStruct);
}

uint16_t modbus_read_adc()
{
	uint16_t reading = 0;
	
	reset_watchdog();

	reading = AdcReadCompensate(MODBUS_ADC_CH);
	
	return reading;
}

void genericModbus_onDownlink(uint8_t *buffer, uint8_t size)
{
	int i;
	int slot;
	uint8_t function = 0;
	
	uint8_t downlink_type = buffer[0]>>4;
	
	
	if(downlink_type == downlink_type_generic_modbus_adc)
	{
		generic_modbus_adc_downlink_t downlink = {0};
		Debug_printf("Generic Modbus Downlink [ADC]");
		
		for(i=0;i<size;i++)
		{
			Debug_printf(" %02X", buffer[i]);
			downlink.payload[GENERIC_MODBUS_DOWNLINK_ADC_SIZE-i-1] = buffer[i];
		}
		
		adc_enabled = downlink.members.adc_enabled;
		
		Debug_printf("ADC %sabled\r\n", adc_enabled?"En":":Dis");
	}

	if(downlink_type == downlink_type_generic_modbus_register)
	{
		generic_modbus_downlink_t downlink = {0};
		
		Debug_printf("Generic Modbus Downlink [Table Slot]\r\n");
		Debug_printf("Received Data: ");
		await_uart_tx();
		
		for(i=0;i<size;i++)
		{
			Debug_printf(" %02X", buffer[i]);
			downlink.payload[GENERIC_MODBUS_DOWNLINK_SLOT_SIZE-i-1] = buffer[i];
		}
		
		Debug_printf("\r\n");
		await_uart_tx();
		
		slot = downlink.register_description.table_slot;

		if(slot >= NUM_MODBUS_UPLINK_SLOTS)
		{
			Debug_printf("Slot out of range\r\n");
			return;
		}

		function = downlink.register_description.function_code +1;
		if(function != 3 && function != 4 && function !=16)
		{
			Debug_printf("Only functions 3 and 4 are currently supported\r\n");
			return;
		}

		if(downlink.register_description.slaveID != 0)
		{
			modbus_upload_regs[slot].slaveID        = downlink.register_description.slaveID       ;
			modbus_upload_regs[slot].function_code  = function                                    ;
			modbus_upload_regs[slot].start_Register = downlink.register_description.start_addr    ;
			modbus_upload_regs[slot].register_count = downlink.register_description.register_count;
		}
		else
		{
			//just like on the CLI, a zero in the slave ID disables the slot
			modbus_upload_regs[slot].slaveID        = 0;
			modbus_upload_regs[slot].function_code  = 0;
			modbus_upload_regs[slot].start_Register = 0;
			modbus_upload_regs[slot].register_count = 0;
		}
		
		Debug_printf("Slot %02d\r\n"     ,slot                                   );
		Debug_printf("\tID  : 0x%02X\r\n",modbus_upload_regs[slot].slaveID       );
		Debug_printf("\tFUN : %02d\r\n"  ,modbus_upload_regs[slot].function_code );
		Debug_printf("\tADDR: 0x%04X\r\n",modbus_upload_regs[slot].start_Register);
		Debug_printf("\tCNT : %05d\r\n"  ,modbus_upload_regs[slot].register_count);
		
	}
	
	
	if(downlink_type == downlink_type_generic_modbus_write)
	{
		generic_modbus_write_downlink_t downlink = {0};
		
		Debug_printf("Generic Modbus Downlink [Write Table]\r\n");
		Debug_printf("Received Data: ");
		await_uart_tx();
		
		for(i=0;i<size;i++)
		{
			Debug_printf(" %02X", buffer[i]);
			downlink.payload[GENERIC_MODBUS_DOWNLINK_WRITE_SIZE-i-1] = buffer[i];
		}
		
		Debug_printf("\r\n");
		await_uart_tx();
		
		if(downlink.members.write_slot < MAX_WRITE_SLOTS)
		{
			modbus_write_data[downlink.members.write_slot] = downlink.members.slot_value;
			Debug_printf("Write Slot %d set to 0x%X (%d)\r\n", downlink.members.write_slot, downlink.members.slot_value);
		}
		
	}
	
	save_generic_modbus_config();
}

void genric_modbus_uplink()
{
	uint16_t temp[MAX_READ_PER_TRANSACTION]   = {0};
	lora_generic_modbus_payload_t payload = {0};
	modbus_transaction_result_t   transaction_result = {0};
	
	int       i               = 0;
	int       j               = 0;
	int       k               = 0;
	int       register_count  = 0;
	uint8_t   sequence_number = 0;
	uint16_t* write_head      = modbus_write_data;
	int16_t   write_limit     = MAX_WRITE_SLOTS;
	float     calc            = 0.0f;
	
	//turn on peripheral supply
	LL_GPIO_SetOutputPin(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN);
	
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	//read generic modbus registers, and construct the uplink packet


	//iterate through the table, performing the requested operations
	for(i=0; i<NUM_MODBUS_UPLINK_SLOTS + adc_enabled;i++)
	{
		//calculate the remaining space in the write table.
		write_limit = MAX_WRITE_SLOTS-(write_head - modbus_write_data);
		//ensure that write_limit >= 0, necessary because we are casing to unsigned later
		if(write_limit < 0)
		{
			write_limit = 0;
		}
		
		if(i < NUM_MODBUS_UPLINK_SLOTS)
		{
			if(modbus_upload_regs[i].function_code)
			{
				transaction_result = modbus_transaction(modbus_upload_regs[i], temp,write_head, MAX_READ_PER_TRANSACTION, (uint16_t)write_limit);
			}
			else
			{
				//if we do not have a function code, there is no action on the slot,
				//and therefore the result should be 0 read 0 write
				transaction_result.read  = 0;
				transaction_result.write = 0;
			}
		}
		else
		{
			//get the ADC reading, treat it as a left-over modbus register
			temp[0] = modbus_read_adc();
			Debug_printf("ADC Reading: %d\r\n", temp[0]);
			
			calc = (float)temp[0];
			calc = calc * 3.3;
			//10.9 ~= 1/(1+10) from the 10:1 resistive divider
			calc = calc * 10.9;
			//4095, full scale of the ADC
			calc = calc / 4095;
			temp[1] = (int)calc;
			calc = calc * 100;
			temp[2] = (int)calc - (100*temp[1]);
			Debug_printf("Ext Voltage %d.%02d(10:1 res div)\r\n",temp[1],temp[2]);
			//store it in the result register, and update transaction_result
			transaction_result.read  = 1;
			transaction_result.write = 0;
		}
		
		//on error, we do not want to move the write-head backwards
		if(transaction_result.write > 0)
		{
			//transaction_result.write is gaurenteed <= write_limit
			write_head  += transaction_result.write;
		}
		//iterate through the read data, adding it to the struct to send.
		//note that if there is an error, this loop will not run, as read_regsters would
		//initially be less than or equal to j.
		
		//also note that write instructions should return 0 on success and -x on failure, for compatibility
		//with this structure.
		for(j=0;j<transaction_result.read;j++)
		{
			payload.members.Modbus[4-(register_count%5)] = temp[j];
			register_count++;
			
			if(register_count % 5 == 0)
			{
				//we can use the slot count to do some kind of data validity check.
				//If the slot count is not what we expect, then we can safely assume that we had
				//a failure, and cannot trust the data we have received.
				payload.members.sequence_number = sequence_number;
				sequence_number +=1;
				Uplink(payload.payload, GENERIC_MODBUS_SIZE);
				//by setting voltage to 0, moving to data2, and having the next
				//byte be the register, we can effectivly extend to a 16-bit frame
				//identifier, therefore avoiding the need to modify the platform
				payload.members.sys_voltage = 0;
				
				//clear out the uplink array, so any incomplete packet can have
				//clean 0'd data at the end
				for(k=0;k<5;k++)
				{
					payload.members.Modbus[k] = 0;
				}
			}
		}
	}
	
	//send the rest of the data, if it is not a multiple of 5.
	//also send data if no data has been read at all.
	//This is necessary to prevent a lockout situation if the modbus table is cleared
	//completely via downlinks.
	if(register_count %5 != 0 || register_count == 0)
	{
		payload.members.sequence_number = sequence_number;
		Uplink(payload.payload, GENERIC_MODBUS_SIZE);
	}
	
	//turn off peripheral supply
	LL_GPIO_ResetOutputPin(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN);
}


void save_generic_modbus_config(void)
{
	generic_modbus_config_extra_page_layout_t data_lower  = {0};
	generic_modbus_config_extra_page_layout_t data_upper  = {0};
	generic_modbus_config_page_layout_t       config_page = {0};
	int i = 0;
	
	config_page.members.adc_enabled = adc_enabled;
	
	//populate config arrays from table
	for(i=0;i<8;i++)
	{
		memcpy(&data_lower.members.modbus_slots[i], &modbus_upload_regs[i], sizeof(modbus_upload_regs[0]));
		data_lower.members.write_slots[i] = modbus_write_data[i];
	}
	
	for(i=8;i<16;i++)
	{
		memcpy(&data_upper.members.modbus_slots[i-8], &modbus_upload_regs[i], sizeof(modbus_upload_regs[0]));
		data_upper.members.write_slots[i-8] = modbus_write_data[i];
	}
	
	
	save_extra_config_page(data_lower.raw_bytes, device_specific_page_1);
	save_extra_config_page(data_upper.raw_bytes, device_specific_page_2);
	save_extra_config_page(config_page.raw_bytes,device_specific_page_3);
}

void load_generic_modbus_config(void)
{
	int i;
	generic_modbus_config_extra_page_layout_t data_lower  = {0};
	generic_modbus_config_extra_page_layout_t data_upper  = {0};
	generic_modbus_config_page_layout_t       config_page = {0};
	
	load_extra_config_page(data_lower.raw_bytes, device_specific_page_1);
	load_extra_config_page(data_upper.raw_bytes, device_specific_page_2);
	load_extra_config_page(config_page.raw_bytes,device_specific_page_3);
	
	adc_enabled = config_page.members.adc_enabled;
	
	//Use config arrays to populate table
	for(i=0;i<8;i++)
	{
		memcpy(&modbus_upload_regs[i], &data_lower.members.modbus_slots[i], sizeof(modbus_upload_regs[0]));
		modbus_write_data[i] = data_lower.members.write_slots[i];
	}
	
	for(i=8;i<16;i++)
	{
		memcpy(&modbus_upload_regs[i], &data_upper.members.modbus_slots[i-8], sizeof(modbus_upload_regs[0]));
		modbus_write_data[i] = data_upper.members.write_slots[i-8];
	}
}

void cli_modbus_help()
{
	Debug_printf("Usage: modbus [slot] [address] [function] [register] [count]\r\n");
	await_uart_tx();
	Debug_printf("\tSlot    : the upload slot                           , 0-%d\r\n", NUM_MODBUS_UPLINK_SLOTS);
	await_uart_tx();
	Debug_printf("\tAddress : the modbus device address                 , 0-255\r\n");
	Debug_printf("\t\tAddress 0 disables the slot\r\n");
	await_uart_tx();
	Debug_printf("\tFunciton: The function code to send with the request, 1-15\r\n");
	await_uart_tx();
	Debug_printf("\t\tCurrently only functions 3, 4, and 16 are supported\r\n");
	await_uart_tx();
	Debug_printf("\tRegister: the register to read from the device      , <65535\r\n");
	await_uart_tx();
	Debug_printf("\tCount   : the number of consecutive registers");
	Debug_printf(" to read from the device\r\n");
	await_uart_tx();
	Debug_printf("Usage: modbus write [slot] [value]\r\n");
	await_uart_tx();
	Debug_printf("\tSet write slot [slot] to [value]\r\n");
	Debug_printf("\tSlot    : The write slot to populate                , 0-%d\r\n", MAX_WRITE_SLOTS);
	Debug_printf("\tValue   : The value to put in the write slot        , 0-65535\r\n");
	await_uart_tx();
	Debug_printf("\t\tUsed for modbus write functions\r\n");
	await_uart_tx();
	Debug_printf("Usage: modbus adc [on|off]\r\n");
	await_uart_tx();
	Debug_printf("\tEnables or disables the ADC read at the end of the modbus functions\r\n");
	await_uart_tx();
	Debug_printf("Usage: modbus show\r\n");
	await_uart_tx();
	Debug_printf("\tShows which modbus registers are linked to uplink slots\r\n");
	await_uart_tx();
	Debug_printf("Usage: modbus clear\r\n");
	await_uart_tx();
	Debug_printf("\tClear all slots in the modbus table\r\n");
}

//this function is for setting and getting the modbus slots for uplink on modbus devices
void cli_modbus_implementation(int argc, char* argv[])
{
	//argv should contain slot, address, register
	//   slot    : the upload slot           [0,1,2,3]
	//   address : the modbus device address [0,1,2,3]
	//   register: the modbus register       [uint16_t]
	uint8_t  slot;
	uint8_t  address;
	uint16_t reg;
	uint8_t  fun;
	uint16_t count;
	
	if(device.cli_commands & cmd_modbus)
	{
		//argc should be 1 for reading slots
		if(argc >= 1)
		{
			if(!strcmp(argv[0], "show"))
			{
				Debug_printf("***********FUNCTION TABLE*************\r\n");
				await_uart_tx();
				Debug_printf("| SLOT |  ID  | FUN |  ADDR  |  CNT  |\r\n");
				Debug_printf("|------|------|-----|--------|-------|\r\n");
				await_uart_tx();
				//Debug_printf("|------|------|-----|--------|-------|\r\n")
				//Debug_printf("|  00  | 0x00 | 00  | 0x0000 | 00000 |\r\n")
				for(slot=0;slot<NUM_MODBUS_UPLINK_SLOTS;slot++)
				{
					
					Debug_printf("|  %02d  | 0x%02X | %02d  | 0x%04X | %05d |\r\n",
		                          slot,
					              modbus_upload_regs[slot].slaveID,
					              modbus_upload_regs[slot].function_code,
					              modbus_upload_regs[slot].start_Register,
					              modbus_upload_regs[slot].register_count);
					await_uart_tx();
				}
				
				Debug_printf("\r\n");
				Debug_printf("***WRITE TABLE***\r\n");
				Debug_printf("| SLOT | VALUE  |\r\n");
				//Debug_printf("| SLOT | VALUE  |");
				//Debuf_printf("|  00  | 0x0000 |");
				
				for(slot=0;slot<MAX_WRITE_SLOTS;slot++)
				{
					Debug_printf("|  %02d  | 0x%04X |\r\n",slot,modbus_write_data[slot]);					              
					await_uart_tx();
				}
				
				Debug_printf("ADC %sabled\r\n", adc_enabled?"En":"Dis");
				
				return;
			}
			
			if(!strcmp(argv[0], "clear"))
			{
				Debug_printf("Clearing tables\r\n");
				for(slot=0;slot<NUM_MODBUS_UPLINK_SLOTS;slot++)
				{
					
					modbus_upload_regs[slot].slaveID        = 0;
					modbus_upload_regs[slot].start_Register = 0;
					modbus_upload_regs[slot].function_code  = 0;
					modbus_upload_regs[slot].register_count = 0;
				}
				
				for(slot=0;slot<MAX_WRITE_SLOTS;slot++)
				{
					modbus_write_data[slot] = 0;
				}
				return;
			}
		}
		
		if(argc == 2)
		{
			if(!strcmp(argv[0], "adc"))
			{
				if(!strcmp(argv[1], "on"))
				{
					adc_enabled = true;
					Debug_printf("ADC Enabled\r\n");
					return;
				}
				if(!strcmp(argv[1], "off"))
				{
					adc_enabled = false;
					Debug_printf("ADC Disabled\r\n");
					return;
				}
				cli_modbus_help();
				return;
			}
		}
		
		if(argc == 3)
		{
			//modbus write [slot] [value]
			if(!strcmp(argv[0],"write"))
			{
				slot = atoi(argv[1]);
				reg  = atoi(argv[2]);
				
				if(slot < MAX_WRITE_SLOTS)
				{
					modbus_write_data[slot] = reg;
					Debug_printf("Write slot %d set to 0x%04X (%d)\r\n", slot, reg, reg);
				}
				else
				{
					cli_modbus_help();
				}
				
				return;
			}
		}
		
		//two arguments is all that is needed to clear a slot.
		//more than two to set the slot.
		if(argc >=2)
		{
			slot    = atoi(argv[0]);
			address = atoi(argv[1]);
		}
		else
		{
			cli_modbus_help();
			return;
		}
		
		//bounds checking, off-by one becasue arrays start at 0
		if(slot > NUM_MODBUS_UPLINK_SLOTS-1)
		{
			//slot out of range
			Debug_printf("Slot out of range\r\n");
			cli_modbus_help();
			return;
		}		
		
		if(address == 0)
		{
			modbus_upload_regs[slot].slaveID        = 0;
			modbus_upload_regs[slot].start_Register = 0;
			modbus_upload_regs[slot].function_code  = 0;
			modbus_upload_regs[slot].register_count = 0;
			Debug_printf("Modbus slot %d cleared\r\n", slot);
			return;
		}
		
		//at five arguments, we can set a slot
		if(argc >= 5)
		{
			fun     = atoi(argv[2]);
			reg     = atoi(argv[3]);
			count   = atoi(argv[4]);
		}
		else
		{
			cli_modbus_help();
			return;
		}

		//Bounds checking for supported functions
		if(fun != 3 && fun != 4 && fun != 16)
		{
			Debug_printf("Only Functions 3, 4, and 16 are currently supported\r\n");
			cli_modbus_help();
			return;
		}
		
		//populate the upload slot with the arguments
		modbus_upload_regs[slot].slaveID        = address;
		modbus_upload_regs[slot].start_Register = reg;
		modbus_upload_regs[slot].function_code  = fun;
		modbus_upload_regs[slot].register_count = count;
		
		Debug_printf("Slot %02d\r\n",slot);
		Debug_printf("\tID  : 0x%02X\r\n",address);
		Debug_printf("\tFUN : %02d\r\n"  ,fun);
		Debug_printf("\tADDR: 0x%04X\r\n",reg);
		Debug_printf("\tCNT : %05d\r\n"  ,count);
	}
}

