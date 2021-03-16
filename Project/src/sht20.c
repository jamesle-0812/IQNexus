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
#include "sht20.h"

#include "stm32l0xx.h"                  // Device header

#include "hw.h"

#include "debug_uart.h"

#include "lora_sensum.h"
#include "radio_common.h"


#define SHT20_ADDR 0x40
#define CRC_POLYNOMIAL 0x131 //P(x)=x^8+x^5+x^4+1 = 100110001


//calculates 8-Bit checksum with given polynomial
static uint8_t validCRC(uint8_t *data, uint8_t data_length, uint8_t checksum)
{
	uint8_t crc = 0;
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

//This function will get the temperature from the sht20, using the hold-master method
int16_t sht20GetTemperature()
{
	uint8_t data_to_send[] = {0xE3};
	uint8_t data_send_length = 1;
	uint8_t data_to_read[3] = {0};
	uint8_t data_read_length = 3;
	int16_t temp;
	float  temperature;
	
	//request temperature data from the SHT20
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot continue.
	}
	else
	{
		i2c1_send(SHT20_ADDR, data_to_send, data_send_length, 0);
		i2c1_receive(SHT20_ADDR, data_to_read, data_read_length,1);
	}
	//check the data validity using CRC
	if(!validCRC(data_to_read, 2, *(data_to_read+2)))
	{
		//CRC error
		return (int16_t)0x8000;
	}
	
	//at this point we know we have valid data
	temp = data_to_read[0];
	temp = temp<<8;
	temp = temp + data_to_read[1];
	temperature = ((float)temp)*0.2681274414 - 4685;
	
	temp = temperature;
	
	return temp;
	
}

//This function will get the temperature from the sht20, using the hold-master method
uint16_t sht20GetHumidity()
{
	uint8_t data_to_send[] = {0xE5};
	uint8_t data_send_length = 1;
	uint8_t data_to_read[3] = {0};
	uint8_t data_read_length = 3;
	uint16_t temp;
	float  humidity;
	
	//request temperature data from the SHT20
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot continue.
	}
	else
	{
		i2c1_send(SHT20_ADDR, data_to_send, data_send_length, 0);
		i2c1_receive(SHT20_ADDR, data_to_read, data_read_length,1);
	}
	//check the data validity using CRC
	if(!validCRC(data_to_read, 2, *(data_to_read+2)))
	{
		//CRC error
		return (uint16_t)0xFFFF;
	}
	
	//at this point we know we have valid data
	temp = data_to_read[0];
	temp = temp<<8;
	temp = temp + data_to_read[1];
	humidity = (float)temp;
	
	//conversion factor from datasheet
	humidity = humidity * 0.001907348633;
	humidity = humidity - 6;
	
	//multiply by 10 to catch the decimal palce
	humidity = humidity * 10;
	
	//convert back to int
	temp = humidity;
	
	return temp;
	
}

void sht20_uplink( void )
{
	sht20_payload_t payload = {0};
	
	payload.members.Temperature = sht20GetTemperature();
	payload.members.Humidity = sht20GetHumidity();
	
	Debug_printf("T:%02d.%02d C\r\n",payload.members.Temperature/100,payload.members.Temperature%100);
	Debug_printf("H:%d.%d %RH\r\n",payload.members.Humidity/10, payload.members.Humidity%10);
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	Uplink(payload.payload, SHT20_SIZE);
}
