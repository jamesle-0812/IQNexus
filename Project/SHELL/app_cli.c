/******************************************************************************************************
*									Natcom Electronics (Pty) Ltd.
*									      P.O. Box 138
*										  Umbogintwini
*										 	  4120
*										 KZN South Africa
*
*                                        www.natcom.co.za
*
*                                       (c) Copyright 2011
*
* This document is protected under the Non Disclosure Agreement between Natcom Electronics and
* their customer. Copying of this document, disclosing it to third parties or using the contents
* thereof for any purposes without the express written authorisation from Natcom Electronics
* and their customer is strictly forbidden.  Any offenders are liable to pay all relevant damages.
* The copyright and the foregoing restrictions extended to all media in which the information may
* be embodied.
*
* @file              app_cli.c
*
* Programmer(s) :
*
* Description :     Command Line Interface
*
* Revision History :
* Changed By    Version    Date        Comments
* --------------------------------------------------------------------------------------------------
* RJH           00-00      2004/11/17  Creation
****************************************************************************************************
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

//HW_RTC.h must be before shell.h, due to conflict on the definition of CR!!
#include "hw_rtc.h"
#include "shell.h"
#include "UsartShell.h"
#include "debug_uart.h"
#include "app_cli.h"
#include "cli_led.h"
#include "modbus_generic.h"
#include "counter.h"
#include "flash_map.h"
#include "i2cFlash.h"
#include "hw_gpio.h"
#include "hw_msp.h"
#include "delays.h"
#include "watchdog.h"
#include "lora.h"
#include "adc.h"
#include "radio_common.h"
#include "lora_sensum.h"

#include "global.h"
#include "Commissioning.h"

#define MAX_ANSWER_SIZE				64

#define OSAL_Const_String( _NAME, _VALUE ) const char _NAME [] = _VALUE


OSAL_Const_String(cmd_error_string, 	"ERR \r\n" 						);
OSAL_Const_String(cmd_ok_string, 		"OK \n\r"						);
OSAL_Const_String(cmd_help, 			"-h"							);
OSAL_Const_String(cmd_help_info, 		"Command parameters : \r\n" 	);
OSAL_Const_String(cmd_help_param_val, 	" [01..]" 						);
OSAL_Const_String(cmd_newline, 			"\n\r" 							); // use: dbg_print(cmd_newline);
OSAL_Const_String(cli_general_str_unsignedval, 	"%u" );

/*!
 * The array of commands registration.
 */
Cmd_registration a_cmd_registration[SHELL_NB_COMMANDS] = {
	{(char *) "!!"        , cli_help     , 0xFFFFFFFF}, // ALWAYS KEEP AS THE FIRST COMMAND IN THE ARRAY.
	{(char *) "help"      , cli_help     , 0xFFFFFFFF},
	{(char *) "show"      , cli_show     , 0xFFFFFFFF},
	{(char *) "run"       , cli_run      , 0xFFFFFFFF},
	{(char *) "led"       , cli_led      , 0xFFFFFFFF},
	{(char *) "modbus"    , cli_modbus   , 0xFFFFFFFF},
	{(char *) "count"     , cli_count    , 0xFFFFFFFF},
	{(char *) "flash"     , cli_flash    , 0xFFFFFFFF},
	{(char *) "wakeup"    , cli_wakeup   , 0xFFFFFFFF},
	{(char *) "rejoin"    , cli_rejoin   , 0xFFFFFFFF},
	{(char *) "mode"      , cli_mode     , 0xFFFFFFFF},
	{(char *) "time"      , cli_time     , 0xFFFFFFFF},
	{(char *) "test"      , cli_test     , 0xFFFFFFFF},
	{(char *) "uplink"    , cli_uplink   , 0xFFFFFFFF},
	{(char *) "gain"      , cli_gain     , 0xFFFFFFFF},
	{(char *) "threshold" , cli_threshold, 0xFFFFFFFF},
	{(char *) "device"    , cli_device   , 0xFFFFFFFF},
	{(char *) "appeui"    , cli_appeui   , 0xFFFFFFFF},
	{(char *) "appkey"    , cli_appkey   , 0xFFFFFFFF},
	{(char *) "channel"   , cli_channel  , 0xFFFFFFFF},
	{(char *) "reboot"    , cli_reboot   , 0xFFFFFFFF},
	
};


#define PRINT_CLI_OK		dbg_print(cmd_ok_string);
#define PRINT_CLI_ERR		dbg_print(cmd_error_string);

const char *const FSCMDS_MSG_HELP            = (char *)"\
"CRLF\
"!! 		- Executes previous command."CRLF"\
help 		- This help."CRLF"\
shell 		- Shell version."CRLF"\
version 	- Firmware version information."CRLF;

eExecStatus cli_help( int argc,   char *argv[],
						    char **ppcStringReply )
{
	dbg_print("-\n\r");
	dbg_print("HELP\n\r");
	dbg_print("-\n\r");
	await_uart_tx();
	dbg_print("led      : Individually control the on board leds\r\n");
	await_uart_tx();
	
	//for generic modbus, show this help
	if(device.cli_commands & (cmd_modbus))
	{
		dbg_print("modbus   : Set modbus devices and registers to uplink\r\n");
		await_uart_tx();
	}

	//for any of the count commands, show this help
	if(device.cli_commands & (cmd_count1 | cmd_count2 | cmd_count3))
	{
		dbg_print("count    : Re/Set the counters and set debounce interval\r\n");
		await_uart_tx();
	}
	
	if(device.cli_commands & cmd_thresholds)
	{
		dbg_print("threshold: Set device alarm thresholds\r\n");
		await_uart_tx();
	}
	
	dbg_print("flash    : Save/load/erase configurationa nd data\r\n");
	await_uart_tx();
	dbg_print("mode     : Set the device mode\r\n");
	await_uart_tx();
	dbg_print("time     : Set or view the device time\r\n");
	await_uart_tx();
	dbg_print("wakeup   : Set the device wakeup interval\r\n");
	await_uart_tx();
	dbg_print("rejoin   : Set the device rejoin interval\r\n");
	await_uart_tx();
	dbg_print("uplink   : Set the device wakeup interval\r\n");
	await_uart_tx();
	dbg_print("appkey   : Get/Set the Device Appkey\r\n");
	await_uart_tx();
	dbg_print("appeui   : Get/Set the Device Appeui\r\n");
	await_uart_tx();
	dbg_print("channel  : Configure Default channel list for AU915\r\n");
	await_uart_tx();
	dbg_print("show     : Display all configuration information\r\n");
	await_uart_tx();
	dbg_print("help     : Display this message\r\n");
	await_uart_tx();


	return SHELL_EXECSTATUS_OK;
}

eExecStatus cli_run( int argc,   char *argv[], char **ppcStringReply )
{
	dbg_print("Running\r\n");
	//should these be enabled? do we want to save or discard by default?
//	save_config();
//	save_data();
	Debug_disableCli(1);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_show( int argc, char *argv[], char **ppcStringReply )
{
	char* argv_internal[] = {"show"};
	int argc_internal = 1;
	
	DeviceClass_t class_var;
	
	LORA_GetCurrentClass( &class_var );
	Debug_printf("LoRaClass: ");
	
	switch(class_var)
	{
		case CLASS_A:
			Debug_printf("A\r\n");
			break;
		case CLASS_B:
			Debug_printf("B\r\n");
			break;
		case CLASS_C:
			Debug_printf("C\r\n");
			break;
		default:
			Debug_printf("Unknown\r\n");
			break;
	}
	
	
	//prints out all configurations
	cli_mode     (argc_internal, argv_internal, ppcStringReply);
	cli_modbus   (argc_internal, argv_internal, ppcStringReply);
	cli_count    (argc_internal, argv_internal, ppcStringReply);
	cli_wakeup   (argc_internal, argv_internal, ppcStringReply);
	cli_rejoin   (argc_internal, argv_internal, ppcStringReply);
	cli_gain     (argc_internal, argv_internal, ppcStringReply);
	cli_time     (argc_internal, argv_internal, ppcStringReply);
	cli_threshold(argc_internal, argv_internal, ppcStringReply);
	cli_device   (argc_internal, argv_internal, ppcStringReply);

	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_led( int argc, char *argv[], char **ppcStringReply )
{
	cli_led_implementation(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_modbus( int argc, char *argv[], char **ppcStringReply )
{
	cli_modbus_implementation(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_count( int argc, char *argv[], char **ppcStringReply )
{
	cli_count_implementation(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}


eExecStatus cli_flash( int argc, char *argv[], char **ppcStringReply )
{	
	cli_flash_implementation(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}


void cli_uplink_help()
{
	Debug_printf("Usage: uplink data\r\n");
	await_uart_tx();
	Debug_printf("\tGet data from a peripheral and send via LoRa\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: uplink startup\r\n");
	await_uart_tx();
	Debug_printf("\tSend the startup packet via LoRa\r\n");
	await_uart_tx();
}

eExecStatus cli_uplink( int argc, char *argv[], char **ppcStringReply )
{	
	int16_t repeats = 0;
	uint32_t timeout_ms = 0;
	if(argc > 1)
	{
		repeats = atoi(argv[1]);
	}
	
	if(argc > 2)
	{
		timeout_ms = 1000* atoi(argv[2]);
	}
	
	repeats--;
	
	if(argc >0)
	{
		if(!strcmp(argv[0],"data"))
		{
			//becuase the device may not be initialised at this point
			device.init();
			
			Debug_printf("Getting data from peripheral and sending\r\n\r\n");
			
			do
			{
				device.send_data();
				delay_timeout_ms(timeout_ms);
			}while(repeats-- > 0);
			Debug_printf("\r\nDone\r\n");
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		
		if(!strcmp(argv[0],"startup"))
		{
			Debug_printf("Sending startup packet\r\n\r\n");
			do
			{
				sendStartupPacket();
				delay_timeout_ms(timeout_ms);
			}while(repeats-- > 0);
			Debug_printf("\r\nDone\r\n");
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		
		#ifdef RAIDO_LORA_INTERNAL
		{
			if(!strcmp(argv[0], "confirmed"))
			{
				//uplink confirmed with dummy data (LoRa)
				lora_config_reqack_set(LORAWAN_CONFIRMED_MSG);
				//uplink
				do
				{
					sendStartupPacket();
					delay_timeout_ms(timeout_ms);
				}while(repeats-- > 0);
				//restore to uncomfirmed state
				lora_config_reqack_set(LORAWAN_UNCONFIRMED_MSG);
				return SHELL_EXECSTATUS_OK_NO_FREE;
			}
			
			if(!strcmp(argv[0], "link_check"))
			{
				do
				{
					//Ensure that a link check is added to the MAC command of the uplink.
					MlmeReq_t mlmeReq;
					mlmeReq.Type = MLME_LINK_CHECK;
					LoRaMacMlmeRequest( &mlmeReq );
					sendStartupPacket();
					delay_timeout_ms(timeout_ms);
				}while(repeats-- > 0);
				return SHELL_EXECSTATUS_OK_NO_FREE;
			}
		}
		#endif
	}
	
	cli_uplink_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_wakeup_help()
{
	Debug_printf("Usage: wakeup base [interval]\r\n");
	await_uart_tx();
	Debug_printf("\tWake up every [interval] minutes\r\n");
	await_uart_tx();
	
	if(device.cli_commands & cmd_thresholds)
	{
		Debug_printf("Usage: wakeup uplink [number]\r\n");
		await_uart_tx();
		Debug_printf("\tTransmit every [number] wakeups\r\n");
		await_uart_tx();
	}
	
	Debug_printf("Usage: wakeup show\r\n");
	await_uart_tx();
	Debug_printf("\tPrints the current wakeup interval in minutes\r\n");
	await_uart_tx();
}

eExecStatus cli_wakeup( int argc, char *argv[], char **ppcStringReply )
{	
	uint32_t newWakeup = 0;
	if(argc == 1)
	{
		if(!strcmp(argv[0],"show"))
		{
			Debug_printf("Wakeup every %d minutes\r\n", transmit_interval_ms/60000);
			Debug_printf("Transmit every %d wakeups\r\n", wakeups_per_uplink);
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
	}
	
	if(argc == 2)
	{
		newWakeup = atoi(argv[1]);
		if(!strcmp(argv[0], "base"))
		{
			if(newWakeup >= 1)
			{
				transmit_interval_ms = newWakeup*60000;
				Debug_printf("Wakeup every %d minutes\r\n", transmit_interval_ms/60000);
				return SHELL_EXECSTATUS_OK_NO_FREE;
			}
		}
		
		if(device.cli_commands & cmd_thresholds)
		{
			if(!strcmp(argv[0], "uplink"))
			{
				if(newWakeup >= 1)
				{
					wakeups_per_uplink = newWakeup;
					Debug_printf("Uplink every %d wakeups\r\n", wakeups_per_uplink);
					return SHELL_EXECSTATUS_OK_NO_FREE;
				}
			}
		}
		
		

		
	}
	
	cli_wakeup_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_gain_help()
{
	Debug_printf("Usage: gain set [gain]\r\n");
	await_uart_tx();
	Debug_printf("\tSets the antenna gain to [gain] dB\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: gain show\r\n");
	await_uart_tx();
	Debug_printf("\tPrints the current Antenna Gain value\r\n");
	await_uart_tx();
}

eExecStatus cli_gain( int argc, char *argv[], char **ppcStringReply )
{	
	int16_t newGain = 0;
	if(argc == 1)
	{
		if(!strcmp(argv[0],"show"))
		{
			Debug_printf("Antenna Gain:%d.%ddB\r\n", lora_get_antenna_gain()/10, abs(lora_get_antenna_gain()%10));
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}		
	}
	
	if(argc == 2)
	{	
		if(!strcmp(argv[0], "set"))
		{
			newGain = atoi(argv[1]);
			
			Debug_printf("Input %d\r\n", newGain);

			lora_set_antenna_gain(newGain);
			Debug_printf("Antenna Gain set to %d.%ddB\r\n", lora_get_antenna_gain()/10, abs(lora_get_antenna_gain()%10));
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
	}
	
	cli_gain_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_rejoin_help()
{
	Debug_printf("Usage: rejoin [interval]\r\n");
	await_uart_tx();
	Debug_printf("\tRejoin the LoRa network every [interval] hours\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: rejoin show\r\n");
	await_uart_tx();
	Debug_printf("\tPrints the current rejoin interval in hours\r\n");
	await_uart_tx();
}

eExecStatus cli_rejoin( int argc, char *argv[], char **ppcStringReply )
{	
	uint32_t newRejoin = 0;
	if(argc == 1)
	{
		if(!strcmp(argv[0],"show"))
		{
			if(lora_hours_until_rejoin == LORA_REJOIN_LOCKOUT_VALUE)
			{
				Debug_printf("Rejoin Disabled\r\n");
			}
			else
			{
				Debug_printf("Rejoin every %d hours\r\n", lora_hours_until_rejoin);
			}
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		
		if(!strcmp(argv[0],"disable"))
		{
			Debug_printf("Rejoins disabled\r\n");
			lora_hours_until_rejoin = LORA_REJOIN_LOCKOUT_VALUE;
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		
		newRejoin = atoi(argv[0]);
		
		lora_hours_until_rejoin = newRejoin;
		
		if(lora_hours_until_rejoin < MIN_HOURS_TO_REJOIN)
		{
			lora_hours_until_rejoin = MIN_HOURS_TO_REJOIN;
		}
		
		Debug_printf("Hours until rejoin set to %d\r\n", lora_hours_until_rejoin);
		await_uart_tx();
		//update second to value
		return SHELL_EXECSTATUS_OK_NO_FREE;
		
		
	}
	
	cli_rejoin_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_mode_help()
{
	Debug_printf("Usage: mode [mode_index]\r\n");
	await_uart_tx();
	Debug_printf("Valid options:\r\n");
	await_uart_tx();
	
	list_device_modes();
	
	Debug_printf("Usage: mode show\r\n");
	await_uart_tx();
	Debug_printf("\tPrints the current device mode\r\n");
	await_uart_tx();
}

eExecStatus cli_mode( int argc, char *argv[], char **ppcStringReply )
{
	int new_mode = 0;
	if(argc == 1)
	{
		if(!strcmp(argv[0],"show"))
		{
			Debug_printf("Current mode: %s\r\n", device.mode_name);
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		
		new_mode = atoi(argv[0]);
		
		//bounds checking
		if((new_mode > DEVICE_UNCONFIGURED) && (new_mode <= get_number_device_modes()))
		{
			device_mode = new_mode;
			
			update_device_behaviour();
			
			//we have a valid mode selection
			Debug_printf("Device mode set to: %s\r\n", device.mode_name);

			
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
	}
	cli_mode_help();
	//switch device mode here
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_time_help()
{
	Debug_printf("Usage: time hour [value]\r\n");
	Debug_printf("\tSet the hours on the device to [value], 24Hr format (0-23)\r\n");
	await_uart_tx();
	Debug_printf("Usage: time minute [value]\r\n");
	Debug_printf("\tSet the minutes on the device to [value]\r\n");
	await_uart_tx();
	Debug_printf("Usage: time second [value]\r\n");
	Debug_printf("\tSet the seconds on the device to [value]\r\n");
	await_uart_tx();
	Debug_printf("Usage: time show\r\n");
	Debug_printf("\tShows the current time on the device\r\n");
	await_uart_tx();
}

eExecStatus cli_time( int argc, char *argv[], char **ppcStringReply )
{
	uint8_t hours   = 0;
	uint8_t minutes = 0;
	uint8_t seconds = 0;
	
	int value = 0;
	if(argc == 2)
	{
		value = atoi(argv[1]);
		
		if(!strcmp(argv[0],"hour"))
		{
			minutes = HW_RTC_GetMinute();
			seconds = HW_RTC_GetSecond();
			HW_RTC_setTime(value,minutes,seconds);
			Debug_printf("Set\r\n");
			//update hour to value
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		if(!strcmp(argv[0],"minute"))
		{
			hours = HW_RTC_GetHour();
			seconds = HW_RTC_GetSecond();
			HW_RTC_setTime(hours,value,seconds);
			Debug_printf("Set\r\n");
			//update minute to value
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
		if(!strcmp(argv[0],"second"))
		{
			hours = HW_RTC_GetHour();
			minutes = HW_RTC_GetMinute();
			HW_RTC_setTime(hours,minutes,value);
			Debug_printf("Set\r\n");
			//update second to value
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}

	}
	
	if(argc == 1)
	{
		if(!strcmp(argv[0],"show"))
		{
			hours = HW_RTC_GetHour();
			minutes = HW_RTC_GetMinute();
			seconds = HW_RTC_GetSecond();
			//print the current time
			Debug_printf("Time is: %02d:%02d:%02d\r\n",hours, minutes, seconds);
			return SHELL_EXECSTATUS_OK_NO_FREE;
		}
	}
	
	cli_time_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_threshold( int argc, char *argv[], char **ppcStringReply )
{	
	device.cli_set_thresholds(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_device( int argc, char *argv[], char **ppcStringReply )
{	
	device.cli_device_specific(argc, argv);
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

eExecStatus cli_test( int argc, char *argv[], char **ppcStringReply )
{
	uint8_t flash_write_data[64];
	uint8_t flash_page0_backup[64];
	uint8_t flash_read_data[64] = {0};
	int matched = 1;
	int i;
	

	for(i=0;i<64;i++)
	{
		flash_write_data[i] = i;
	}
	
	if(argc >= 1)
	{
		/*
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"temp"))
		{
			
		}
		*/
		
		//Flash and I2C
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"flash"))
		{
			Debug_printf("\r\nFLASH & I2C\r\n");
			Debug_printf("Backing up page 0\r\n");
			flash_readblock(0,flash_page0_backup,64);
			
			
			Debug_printf("Writing Array to Flash Page 0\r\n");
			
			flash_writeblock(0, flash_write_data, 64);
			
			Debug_printf("Reading Array from Flash Page 0\r\n");
			
			flash_readblock(0,flash_read_data, 64);
			
			for(i=0; i<64; i++)
			{
				if(flash_read_data[i] != flash_write_data[i])
				{
					matched = 0;
				}
			}
			
			if(matched)
			{
				Debug_printf("Flash Check OK\r\n");
			}
			else
			{
				Debug_printf("Flash Check FAIL\r\n");
			}
			
			//clear the flash
			Debug_printf("Restoring Page 0\r\n");
			flash_writeblock(0,flash_page0_backup,64);
			
		}

		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"led"))
		{
			Debug_printf("\r\nLED\r\n");
			Debug_printf("Toggling RED led pin\r\nPress Any key to contimue\r\n");
			while(Debug_getRxDataLength() == 0)
			{
				LL_GPIO_TogglePin(LED_RED_PORT,LED_RED_PIN);
				delay_timeout_ms(100);
			}
			//get the character from the buffer, sets RxDataLength back to 0
			Debug_getChar();
			Debug_printf("Toggling ORANGE led pin\r\nPress Any key to contimue\r\n");
			while(Debug_getRxDataLength() == 0)
			{
				LL_GPIO_TogglePin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				delay_timeout_ms(100);
			}
			//get the character from the buffer, sets RxDataLength back to 0
			Debug_getChar();
			Debug_printf("Toggling GREEN led pin\r\nPress Any key to contimue\r\n");
			while(Debug_getRxDataLength() == 0)
			{
				LL_GPIO_TogglePin(LED_GREEN_PORT,LED_GREEN_PIN);
				delay_timeout_ms(100);
			}
			//get the character from the buffer, sets RxDataLength back to 0
			Debug_getChar();
			
			//turn all LED's back on
			LL_GPIO_SetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
			LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
			LL_GPIO_SetOutputPin(LED_RED_PORT,LED_RED_PIN);
		}
		
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"uart"))
		{
			Debug_printf("\r\nUART\r\n");
			Debug_printf("Sending String on peripheral UART\r\nPress any key to continue\r\n");
			modbus_init();
			
			i=-1;
			while(Debug_getRxDataLength() == 0)
			{
				i++;
				if(i > 9) i=0;
				modbus_sendChar('T');
				modbus_sendChar('E');
				modbus_sendChar('S');
				modbus_sendChar('T');
				modbus_sendChar(' ');
				modbus_sendChar(i+48);
				modbus_sendChar('\r');
				modbus_sendChar('\n');
				
				delay_timeout_ms(100);
			}
			
			//get the character from the buffer, sets RxDataLength back to 0
			Debug_getChar();
		}

		
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"count"))
		{
			Debug_printf("\r\nDIGITAL INPUTS\r\n");
			GPIO_InitTypeDef GPIO_InitStruct;
			/* Digital Input Initialisation */
			GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
			GPIO_InitStruct.Pull      = GPIO_NOPULL;
			GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
			//alternate function doesn't matter, because this is not configured for af.
		
			HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
			HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
			HW_GPIO_Init(COUNT3_PORT    , COUNT3_PIN    , &GPIO_InitStruct);	
			
			
			Debug_printf("Pin States\r\nPress any key to continue\r\n");
			while(Debug_getRxDataLength() == 0)
			{
				Debug_printf("                             \r");
				Debug_printf("C1 %s | C2 %s | C3 %s\r", (HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN))? "High":"Low ",
				                                          (HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN))? "High":"Low ",
				                                          (HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN))? "High":"Low ");
				delay_timeout_ms(1000);
				
			}
			Debug_getChar();
			Debug_printf("\r\n");
		}
		
				
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"adc"))
		{
			Debug_printf("\r\nADC\r\n");
			Debug_printf("Testing ADC, Supply voltage:%d mV\r\n",HW_GetBatteryLevel_mv());
		}
		
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"tx"))
		{
			Debug_printf("\r\nLORA\r\n");
			Debug_printf("Ensure that RFI is ready, then press any key to continue\r\n");
			while(Debug_getRxDataLength() == 0)
			{
					reset_watchdog();
			}
			Debug_getChar();
			Debug_printf("Sending Data over LoRa\r\n");
			LORA_Join();
		}
		
		if(!strcmp(argv[0],"all") || !strcmp(argv[0],"wdt"))
		{
			Debug_printf("\r\nWATCHDOG\r\n");
			Debug_printf("Testing WDT, Press any key to reset\r\n");
			await_uart_tx();
			
			while(Debug_getRxDataLength() == 0)
			{
					reset_watchdog();
			}
			
			while(1);
		}
		
		if(!strcmp(argv[0], "ext_adc"))
		{
			Debug_printf("\r\n External ADC\r\n");
			while(Debug_getRxDataLength() == 0)
			{
				Debug_printf("ADC:%04d mvV\r",read_adc()*3300/4095);
				delay_timeout_ms(100);
			}
		}
	}
	
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

static uint8_t axtoi(char input)
{
	//handle simplest case (number) first.
	if(input >= '0' && input <= '9')
	{
		return input - '0';
	}
	
	input &= ~0x20;
	
	if(input >= 'A' && input <='F')
	{
		return  10 + (input - 'A');
	}
	
	return 0;
}


//assume that input is zero-terminated, ascii encoded, hex
void process_hexstring(char* input, char *output, int length)
{
	//notice that we are processing ascii encoded hex, so each character we read
	//is 4 bits
	int i = 0;
	//we also make the assumption that there are no characters outside [0-9,A-F]
	while(*input && i < length*2)
	{
		output[i>>1] = output[i>>1] << 4;
		output[i>>1] += axtoi(*input);
		input++;
		i++;
	}
}

void cli_appeui_help()
{
	Debug_printf("Usage: appeui set <value>\r\n");
	Debug_printf("\tSets the appeui to <value>\r\n");
	Debug_printf("\t<value> should be a 16-Character HEX string (8Bytes)\r\n");
	Debug_printf("Usage: appeui get \r\n");
	Debug_printf("\tPrints the currently configured APPEUI\r\n");
	Debug_printf("Usage: appeui random\r\n");
	Debug_printf("\tSets a random appeui\r\n");
}

static void print_appeui()
{
	uint8_t *pt = lora_config_appeui_get();
	Debug_printf("EUID:\t%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
				pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7]);
	await_uart_tx();
}

eExecStatus cli_appeui( int argc, char *argv[], char **ppcStringReply )
{
	uint8_t i;
	uint8_t new_appeui[8] = LORAWAN_APPLICATION_EUI;
	if(argc < 1)
	{
		cli_appeui_help();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "get"))
	{
		print_appeui();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "default"))
	{
		lora_config_appeui_set(new_appeui);
		Debug_printf("APPEUI reset\r\n");
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "random"))
	{
		for(i=0; i<8; i++)
		{
			new_appeui[i] = rand() & 0xFF;
		}
		lora_config_appeui_set(new_appeui);
		print_appeui();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(argc < 2)
	{
		cli_appeui_help();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "set"))
	{
		process_hexstring(argv[1], (char*)new_appeui, 8);
		lora_config_appeui_set(new_appeui);
		uint8_t *pt = lora_config_appeui_get();
		Debug_printf("EUID:\t%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
					pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7]);
		await_uart_tx();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	cli_appeui_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

void cli_appkey_help()
{
	Debug_printf("Usage: appkey set <value>\r\n");
	Debug_printf("\tSets the appkey to <value>\r\n");
	Debug_printf("\t<value> should be a 32-Character HEX string (16Bytes)\r\n");
	Debug_printf("Usage: appkey get \r\n");
	Debug_printf("\tPrints the currently configured APPKEY\r\n");
	Debug_printf("Usage: appkey default\r\n");
	Debug_printf("\tResets the appkey to defualt\r\n");
	Debug_printf("Usage: appkey random\r\n");
	Debug_printf("\tSets a random appkey\r\n");
}

static void print_appkey()
{
	uint8_t *pt = lora_config_appkey_get();
	Debug_printf("APPKEY:\t%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
		pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7],
		pt[8], pt[9], pt[10], pt[11], pt[12], pt[13], pt[14], pt[15]);
	await_uart_tx();
}

eExecStatus cli_reboot( int argc, char *argv[], char **ppcStringReply )
{
	while(1);
}


eExecStatus cli_appkey( int argc, char *argv[], char **ppcStringReply )
{
	uint8_t i;
	uint8_t new_appkey[16] = LORAWAN_APPLICATION_KEY;
	if(argc < 1)
	{
		cli_appeui_help();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	
	if(!strcmp(argv[0], "get"))
	{
		print_appkey();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "default"))
	{
		lora_config_appkey_set(new_appkey);
		Debug_printf("APPKEY reset\r\n");
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "random"))
	{
		for(i=0; i<16; i++)
		{
			new_appkey[i] = rand() & 0xFF;
		}
		lora_config_appkey_set(new_appkey);
		print_appkey();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(!strcmp(argv[0], "set"))
	{
		process_hexstring(argv[1], (char*)new_appkey, 16);
		lora_config_appkey_set(new_appkey);
		print_appkey();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	cli_appkey_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}

static void cli_channel_help()
{
	Debug_printf("NOTE: CHANNEL CONFIG ONLY FOR AU915\r\n");
	Debug_printf("NOTE: CLI CANNOT HANDLE MORE THAN 8 EXTRA ARGUMENTS\r\n");
	Debug_printf("Usage: channel add <ch 1> ... <ch n>\r\n");
	Debug_printf("\tAdd specified channels to the preconfigured channel list\r\n");
	Debug_printf("\tReplace the Channel arguments with \"All\" to add all channels\r\n");
	await_uart_tx();
	Debug_printf("Usage: channel remove <ch 1> ... <ch n>\r\n");
	Debug_printf("\tRemove specified channels from the preconfigured channel list\r\n");
	Debug_printf("\tReplace the Channel arguments with \"All\" to remove all channels\r\n");
	await_uart_tx();
	Debug_printf("Usage: channel show\r\n");
	Debug_printf("\tShows the currently enabled channels\r\n");
	await_uart_tx();
}

eExecStatus cli_channel( int argc, char *argv[], char **ppcStringReply )
{
	await_uart_tx();
	uint8_t cli_channel_list[72] = {0};
	int i = 0;
	if(argc < 1)
	{
		cli_channel_help();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	//channel show
	if(!strcmp(argv[0], "show"))
	{
		//get the channel list as a printable string
		print_active_channels();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	if(argc < 2)
	{
		cli_channel_help();
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	
	//if the next argument is "all", then we should populate the list with all channels.
	//This is more conveniant than writing out the full list
	if(!strcmp(argv[1], "all"))
	{
		for(i=0;i<72;i++)
		{
			cli_channel_list[i] = i;
		}
		//73 to account for the -1 in the call to add/remove channels.
		argc = 73;
	}
	else
	{
		//truncate to 73 arguments maximum, to prevent array out of bounds access
		if(argc > 9)
		{
			argc = 9;
		}
		//form a list of channels. We know that we will have no more than 72 channels, so limit the array to that.
		//argv[0] is the command word. argv[1->73] will be channels
		for(i=1;i<argc; i++)
		{
			cli_channel_list[i-1] = atoi(argv[i]);
		}
	}
	//channel add
	if(!strcmp(argv[0], "add"))
	{
		Debug_printf("Adding Channels to list\r\n");
		//add specified channels to the list
		//-1 to account for the command name (add)
		add_channels(cli_channel_list, argc-1);
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	//channel remove
	if(!strcmp(argv[0], "remove"))
	{
		Debug_printf("Removing channels from list\r\n");
		//add specified channels to the list
		//-1 to account for the command name (remove)
		remove_channels(cli_channel_list, argc-1);
		return SHELL_EXECSTATUS_OK_NO_FREE;
	}
	
	
	cli_channel_help();
	return SHELL_EXECSTATUS_OK_NO_FREE;
}


