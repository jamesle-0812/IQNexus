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

#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "modbus_uart.h"
#include "lora_sensum.h"
#include "delays.h"
#include "watchdog.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "uart_soil_probe.h"
#include "global.h"
#include "radio_common.h"



 


void probe_bleed_on()
{
			//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	/* Probe Bleed Pin configuration  */
	//open drain,  to ensure that we do not try to power the probe using a GPIO
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	

	//enable the bleed pin, this pin has a 330+ Ohm resistor on it, to prevent overcurrent on the GPIO.
	HW_GPIO_Init(PROBE_BLEED_PORT, PROBE_BLEED_PIN, &GPIO_InitStruct);
	//bleed the charge off the probe, by tying the pin to ground
	LL_GPIO_ResetOutputPin(PROBE_BLEED_PORT,PROBE_BLEED_PIN);
}

void probe_bleed_off()
{
	//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	//ensure that the bleed is disabled, to prevent wasting power
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	HW_GPIO_Init(PROBE_BLEED_PORT, PROBE_BLEED_PIN, &GPIO_InitStruct);
}

void probe_power_on()
{
		//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	//configuration to enable the power pin
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;

	//enable the power pin
	HW_GPIO_Init(PROBE_POWER_PORT, PROBE_POWER_PIN, &GPIO_InitStruct);
	
	//set pin high to power on the probe
	LL_GPIO_SetOutputPin(PROBE_POWER_PORT,PROBE_POWER_PIN);
}

void probe_power_off()
{
	//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	//configuration to enable the power pin
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	//power off the probe by clearing the pin
	LL_GPIO_ResetOutputPin(PROBE_POWER_PORT,PROBE_POWER_PIN);
	//disable the power pin
	HW_GPIO_Init(PROBE_POWER_PORT, PROBE_POWER_PIN, &GPIO_InitStruct);
}

int8_t probe_temperature_process(uint8_t MSB, uint8_t LSB)
{
	int16_t temp;
	float flt;
	
	temp = MSB;
	temp = temp << 8;
	temp += LSB;
	//divide by 256 to get the temperature.
	//note that this is a float conversion, and not equivilant to a >>8.
	flt = ((float)temp)/256.0;
	return (int8_t)flt; 
}

uint8_t probe_humidity_process(uint8_t MSB, uint8_t LSB)
{
	uint16_t temp = 0;
	//construct the 16-bit value
	temp = MSB;
	temp = temp << 8;
	temp += LSB;
	//The result is on a scale of 0-10000 (0-100.00%).
	//Divide by 50 to get 0.5% steps (0-200 -> 0-100%)
	//Remember to divide by 2 at the other end to get back to %Humidity
	temp = temp / 50;
	
	return (uint8_t)temp;
}

lora_probe_error_t probe_form_error_packet(uint8_t error)
{
	lora_probe_error_t payload = {0};
	
	if(error == probe_fail_crc)
	{
		payload.members.crc_error = 1;
	}
	
	if(error == probe_fail_noprobe)
	{
		payload.members.no_probe = 1;
	}
	
	payload.members.pkt_type = packet_type_error;
	payload.members.sys_voltage = fourBit_battery_calculation();
	
	return payload;
}


lora_probe_temperature_payload_t probe_form_temperature_packet(uint8_t data[static 32])
{
	lora_probe_temperature_payload_t payload = {0};
	//now form the packet
	
	//Temperature 1
	payload.members.T1 = probe_temperature_process(data[14], data[15]);
	
	//Temperature 2
	payload.members.T2 = probe_temperature_process(data[16], data[17]);
	
	//Temperature 3
	payload.members.T3 = probe_temperature_process(data[18], data[19]);
	
	
	//Temperature 4
	payload.members.T4 = probe_temperature_process(data[20], data[21]);
	
	
	//Temperature 5
	payload.members.T5 = probe_temperature_process(data[22], data[23]);
	
	//Temperature 6
	payload.members.T6 = probe_temperature_process(data[24], data[25]);
	
	//Temperature Ambient
	payload.members.Tambient = probe_temperature_process(data[26], data[27]);
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = probe_packet_type_temperature;
	
	return payload;
}

lora_probe_humidity_payload_t probe_form_humidity_packet(uint8_t data[static 32])
{
	lora_probe_humidity_payload_t payload = {0};

	//humidity 1
	payload.members.H1 = probe_humidity_process(data[2], data[3]);
	
	//humidity 2
	payload.members.H2 = probe_humidity_process(data[4], data[5]);
	
	//humidity 3
	payload.members.H3 = probe_humidity_process(data[6], data[7]);
	
	//humidity 4
	payload.members.H4 = probe_humidity_process(data[8], data[9]);
	
	//humidity 5
	payload.members.H5 = probe_humidity_process(data[10], data[11]);
	
	//humidity 6
	payload.members.H6 = probe_humidity_process(data[12], data[13]);
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = probe_packet_type_humidity;
	
	return payload;
}

int probe_getData(uint8_t data[static 32])
{
	uint16_t crc = 0;
	uint16_t calc_crc = 0;
	uint8_t input_char = 0;
	int8_t position = -2;
	uint8_t attempts = 0;
	
	static TimerEvent_t ProbeTimer;
	
	probe_bleed_off();
	
	//this will be the return value.
	//it is set if we complete the packet before the timeout
	uint8_t success = probe_fail_noprobe;
	
	Debug_printf("Probe On\r\n");

	await_uart_tx();
	
	probe_power_on();
	
	//reset the UART here, to fix some timing issues.
	//Basically, if this is not done, then the UART will not receive characters from the Probe.
	//I believe that this is to do with the line effectivly being ilde-low while the probe is powered off,
	//while the UART expects the line to idle high.
		
	while(success != probe_success_ok && attempts < 3)
	{
		modbus_init();
		probe_power_on();
		//disable the power pin, to prevent power leak into the probe
		modbus_disable_tx_pin();
		
		//from here, it should take 13.6 seconds to get the data.
		//use a timeout of 20 seconds to be safe
			//timeout before re-activation
		start_timeout_timer(&ProbeTimer, 20000);
		
		Debug_printf("Probe:");
		while(!timer_expired(&ProbeTimer))
		{
			//prevent the watchdog from timing out while waiting for the probe.
			reset_watchdog();
			
			//try to get a character from the omdbus-uart peripheral.
			if(modbus_getChar(&input_char))
			{
				Debug_printf(" %02X", input_char);
				//first non-null character should be a '*'
				if(position == -2)
				{
					//looking for the '*'
					if(input_char == '*')
					{
						position ++;
						continue;
					}
				}
				
				if(position == -1 || position == -2)
				{
					//should receive 0x5E
					if(input_char == 0x5E)
					{
						position = 0;
						continue;
					}
				}
				
				if(position >= 0 && position < 32)
				{
					//data
					data[position] = input_char;
					position++;
					continue;
				}
			
				if(position == 32)
				{
					//CRC LSB
					crc = input_char;
					position++;
					continue;
				}
				
				if(position == 33)
				{
					//CRC MSB
					crc += input_char << 8;
					position++;
					continue;
				}
				
				if(position == 34)
				{
					//end byte, should receive 0x5F
					if(input_char == 0x5F)
					{
						success = probe_success_ok;
						break;
					}
				}
			}
		}
		stop_timeout_timer(&ProbeTimer);
		Debug_printf("\r\n");
		await_uart_tx();


		
		if(success == probe_success_ok)
		{
			//check the CRC
			Debug_printf("Received CRC  : %04X\r\n", crc);
			calc_crc = modbus_calculateCrc(data, 32);
			await_uart_tx();
			Debug_printf("Calculated CRC: %04X\r\n", calc_crc);
			
			if(crc != calc_crc)
			{
				Debug_printf("CRC Mismatch!\r\n");
				success = probe_fail_crc;
			}
		}
		attempts ++;
		
		//no-probe, ask for a new reading
		if(success == probe_fail_noprobe)
		{	
			
			modbus_enable_tx_pin();
			//send "x " to wake the probe up
			modbus_sendChar('x');
			modbus_sendChar(' ');
			//request the measurement be taken again
			modbus_sendChar('^');
			modbus_sendChar('M');
			//allow time for the TX buffer to empty
			delay_timeout_ms(5);
		}
		
		//incorrect crc, ask for the same data agian
		if(success == probe_fail_crc)
		{	
			modbus_enable_tx_pin();
			//send "x " to wake the probe up
			modbus_sendChar('x');
			modbus_sendChar(' ');
			//ask for the data to be sent again
			modbus_sendChar('^');
			modbus_sendChar('D');
			//allow time for the TX buffer to empty
			delay_timeout_ms(5);
		}

	}
	
	//ensure that the TX pin is disabled
	modbus_disable_tx_pin();
	
	Debug_printf("Probe Off\r\n");
	await_uart_tx();
	probe_power_off();
	
	//Turn the bleed on, to discharge the internal cap in the probe
	probe_bleed_on();
	
	return success;
}

void probe_sendData()
{
	//suppresses unused function warning
	(void)probe_sendData;
	
	uint8_t data[32] = {0};

	uint8_t success = probe_getData(data);
	
	//get the data from the probe
	if(success == probe_success_ok)
	{
		//form the data packet
		Uplink(probe_form_temperature_packet(data).payload, PROBE_TEMPERATURE_SIZE);
		Uplink(probe_form_humidity_packet(data).payload   , PROBE_HUMIDITY_SIZE);
	}
	else
	{
		//form the error packet
		Uplink(probe_form_error_packet(success).payload, PROBE_ERROR_SIZE);
	}


}

test_status_e test_probe()
{
	uint8_t data[32];
	uint8_t success = probe_getData(data);
	if(success != probe_success_ok)
	{
		//error with the probe.
		if(success == probe_fail_crc)
		{
			Debug_printf("Probe COMM Error\r\n");
			return test_fail;
		}

		if(success == probe_fail_noprobe)
		{
			Debug_printf("Probe HW Error\r\n");
			return test_fail;
		}
	}
	
	return test_pass;
}

void probe_init(void)
{
	modbus_init();
	modbus_disable_tx_pin();
	probe_power_off();
	probe_bleed_on();
}




