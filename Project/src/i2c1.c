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


//private header with register descriptions, defines, etc
#include "_i2c1.h"
//public header with function prototypes, definces, vars, etc
#include "i2c1.h"

#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#include "debug_uart.h"
#include "global.h"

#include "delays.h"
#include "watchdog.h"

#ifdef DISABLE_I2C_DATA_DEBUG
	#define DBG_DAT_printf(...)
#else
	#define DBG_DAT_printf(...) Debug_printf(__VA_ARGS__)
#endif

#ifdef DISABLE_I2C_DEBUG
	#define DBG_printf(...) 
#else
	#define DBG_printf(...) Debug_printf(__VA_ARGS__)
#endif

#define FLASH_I2C_ADDR	0x05

/********************************************************************
 *Register Access                                                   *
 ********************************************************************/
 //Uncomment these as you find a use for them, as having them without
 //a refrence will throw compiler warning #177-D
 
volatile static I2C_CR1_t						*REG_I2CBus_CR1 			= I2C1_CR1_ADDR;
volatile static I2C_CR2_t						*REG_I2CBus_CR2 			= I2C1_CR2_ADDR;
//volatile static I2C_OAR1_t					*REG_I2CBus_OAR1 		= I2C1_OAR1_ADDR;
//volatile static I2C_OAR2_t					*REG_I2CBus_OAR2 		= I2C1_OAR2_ADDR;
volatile static I2C_TIMINGR_t 			*REG_I2CBus_TIMINGR 	= I2C1_TIMINGR_ADDR;
//volatile static I2C_TIMEOUTR_t			*REG_I2CBus_TIMEOUTR	= I2C1_TIMEOUTR_ADDR;
volatile static I2C_ISR_t						*REG_I2CBus_ISR			= I2C1_ISR_ADDR;
volatile static I2C_ICR_t						*REG_I2CBus_ICR			= I2C1_ICR_ADDR;
//volatile static I2C_PECR_t					*REG_I2CBus_PECR			=	I2C1_PECR_ADDR;
volatile static I2C_RXDR_t					*REG_I2CBus_RXDR			= I2C1_RXDR_ADDR;
volatile static I2C_TXDR_t					*REG_I2CBus_TXDR			= I2C1_TXDR_ADDR;
//volatile static I2C_RCC_APB1RSTR_t	*REG_I2CBus_CLKRST		= I2C_RCC_APB1RSTR_ADDR;
volatile static I2C_RCC_APB1ENR_t		*REG_I2CBus_CLKEN		= I2C_RCC_APB1ENR_ADDR;
//volatile static I2C_RCC_APB1SMENR_t	*REG_I2CBus_SMEN			= I2C_RCC_APB1SMENR_ADDR;
volatile static I2C_RCC_CCIPR_t			*REG_I2CBus_CCIPR		= I2C_RCC_CCIPR_ADDR;

/********************************************************************
 *Functions                                                         *
 ********************************************************************/
 
 void dump_regs()
 {
	 return;
//		DBG_printf("CR1     :0x%08X\r\n", *((uint32_t*)REG_I2CBus_CR1));
//		DBG_printf("CR2     :0x%08X\r\n", *((uint32_t*)REG_I2CBus_CR2));
//		//DBG_printf("OAR1    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_OAR1));
//		//DBG_printf("OAR2    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_OAR2));
//		DBG_printf("TIMINGR :0x%08X\r\n", *((uint32_t*)REG_I2CBus_TIMINGR));
//		//DBG_printf("TIMEOUTR:0x%08X\r\n", *((uint32_t*)REG_I2CBus_TIMEOUTR));
//		DBG_printf("ISR     :0x%08X\r\n", *((uint32_t*)REG_I2CBus_ISR));
//		DBG_printf("ICR     :0x%08X\r\n", *((uint32_t*)REG_I2CBus_ICR));
//		//DBG_printf("PECR    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_PECR));
//		DBG_printf("RXDR    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_RXDR));
//		DBG_printf("TXDR    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_TXDR));
//		//DBG_printf("CLKRST  :0x%08X\r\n", *((uint32_t*)REG_I2CBus_CLKRST));
//		DBG_printf("CLKEN   :0x%08X\r\n", *((uint32_t*)REG_I2CBus_CLKEN));
//		//DBG_printf("SMEN    :0x%08X\r\n", *((uint32_t*)REG_I2CBus_SMEN));
//		DBG_printf("CCIPR   :0x%08X\r\n", *((uint32_t*)REG_I2CBus_CCIPR));
 }
 
 bool i2c_is_clk_physical_idle()
 {
	 //set the pins as inputs
	GPIO_InitTypeDef GPIO_InitStruct;

	/* I2C1 pin configuration  */
	//Configure as PP to get around limitation of the HW_GPIO_Iint function
	GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;				//I2C is an open-drain protocol
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	 
	HW_GPIO_Init(GPIOB, GPIO_PIN_8, &GPIO_InitStruct);
	 
	 //now that they are inputs, check their levels, they should both be high.
	 if(LL_GPIO_IsInputPinSet(GPIOB, GPIO_PIN_8))
	 {
		 return true;
	 }
	 else
	 {
		 return false;
	 }
 }
 
bool i2c_is_dat_physical_idle()
 {
	 //set the pins as inputs
	GPIO_InitTypeDef GPIO_InitStruct;

	/* I2C1 pin configuration  */
	//Configure as PP to get around limitation of the HW_GPIO_Iint function
	GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;				//I2C is an open-drain protocol
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
	 
	HW_GPIO_Init(GPIOB, GPIO_PIN_9, &GPIO_InitStruct);
	 
	 //now that they are inputs, check their levels, they should both be high.
	 if(LL_GPIO_IsInputPinSet(GPIOB, GPIO_PIN_9))
	 {
		 return true;
	 }
	 else
	 {
		 return false;
	 }
 }
 
 bool i2c1_check_physical()
 {
	 GPIO_InitTypeDef GPIO_InitStruct;
	 
		int consecutive_idle = 0;
		int idle_attempts = 100;
		 
		//Ensure that CLK is not pulled low.
		//if it is, we cannot recover the line without powering off the peripherals
		//and that is a problem, as the peripherals are currently tied to the MCU power.
		if(i2c_is_clk_physical_idle() == false)
		{
			//return error?
			DBG_printf("I2C CLK ERROR\r\n");
			return false;
		}
		
		//configure the clk as a OD output
			/* I2C1 pin configuration  */
		//Configure as PP to get around limitation of the HW_GPIO_Iint function
		GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;				//I2C is an open-drain protocol
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
		GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;
		
		HW_GPIO_Init(GPIOB, GPIO_PIN_8, &GPIO_InitStruct);
		LL_GPIO_SetPinOutputType(GPIOB, GPIO_PIN_8, LL_GPIO_OUTPUT_OPENDRAIN);
		
		//now pulse clk until we see the data line is high for 9 consecutive cycles
		while(consecutive_idle < 9)
		{
			//pulse the clk
			LL_GPIO_ResetOutputPin(GPIOB, GPIO_PIN_8);
			delay_timeout_ms(1);
			LL_GPIO_SetOutputPin(GPIOB, GPIO_PIN_8);
			delay_timeout_ms(1);
			
			//read the data, and update the consecutive idle count.
			if(i2c_is_dat_physical_idle())
			{
				consecutive_idle++;
			}
			else
			{
				consecutive_idle = 0;
			}
			
			//we need to limit how long we can spend trying to clear the line.
			idle_attempts--;
			if(idle_attempts == 0)
			{
				DBG_printf("I2C DAT ERROR\r\n");
				return false;
			}
		}
		
		//at this point we know that the physical lines are OK
		return true;
 }
 
 
bool i2c1_init()
{
	GPIO_InitTypeDef GPIO_InitStruct;
	
	DBG_printf("Enter I2C Init\r\n");
	//Disable I2C1
	REG_I2CBus_CR1->PE = 0;
	
	//dump_regs();

	if(i2c1_check_physical() == false)
	{
		return false;
	}
	//at this point, we have ensured that the line is clear, and we can start our communications
	
	
	//We want to use I2C1, on pins PB8 (SCL) and PB9 (SDA).
	//Lets initialise the pins first

	/* I2C1 pin configuration  */
	//Configure as PP to get around limitation of the HW_GPIO_Iint function
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;				//I2C is an open-drain protocol
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF4_I2C1;

	HW_GPIO_Init(I2C_CLK_PORT, I2C_CLK_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(I2C_DAT_PORT, I2C_DAT_PIN, &GPIO_InitStruct);
	
	//This is needed because the HW_GPIO_INIT will not configure an AF-OD!!!!
	//Look at line 241 of HW_GPIO.c for proof
	LL_GPIO_SetPinOutputType(I2C_CLK_PORT, I2C_CLK_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
	LL_GPIO_SetPinOutputType(I2C_DAT_PORT, I2C_DAT_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
	
	//Now the peripheral clock
	//select HSI16 clock for I2C1
	REG_I2CBus_CCIPR->I2C1SEL = 2; //From page 162 of PM (section 7.3.20)
	//Enable the peripheral clock
	REG_I2CBus_CLKEN->I2C1EN = 1;
	
	//Configure Analog noise filter and Digital noise filter
	//For now leave them disabled
	REG_I2CBus_CR1->ANFOFF = 1;
	REG_I2CBus_CR1->DNF=0;
	
	//Configure the data timing
	//Using values from table 117, page 686, section 27.4.10 of the PRM
	REG_I2CBus_TIMINGR->PRESC 	= 3;
	//REG_I2CBus_TIMINGR->SCLL 	= 0x13;
	REG_I2CBus_TIMINGR->SCLL 	= 0x26;
	//REG_I2CBus_TIMINGR->SCLH		= 0x0F;
	REG_I2CBus_TIMINGR->SCLH		= 0x1E;
	REG_I2CBus_TIMINGR->SDADEL	= 10;
	REG_I2CBus_TIMINGR->SCLDEL	= 10;
	
	//configure nostretch, must be kept clear in master mode
	REG_I2CBus_CR1->NOSTRETCH = 0;
	
	
	//Re-enable I2C1
	REG_I2CBus_CR1->PE = 1;
	
	DBG_printf("I2C1 init complete\r\n");
	//dump_regs();
	
	return true;
}

//can only send up to 255 bytes
int i2c1_send_feedback(uint8_t slave_address, uint8_t *data, int data_length, int send_stop)
{
	int result = 1;
	//Addressing mode, 7 bit
	REG_I2CBus_CR2->ADD10 = 0;
	//Slave address
	REG_I2CBus_CR2->SADD = slave_address<<1;
	//Transfer direction, write
	REG_I2CBus_CR2->RD_WRN = 0;
	
	//Setup NBYTES, to the data length
	DBG_DAT_printf("Data length %d", data_length);
	REG_I2CBus_CR2->NBYTES = data_length;
	//Make autoend 0 before setting the start bit, to enable repeated-starts
	REG_I2CBus_CR2->AUTOEND = 0;
	//set reload to 0, as we are not writing more than 255 bytes
	REG_I2CBus_CR2->RELOAD = 0;
	
	DBG_printf("Setting START TX\r\n");
	DBG_DAT_printf("Writing Data:");
	
	//pre-load the first data byte, before setting start, to prevent an aborted write from affecting the new transaction
	REG_I2CBus_TXDR->TXDATA = *data;
	DBG_DAT_printf(" %02X", *data);
	//REG_I2CBus_CR2->START = 1;
	//increment the pointer to buffer memory
	data++;
	data_length--;
	
	
	reset_watchdog();
	//Start transmission
	REG_I2CBus_CR2->START = 1;
	//wait for the start frame to be sent
	while(REG_I2CBus_CR2->START);
	//now set autoend to the user defined value, to determine if we should send a stop
	//condition after the data is transmitted
	REG_I2CBus_CR2->AUTOEND = send_stop;
	
	
	//while transfer is not complete, and we have not received a nack from the slave
	//Busy should be set by hardware when the communication begins, and cleared by hardware when communication ends
	while(REG_I2CBus_ISR->BUSY && !REG_I2CBus_ISR->TC)
	{
		dump_regs();
		//DBG_printf("REG_I2CBus_ISR = 0x%X\r\n", *((uint32_t*)REG_I2CBus_ISR));
		//if the transmit data register is empty, we can load our next byte
		//looks like occasionally an extra byte was being loaded at the end
		//which was causing communication issues. Restricting the TX to only the ammount
		//specified should fix this.
		if(REG_I2CBus_ISR->TXE && data_length)
		{
			//DBG_printf("Writing data\r\n");
			reset_watchdog();
			//load the byte
			REG_I2CBus_TXDR->TXDATA = *data;
			DBG_DAT_printf(" %02X", *data);
			//REG_I2CBus_CR2->START = 1;
			//increment the pointer to buffer memory
			data++;
			data_length--;
		}
	}
	
	DBG_DAT_printf("\r\nData write complete\r\n");
	reset_watchdog();
	//NACK received, data transfer incomplete
	if(REG_I2CBus_ISR->NACKF)
	{
		//clear the flag
		REG_I2CBus_ICR->NACKCF = 1;
		
		DBG_printf("NACK Received on write\r\n");
		reset_watchdog();
		//TODO: Something here to indicate faulure
		result = 0;
	}
	reset_watchdog();
	//clear the flag set by the stop signal that was automatically sent ala AUTOEND
	REG_I2CBus_ICR->STOPCF = 1;
	
	return result;
}


//can only send up to 255 bytes
void i2c1_send(uint8_t slave_address, uint8_t *data, int data_length, int send_stop)
{
	i2c1_send_feedback(slave_address, data, data_length, send_stop);
}

int i2c1_receive_feedback(uint8_t slave_address, uint8_t *data, int data_length, int send_stop)
{
	int result = 1;
	//Addressing mode, 7 bit
	REG_I2CBus_CR2->ADD10 = 0;
	//Slave address
	REG_I2CBus_CR2->SADD = slave_address<<1;
	//Transfer direction, read
	REG_I2CBus_CR2->RD_WRN = 1;
	
	//Setup NBYTES, to the data length
	REG_I2CBus_CR2->NBYTES = data_length;
	//Make autoend 0 before setting the start bit, to enable repeated-starts
	REG_I2CBus_CR2->AUTOEND = 0;
	//set reload to 0, as we are not writing more than 255 bytes
	REG_I2CBus_CR2->RELOAD = 0;
	
	
	DBG_printf("Setting START RX\r\n");
	//Start transmission
	REG_I2CBus_CR2->START = 1;
	//wait for start frame to be sent
	while(REG_I2CBus_CR2->START);
	//now set autoend to the user defined value, to determine if we should send a stop
	//condition after the data is received
	REG_I2CBus_CR2->AUTOEND = send_stop;
	
	DBG_DAT_printf("Reading Data");
	//while there is communication
	while(REG_I2CBus_ISR->BUSY && !REG_I2CBus_ISR->TC)
	{
		//if there is data in the receive register
		if(REG_I2CBus_ISR->RXNE)
		{
			*data = REG_I2CBus_RXDR->RXDATA;
			//increment the pointer to buffer memory
			DBG_DAT_printf(" %02X", *data);
			data++;
		}
	}
	
	
	
	DBG_DAT_printf("\r\nData Read complete\r\n");
	
	//check for error
	if(REG_I2CBus_ISR->NACKF)
	{
		result = 0;
		//clear the flag
		REG_I2CBus_ICR->NACKCF = 1;
		
		DBG_printf("NACK Received on read\r\n");
		//TODO: Something here to indicate faulure
	}
	
	//clear the flag set by the stop signal that was automatically sent ala AUTOEND
	REG_I2CBus_ICR->STOPCF = 1;
	
	return result;
}

void i2c1_receive(uint8_t slave_address, uint8_t *data, int data_length, int send_stop)
{
	i2c1_receive_feedback(slave_address, data, data_length, send_stop);
}


