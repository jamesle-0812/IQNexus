/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: USART Register Level driver for LoRa only implementation project

	Maintainer: Shea Gosnell


*/


#include "modbus_uart.h"
#include "_modbus_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "tiny_vsnprintf.h"
#include "debug_uart.h"
#include "watchdog.h"
#include "delays.h"
#include "global.h"
#include "counter.h"
#include "adc.h"

#define MODBUS_RETRY_MAX 5

#ifndef DISABLE_MODBUS_DEBUG
	#define DBG_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define DBG_printf(...)
#endif

typedef enum
{
	modbus_function_read_coil        = 1,
	modbus_function_read_input       = 2,
	modbus_function_read_holding_reg = 3,
	modbus_function_read_input_reg   = 4,
	modbus_function_force_coil       = 5,
	modbus_function_write_reg        = 6,
	modbus_function_force_coils      = 15,
	modbus_function_write_regs       = 16,
}modbus_function_e;


volatile static LPUART_TDR_t *REG_Modbus_TDR = LPUART1_TDR_ADDR;
volatile static LPUART_RDR_t *REG_Modbus_RDR = LPUART1_RDR_ADDR;
volatile static LPUART_BRR_t *REG_Modbus_BRR = LPUART1_BRR_ADDR;
volatile static LPUART_CR1_t *REG_Modbus_CR1 = LPUART1_CR1_ADDR;
volatile static LPUART_ISR_t *REG_Modbus_ISR = LPUART1_ISR_ADDR;


volatile static LPUART_CLOCK_t *REG_Modbus_CLKSEL = LPUART1_CLOCK_SELECTION_ADDR;
volatile static RCC_APB1ENR_t *REG_Modbus_CLKEN = LPUART1_CLKEN_ADDR;


//needed to reflect the input and output of the CRC calculation
uint8_t reflect_byte(uint8_t input)
{
	//swap upper and lower nybbles
	input = (input & 0xF0) >> 4 | (input & 0x0F) << 4;
	//swap adjacent pairs
	input = (input & 0xCC) >> 2 | (input & 0x33) << 2;
	//swap adjacent bits
	input = (input & 0xAA) >> 1 | (input & 0x55) << 1;
	return input;
}

#define CRC_POLYNOMIAL 0x8005 //P(x)=x^16+x^15+x^2+1 = 1000 0000 0000 0101
uint16_t modbus_calculateCrc(uint8_t* data, int data_length)
{
	uint16_t crc = 65535;
	uint16_t byte_counter;
	uint8_t bit_counter;
	
	
	for (byte_counter = 0; byte_counter < data_length; byte_counter++)
	{ 
		//turns out that the MODBUS CRC implementation requres that the input bytes be reflected
		crc ^= reflect_byte(data[byte_counter])<<8;
		for (bit_counter = 0; bit_counter < 8; bit_counter++)
		{ 
			if (crc & 0x8000)
			{
				crc = (crc << 1) ^ CRC_POLYNOMIAL;
			}
			else
			{
				crc = (crc << 1);
			}
		}
	}
	//reuse byte counter here, to reflect the result, as the MODBUS CRC calculation requires that the
	//crc be reflected
	byte_counter = reflect_byte(crc&0xFF)<<8;
	byte_counter += reflect_byte(crc>>8);
	
	return byte_counter;
}

void modbus_disable_tx_pin()
{
		//Pins (A9 as TX)
	GPIO_InitTypeDef GPIO_InitStruct;
		/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	
	HW_GPIO_Init(MODBUS_TX_PORT, MODBUS_TX_PIN, &GPIO_InitStruct);
}

void modbus_disable_rx_pin()
{
		//Pins (A9 as TX)
	GPIO_InitTypeDef GPIO_InitStruct;
		/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	
	HW_GPIO_Init(MODBUS_RX_PORT, MODBUS_RX_PIN, &GPIO_InitStruct);
}

void modbus_enable_tx_pin()
{
		//Pins (A9 as TX)
	GPIO_InitTypeDef GPIO_InitStruct;
		/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	
	HW_GPIO_Init(MODBUS_TX_PORT, MODBUS_TX_PIN, &GPIO_InitStruct);
}

void modbus_enable_rx_pin()
{
		//Pins (A9 as TX)
	GPIO_InitTypeDef GPIO_InitStruct;
		/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;
	
	HW_GPIO_Init(MODBUS_RX_PORT, MODBUS_RX_PIN, &GPIO_InitStruct);
}

void modbus_enable_pins()
{
	modbus_enable_tx_pin();
	modbus_enable_rx_pin();
}

void modbus_disable_pins()
{		
	modbus_disable_tx_pin();
	modbus_disable_rx_pin();
}

void modbus_init()
{
		//USART disable
		REG_Modbus_CR1->UE = 0;
	
	
		//set up the registers for USART
	
		//Enable the peripheral clock of USART1
		//HSI16 clock
		REG_Modbus_CLKSEL->LPUSART1_SEL = 2;
	
		//Enable the clock
		REG_Modbus_CLKEN->LPUART1EN = 1;
	


		/* USART TX GPIO pin configuration  */
		modbus_enable_pins();
	
		//M-bits
			//Leave at default for 8N1 operation
		//Baud
		
		REG_Modbus_BRR->BRR = 426667;
		
		//Stop Bits
			//Leave at default for 8N1
		//Transmit Enable
		REG_Modbus_CR1->TE = 1;
		//Receive enable
		REG_Modbus_CR1->RE = 1;
	
	
	
		//USART enable
		REG_Modbus_CR1->UE = 1;
		
		//initialise the direction pins
			//Set up the ModBus pins
		GPIO_InitTypeDef GPIO_InitStruct;
		GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = 0; //no alternate function

		HW_GPIO_Init(GPIOB, GPIO_PIN_2, &GPIO_InitStruct);
		HW_GPIO_Init(GPIOB, GPIO_PIN_7, &GPIO_InitStruct);
		
		//set output state of MODBUS pins to Low power mode
		LL_GPIO_SetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
		LL_GPIO_ResetOutputPin(MODBUS_M2_PORT,MODBUS_M2_PIN);
		
		//the modes are 
		//TX: B7->1, B2->1
		//RX: B7->0, B2->0
		//LP:	B7->1, B2->0
		
		//turn off peripheral supply
		LL_GPIO_ResetOutputPin(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN);
		
		//initialise the ADC
		init_adc();
}


void modbus_tamper_init()
{
	modbus_init();
	init_tamper();
}

uint8_t modbus_getChar(uint8_t *storage_loc)
{
	if(REG_Modbus_ISR->RXNE)
	{
		*storage_loc = REG_Modbus_RDR->RDR;
		return 1;
	}
	
	return 0;
}

void modbus_sendChar(uint8_t toSend)
{
		//wait for previous character to leave buffer
		while(!REG_Modbus_ISR->TXE);
		//send the byte
		REG_Modbus_TDR->TDR = toSend;
}


int modbus_write_registers(modbus_register_t modbus_register, uint16_t *transmit_buffer)
{
	static TimerEvent_t read_timeout_timer;
	uint8_t data_bytes_count = (uint8_t)((2*modbus_register.register_count)&0xFF);
	uint8_t txData[9+data_bytes_count];
	//to write a register, use function 16
	uint16_t crc;
	uint8_t rx_buffer[8];
	uint16_t rx_buffer_length = 8;
	int i;
	
	
	//ensure that the function is a write function
	if(modbus_register.function_code != 16)
	{
		return modbus_error_no_support;
	}
	
	//device address
	txData[0] = modbus_register.slaveID;
	//function
	txData[1] = modbus_register.function_code;
	//start address
	txData[2]=(uint8_t)(modbus_register.start_Register>>8);
	txData[3]=(uint8_t)(modbus_register.start_Register&0xFF);
	//number of registers to write to
	txData[4]=(uint8_t)(modbus_register.register_count>>8);
	txData[5]=(uint8_t)(modbus_register.register_count&0xFF);
	//number of data bytes
	txData[6]=data_bytes_count;
	
	//values
	//note that because we are writing two bytes per count, we need to use 
	//half of the data_byte_count. Conveniantly, that would be the register count.
	for(i=0;i<modbus_register.register_count;i++)
	{
		txData[2*i+7] = (uint8_t)(transmit_buffer[i] >>8);
		txData[2*i+8] = (uint8_t)(transmit_buffer[i] & 0xFF);
	}
	
	//CRC
	crc = modbus_calculateCrc(txData,7+data_bytes_count);
	txData[7+data_bytes_count]=(uint8_t)(crc&0xFF);
	txData[8+data_bytes_count]=(uint8_t)(crc>>8);
	
		
	//set the MODBUS to transmit mode
	LL_GPIO_SetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	LL_GPIO_SetOutputPin(MODBUS_M2_PORT,MODBUS_M2_PIN);
	//wait for the idle period (4ms)
	delay_timeout_ms(4);
	
	DBG_printf("MODBUS_TX:");
	
	for(i=0;i<9+data_bytes_count;i++)
	{
		modbus_sendChar(txData[i]);
		DBG_printf(" %02X", txData[i]);
	}
	
	DBG_printf("\r\n");
	
	//required delay for idle period of modbus specification
	delay_timeout_ms(4);
	
	//switch to receive mode
	LL_GPIO_ResetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	LL_GPIO_ResetOutputPin(MODBUS_M2_PORT,MODBUS_M2_PIN);
	
	
	//the response from this will be [address]                     1 byte 
	                               //[function code]               1 byte
	                               //[address of first register]   2 bytes
	                               //[number of registers written] 2 bytes
	                               //[CRC]                         2 bytes
	//100ms time-out
	start_timeout_timer(&read_timeout_timer ,1000);
	i=0;
	
	DBG_printf("MODBUS_RX:");
	while((!timer_expired(&read_timeout_timer)) && (i < rx_buffer_length))
	{
		//reset the watchdog to prevent a reset while waiting on the timeout
		reset_watchdog();
		if(modbus_getChar(&rx_buffer[i]))
		{
			//first character should be the SlaveID
			if(i == 0)
			{
				if(rx_buffer[i]!= modbus_register.slaveID)
				{
					continue;
				}
			}
			//if we get a character, set the timeout to 100ms
			start_timeout_timer(&read_timeout_timer ,100);
			DBG_printf(" %02X",rx_buffer[i]);
			i++;
		}
	}
	
	stop_timeout_timer(&read_timeout_timer);
	DBG_printf("\r\n");

	//set the modbus into LP mode
	LL_GPIO_SetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	
	
	//now check the CRC of the message, last 2 bytes are the crc, so don't include them
	//in the calculation
	crc = modbus_calculateCrc(rx_buffer, i-2);
	//if the crc does not match, return an error
	DBG_printf("CRC:\r\n");
	DBG_printf("Received:0x%02X%02X\r\n", rx_buffer[i-2], rx_buffer[i-1]);
	DBG_printf("Expected:0x%02X%02X\r\n", crc&0xFF, crc>>8);
	if((uint8_t)(crc & 0xFF) != rx_buffer[i-2])
	{
		if((uint8_t)(crc>>8) != rx_buffer[i-1])
		{
			Debug_printf("CRC ERR\r\n");
			return modbus_error_crc;
		}
	}
	
	//check that the number of transmitted registers matches the number written
	if((rx_buffer[4]<<8)+rx_buffer[5] != modbus_register.register_count)
	{
		Debug_printf("Count Error\r\n");
		return modbus_error_count;
	}
	
	//return the number of written bytes, according to the arguments
	//could use rx_buffer[5] to get result according to the remote device, if that is more appropriate
	return modbus_register.register_count;
}

int modbus_read_registers(modbus_register_t modbus_register, uint16_t *receive_buffer)
{
	static TimerEvent_t read_timeout_timer;
	uint8_t txData[8];
	uint8_t rx_buffer[260];
	uint16_t rx_buffer_length = 260;
	//to read a register, transmit a request frame, and read the response
	//to read a holding register, use function 3
	uint16_t crc;
	int i;
	
	if(modbus_register.function_code !=3 && modbus_register.function_code != 4)
	{
		return modbus_error_no_support;
	}
	
	//construct the array to transmit
	txData[0]=modbus_register.slaveID;
	txData[1]=modbus_register.function_code;
	txData[2]=(uint8_t)(modbus_register.start_Register>>8);
	txData[3]=(uint8_t)(modbus_register.start_Register&0xFF);
	txData[4]=(uint8_t)(modbus_register.register_count>>8);
	txData[5]=(uint8_t)(modbus_register.register_count&0xFF);
	//6 and 7 have the CRC
	crc = modbus_calculateCrc(txData,6);
	txData[6]=(uint8_t)(crc&0xFF);
	txData[7]=(uint8_t)(crc>>8);

	
		//set the MODBUS to transmit mode
	LL_GPIO_SetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	LL_GPIO_SetOutputPin(MODBUS_M2_PORT,MODBUS_M2_PIN);
	//wait for the idle period (4ms)
	delay_timeout_ms(4);
	
	DBG_printf("MODBUS_TX:");
	
	for(i=0;i<8;i++)
	{
		modbus_sendChar(txData[i]);
		DBG_printf(" %02X",txData[i]);
	}
	
	DBG_printf("\r\n");
	
	//required delay for idle period of modbus specification
	delay_timeout_ms(4);
	
	//switch to receive mode
	LL_GPIO_ResetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	LL_GPIO_ResetOutputPin(MODBUS_M2_PORT,MODBUS_M2_PIN);
	
	//100ms time-out, larger initial delay to allow a unit to process if necessary.
	start_timeout_timer(&read_timeout_timer ,1000);
	i=0;
	DBG_printf("MODBUS_RX:");
	while((!timer_expired(&read_timeout_timer)) && (i < (2*modbus_register.register_count) + 5))
	{
		//reset the watchdog to prevent a reset while waiting on the timeout
		reset_watchdog();
		
		if(modbus_getChar(&rx_buffer[i]))
		{
			//first character should be the SlaveID
			if(i == 0)
			{
				if(rx_buffer[i]!= modbus_register.slaveID)
				{
					continue;
				}
			}
			//if we get a character, set the timeout to 100ms
			start_timeout_timer(&read_timeout_timer ,100);
			DBG_printf(" %02X",rx_buffer[i]);
			i++;
		}
	}
	
	stop_timeout_timer(&read_timeout_timer);
	
	DBG_printf("\r\n");

	//set the modbus into LP mode
	LL_GPIO_SetOutputPin(MODBUS_M1_PORT,MODBUS_M1_PIN);
	
	//now check the CRC of the message, last 2 bytes are the crc, so don't include them
	//in the calculation
	crc = modbus_calculateCrc(rx_buffer, i-2);
	//if the crc does not match, return an error
	DBG_printf("CRC:\r\n");
	DBG_printf("Received:0x%02X%02X\r\n", rx_buffer[i-2], rx_buffer[i-1]);
	DBG_printf("Expected:0x%02X%02X\r\n", crc&0xFF, crc>>8);
	if((uint8_t)(crc & 0xFF) != rx_buffer[i-2])
	{
		if((uint8_t)(crc>>8) != rx_buffer[i-1])
		{
			Debug_printf("CRC ERR\r\n");
			return modbus_error_crc;
		}
	}
	
	//the count from the rx frame should be twice the number of registers requested in tx
	//because each register is two bytes wide.
	if(modbus_register.register_count*2 != rx_buffer[2])
	{
		Debug_printf("Count ERR\r\n");
		return modbus_error_count;
	}
	
	//otherwise, extract the data, and place it into the return array
	// Divide by 2, because we are reading two values with each iteration
	// -5 because there are 5 bytes we don't care about (addr,function,count,CRC,CRC).
	rx_buffer_length = (i-5)/2;
	for(i=0;i<rx_buffer_length; i++)
	{
		//high byte, offset by 3 to account for the address, function and count
		//note that the data is big-endian, MSB first, and therefore the MSB is in the lower position in the array
		receive_buffer[i] = rx_buffer[2*i+3]<<8;
		//low byte
		receive_buffer[i] +=rx_buffer[2*i+4];
	}
	
	return rx_buffer_length;
}

//Note the void pointers for the read and write buffers, these are necessary because the coil/inputs are 8-bit orineted, while the registes are 16-bit oritented
modbus_transaction_result_t modbus_transaction(modbus_register_t modbus_register, uint16_t *read_data, uint16_t *write_data, uint16_t read_limit, uint16_t write_limit)
{
	modbus_transaction_result_t result = {0};

	int retry = MODBUS_RETRY_MAX;
	
	while((result.write <=0) && (result.read <= 0) && retry)
	{
		retry--;

		//read_data and write_data are both uint8_t because of the read/write coil instructions
		//worst case we end up with some uint16_t that have their bytes swapped.
		//if nessessary, we can add a wrapper that swaps the bytes when necessary.
		
		//for coil/input instructions, we might want to pad the result out with a null byte, to make it
		//a uint16 compatible result. This would then play nicely with the rest of the code.
		
		//for write instructions, ensure that write_data is not null.
		//for read instructions, ensure that read_data is not null.
		
		//For now we only support functions 3 and 4, which are both read registers. (3-> holding, 4->input)
		//Added support for function 16 (write multiple registers)
		//we still need to add support for funcitons 1,2,5,6,15
		switch(modbus_register.function_code)
		{
			//read coil status
			case modbus_function_read_coil:
			//read input status
			case modbus_function_read_input:
				Debug_printf("Function %d not supported\r\n", modbus_register.function_code);
				return result;
			//Read Holding Register
			case modbus_function_read_holding_reg:
				//intentional fallthrough, read holding and read input differ only by funciton code
			//read input registers
			case modbus_function_read_input_reg:
				if(read_data != 0)
				{
					//somewhat protect against overflows
					if(modbus_register.register_count > read_limit)
					{
						modbus_register.register_count = read_limit;
					}
					
					if(read_limit > 0)
					{
						result.read = modbus_read_registers(modbus_register, (uint16_t*)read_data);
					}
				}
				break;
			case modbus_function_force_coil:
			case modbus_function_write_reg:
			case modbus_function_force_coils:
				Debug_printf("Function %d not supported\r\n", modbus_register.function_code);
				return result;
			case modbus_function_write_regs:
				if(write_data != 0)
				{
					//somewhat protect against overflows
					if(modbus_register.register_count > write_limit)
					{
						modbus_register.register_count = write_limit;
					}
					
					if(write_limit > 0)
					{
						result.write = modbus_write_registers(modbus_register, (uint16_t*)write_data);
					}
				}
				break;
			default:
				Debug_printf("Function %d not supported\r\n", modbus_register.function_code);
				return result;
		}
	}
	
	return result;
}

