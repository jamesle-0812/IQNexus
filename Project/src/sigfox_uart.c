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


#include "sigfox_uart.h"
#include "_sigfox_uart.h"
#include "debug_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "tiny_vsnprintf.h"
#include "watchdog.h"
#include "global.h"
#include "timeServer.h"
#include "delays.h"

volatile static USART_TDR_t *REG_Sigfox_TDR = USART5_TDR_ADDR;
volatile static USART_RDR_t *REG_Sigfox_RDR = USART5_RDR_ADDR;
volatile static USART_BRR_t *REG_Sigfox_BRR = USART5_BRR_ADDR;
volatile static USART_CR1_t *REG_Sigfox_CR1 = USART5_CR1_ADDR;
//volatile static USART_CR3_t *REG_Sigfox_CR3 = USART5_CR3_ADDR;
volatile static USART_ISR_t *REG_Sigfox_ISR = USART5_ISR_ADDR;
volatile static USART_ICR_t *REG_Sigfox_ICR = USART5_ICR_ADDR;

volatile static RCC_APB1ENR_t *REG_Sigfox_CLKEN = USART5_CLKEN_ADDR;


//RX buffer, of size MAX_COMMAND_LENGTH, will be used to read received data from CLI
volatile static char rx_buffer[RX_BUFFER_LENGTH];
volatile static int  rx_buffer_read_pos=0;
volatile static int  rx_buffer_write_pos=0;

volatile static char tx_buffer[TX_BUFFER_LENGTH];
volatile static int  tx_buffer_read_pos=0;
volatile static int  tx_buffer_write_pos=0;

void sigfox_usart_init()
{
	//USART disable
		REG_Sigfox_CR1->UE = 0;
	
		//set up the registers for USART
	
		//Enable the peripheral clock of USART5
		//HSI16 clock
		//USES PCLK
	
		//Enable the clock
		REG_Sigfox_CLKEN->USART5_EN = 1;
	
		//Pins (A9 as TX) (10 as RX)
		GPIO_InitTypeDef GPIO_InitStruct;

		/* USART TX GPIO pin configuration  */
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
		GPIO_InitStruct.Alternate = GPIO_AF6_USART5;

		HW_GPIO_Init(SF_UART_TX_PORT, SF_UART_TX_PIN, &GPIO_InitStruct);
		HW_GPIO_Init(SF_UART_RX_PORT, SF_UART_RX_PIN, &GPIO_InitStruct);
	
		//M-bits
			//Leave at default for 8N1 operation
		//Baud
		//enable over8 (instead of over16)
		REG_Sigfox_CR1->OVER8 = 1;
		REG_Sigfox_BRR->BRR   = 0x1A0;
		
		//Stop Bits
			//Leave at default for 8N1
		//Transmit Enable
		REG_Sigfox_CR1->TE = 1;
		//Receive enable
		REG_Sigfox_CR1->RE = 1;
		
		//enable receive interrupts, so we can quickly get the data
		REG_Sigfox_CR1->RXNEIE = 1;
		REG_Sigfox_CR1->TXEIE  = 0;
		
		//configure the NVIC for the uart
		//TODO: use a manual implementation instead of lib?
		NVIC_SetPriority(USART4_5_IRQn, 2);
		NVIC_EnableIRQ(USART4_5_IRQn);
		
		
		//ensure that the uart runs in stop mode
		REG_Sigfox_CR1->UESM = 1;
		//USART enable
		REG_Sigfox_CR1->UE = 1;
}

void disable_sigfox_usart()
{
	//USART disable
	REG_Sigfox_CR1->UE = 0;
	
	//Pins (A9 as TX) (10 as RX)
	GPIO_InitTypeDef GPIO_InitStruct;

	/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_USART5;

	HW_GPIO_Init(SF_UART_TX_PORT, SF_UART_TX_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(SF_UART_RX_PORT, SF_UART_RX_PIN, &GPIO_InitStruct);
	//ensure that the uart is not running in stop mode
	REG_Sigfox_CR1->UESM = 0;

	//Disable the clock
	REG_Sigfox_CLKEN->USART5_EN = 0;
}


void sigfox_send( const char *format, ... )
{	
	va_list args;
	va_start(args, format);
	uint8_t len=0;
	char tempBuff[64];
	reset_watchdog();
	/*convert into string at buff[0] of length iw*/
	len = tiny_vsnprintf_like(&tempBuff[0], sizeof(tempBuff), format, args); 
	reset_watchdog();
	sigfox_AddToWriteBuffer(tempBuff, len);
	//Leave the flag set, and carry on, writing to the buffer will clear the flag for the next transmission.

	va_end(args);
	await_sigfox_uart_tx();
}

void sigfox_AddToWriteBuffer(char* message, int length)
{ 
	await_sigfox_uart_tx();
	while(length)
	{
		reset_watchdog();
		//add the characters from the message to the buffer
		tx_buffer[tx_buffer_write_pos] = *message;
		message++;
		length --;
		tx_buffer_write_pos++;
		tx_buffer_write_pos = tx_buffer_write_pos % TX_BUFFER_LENGTH;
	}
	sigfox_usart_init();
	//enable the tx interrupt
	REG_Sigfox_CR1->TXEIE = 1;
	

}

void await_sigfox_uart_tx()
{
		//wait for the tx buffer to be empty
		while(sigfox_isCharToSend())
		{
				reset_watchdog();
		}
		reset_watchdog();
}

int sigfox_isCharToSend()
{
	return REG_Sigfox_CR1->TXEIE;
}
int Sigfox_getRxDataLength()
{
	//we need some logic here for the circular buffer
	if(rx_buffer_read_pos == rx_buffer_write_pos)
		return 0;
	
	//the write pointer is after the read pointer, we do not need to account
	//for resetting to the beginning of the array
	if(rx_buffer_write_pos > rx_buffer_read_pos)
	{
		return rx_buffer_write_pos - rx_buffer_read_pos;
	}
	
	//otherwise we need the sum of from read->end and from start->write
	return RX_BUFFER_LENGTH - rx_buffer_read_pos + rx_buffer_write_pos;
}

char Sigfox_getChar( void )
{
	char c = 0;
	//clear the new data from the rx buffer
	if(rx_buffer_read_pos != rx_buffer_write_pos)
	{
		c = rx_buffer[rx_buffer_read_pos];
		
		rx_buffer_read_pos++;
		rx_buffer_read_pos = rx_buffer_read_pos % RX_BUFFER_LENGTH;
	}
	return c;
	//Sigfox_printf("\tRead_pos : %d\r\n\tWrite_pos: %d\r\n", rx_buffer_read_pos, rx_buffer_write_pos);
}

static TimerEvent_t read_timeout;
static uint8_t read_timeout_flag = 0;
void read_timeout_flag_set()
{
	Debug_printf("\r\nSigfox Read Timeout\r\n");
	read_timeout_flag = 1;
}

int sigfox_readLine(char* input, int max_length, int timeout_ms)
{
	char c = 0;
	int count = 0;

	read_timeout_flag = 0;
	
	TimerInit(&read_timeout, &read_timeout_flag_set);
	//100ms to read from sf before timeout
	TimerSetValue(&read_timeout, timeout_ms);
	TimerStart(&read_timeout);
	
	c = Sigfox_getChar();
	//todo: add timeout around this
	while(c!='\n'&& (max_length - 1) > count && (read_timeout_flag == 0))
	{
		if(c!=0 && c != '\r')
		{
			input[count] = c;
			count++;
			TimerStop(&read_timeout);
			TimerStart(&read_timeout);
		}
		reset_watchdog();
		c = Sigfox_getChar();
	}
	reset_watchdog();
	TimerStop(&read_timeout);
	//add a null terminator
	input[count] = 0;
	
	delay_timeout_ms(100);
	
	return count;
}


//Interrupt handler for the Sigfox_uart
void USART4_5_IRQHandler( void )
{
	//if the TX register is empty, we can load a new character into the buffer
	if(REG_Sigfox_ISR->TXE)
	{
		//if we are not at the end of the buffer, write the next character to the tx register
		if(tx_buffer_read_pos != tx_buffer_write_pos)
		{
			REG_Sigfox_TDR->TDR = tx_buffer[tx_buffer_read_pos];
			tx_buffer_read_pos++;
			tx_buffer_read_pos = tx_buffer_read_pos % TX_BUFFER_LENGTH;
		}
		//if we have reached the end of the buffer, we can stop triggering interrupts
		else
		{
			REG_Sigfox_CR1->TXEIE=0;
		}

	}
	//check the rx-not-empty flag
	if(REG_Sigfox_ISR->RXNE)
	{
		//we have a character in the receive buffer.
		//we potentially only have 1ms to read this character before the next one arrives
		rx_buffer[rx_buffer_write_pos] = REG_Sigfox_RDR->RDR;
		//reading the RDR register clears the interrupt flag
		rx_buffer_write_pos++;
		rx_buffer_write_pos = rx_buffer_write_pos % RX_BUFFER_LENGTH;
	}
	
	/* UART Wake Up interrupt occured ------------------------------------------*/
	//and the alarm is set (uc is asleep)
	if (REG_Sigfox_ISR->WUF)
	{
		REG_Sigfox_ICR->WUCF = 1;
	}
}

