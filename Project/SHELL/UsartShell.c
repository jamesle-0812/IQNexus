/* This source file is part of the ATMEL AVR32-SoftwareFramework-AT32UC3-1.5.0 Release */

/*This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file *********************************************************************
 *
 * \brief Control Panel COM1 shell module.
 *
 * This module manages a command shell on COM1.
 *
 * - Compiler:           GNU GCC for AVR32
 * - Supported devices:  All AVR32 devices.
 *
 * \author               Atmel Corporation: http://www.atmel.com \n
 *                       Support and FAQ: http://support.atmel.no/
 *
 *****************************************************************************/

/* Copyright (c) 2009 Atmel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an Atmel
 * AVR product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE
 *
 */

/*!
 * Brief description of the module.
 * This module is in charge of a command line shell on COM1.
 *
 * Detailled description of the module.
 * Creation of one task that is in charge of getting a command line from COM1,
 * launching the execution of the command and giving feedback of this execution
 * to COM1.
 *
 */

/* Scheduler include files. */
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Demo program include files. */
#include "shell.h"
#include "UsartShell.h"
#include "shell_interface.h"

//_____ M A C R O S ________________________________________________________

// String messages


#define COM1SHELL_MSG_LINETOOLONG       "\r\nLine too long!\r\n"

#define MAX_FILE_PATH_LENGTH  	30

/*! The max length of a cmd name. */
#define MAX_CMD_LEN                 32
/*! The max length of a cmd line. */
#define COM1SHELL_MAX_CMDLINE_LEN   (MAX_CMD_LEN + 2 * MAX_FILE_PATH_LENGTH)

/* We should find that each character can be queued for Tx immediately and we
don't have to block to send. */
#define COM1SHELL_NO_BLOCK		    ( ( long ) 0 )

/* The Rx task will block on the Rx queue for a 10ms. */
#define COM1SHELL_RX_BLOCK_TIME		    ( ( long ) 0x0A )
#define COM1SHELL_RX_SHORT_BLOCK_TIME	    ( ( long ) 1 )


//! Messages printed on the COM1 port.
#define COM1SHELL_PDU_BANNER  "\r\n SonaSafe PDU Shell\r\n"
#define COM1SHELL_APT_BANNER  "\r\n SonaSafe Access Point\r\n"
#define COM1SHELL_ACC_BANNER  "\r\n SonaSafe Access Control\r\n"
#define COM1SHELL_SPC_BANNER  "\r\n SonaSafe Speed Control\r\n"
#define COM1SHELL_SNI_BANNER  "\r\n SonaSafe Sniffer Mode\r\n"
#define COM1SHELL_NO_BATT	  "\r\n WARNING: No battery Management\r\n"
#define COM1SHELL_NO_WDT	  "\r\n WARNING: No Watchdog Enabled\r\n"
#define COM1SHELL_MSG_PROMPT  "$>"



/*! The cmd line string. */
static char acStringCmd[COM1SHELL_MAX_CMDLINE_LEN+1];

static char prvGetCmdLine( void );                    	// Forward.


/*!
 * Start the COM1 shell module.
 * \param uxPriority The priority base of the COM1 shell tasks.
 */
void cli_init( void  )
{
   /*
    * Initialize CLI
    * Pulling method to receive data from usart
    * The PDCA will transfer data to the usart
   */
	//initialise the uart
	cli_uart_init();
}



bool vComShellTask( void )
{
	char *pcStringReply = NULL;
	static uint8_t banner = true;

	if (banner)
	{
		//banner function goes here

		banner = false;
	}

	// 1) Get the command.
	if (prvGetCmdLine() == 0)
	{
		return false;
	}

	// 2) Execute the command.
	Shell_exec(acStringCmd, SYS_MODID_COM1SHELL, &pcStringReply);
	
	return true;

}
/*-----------------------------------------------------------*/



/*!
 * \brief Get a command line.
 *
 * \return Command line length.
 */
static char prvGetCmdLine( void )
{
   char c;
   static char  idx = 0;
   static uint8_t print_prompt = true;

	if ( print_prompt == true )
	{
	   vcom1shell_PrintMsg( (char *)COM1SHELL_MSG_PROMPT );
	   print_prompt = false;
	   idx = 0;
	}

	// Getting data from circular buffer
	while ( bufferDataLength() > 0 )
	{
		c = bufferGetFromFront();
		switch (c)
		{
			case LF:
			   vcom1shell_PutChar(CR);  // Add CR
			   vcom1shell_PutChar(c);   // LF
			   acStringCmd[idx] = '\0';  // Add NUL char
			   print_prompt = true;
			   return(idx);
			 case CR:
			   vcom1shell_PutChar(c);   // CR
			   vcom1shell_PutChar(LF);  // Add CR
			   acStringCmd[idx] = '\0';  // Add NUL char
			   print_prompt = true;
			   return(idx);
			case ESCAPE_CHAR:
				//TODO: handle escape character
				break;
			 case ABORT_CHAR:             // ^c abort cmd
			   idx = 0;
			   vcom1shell_PutChar(LF);           // Add LF
			   vcom1shell_PrintMsg( (char *)COM1SHELL_MSG_PROMPT );
			   break;
			case BKSPACE_CHAR:           // Backspace
			   if (idx > 0)
			   {
				  vcom1shell_PrintMsg( (char *)"\b \b" );
				  idx--;
			   }
			   break;
			default:
			   vcom1shell_PutChar(c);            // Echo
			   acStringCmd[idx++] = c;   // Append to cmd line
			   break;
		}
	}

	/*
	* Output data if any is available
	*/
	//already done by adding to the buffer
	uart_start_tx();
   //vcom1shell_PrintMsg( (char *)COM1SHELL_MSG_LINETOOLONG );
   return 0;
}


/*-----------------------------------------------------------*/

/*!
 * \brief Print a string to the COM1 port.
 */
void vcom1shell_PrintMsg(const char *pcString)
{
   unsigned short usMsgLen = strlen((const char * )pcString);

   if(usMsgLen==0)
      return;

   cli_output_data( (char*)pcString, usMsgLen);

}
/*-----------------------------------------------------------*/

/*!
 * \brief Put a char to the COM1 port.
 */
void vcom1shell_PutChar( char cByte)
{
   cli_output_data( &cByte, 1);
}
/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/
//
///*!
// * \brief Mount default local drive 'a:'
// */
//void v_com1shell_mount_local_drive( void )
//{
//   fsaccess_take_mutex();
//   nav_select( sCom1ShellNavId );   // Select the COM1SHELL navigator.
//   nav_drive_set(LUN_ID_AT45DBX_MEM);
//   nav_partition_mount();
//   fsaccess_give_mutex();
//}
