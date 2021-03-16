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

#include "hw.h"
#include "debug_uart.h"
#include "cli_led.h"
#include "global.h"

void cli_led_help()
{
	Debug_printf("Usage: led [red|orange|green|all] [on|off|toggle]\r\n");
	return;
}

void cli_led_implementation(int argc,   char *argv[])
{
			//turn on the LEDs
	int new_state = 0;
	if(argc == 2)
	{
		if(!strcmp(argv[1], "on"))
		{
			new_state = 1;
		}
		else if(!strcmp(argv[1], "off"))
		{
			new_state = 0;
		}
		else if(!strcmp(argv[1], "toggle"))
		{
			new_state = 2;
		}
		else
		{
			cli_led_help();
			return;
		}
		
		if(!strcmp(argv[0],"all"))
		{
			if(new_state == 1)
			{
				LL_GPIO_SetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
				LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				LL_GPIO_SetOutputPin(LED_RED_PORT,LED_RED_PIN);
			}
			else if (new_state ==2)
			{
				LL_GPIO_TogglePin(LED_GREEN_PORT,LED_GREEN_PIN);
				LL_GPIO_TogglePin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				LL_GPIO_TogglePin(LED_RED_PORT,LED_RED_PIN);
			}
			else
			{
				LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
				LL_GPIO_ResetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
			}
		}
		//red led
		else if(!strcmp(argv[0], "green"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
			else if (new_state ==2)
				LL_GPIO_TogglePin(LED_GREEN_PORT,LED_GREEN_PIN);
			else
				LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
		}
		//orange led
		else if(!strcmp(argv[0], "orange"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
						else if (new_state ==2)
				LL_GPIO_TogglePin(LED_ORANGE_PORT,LED_ORANGE_PIN);
			else
				LL_GPIO_ResetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
		}
		//green led
		else if(!strcmp(argv[0], "red"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(LED_RED_PORT,LED_RED_PIN);
			else if (new_state ==2)
				LL_GPIO_TogglePin(LED_RED_PORT,LED_RED_PIN);
			else
				LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
		}
		else
		{
			cli_led_help();
		}
	}
	else
	{
		cli_led_help();
	}
}


