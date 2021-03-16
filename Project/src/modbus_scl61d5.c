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
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "delays.h"
#include "watchdog.h"
#include "flash_map.h"
#include "radio_common.h"

#ifndef DISABLE_MODBUS_DEBUG
	#define DBG_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define DBG_printf(...)
#endif

//Debug_printf("\tpower: %d.%d\r\n", voltage/10, voltage%10);
#define  SCL61D5_INSTANT_FLOW_REGISTER 0
#define  SCL61D5_READ_FUNCTION 3
#define  SCL61D5_INSTNAT_FLOW_SIZE 2 //registers

#define FLOAT_MIN -3.4e38
#define FLOAT_MAX  3.4e38

typedef union
{
	uint32_t uint32;
	float flt;
} b32_converter_t;


float    watermark_high = FLOAT_MIN;
uint16_t watermark_high_TOD = 0;

float    watermark_low = FLOAT_MAX;
uint16_t watermark_low_TOD = 0;



uint16_t reads_per_uplink = 288;
uint8_t  device_address = 1;

float scl61d5_get_instant_flow()
{
	uint16_t temp[SCL61D5_INSTNAT_FLOW_SIZE] = {0};
	modbus_transaction_result_t read_registers = {0};
	
	modbus_register_t reg = {0};
	
	reg.slaveID        = device_address               ;
	reg.function_code  = SCL61D5_READ_FUNCTION        ;
	reg.start_Register = SCL61D5_INSTANT_FLOW_REGISTER;
	reg.register_count = SCL61D5_INSTNAT_FLOW_SIZE    ;
	
	
	read_registers = modbus_transaction(reg, temp, (void*)0, SCL61D5_INSTNAT_FLOW_SIZE, 0);
	
	if(read_registers.read == 2)
	{
		//now convert the 2*uint16_t to float
		b32_converter_t converter;
		converter.uint32 = (((uint32_t)temp[0])<<16)+temp[1];
		return converter.flt;
	}
	else
	{
		return 0.0;
	}
}

//fairly certain that this will be removed soon. The whole file is likely redundant
void scl61d5_uplink()
{
	//form packet using watermark
	scl61d5_payload_t payload;
	
	payload.members.watermark_high = watermark_high;
	payload.members.watermark_low  = watermark_low;
	payload.members.high_tod = watermark_high_TOD;
	payload.members.low_tod = watermark_low_TOD;
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;

	//uplink data
	Uplink(payload.payload,SCL61D5_SIZE);
}

//needs to get the current minute of day, in order to record watermark
//wake up 1440 times, transmit once.
void scl61d5_wakeup()
{
	static uint16_t uplink_counter = 0;
	//get reading
	float reading;
	
	Debug_printf("Reading Flow\r\n");
	reading = scl61d5_get_instant_flow();
	
	uplink_counter++;
	
	//compare to watermark
		//if higher, record new watermark and time of day
		//if lower, ignore
	if(reading > watermark_high)
	{
		Debug_printf("New High Watermark set\r\n");
		watermark_high = reading;
		watermark_high_TOD = uplink_counter;
	}
	
	if(reading < watermark_low)
	{
		Debug_printf("New Low Watermark set\r\n");
		watermark_low = reading;
		watermark_low_TOD = uplink_counter;
	}
	

	
	//at end of day, uplink the watermark, and reset.
	if(uplink_counter >= reads_per_uplink)
	{
		Debug_printf("Sending data and resetting watermarks\r\n");
		//uplink watermark
		scl61d5_uplink();
		//reset
		watermark_high     = FLOAT_MIN;
		watermark_low      = FLOAT_MAX;
		watermark_high_TOD = 0;
		watermark_low_TOD  = 0;
		
		//reset uplink counter
		uplink_counter = 0;
	}
}

void scl61d5_save_config()
{
	scl61d5_config_page_layout_t config = {0};
	
	config.members.reads_per_uplink = reads_per_uplink;
	config.members.device_address   = device_address  ;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void scl61d5_load_config()
{
	scl61d5_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	
	reads_per_uplink = config.members.reads_per_uplink;
	device_address   = config.members.device_address  ;
}




void scl61d5Downlink(uint8_t *buffer, uint8_t size)
{
	scl61d5_downlink_t downlink = {0};
	int i;
	
	Debug_printf("SCL61D5 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[SCL61D5_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.config.downlink_type == downlink_type_scl61d5_config)
	{
		Debug_printf("Downlink scl61d5_config Type OK\r\n");
		if(downlink.config.reads_per_uplink != 0)
		{
			reads_per_uplink = downlink.config.reads_per_uplink;
			Debug_printf("New reads per uplink: %d\r\n", reads_per_uplink);
		}
		
		if(downlink.config.device_address != 0)
		{
			device_address = downlink.config.device_address;
			Debug_printf("New Device Address: 0x%02X\r\n", device_address);
		}
	}

	
	//because one of config or data must have changed, we should save them
	save_config();
}

