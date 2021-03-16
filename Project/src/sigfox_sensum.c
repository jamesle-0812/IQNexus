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

#include "sigfox_sensum.h"
#include "sigfox_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "tiny_vsnprintf.h"
#include "tiny_sscanf.h"
#include "watchdog.h"
#include "counter.h"
#include "delays.h"
#include "global.h"
#include "flash_map.h"
#include "sensum_version.h"
#include "timeServer.h"

/* Private Defines -----------------------------------------------------------*/
#define SF_AT_DUMMY    "\r\nAT\r\n"          //Used to Check for AT presence
#define SF_AT_INFO     "\r\nAT$I=%d\r\n"     //Get device info, see sf_at_info_e
#define SF_AT_POWER    "\r\nAT$P=%d\r\n"     //Put SF radio to sleep
#define SF_AT_TX_BYTES "\r\nAT$SF=%s,1\r\n"  //send a tx and wait for downlink, ensure that %s is in hex
#define SF_AT_TX_BYTES_NO_DOWNLINK "\r\nAT$SF=%s\r\n"  //send a tx and wait for downlink, ensure that %s is in hex
#define SF_AT_RTS      "\r\nAT$GI?\r\n"       //need to send this before TX_BYTES
#define SF_AT_RC       "\r\nAT$RC\r\n"       //send to set the correct macro channel?



#define printf(...) Debug_printf(__VA_ARGS__)

#ifdef DISABLE_SIGFOX_DEBUG
	#define dbg_printf(...) 
#else
	#define dbg_printf(...) Debug_printf(__VA_ARGS__)
#endif


/* Private typedef -----------------------------------------------------------*/
typedef enum
{
	sf_at_info_softwareVersion       = 0,
	sf_at_info_contactDetails        = 1,
	sf_at_info_hardwareRevisionLower = 2,
	sf_at_info_hardwareRevisionUpper = 3,
	sf_at_info_firmwareMajor         = 4,
	sf_at_info_firmwareMinor         = 5,
	sf_at_info_firmwareVarient       = 7,
	sf_at_info_firmwareVCS           = 8,
	sf_at_info_sigfoxLibraryVersion  = 9,
	sf_at_info_deviceID              = 10,
	sf_at_info_devicePAC             = 11,
}sf_at_info_e;

typedef enum
{
	sf_at_power_softwareReset        = 0,
	sf_at_power_sleep                = 1,
	sf_at_power_deepSleep            = 2,
}sf_at_power_e;

/* Private variables ---------------------------------------------------------*/
#define COMMAND_RX_BUFFER_SIZE 100
static char command_rx_buffer[COMMAND_RX_BUFFER_SIZE];


#define RX_BUFFER_SIZE 8
uint8_t sf_rx_buffer[RX_BUFFER_SIZE];

bool sigfox_test_at(void);

/* Private function prototypes -----------------------------------------------*/



bool sf_send_rc()
{
	int retries = 3;
	
	for(retries = 3; retries > 0; retries --)
	{
		//send the AT$GI? command
		sigfox_send(SF_AT_RC);
		sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 50);

		if(!strcmp(command_rx_buffer,"OK"))
		{
			return true;
		}
	}
	
	return false;
}

bool sf_channel_ensure(void)
{
	int retries = 3;
	int length = 0;
	int X = 1;
	int Y = 4;
	
	for(retries = 3; retries > 0; retries --)
	{
		//send the AT$GI? command
		sigfox_send(SF_AT_RTS);
		length = sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 50);
		
		dbg_printf("Length: %d\r\n", length);
		await_uart_tx();
		//return value should be X,Y
		if(length)
		{
			//tiny_sscanf(command_rx_buffer,"%d,%d",X,Y); 
			X = command_rx_buffer[0]-0x30;
			Y = command_rx_buffer[2]-0x30;
			dbg_printf("%s, X=%d, Y=%d\r\n", command_rx_buffer, X, Y);
			if(X == 0 || Y<3)
			{
				if(sf_send_rc())
				{
					return true;
				}
			}
			else
			{
				return true;
			}
		}
	}
	
	return false;
	
}

static int SigfoxFormatTxString(char* output, int output_length,const char *format, ... )
{
	va_list args;
	va_start(args, format);
	uint8_t len=0;

	/*convert into string at buff[0] of length iw*/
	len = tiny_vsnprintf_like(output, output_length, format, args); 

	va_end(args);

	return len;
}

bool sf_send(uint8_t* data, int length)
{
	if(length > 12)
	{
		return false;
	}
	char send_buffer[25] = {0};
	int i = 0;
	
	sigfox_wake();
	
	//first ensure the channel is correct
	if(!sf_channel_ensure())
	{
		sigfox_sleep();
		return false;
	}
	
	for(i=0;i<length;i++)
	{
		SigfoxFormatTxString(&send_buffer[2*i],3,"%02X", data[length-i-1]);
		reset_watchdog();
	}
	
	printf("Packet: %s\r\n", send_buffer);
	
	// sigfox_send(SF_AT_TX_BYTES,send_buffer); // request for a downlink
	sigfox_send(SF_AT_TX_BYTES_NO_DOWNLINK,send_buffer);
	// sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 60000);	// long wait for downlink
	sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 3700);	// short wait for confirmation
	printf("%s\r\n",command_rx_buffer);
	
	//we have OK if a downlink is received.
	//we also have ERR_SFX_ERR_SEND_FRAME_WAIT_TIMEOUT if we timedout waiting for a downlink
	//unfortunatly we might have to wait for the downlink after each uplink, and most of them will
	//not have any downlink to send us. This will be a bit of a waste of power.

	if(!strcmp(command_rx_buffer,"OK"))
	{
		/* //process downlink
		//first, read data into the RX buffer, we will give it a minute for now, 
		//but I suspect that the actual requirement is going to be much shorter.
		int len = sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 60000);
		printf("RX: %s\r\n", command_rx_buffer);
		//we need to discard the first two characters, and then read the remainder into
		//the rx_data array.
		i=3;
		int j = 0;
		while(i<len && j<8)
		{
			sf_rx_buffer[j] =  atoi(command_rx_buffer+i);
			i+=3;
			j++;
		}
		
		Debug_printf("RX_array: %02X %02X %02X %02X %02X %02X %02X %02X\r\n"
			,sf_rx_buffer[0]
			,sf_rx_buffer[1]
			,sf_rx_buffer[2]
			,sf_rx_buffer[3]
			,sf_rx_buffer[4]
			,sf_rx_buffer[5]
			,sf_rx_buffer[6]
			,sf_rx_buffer[7]
		);
		Debug_printf("Send OK, Receive OK\r\n");
		 */
		sigfox_sleep();
		return true;
	}

	if(!strcmp(command_rx_buffer,"ERR_SFX_ERR_SEND_FRAME_WAIT_TIMEOUT"))
	{
		Debug_printf("Send OK, No Receive\r\n");
		sigfox_sleep();
		return true;
	}
	sigfox_sleep();
	return false;
	
}

void sf_hardware_init()
{
	GPIO_InitTypeDef GPIO_InitStruct;
		/* LED Initialisation  */
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_OD;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;

	HW_GPIO_Init(SF_RESET_PORT, SF_RESET_PIN, &GPIO_InitStruct);
	LL_GPIO_SetPinOutputType(SF_RESET_PORT, SF_RESET_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
	LL_GPIO_SetOutputPin(SF_RESET_PORT, SF_RESET_PIN);
	
	sigfox_usart_init();
}

void sf_hardware_deinit()
{
	disable_sigfox_usart();
}

bool sigfox_test_at()
{
	int i = 3;
	
	for(i=3;i>0;i--)
	{
		sigfox_send(SF_AT_DUMMY);
		sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 50);
		
		if(!strcmp(command_rx_buffer, "OK"))
		{
			return true;
		}
		delay_timeout_ms(100);
	}
	return false;
}

bool sigfox_sleep()
{
	//to put the sigfox unit to sleep, send the SF_AT_POWER with argument sf_at_power_deepSleep	
	
	int i = 3;
	
	for(i=3;i>0;i--)
	{
		sigfox_send(SF_AT_POWER, sf_at_power_deepSleep);
		sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 50);
		
		if(!strcmp(command_rx_buffer, "OK"))
		{
			printf("SF sleeping\r\n");
			sf_hardware_deinit();
			return true;
		}
		
		delay_timeout_ms(100);
	}
	printf("SF not sleeping\r\n");
	return false;
}

bool sigfox_wake()
{
	sf_hardware_init();
	//we wake the module by momentarily pulling the reset pin low.
	LL_GPIO_ResetOutputPin(SF_RESET_PORT, SF_RESET_PIN);
	delay_timeout_ms(10);
	LL_GPIO_SetOutputPin(SF_RESET_PORT, SF_RESET_PIN);
	delay_timeout_ms(100);
	
	if(sigfox_test_at())
	{
		dbg_printf("Wake OK\r\n");
		return true;
	}
	else
	{
		dbg_printf("Wake Fail\r\n");
		return false;
	}
}

bool sf_print_info()
{
	sigfox_wake();
	
	//sigfox_usart_init();
	int len = 0;
	bool success = true;
	
	
	//get the PAC
	sigfox_send(SF_AT_INFO, sf_at_info_devicePAC);
	len = sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 100);
	
	printf("(%02d)PAC: %s\r\n", len, command_rx_buffer);
	if(len == 0)
	{
		success = false;
	}
	
	//get the ID
	//sigfox_usart_init();
	sigfox_send(SF_AT_INFO, sf_at_info_deviceID);
	len = sigfox_readLine(command_rx_buffer, COMMAND_RX_BUFFER_SIZE, 100);
	
	printf("(%02d)ID : %s\r\n", len, command_rx_buffer);
	if(len==0)
	{
		success = false;
	}
	
	sigfox_sleep();
	return success;
}



