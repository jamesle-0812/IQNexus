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



#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "watchdog.h"
#include "global.h"
#include "delays.h"
#include "radio_common.h"
#include "packets.h"
#include "debug_uart.h"


#define VREFINT_CAL ((uint16_t *) ((uint32_t) 0x1FF80078))

uint16_t AdcReadCompensate(uint32_t Channel)
{
	uint16_t adc_data = 0;
	uint16_t vrefint_data = 0;
	uint16_t output_data = 0;
	float processor = 0;
	
	adc_data = HW_AdcReadChannel(Channel);
	
	
	
	
	// 30.*VREFINT_CAL*adc_data
	//--------------------------
	//VREF_INT_DATA * FULL_SCALE
	
	//VREFINT_CAL = (*VREFINT_CAL)
	//adc_data = reading
	//FULL_SCALE = 4095


	vrefint_data = HW_AdcReadChannel(LL_ADC_CHANNEL_VREFINT);
	
	
	//processor = 3.0;
	processor = (float)adc_data;
	processor /= (float)vrefint_data;
	processor *= (float)(*VREFINT_CAL);
	//processor /= 4095.0;
	
	output_data = (uint16_t)processor;
	
	return output_data;
}


void init_three_adc( void )
{
	//configure the pin as an analog input
	//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	// ADC-input Configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	

	//Initialise the pins
	HW_GPIO_Init(ADC1_PORT, ADC1_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(ADC2_PORT, ADC2_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(ADC3_PORT, ADC3_PIN, &GPIO_InitStruct);

}

void three_adc_send( void )
{
	lora_three_adc_payload_t packet;

	
	packet.members.reading1 = AdcReadCompensate(ADC1_CH);
	packet.members.reading2 = AdcReadCompensate(ADC2_CH);
	packet.members.reading3 = AdcReadCompensate(ADC3_CH);
	
	
	//calculate the actual voltage from this value using the following formula
	//     3.0*reading
	// ----------------------
	//   4095 * div_ratio
	//
	//Caluculate the div_ratio as
	//      R2
	//  -----------
	//    R1 + R2
	Debug_printf("Reading 1: %d \r\n", packet.members.reading1);
	Debug_printf("Reading 2: %d \r\n", packet.members.reading2);
	Debug_printf("Reading 3: %d \r\n", packet.members.reading3);
	
	

	
	packet.members.pkt_type    = packet_type_data;
	packet.members.sys_voltage = fourBit_battery_calculation();
	
	Uplink(packet.payload, THREE_ADC_SIZE);
}

void init_adc( void )
{
	//configure the pin as an analog input
	//initialise the GPIO
	GPIO_InitTypeDef GPIO_InitStruct;
	
	// ADC-input Configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	

	//Initialise the pin
	HW_GPIO_Init(ADC3_PORT, ADC3_PIN, &GPIO_InitStruct);
}

uint16_t read_adc()
{
	uint16_t reading = 0;
	
	//reset the watchdog, as this ADC reading will be polling, and there are no WDT-resets in the provided code.
	reset_watchdog();


	//It seems that this library function performs a hardware calibration which
	// (may?) correct for changes in the analog refrence input.
	// If this is true, then we do not need to manually compensate for the supply
	// voltage.
	reading = AdcReadCompensate(ADC3_CH);
	//Debug_printf("READING: %d\r\n", reading);
	
	
	return reading;
}


