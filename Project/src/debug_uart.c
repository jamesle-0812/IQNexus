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


#include "debug_uart.h"
#include "_debug_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "tiny_vsnprintf.h"
#include "watchdog.h"
#include "global.h"

volatile static USART_TDR_t *REG_Debug_TDR = USART1_TDR_ADDR;
volatile static USART_RDR_t *REG_Debug_RDR = USART1_RDR_ADDR;
volatile static USART_BRR_t *REG_Debug_BRR = USART1_BRR_ADDR;
volatile static USART_CR1_t *REG_Debug_CR1 = USART1_CR1_ADDR;
//volatile static USART_CR3_t *REG_Debug_CR3 = USART1_CR3_ADDR;
volatile static USART_ISR_t *REG_Debug_ISR = USART1_ISR_ADDR;
volatile static USART_ICR_t *REG_Debug_ICR = USART1_ICR_ADDR;

volatile static USART_CLOCK_t *REG_Debug_CLKSEL = USART1_CLOCK_SELECTION_ADDR;
volatile static RCC_APB2ENR_t *REG_Debug_CLKEN = USART1_CLKEN_ADDR;


//RX buffer, of size MAX_COMMAND_LENGTH, will be used to read received data from CLI
volatile static char rx_buffer[RX_BUFFER_LENGTH];
volatile static int  rx_buffer_read_pos=0;
volatile static int  rx_buffer_write_pos=0;

volatile static char tx_buffer[TX_BUFFER_LENGTH];
volatile static int  tx_buffer_read_pos=0;
volatile static int  tx_buffer_write_pos=0;

inline void Debug_init()
{
	//USART disable
		REG_Debug_CR1->UE = 0;
	
	
		//set up the registers for USART
	
		//Enable the peripheral clock of USART1
		//HSI16 clock
		REG_Debug_CLKSEL->USART1_SEL = 2;
	
		//Enable the clock
		REG_Debug_CLKEN->USART1_EN = 1;
	
		//Pins (A9 as TX) (10 as RX)
		GPIO_InitTypeDef GPIO_InitStruct;

		/* USART TX GPIO pin configuration  */
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
		GPIO_InitStruct.Alternate = GPIO_AF4_USART1;

		HW_GPIO_Init(DEBUG_TX_PORT, DEBUG_TX_PIN, &GPIO_InitStruct);
		HW_GPIO_Init(DEBUG_RX_PORT, DEBUG_RX_PIN, &GPIO_InitStruct);
	
		//M-bits
			//Leave at default for 8N1 operation
		//Baud
		
		REG_Debug_BRR->BRR = 1666;
		
		//Stop Bits
			//Leave at default for 8N1
		//Transmit Enable
		REG_Debug_CR1->TE = 1;
		//Receive enable
		REG_Debug_CR1->RE = 1;
		
		//enable receive interrupts, so we can quickly get the data
		REG_Debug_CR1->RXNEIE = 1;
		REG_Debug_CR1->TXEIE  = 0;
		
		//configure the NVIC for the uart
		//TODO: use a manual implementation instead of lib?
		HAL_NVIC_SetPriority(USART1_IRQn, 2, 0);
		HAL_NVIC_EnableIRQ(USART1_IRQn);
		
		
		//ensure that the uart runs in stop mode
		REG_Debug_CR1->UESM = 1;
		//USART enable
		REG_Debug_CR1->UE = 1;

}

void disable_Debug()
{
	//USART disable
	REG_Debug_CR1->UE = 0;
	
	//Pins (A9 as TX) (10 as RX)
	GPIO_InitTypeDef GPIO_InitStruct;

	/* USART TX GPIO pin configuration  */
	GPIO_InitStruct.Mode      = GPIO_MODE_ANALOG;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF4_USART1;

	HW_GPIO_Init(DEBUG_TX_PORT, DEBUG_TX_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(DEBUG_RX_PORT, DEBUG_RX_PIN, &GPIO_InitStruct);
	//ensure that the uart is not running in stop mode
	REG_Debug_CR1->UESM = 0;

	//Disable the clock
	REG_Debug_CLKEN->USART1_EN = 0;
}


void Debug_printf( const char *format, ... )
{
	va_list args;
  va_start(args, format);
  uint8_t len=0;
  char tempBuff[64];

	reset_watchdog();
  /*convert into string at buff[0] of length iw*/
	len = tiny_vsnprintf_like(&tempBuff[0], sizeof(tempBuff), format, args); 
	reset_watchdog();
	Debug_AddToWriteBuffer(tempBuff, len);
	//Leave the flag set, and carry on, writing to the buffer will clear the flag for the next transmission.
  
  va_end(args);
	
}

void Debug_AddToWriteBuffer(char* message, int length)
{
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
	
	//enable the tx interrupt
	REG_Debug_CR1->TXEIE = 1;
	
}

void await_uart_tx()
{
		//wait for the tx buffer to be empty
		while(isCharToSend())
		{
				reset_watchdog();
		}
		reset_watchdog();
}

int isCharToSend()
{
	return REG_Debug_CR1->TXEIE;
}
int Debug_getRxDataLength()
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

char Debug_getChar( void )
{
	char c;
	//clear the new data from the rx buffer
	if(rx_buffer_read_pos != rx_buffer_write_pos)
	{
		c = rx_buffer[rx_buffer_read_pos];
		
		rx_buffer_read_pos++;
		rx_buffer_read_pos = rx_buffer_read_pos % RX_BUFFER_LENGTH;
	}
	return c;
	//Debug_printf("\tRead_pos : %d\r\n\tWrite_pos: %d\r\n", rx_buffer_read_pos, rx_buffer_write_pos);
}

int cliActive = 0;
int Debug_cliActive()
{
	return cliActive;
}

void Debug_disableCli(int disableRX)
{
	if(disableRX)
	{
		//disable the uart to change settings
		REG_Debug_CR1->UE = 0;
		//disable the receive functions
		REG_Debug_CR1->RXNEIE = 0;
		//Receive disable
		REG_Debug_CR1->RE = 0;
		
		//reenable the uart
		REG_Debug_CR1->UE = 1;
	}
	cliActive = 0;
}


//Interrupt handler for the debug_uart
void USART1_IRQHandler( void )
{
	//if the TX register is empty, we can load a new character into the buffer
	if(REG_Debug_ISR->TXE)
	{
		//if we are not at the end of the buffer, write the next character to the tx register
		if(tx_buffer_read_pos != tx_buffer_write_pos)
		{
			REG_Debug_TDR->TDR = tx_buffer[tx_buffer_read_pos];
			tx_buffer_read_pos++;
			tx_buffer_read_pos = tx_buffer_read_pos % TX_BUFFER_LENGTH;
		}
		//if we have reached the end of the buffer, we can stop triggering interrupts
		else
		{
			REG_Debug_CR1->TXEIE=0;
		}

	}
	//check the rx-not-empty flag
	if(REG_Debug_ISR->RXNE)
	{
		//if we receive a character, assume that the command line is active
		cliActive = 1;
		//we have a character in the receive buffer.
		//we potentially only have 1ms to read this character before the next one arrives
		rx_buffer[rx_buffer_write_pos] = REG_Debug_RDR->RDR;
		//reading the RDR register clears the interrupt flag
		rx_buffer_write_pos++;
		rx_buffer_write_pos = rx_buffer_write_pos % RX_BUFFER_LENGTH;
	}
	
	/* UART Wake Up interrupt occured ------------------------------------------*/
	//and the alarm is set (uc is asleep)
	if (REG_Debug_ISR->WUF)
	{
		REG_Debug_ICR->WUCF = 1;
	}
}

