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


#include "radio_common.h"
#include "lora_sensum.h"
#include "sigfox_sensum.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "tiny_vsnprintf.h"
#include "lora.h"
#include "at.h"
#include "watchdog.h"
#include "counter.h"
#include "delays.h"
#include "global.h"
#include "flash_map.h"
#include "radio.h"
#include "sx1276.h"
#include "sensum_version.h"
#include "timeServer.h"

uint8_t  rx_response_buffer[MAX_RX_DATA+1] = {0xFF};
uint8_t  rx_buffer_length;

void Print_radio_information()
{
	#ifdef RAIDO_LORA_INTERNAL
	{
		print_lora_radio_information();
	}
	#endif
	
	#ifdef RADIO_SIGFOX_AT
	{
		sf_print_info();
	}
	#endif
}

bool radio_joined()
{
	#ifdef RAIDO_LORA_INTERNAL
	{
		return internal_NetworkJoinStatus();
	}
	#endif
	#ifdef RADIO_SIGFOX_AT
	{
		return true;
	}
	#endif
}

uint8_t fourBit_battery_calculation()
{
	uint32_t x = 0;
	//get the system voltage
	x = HW_GetBatteryLevel_mv();
	//subtract 2 volts, as 2V is the minimum we decided
	x = x-2000;
	//limit to 3.6V max
	if(x > 1500)
	{
				 x = 1500;
	}
	//now divide by 100 to get the range we are after
	x = x/100;
	return (uint8_t)x;
}

static uint8_t lookup_char(uint8_t in)
{
	switch(in)
	{
		case '0':
			return 0x0;
		case '1':
			return 0x1;
		case '2':
			return 0x2;
		case '3':
			return 0x3;
		case '4':
			return 0x4;
		case '5':
			return 0x5;
		case '6':
			return 0x6;
		case '7':
			return 0x7;
		case '8':
			return 0x8;
		case '9':
			return 0x9;
		case 'A':
		case 'a':
			return 0xA;
		case 'B':
		case 'b':
			return 0xB;
		case 'C':
		case 'c':
			return 0xC;
		case 'D':
		case 'd':
			return 0xD;
		case 'E':
		case 'e':
			return 0xE;
		case 'F':
		case 'f':
			return 0xF;
		default:
			return 0xF;
	
	}
}

void populateHash(uint8_t* storage, int count)
{
	uint8_t* version = VERSION_HASH;
	int i = 0;
	for(;count > 0; count--)
	{
		storage[count-1] = lookup_char(version[i])<<4;
		i++;
		storage[count-1] += lookup_char(version[i]);
		i++;
	}
}

void radio_init()
{
	#ifdef RAIDO_LORA_INTERNAL
	{
		lora_init();
	}
	#endif
	#ifdef RADIO_SIGFOX_AT
	{
		//no specific init funcitons
	}
	#endif
}

void sendStartupPacket(void)
{
	startup_packet_t packet ={0};
	
	packet.members.sys_voltage = fourBit_battery_calculation();
	packet.members.pkt_type = packet_type_boot;
	
	packet.members.hour = HW_RTC_GetHour();
	packet.members.minute = HW_RTC_GetMinute();
	packet.members.second = HW_RTC_GetSecond();
	
	packet.members.device_mode = device_mode;
	
	
	populateHash(packet.members.Hash, sizeof(packet.members.Hash)/sizeof(uint8_t));
	packet.members.HW_Major = HW_MAJOR;
	packet.members.HW_Minor = HW_MINOR;
	
	Debug_printf("First Transmission\r\n");
	await_uart_tx();
	Debug_printf("Time       : %02d:%02d:%02d\r\n",packet.members.hour,
	                                               packet.members.minute,
	                                               packet.members.second);
	await_uart_tx();
	Debug_printf("Device Mode: %s\r\n", device.mode_name);
	
	Uplink(packet.payload, STARTUP_PACKET_SIZE);
}



transmit_status_e Uplink(uint8_t payload[], uint8_t size)
{
	transmit_status_e result;

	//if we are sigfox, we want sigfox uplink.
	//if we are LoRa, we want LoRa uplink.
	
	#ifdef RAIDO_LORA_INTERNAL
	{
		result = Lora_Uplink(payload, size);
	}
	#endif
	
	#ifdef RADIO_SIGFOX_AT
	{
		sf_send(payload, size);
	}
	#endif
	
	while(result == transmit_status_received_downlink)
	{
		//respond to downlink
		//populate a header with packet type and voltage
		rx_response_buffer[rx_buffer_length]  = ((uint8_t)packet_type_downlink_response) << 4;
		rx_response_buffer[rx_buffer_length] += fourBit_battery_calculation();
		
		Debug_printf("Header: %02X\r\n",rx_response_buffer[rx_buffer_length]);
		Debug_printf("RX Buffer Length: %d", rx_buffer_length);
		
		#ifdef RAIDO_LORA_INTERNAL
		{
			result = Lora_Uplink(rx_response_buffer,rx_buffer_length+1);
		}
		#endif
		
		#ifdef RADIO_SIGFOX_AT
		{
			sf_send(payload, size);
		}
		#endif
	}
	
	return result;
}

bool default_downlink(uint8_t *buffer, uint8_t size)
{
	uint8_t i;
	uint8_t hours;
	uint8_t minutes;
	uint8_t seconds;	
	
	
	for(i=0;i<size;i++)
	{
		rx_response_buffer[(size-i)-1] = buffer[i];
	}
	
	rx_buffer_length = size;
	
	uint8_t downlink_type = buffer[0]>>4;
	Debug_printf("Downlink type: %d\r\n", downlink_type);
	
	//check if the downlink packet header is 0x0, this would indicate a general
	//configuration downlink
	if(downlink_type == downlink_type_generic_config)
	{
		generic_downlink_packet_t downlink = {0};
		Debug_printf("Generic downlink Function\r\n");
		Debug_printf("Received Data: ");
		await_uart_tx();
		
		for(i=0;i<size;i++)
		{
			Debug_printf(" %02X", buffer[i]);
			downlink.payload[GENERIC_CONFIG_DOWNLINK_SIZE-i-1] = buffer[i];
		}
		
		Debug_printf("\r\n");
		await_uart_tx();
	
		Debug_printf("Generic Configuration Downlink Received\r\n");
		await_uart_tx();
		Debug_printf("\tMode              : %s\r\n"        , get_string_from_mode_number(downlink.members.device_mode));
		await_uart_tx();
		Debug_printf("\tWakeup            : %d minutes\r\n", downlink.members.wakeup_period);
		await_uart_tx();
		Debug_printf("\tTransmit Interval : %d wakeups\r\n", downlink.members.transmit_period);
		await_uart_tx();
		Debug_printf("\tRejoin            : %d hours\r\n"  , downlink.members.rejoin_period);
		await_uart_tx();
		Debug_printf("\tHour              : %d\r\n"        , downlink.members.hour);
		await_uart_tx();
		Debug_printf("\tMinute            : %d\r\n"        , downlink.members.minute);
		await_uart_tx();
		Debug_printf("\tSecond            : %d\r\n"        , downlink.members.second);
		await_uart_tx();
		Debug_printf("\tUpdate Time       : %s\r\n"        , (downlink.members.update_time)? "YES":"NO");
		await_uart_tx();
		
		
		if(downlink.members.device_mode != 0)
		{
			if(downlink.members.device_mode < get_number_device_modes())
			{
				device_mode = downlink.members.device_mode;
				//we update the device behaviour, to ensure that future calls are correct
				update_device_behaviour();
				Debug_printf("Device Mode Updated\r\n");
				
			}
			else
			{
				Debug_printf("Device Mode Out of Range\r\n");
			}
		}
		else
		{
			Debug_printf("Device Mode Not Updated\r\n");
		}
		
		if(downlink.members.wakeup_period != 0)
		{
			transmit_interval_ms = downlink.members.wakeup_period * 60000;
			Debug_printf("Wakeup Period Updated\r\n");
		}
		else
		{
			Debug_printf("Wakeup Period Not Updated\r\n");
		}
		
		if(downlink.members.transmit_period !=0)
		{
			wakeups_per_uplink = downlink.members.transmit_period;
			Debug_printf("Transmit Period Updated\r\n");
		}
		else
		{
			Debug_printf("Transmit Period Not Updated\r\n");
		}
		
		if(downlink.members.update_time != 0
			&& downlink.members.hour <24
			&& downlink.members.minute <60
			&& downlink.members.second < 60)
		{
			if(downlink.members.hour != HW_RTC_GetHour())
			{
				//if we have crossed an hour boundary by changing the time
				//we should fire the interrupt callback, and ensure that the
				//main-program function is set to run.
				device.on_hourly_alarm_interrupt();
				wake_via_rtc_b = 1;

				//we are not going to call manage rejoin here, as the worst that will
				//happen by not calling is is that the device will rejoin an hour late.
			}
			
			hours   = downlink.members.hour;
			minutes = downlink.members.minute;
			seconds = downlink.members.second;
			HW_RTC_setTime(hours,minutes,seconds);
			Debug_printf("Time Updated \r\n");
			
			//how to manage the alarms being incorrect after a update?
			//Theoretically all timers have expired at this point, except for the
			//wakeup timer. If this is the case, then all we should have to do is
			//indicate to the wakeup timer that the clock has changed
			
			//We will handle this for now by forcibly expiring all timers.
			//This should ensure that no timing errors occur here.
			//It may be dangerous though, as it removes whatever delays the program was relying on.
			RTC_modified = true;
			TimerClearQueue();
			
			Debug_printf("Queue Cleared \r\n");
			await_uart_tx();

			
		}
		else
		{
			Debug_printf("Time Not Updated\r\n");
		}
		
		if(downlink.members.rejoin_period != 0)
		{
			if(hours_until_rejoin > downlink.members.rejoin_period)
			{
				hours_until_rejoin = downlink.members.rejoin_period;
			}
			
			lora_hours_until_rejoin = downlink.members.rejoin_period;

			if (device_mode == 20)
			{
				lora_hours_until_rejoin *= 12;
			}
			
			//lets not force the rejoin
			Debug_printf("Rejoin Period Updated\r\n");
		}
		else
		{
			Debug_printf("Rejoin Period Not Updated\r\n");
		}
		
		
		
		save_config();
		
		//save data here should override the old data structure, if the device mode
		//has changed. Another downlink (device specific) should manage the 
		device.save_data();
		
		//if we didn't reset, then we need to queue up a new message to reply
		//to the received message.
		//This might be handled by the lora send method though.
		
		return true;
	}
	else
	{
		device.on_downlink(buffer,size);
	}
	
	return false;
}

void save_radio_config_page(void)
{
	#ifdef RAIDO_LORA_INTERNAL
		save_lora_config_page();
	#endif
}
void load_radio_config_page(void)
{
	#ifdef RAIDO_LORA_INTERNAL
		load_lora_config_page();
	#endif
}


