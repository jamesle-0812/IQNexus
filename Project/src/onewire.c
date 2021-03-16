/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	

	Maintainer: Shea Gosnell


*/

#include "global.h"
#include "i2c1.h"
#include "sht20.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "lora_sensum.h"
#include "utilities.h"
#include "delays.h"

uint16_t      OWP_PIN  = COUNT1_PIN ;
GPIO_TypeDef* OWP_PORT = COUNT1_PORT;

void OWP_pin_input()
{
	LL_GPIO_SetPinMode(OWP_PORT, OWP_PIN, LL_GPIO_MODE_INPUT);
}

void OWP_pin_output()
{
	LL_GPIO_SetPinMode(OWP_PORT, OWP_PIN, LL_GPIO_MODE_OUTPUT);
	LL_GPIO_ResetOutputPin(OWP_PORT, OWP_PIN);
}

void OWP_init(GPIO_TypeDef* port, uint16_t pin)
{
	OWP_PIN  = pin ;
	OWP_PORT = port;
	//set COUNT1 as an IO pin, which we can use to read/write from the device
	//set COUNT1 as an IO pin, which we can use to read/write from the device
	GPIO_InitTypeDef GPIO_InitStruct;

	/* LED Initialisation  */
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	
	LL_GPIO_SetPinOutputType(OWP_PORT, OWP_PIN, LL_GPIO_OUTPUT_OPENDRAIN);

	HW_GPIO_Init(OWP_PORT, OWP_PIN, &GPIO_InitStruct);
	
	OWP_pin_input();
}

void OWP_reset_bus()
{
	//timing critical code, disable interrupts
	BACKUP_PRIMASK();
	DISABLE_IRQ( );
	/*
	Reset bus:
		Pull low               | Initiates the reset
		Wait 500uS             | Required period
		Release high           | To indicate end of reset pulse
		Wait 90uS              | To allow for slave setup
		Read line              | To detect slave
		Wait 1mS               | To allow reset to finish
	*/
	
	
	//pull low
	OWP_pin_output();
	//500us delay
	delay_us(500);
	//release line high
	OWP_pin_input();
	//90us delay
	delay_us(90);
	//here is where the read would be, we are going to ignore for now
	//delay 1ms to clear line conditions
	delay_us(1000);
	//restore interrupts
	RESTORE_PRIMASK( );
}

void OWP_write_bit(bool bit)
{
	//timing critical code, disable interrupts
	BACKUP_PRIMASK();
	DISABLE_IRQ( );
	/*
	Write 1:
		Pull low               | Initiates Write
		Wait 7uS               | Must be held for no more than 15uS
		Release high           | Indicates writing 1
		Wait 60uS              | Entire transaction is required to be >60uS
	
	Write 0:
		Pull low               | Initiates Write
		Wait 90uS              | Must be held low for TuS (60<T<120) 
		Release high           | End transaction
	*/
	
	if(bit)
	{
		OWP_pin_output();
		delay_us(7);
		OWP_pin_input();
		delay_us(60);
	}
	else
	{
		OWP_pin_output();
		delay_us(90);
		OWP_pin_input();
		delay_us(30);
	}
	
	RESTORE_PRIMASK( );
}

bool OWP_read_bit()
{
	//timing critical code, disable interrupts
	BACKUP_PRIMASK();
	DISABLE_IRQ( );
	
	bool output = 0;
	/*
	Read:
		Pull low               | Initiate Read
		Wait 5uS               | Require holdoff of > 1uS < 15uS
		Release high           | Allow slave to write to line
		Wait 5uS               | All for time for slave to write
		Read                   | Read the line state high->1, low->0.
							|  15uS from falling edge at latest
		Wait 60uS              | Required transaction longer than 60uS
	*/
	OWP_pin_output();
	delay_us(5);
	OWP_pin_input();
	delay_us(5);
	output = LL_GPIO_IsInputPinSet(OWP_PORT, OWP_PIN);
	delay_us(60);
	
	//restore interrupts
	RESTORE_PRIMASK( );
	return output;
}

void OWP_write_byte(uint8_t byte)
{
	//write a byte by writing 8 bits in succession
	int i;
	
	for(i=0;i<8;i++)
	{
		OWP_write_bit(byte & (1<<i));
	}
}

uint8_t OWP_read_byte()
{
	//read a byte by reading 8 bits in succession
	int i;
	uint8_t result = 0;
	
	for(i=0; i<8;i++)
	{
		result |= (OWP_read_bit() << i);
	}
	
	return result;
}


//this function implements function 33h, to read the ROM code from a single-drop device.
uint64_t OWP_getSingleID()
{
	uint64_t result = 0;
	int i;
	
	//send command 33
	OWP_reset_bus();
	OWP_write_byte(0x33);
	
	
	//we should get an immediate response, so no need for a timeout.
	//Also because this is a timing dependant protocol, the read cannot take indefinite time.
	for(i=0;i < sizeof(uint64_t)/sizeof(uint8_t); i++)
	{
		result |= ((uint64_t)OWP_read_byte()) << (8*i);
	}
	
	//complete transaction with meaningless write
	OWP_write_byte(0x00);
	
	Debug_printf("OWP ID: 0x%08X%08X\r\n", (uint32_t)(result>>32), (uint32_t)(result & 0xFFFFFFFF));
	
	return result;
}

