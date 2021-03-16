/* This source file is part of the ATMEL AVR32-SoftwareFramework-AT32UC3-1.5.0 Release */

/*This file has been prepared for Doxygen automatic documentation generation.*/
/*! \file *********************************************************************
 *
 * \brief AVR32 UC3 Control Panel Command Exec module.
 *
 * This module provides a Command execution service.
 *
 *
 * - Compiler:           IAR EWAVR32 and GNU GCC for AVR32
 * - Supported devices:  All AVR32 devices can be used.
 *                       The example is written for UC3 and EVK1100.
 * - AppNote:
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
 * This module is a central point for commands execution.
 *
 * Detailled description of the module.
 * It proposes only one interface function, Shell_exec(). This function parses
 * a command line string to:
 *    1) identify the command
 *    2) verify that the caller has the right to execute the command
 *    3) tokenize all arguments of the command
 * If the command has been found, and the caller has the right to execute it,
 * the command is executed. The command execution is managed by the target module.
 * Target modules are: sensor, actuator, filesys, supervisor.
 *
 * The input command line string must have the following format:
 * cmd [arg[=val]], 5 (arg,val) maximum.
 */



#include "stdio.h"
#include "stdlib.h"
#include <string.h>
#include "shell.h"
#include "UsartShell.h"
#include "app_cli.h"

//_____ M A C R O S ________________________________________________________


/*! Max number of tokens */
#define SHELL_MAX_NBTOKEN       8

/*! First file system cmd idx in the a_cmd_registration[] array. */
#define SHELL_FS_FIRSTCMDIDX    9

/*! Last file system cmd idx in the a_cmd_registration[] array. */
#define SHELL_FS_LASTCMDIDX     29


////_____ COMMAND FUNCTION PROTOTYPES______________________________________________
//
//
//static eExecStatus e_Shell_help( int ac, char *av[],
//                                 char **ppcStringReply ); // FORWARD
//
//eExecStatus e_syscmds_version( int ac, char *av[],
//                               char **ppcStringReply );






/*!
 *  String messages.
 */
/*! Error msg upon command not found. */
const char *const SHELL_MSG_CMDNOTFOUND      = (char *)"Error"CRLF"Command not found"CRLF;
/*! Error msg upon syntax error. */
const char *const SHELL_MSG_SYNTAXERROR      = (char *)"Error"CRLF"Syntax error"CRLF;
/*! Error msg upon command execution permission denied. */
const char *const SHELL_MSG_PERMISSIONDENIED = (char *)"Error"CRLF"Permission denied"CRLF;

const char *const SHELL_MSG_HELP = (char *)"Commands summary"CRLF"help"CRLF"!!: execute the previous command"CRLF;

/*! Error msg if !! is executed and there is no previous comand. */
const char *const SHELL_MSG_NOHISTORY = (char *)"Error"CRLF"No previous command"CRLF;

/*! Error msg upon help syntax error. */
const char *const SHELL_ERRMSG_HELP_SYNTAXERROR       = (char *)"Error"CRLF"Usage: help [sensor,actuator,sys,fs]"CRLF;

/*! Previous cmd index. */
static long   PrevCmdIdx = -1;
static long   PrevCmdAc;
char        *PrevCmdAv[SHELL_MAX_NBTOKEN];

static eCmdStatus prvCmdIdentify_Tokenize( char *pcStringCmd,
                                           eModId xModId,
                                           long *ac,
                                           char **av,
                                           long *pCmdIdx ); // FORWARD


eExecStatus check_passwd ( int argc,   char *argv[],
						    char **ppcStringReply ); //FORWARD


//_____ D E C L A R A T I O N S ____________________________________________

/*!
 *  \brief a command line executor.
 *
 *  \param pcStringCmd    Input. The cmd line to execute. NULL-terminated string.
 *                        Format: cmd [arg] [arg=val], with 6 (arg,val) maximum.
 *                        WARNING: this string will be modified.
 *  \param xModId         Input. The module that is calling this exe function.
 *  \param FsNavId        Input. The file system navigator id to use if the cmd
 *                        is a file system command.
 *  \param ppcStringReply Output. The caller must free this string (by calling vportFree())
 *                        only if it is non-NULL and the returned status is OK.
 *
 *  \return the status of the command execution.
 */
eExecStatus Shell_exec( char *pcStringCmd,
                        eModId xModId,
                        char **ppcStringReply)
{
   eCmdStatus        xCmdStatus;
   long     ac;
   char   *av[SHELL_MAX_NBTOKEN];
   long     CmdIdx;
   int               i;
   eExecStatus       xRet;


   // 1) Identify the command and tokenize the rest of the command line.
   xCmdStatus = prvCmdIdentify_Tokenize( pcStringCmd, xModId, &ac, av, &CmdIdx );
   if(SHELL_CMDSTATUS_NOTFOUND == xCmdStatus)
   {   // Command not found.
      if(ppcStringReply != NULL)
      {
         *ppcStringReply = (char *)SHELL_MSG_CMDNOTFOUND;
      }
      pcStringCmd[0] = '\0';
      //print_prompt = true;
      return(SHELL_EXECSTATUS_KO);
   }
   else if(SHELL_CMDSTATUS_PERMISSIONDENIED == xCmdStatus)
   {   // Permission denied.
      if(ppcStringReply != NULL)
      {
         *ppcStringReply = (char *)SHELL_MSG_PERMISSIONDENIED;
      }
      pcStringCmd[0] = '\0';
      //print_prompt = true;
      return(SHELL_EXECSTATUS_KO);
   }


   /* Special case for the !! command. Only for the SYS_MODID_COM1SHELL module,
      cf. a_cmd_registration[]. */
   if( 0 == CmdIdx ) // !! command
   {
      CmdIdx = PrevCmdIdx; // Execute the previous cmd.
      if( -1 == CmdIdx )
      {   // No previous command.
         if(ppcStringReply != NULL)
         {
            *ppcStringReply = (char *)SHELL_MSG_NOHISTORY;
         }
         //pcStringCmd[0] = '\0';
         //print_prompt = true;
         //return(SHELL_EXECSTATUS_KO);
      }
      ac = PrevCmdAc;      // NOTE: we take a shortcut here: we suppose the caller
                           // keeps the same pcStringCmd from one call to another.
      for(i=0; i<ac; i++)   av[i] = PrevCmdAv[i];
   }
   else
   {
      PrevCmdIdx = CmdIdx; // Save the cmd (used by the !! cmd).
      PrevCmdAc = ac;
      for(i=0; i<ac; i++)   PrevCmdAv[i] = av[i];
   }

   // 2) Execute the command.
   if(ppcStringReply != NULL)
      *ppcStringReply = NULL;

	   xRet = a_cmd_registration[CmdIdx].pf_exec_cmd( ac, av, ppcStringReply ); // I've taken off unused parameters AJ
	   pcStringCmd[0] = '\0';

   //print_prompt = true;
   return( xRet );
}


/*!
 *  \brief Identify the cmd and tokenize the rest of the command line string.
 *
 *  \param pcStringCmd    Input. The cmd line to parse. NULL-terminated string.
 *                        Format: cmd [arg] [arg=val], with 6 (arg,val) maximum.
 *                        WARNING: this string will be modified.
 *  \param xModId         Input. The module that is requesting the exe of this cmd line.
 *  \param ac             Output. Argument count, in [0,SHELL_MAX_NBTOKEN].
 *  \param av             Output. Argument vector, made of SHELL_MAX_NBTOKEN string ptrs maximum.
 *  \param pCmdIdx        Output. The index of the command, in one of the cmd array.
 *
 *  \return the command status(eCmdStatus), indicating the Identify & Tokenize status.
 */
static eCmdStatus prvCmdIdentify_Tokenize( char *pcStringCmd, eModId xModId,
                                           long *ac, char **av,
                                           long *pCmdIdx )
{
   char   *pcStringPtr = pcStringCmd;
   size_t            token_len, parsed_len, tempo_len;
   long     cmd_len = strlen( (char *)pcStringCmd );

   /*
    * 1) Identify the command.
    */
   //    i) skip space.
   pcStringPtr += parsed_len = strspn((char *)pcStringCmd, " ");
   //    ii) Get the len of the cmd name (i.e. the token we're on).
   // NOTE: a cmd name is delimited by: {space,\0}.
   parsed_len += token_len = strcspn((char *)pcStringPtr," ");
   // Insert a \0 on the space or on \0.
   pcStringPtr[token_len] = '\0';

   //    iii) Spot the cmd in the cmd registration array.
   for(*pCmdIdx = 0; *pCmdIdx < SHELL_NB_COMMANDS; (*pCmdIdx)++)
   {   // Search in the regular commands array.
      if(0 == strcmp( (char *)pcStringPtr,
                      (char *)a_cmd_registration[*pCmdIdx].pc_string_cmd_name ) )
         break;
   }
   if(SHELL_NB_COMMANDS == *pCmdIdx)
      return(SHELL_CMDSTATUS_NOTFOUND);   // Command not found.

   /*
    * 2) Check the module rights on this cmd.
    */
   if( 0 == ( xModId & a_cmd_registration[*pCmdIdx].mod_rights_map ) )
      return(SHELL_CMDSTATUS_PERMISSIONDENIED);

   pcStringPtr += token_len+1;

   /******************************************************************
    ***  			LOCAL COMMAND FUNCTION IMPLEMENTATIONS
    ******************************************************************/


   //
   //
   ///*!
   // *  \brief The help command: displays all available commands.
   // *         Format: help
   // *
   // *  \note  This function must be of the type pfShellCmd defined by the shell module.
   // *
   // *  \param xModId         Input. The module that is calling this exe function.
   // *  \param FsNavId        Input. The file system navigator id to use if the cmd
   // *                        is a file system command. NOT USED.
   // *  \param ac             Input. The argument counter. Not considered for this command.
   // *  \param av             Input. The argument vector. Not considered for this command.
   // *  \param ppcStringReply Input/Output. The response string.
   // *                        If Input is NULL, no response string will be output.
   // *                        Else a malloc for the response string is performed here;
   // *                        the caller must free this string.
   // *
   // *  \return the status of the command execution.
   // */
   //static eExecStatus e_Shell_help( int ac, char *av[],
   //                                 char **ppcStringReply )
   //{
   //   int             ShellMsgLen = strlen((char *)FSCMDS_MSG_HELP);
   //   eExecStatus     eRetStatus = SHELL_EXECSTATUS_KO;
   //
   //
   //   // 0) If the way to reply is unavailable, it's no use to continue.
   //   if( ppcStringReply == NULL )
   //      return( eRetStatus );
   //
   //   // i.e.: help, help sensor, help actuator, help sys, help fs.
   //   // Check the input: no argument.
   //   if( 0 != ac )
   //   {   // Syntax error.
   //      *ppcStringReply = (char *)SHELL_ERRMSG_HELP_SYNTAXERROR;
   //      return( eRetStatus );
   //   }
   //
   //   // Malloc the necessary memory to concatenate the messages.
   //   ShellMsgLen = strlen((char *)FSCMDS_MSG_HELP);
   //   *ppcStringReply = (char *)pvPortMalloc(ShellMsgLen);
   //
   //   // Build the message.
   //   strcpy((char *)*ppcStringReply, (char *)FSCMDS_MSG_HELP);
   //   //strcpy((char *)*ppcStringReply, (char *)SHELL_MSG_HELP);
   //   // NOTE: we know that the strings returned from the misc _help functions
   //   // are const. So we don't free these strings.
   //
   //   return(SHELL_EXECSTATUS_OK);
   //}
   //
   //
   //
   //eExecStatus e_syscmds_version( int ac, char *av[],
   //                               char **ppcStringReply )
   //{
   //	const char *const pcCtrlPanelVersion = "GCC "__VERSION__" "__DATE__" "__TIME__" BATCOM AVR32 UC3A0256 v1"CRLF; // 1.04.00
   //
   //   // 1) If the way to reply is unavailable, it's no use to continue.
   //   if( ppcStringReply == NULL )
   //      return( SHELL_EXECSTATUS_KO );
   //
   //   // 2) Perform the command.
   //   *ppcStringReply = (char *)pcCtrlPanelVersion;
   //
   //   return( SHELL_EXECSTATUS_OK_NO_FREE );
   //}



   parsed_len++;
   // pcStringPtr now points just after the \0 after the cmd name.

   /*
    * 3) Tokenize the rest of the string.
    */
   *ac = 0; // Init arg count to 0 (no args yet).
   while( ( parsed_len < cmd_len ) && (*pcStringPtr != '\0') && (*ac <= SHELL_MAX_NBTOKEN) )
   {
      /**
       ** Parse one arg.
       **/
      // Skip space.
      parsed_len += tempo_len = strspn((char *)pcStringPtr, " =");
      pcStringPtr += tempo_len;
      // pcStringPtr now points to the beginning of a token or at the end of the
      // cmd line.
      if(*pcStringPtr == '\0')
         break; // End of command line.
      // pcStringPtr now points to the beginning of a token.

      // Get the len of this token.
      if(*pcStringPtr == '"')
      {
         pcStringPtr++;   // Dump the ".
         parsed_len++;
         // Deal with string values, beginning with a " and ending with a ".
         // Strings can hold spaces, thus a space should not be considered a
         // delimiter when between "".
         // => a token is delimited by: {",\0}.
         token_len = strcspn((char *)pcStringPtr,"\"");
         // NOTE: token_len is == 0 if the string is just "". We consider this
         // a parameter (i.e. a string value is set to empty). So ac will be
         // incremented and the corresponding av will be set to an empty string.
      }
      else
      {
         // NOTE: a token is delimited by: {space,equal-sign,\0}.
         token_len = strcspn((char *)pcStringPtr," =");
      }

      // One more token.
      av[*ac] = pcStringPtr;
      (*ac)++;
      pcStringPtr += token_len;
      parsed_len += token_len;
      if(*pcStringPtr == '\0')
         break; // End of command line.
      else
         *pcStringPtr++ = '\0';
      // pcStringPtr now points on a space or on an equal-sign or at the end
      // of the cmd line.
   }

   return(SHELL_CMDSTATUS_FOUND);
}

