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
#include "sht30.h"

#include "stm32l0xx.h"                  // Device header

#include "hw.h"

#include "debug_uart.h"
#include "flash_map.h"
#include "lora_sensum.h"
#include "global.h"
#include "radio_common.h"

#define SHT30_ADDR_1 0x44
#define SHT30_ADDR_2 0x45
#define CRC_POLYNOMIAL 0x131 //P(x)=x^8+x^5+x^4+1 = 100110001

static int16_t upper_temperature_threshold         = INT16_MAX;
static int16_t lower_temperature_threshold         = INT16_MIN;
static bool    upper_temperature_threshold_enabled = false;
static bool    lower_temperature_threshold_enabled = false;
static int16_t upper_humidity_threshold            = INT16_MAX;
static int16_t lower_humidity_threshold            = INT16_MIN;
static bool    upper_humidity_threshold_enabled    = false;
static bool    lower_humidity_threshold_enabled    = false;
static uint8_t threshold_wakeups                   = 1;


//calculates 8-Bit checksum with given polynomial
static uint8_t validCRC(uint8_t *data, uint8_t data_length, uint8_t checksum)
{
	uint8_t crc = 0xFF;
	uint8_t byte_counter;
	uint8_t bit_counter;
	
	for (byte_counter = 0; byte_counter < data_length; byte_counter++)
	{ 
		crc ^= (*data);
		data++;
		for (bit_counter = 0; bit_counter < 8; bit_counter++)
		{ 
			if (crc & 0x80)
			{
				crc = (crc << 1) ^ CRC_POLYNOMIAL;
			}
			else
			{
				crc = (crc << 1);
			}
		}
	}
	if (crc != checksum) return 0;
	else return 1;
}

//This function will get the temperature from the sht30, using the hold-master method
sht30Reading_t sht30GetReading()
{
	sht30Reading_t results = {0};
	uint8_t data_to_send[] = {0x2C, 0x06};
	uint8_t data_send_length = 2;
	uint8_t data_to_read[6] = {0};
	uint8_t data_read_length = 6;
	int16_t temp;
	float  tempf;
	
	//request temperature data from the sht30
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot read the sensors.
		for(temp = 0; temp < data_read_length; temp++)
		{
			data_to_read[temp] = 0;
		}
	}
	else
	{
		i2c1_send(SHT30_ADDR_1, data_to_send, data_send_length, 1);
		i2c1_receive(SHT30_ADDR_1, data_to_read, data_read_length,1);
	}
	//check the data validity using CRC, temperature
	if(validCRC(data_to_read, 2, *(data_to_read+2)))
	{
		Debug_printf("T1 OK\r\n");
		//CRC OK
		//at this point we know we have valid data, time to convert T1
		temp = data_to_read[0];
		temp = temp<<8;
		temp = temp + data_to_read[1];
		
		//conversion from raw to Deg C
		tempf = (float)temp;
		tempf = tempf*0.002670328832;
		tempf = tempf - 45;
		
		//scale up for 2dp precision
		tempf = tempf * 100;
		
		results.T1 = (int16_t)tempf;
	}
	else
	{
		//CRC ERROR, return minimum value
		results.T1 = 0x8000;
	}
	
		//check the data validity using CRC, humidity
	if(validCRC(data_to_read+3, 2, *(data_to_read+5)))
	{
		Debug_printf("H1 OK\r\n");
		//CRC OK
		//at this point we know we have valid data, time to convert H1
		temp = data_to_read[3];
		temp = temp<<8;
		temp = temp + data_to_read[4];
		
		//conversion from raw to %RH
		tempf = (float)((uint16_t)temp);
		tempf = tempf*0.00152590219;
		
		//scale up for 2dp precision
		tempf = tempf * 100;
		
		results.H1 = (int16_t)tempf;
	}
	else
	{
		//CRC ERROR, return maximum value to signify
		results.H1 = 0xFFFF;
	}
	
	//request temperature data from the sht30
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot read the sensors.
		for(temp = 0; temp < data_read_length; temp++)
		{
			data_to_read[temp] = 0;
		}
	}
	else
	{
		i2c1_send(SHT30_ADDR_2, data_to_send, data_send_length, 1);
		i2c1_receive(SHT30_ADDR_2, data_to_read, data_read_length,1);
	}

	//check the data validity using CRC, temperature
	if(validCRC(data_to_read, 2, *(data_to_read+2)))
	{
		Debug_printf("T2 OK\r\n");
		//CRC OK
		//at this point we know we have valid data, time to convert T1
		temp = data_to_read[0];
		temp = temp<<8;
		temp = temp + data_to_read[1];
		
		//conversion from raw to Deg C
		tempf = (float)temp;
		tempf = tempf*0.002670328832;
		tempf = tempf - 45;
		
		//scale up for 2dp precision
		tempf = tempf * 100;
		
		results.T2 = (int16_t)tempf;
	}
	else
	{
		//CRC ERROR, return minimum value
		results.T2 = 0x8000;
	}
	
		//check the data validity using CRC, humidity
	if(validCRC(data_to_read+3, 2, *(data_to_read+5)))
	{
		Debug_printf("H2 OK\r\n");
		//CRC OK
		//at this point we know we have valid data, time to convert H1
		temp = data_to_read[3];
		temp = temp<<8;
		temp = temp + data_to_read[4];
		
		//conversion from raw to %RH
		tempf = (float)((uint16_t)temp);
		tempf = tempf*0.00152590219;
		
		//scale up for 2dp precision
		tempf = tempf * 100;
		
		results.H2 = (int16_t)tempf;
	}
	else
	{
		//CRC ERROR, return maximum value to signify
		results.H2 = 0xFFFF;
	}
	
	return results;
	
}

void sht30_uplink( void )
{
	sht30_payload_t payload = {0};
	sht30Reading_t data = {0};
	
	data = sht30GetReading();
	
	payload.members.Temperature1 = data.T1;
	payload.members.Temperature2 = data.T2;
	payload.members.Humidity1 = data.H1;
	payload.members.Humidity2 = data.H2;
	
	Debug_printf("T1:%02d.%02d C\r\n",payload.members.Temperature1/100,payload.members.Temperature1%100);
	Debug_printf("H1:%02d.%02d RH\r\n",payload.members.Humidity1/100, payload.members.Humidity1%100);
	Debug_printf("T2:%02d.%02d C\r\n",payload.members.Temperature2/100,payload.members.Temperature2%100);
	Debug_printf("H2:%02d.%02d RH\r\n",payload.members.Humidity2/100, payload.members.Humidity2%100);

	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	Uplink(payload.payload, SHT30_SIZE);
}

void sht30_onWakeup( void )
{
	if(wakeup_count % wakeups_per_uplink == 0)
	{
		Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
		sht30_uplink();
		return;
	}
	
	//this is only executed if at least one of the thresholds is enabled
	if((wakeup_count % threshold_wakeups == 0) && 
		(upper_temperature_threshold_enabled ||
		 lower_temperature_threshold_enabled ||
		 upper_humidity_threshold_enabled    ||
		 lower_humidity_threshold_enabled))
	{
		Debug_printf("Threshold Check (%d)\r\n", threshold_wakeups);
		sht30Reading_t result = sht30GetReading();
		
		if((result.T1 > upper_temperature_threshold || result.T2 > upper_temperature_threshold) && upper_temperature_threshold_enabled)
		{
			Debug_printf("Upper Threshold exceeded\r\n");
			sht30_uplink();
			return;
		}
		
		if((result.H1 > upper_humidity_threshold || result.H2 > upper_humidity_threshold) && lower_humidity_threshold_enabled)
		{
			Debug_printf("Lower Threshold exceeded\r\n");
			sht30_uplink();
			return;
		}
	}
}

              

void sht30_save_config()
{
	sht30_config_page_layout_t config = {0};
	
	config.members.humidity_threshold_upper    = upper_humidity_threshold           ;
	config.members.humidity_thredhold_lower    = lower_humidity_threshold           ;
	config.members.temperature_threshold_upper = upper_temperature_threshold        ;
	config.members.temperature_threshold_lower = lower_temperature_threshold        ;
	config.members.humidity_upper_enabled      = upper_humidity_threshold_enabled   ;
	config.members.humidity_lower_enabled      = lower_humidity_threshold_enabled   ;
	config.members.temperature_upper_enabled   = upper_temperature_threshold_enabled;
	config.members.temperature_lower_enabled   = lower_temperature_threshold_enabled;
	config.members.threshold_wakeups           = threshold_wakeups                  ;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void sht30_load_config()
{
	sht30_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	

	upper_humidity_threshold            = config.members.humidity_threshold_upper   ;
	lower_humidity_threshold            = config.members.humidity_thredhold_lower   ;
	upper_temperature_threshold         = config.members.temperature_threshold_upper;
	lower_temperature_threshold         = config.members.temperature_threshold_lower;
	upper_humidity_threshold_enabled    = config.members.humidity_upper_enabled     ;
	lower_humidity_threshold_enabled    = config.members.humidity_lower_enabled     ;
	upper_temperature_threshold_enabled = config.members.temperature_upper_enabled  ;
	lower_temperature_threshold_enabled = config.members.temperature_lower_enabled  ;
	threshold_wakeups                   = config.members.threshold_wakeups          ;                

}


void sht32_onDownlink(uint8_t *buffer, uint8_t size)
{
	//downlink_type_threshold_1
	threshold_downlinks_t downlink = {0};
	int i;
	
	Debug_printf("SHT30 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[THRESHOLD_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.threshold.downlink_type == downlink_type_sht30_temperature)
	{
		Debug_printf("Temperature Threshold Downlink\r\n");
		
		Debug_printf("Temperature Upper: %d.%02d\r\n"   ,downlink.threshold.upper/100, downlink.threshold.upper%100);
		Debug_printf("Temperature Lower: %d.%02d\r\n"   ,downlink.threshold.lower/100, downlink.threshold.lower%100);
		Debug_printf("Upper Enabled    : %s\r\n"        ,downlink.threshold.enabled_h ? "YES":"NO");
		Debug_printf("Lower Enabled    : %s\r\n"        ,downlink.threshold.enabled_l ? "YES":"NO");
		Debug_printf("Update Upper     : %s\r\n"        ,downlink.threshold.update_h  ? "YES":"NO");
		Debug_printf("Update Lower     : %s\r\n"        ,downlink.threshold.update_l  ? "YES":"NO");
		Debug_printf("Interval         : %d wakeups\r\n",downlink.threshold.period);
		Debug_printf("Update Interval  : %s\r\n"        ,downlink.threshold.period    ? "YES":"NO");
		
		if(downlink.threshold.update_h)
		{
			upper_temperature_threshold = downlink.threshold.upper;
			upper_temperature_threshold_enabled = downlink.threshold.enabled_h;
		}
		
		if(downlink.threshold.update_l)
		{
			lower_temperature_threshold = downlink.threshold.lower;
			lower_temperature_threshold_enabled = downlink.threshold.enabled_l;
		}
		
		if(downlink.threshold.period != 0)
		{
			threshold_wakeups = downlink.threshold.period;
		}
	}
	
	if(downlink.threshold.downlink_type == downlink_type_sht30_humidity)
	{
		Debug_printf("Humidity Threshold Downlink\r\n");
		
		Debug_printf("Humidity Upper: %d.%02d\r\n"      ,downlink.threshold.upper/100, downlink.threshold.upper%100);
		Debug_printf("Humidity Lower: %d.%02d\r\n"      ,downlink.threshold.lower/100, downlink.threshold.lower%100);
		Debug_printf("Upper Enabled    : %s\r\n"        ,downlink.threshold.enabled_h ? "YES":"NO");
		Debug_printf("Lower Enabled    : %s\r\n"        ,downlink.threshold.enabled_l ? "YES":"NO");
		Debug_printf("Update Upper     : %s\r\n"        ,downlink.threshold.update_h  ? "YES":"NO");
		Debug_printf("Update Lower     : %s\r\n"        ,downlink.threshold.update_l  ? "YES":"NO");
		Debug_printf("Interval         : %d wakeups\r\n",downlink.threshold.period);
		Debug_printf("Update Interval  : %s\r\n"        ,downlink.threshold.period    ? "YES":"NO");
		
		if(downlink.threshold.update_h)
		{
			upper_humidity_threshold = downlink.threshold.upper;
			upper_humidity_threshold_enabled = downlink.threshold.enabled_h;
		}
		
		if(downlink.threshold.update_l)
		{
			lower_humidity_threshold = downlink.threshold.lower;
			lower_humidity_threshold_enabled = downlink.threshold.enabled_l;
		}
		
		if(downlink.threshold.period != 0)
		{
			threshold_wakeups = downlink.threshold.period;
		}
	}
	
	sht30_save_config();
}


void sht30_cli_threshold_help()
{
	Debug_printf("Usage: threshold [enable|disable] [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\tEnable or Disable [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	Debug_printf("\t\th_upper\r\n");
	Debug_printf("\t\th_lower\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold set [threshold] [value]\r\n");
	await_uart_tx();
	Debug_printf("\tset [threshold] trigger value to [value]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	Debug_printf("\t\th_upper\r\n");
	Debug_printf("\t\th_lower\r\n");
	await_uart_tx();
	Debug_printf("\t[value] is an integer\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold wakeup [value]\r\n");
	await_uart_tx();
	Debug_printf("\tCheck threshold every [value] wakeups\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold show\r\n");
	await_uart_tx();
	Debug_printf("\tShows the current threshold configuration\r\n");
	await_uart_tx();
}

void sht30_cli_threshold(int argc, char *argv[])
{
	//"threshold enable t_upper"
	//"threshold disable t_upper"
	//"threshold set t_upper [value]"
	float value;
	
	if(argc == 1)
	{
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("Upper temperature        :%d.%02d\r\n"   , upper_temperature_threshold/100, upper_temperature_threshold%100);
			await_uart_tx();
			Debug_printf("Upper temperature Enabled:%s\r\n"        , upper_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("lower temperature        :%d.%03d\r\n"   , lower_temperature_threshold/100, lower_temperature_threshold%100);
			await_uart_tx();
			Debug_printf("lower temperature Enabled:%s\r\n"        , lower_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("Upper humidity           :%d.%02d\r\n"   , upper_humidity_threshold/100, upper_humidity_threshold%100);
			await_uart_tx();
			Debug_printf("Upper humidity Enabled   :%s\r\n"        , upper_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("lower humidity           :%d.%03d\r\n"   , lower_humidity_threshold/100, lower_humidity_threshold%100);
			await_uart_tx();
			Debug_printf("lower humidity Enabled   :%s\r\n"        , lower_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("Checking thresholds every %d wakeups\r\n", threshold_wakeups);
			await_uart_tx();
			return;
		}
	}
	
	if(argc == 2)
	{
		if(!strcmp(argv[0],"wakeup"))
		{
			uint8_t value = atoi(argv[1]);
			
			if(value <= 0 || value >= 255)
			{
				Debug_printf("Value out of range\r\n");
			}
			
			threshold_wakeups = value;
			Debug_printf("Check thresholds every %d wakeups\r\n", threshold_wakeups);
			return;
			
		}
		if(!strcmp(argv[0], "enable"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//enable upper
				upper_temperature_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//enable lower
				lower_temperature_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			//options here are upper or lower
			if(!strcmp(argv[1], "h_upper"))
			{
				//enable upper
				upper_humidity_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//enable lower
				lower_humidity_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
		}
		if(!strcmp(argv[0], "disable"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//disable upper
				upper_temperature_threshold_enabled = false;
				Debug_printf("Upper threshold Disabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//disable lower
				lower_temperature_threshold_enabled = false;
				Debug_printf("Lower threshold Disabled\r\n");
				return;
			}
		}
	}
	
	if(argc == 3)
	{
		value = atof(argv[2]);
		
		if(!strcmp(argv[0], "set"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//set upper
				upper_temperature_threshold = (int16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", upper_temperature_threshold/100, upper_temperature_threshold%100);
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//set lower
				lower_temperature_threshold = (int16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", lower_temperature_threshold/100, lower_temperature_threshold%100);
				return;
			}
			//options here are upper or lower
			if(!strcmp(argv[1], "h_upper"))
			{
				//set upper
				upper_humidity_threshold = (uint16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", upper_humidity_threshold/100, upper_humidity_threshold%100);
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//set lower
				lower_humidity_threshold = (uint16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", lower_humidity_threshold/100, lower_humidity_threshold%100);
				return;
			}
		}
	}
	
	sht30_cli_threshold_help();
}



