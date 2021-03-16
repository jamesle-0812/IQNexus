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


#include "2I2O.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#include "debug_uart.h"
#include "global.h"
#include "delays.h"
#include "radio_common.h"

#include "watchdog.h"

#include "LoRaMac.h"
#include "lora.h"

#define OUTPUT1 1
#define OUTPUT1_PORT MODBUS_TX_PORT
#define OUTPUT1_PIN  MODBUS_TX_PIN

#define OUTPUT2 2
#define OUTPUT2_PORT MODBUS_RX_PORT
#define OUTPUT2_PIN  MODBUS_RX_PIN

#define INPUT1_PORT  COUNT1_PORT
#define INPUT1_PIN   COUNT1_PIN

#define INPUT2_PORT  COUNT2_PORT
#define INPUT2_PIN   COUNT2_PIN

typedef enum
{
	output_state_hiz_pullup = 0,
	output_state_hiz        = 1,
	output_state_low        = 2,
	output_state_high       = 3,
}output_state;

char *output_state_names[] =
{
	"HIZ_PULLUP",
	"HIZ",
	"LOW",
	"HIGH",
};

static output_state output1_state = output_state_hiz;
static output_state output2_state = output_state_hiz;




void set_output(char output, output_state state);

/********************************************************************
 *Functions                                                         *
 *******************************************************************/
void twoIO_init()
{
	//count 1 and count2 are input 1 and input 2.
	//micro_usart_tx is output 1
	//micro_usart_rx is output 2
	GPIO_InitTypeDef GPIO_InitStruct;

	// ADC-input Configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	
	HW_GPIO_Init(INPUT1_PORT , INPUT1_PIN , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT3_PORT , COUNT3_PIN , &GPIO_InitStruct);
}

void set_output(char output, output_state state)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;   
	GPIO_TypeDef* PORT;
	uint16_t PIN;
	
	if(output == OUTPUT1)
	{
		PORT = OUTPUT1_PORT;
		PIN  = OUTPUT1_PIN;
		output1_state = state;
	}
	
	if(output == OUTPUT2)
	{
		PORT = OUTPUT2_PORT;
		PIN  = OUTPUT2_PIN;
		output2_state = state;
	}
	
	Debug_printf("State = %s\r\n", output_state_names[state]);

	switch (state)
	{
		case output_state_hiz_pullup:
			GPIO_InitStruct.Pull = GPIO_PULLUP;
			Debug_printf("\tPullup\r\n");
			//intentional fallthrough
		case output_state_hiz:
			GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
			HW_GPIO_Init(PORT,PIN,&GPIO_InitStruct);
			Debug_printf("\tHIZ\r\n");
			break;
		case output_state_high:
			HW_GPIO_Init(PORT,PIN,&GPIO_InitStruct);
			LL_GPIO_SetOutputPin(PORT,PIN);
			Debug_printf("\tHIGH\r\n");
			break;
		case output_state_low:
			HW_GPIO_Init(PORT,PIN,&GPIO_InitStruct);
			LL_GPIO_ResetOutputPin(PORT,PIN);
			Debug_printf("\tLOW\r\n");
			break;
	}

}
 
//uplink the current state of the inputs and outputs.
void twoIO_uplink_state()
{
	lora_twoIO_t payload = {0};
	
	payload.members.output1_state  = output1_state;
	payload.members.output2_state  = output2_state;
	payload.members.input1_reading = HW_AdcReadChannel(COUNT1_ADC_CH);
	payload.members.input2_reading = HW_AdcReadChannel(STM_ADC_IN_CH);
	
	//print out the data to CLI
	Debug_printf("Output 1 State  : %s\r\n", output_state_names[payload.members.output1_state]);
	Debug_printf("Output 2 State  : %s\r\n", output_state_names[payload.members.output2_state]);
	Debug_printf("Input  1 Reading: %d\r\n", payload.members.input1_reading);
	Debug_printf("Input  2 Reading: %d\r\n", payload.members.input2_reading);
	
	payload.members.pkt_type    = packet_type_data;
	payload.members.sys_voltage = fourBit_battery_calculation();
	
	Uplink(payload.payload, TWO_IO_SIZE);
}

void twoIO_cli_threshold(int argc, char *argv[])
{
	//use thershold as a surrogate for device specific cli
	//connamds will be 
	// X high
	// X low
	// X hiz
	// x pullup
	
	if(argc ==2)
	{
		
		if(!strcmp(argv[1],"high"))
		{
			set_output(atoi(argv[0]), output_state_high);
		}
		
		if(!strcmp(argv[1],"low"))
		{
			set_output(atoi(argv[0]), output_state_low);
		}
		
		if(!strcmp(argv[1],"hiz"))
		{
			set_output(atoi(argv[0]), output_state_hiz);
		}
		
		if(!strcmp(argv[1],"pullup"))
		{
			set_output(atoi(argv[0]), output_state_hiz_pullup);
		}
	}
	
}
 
 
void twoIO_onDownlink(uint8_t *buffer, uint8_t size)
{
	twoIO_downlinks_t downlink = {0};
	int i;
	
	Debug_printf("2IO Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[TWO_IO_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.newStates.downlink_type == downlink_type_twoIO)
	{
		if(downlink.newStates.update1)
		{
			set_output(OUTPUT1, (output_state)downlink.newStates.state1);
			Debug_printf("Setting output1 state to %s\r\n", output_state_names[downlink.newStates.state1]);
		}
		
		if(downlink.newStates.update2)
		{
			set_output(OUTPUT2, (output_state)downlink.newStates.state2);
			Debug_printf("Setting output2 state to %s\r\n", output_state_names[downlink.newStates.state2]);
		}
	}
	
}


