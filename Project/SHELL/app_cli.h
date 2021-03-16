/*
 * app_cli.h
 *
 *  Created on: 31/03/2010
 *      Author: alvaro
 */

#ifndef APP_CLI_H_
#define APP_CLI_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "app_cli.h"
#include "shell.h"

#define SHELL_NB_COMMANDS 35
/*!
 * The cmd status enum.
 */
typedef enum {
   SHELL_CMDSTATUS_FOUND,              // All commands.
   SHELL_CMDSTATUS_NOTFOUND,           // Cmd not found.
   SHELL_CMDSTATUS_PERMISSIONDENIED,   // Cmd not found.
} eCmdStatus;

//use this enum to restrict the options available on the CLI
typedef enum {
	cmd_modbus      = 0x00000001,
	cmd_count1      = 0x00000002,
	cmd_count2      = 0x00000004,
	cmd_count3      = 0x00000008,
	cmd_count_burst = 0x00000010,
	cmd_count_leak  = 0x00000020,
	cmd_thresholds  = 0x00000040,
	cmd_mux_adc		= 0x00000080,
}cli_commands_e;


/*!
 * The type of a command registration entry.
 */
typedef struct st_cmd_registration {
   const char *const pc_string_cmd_name;  // The name of the command
   pfShellCmd      pf_exec_cmd;          // Ptr on the exec function
   long   mod_rights_map;                // The module rights map for this command.
                                         // Each bit is assigned to a module.
                                         // Built with eModId enums.
}Cmd_registration;

extern Cmd_registration a_cmd_registration[];

/*
 * CLI function prototypes
 * Register with CLI by adding to -> a_cmd_registration
 */

eExecStatus cli_help     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_run      ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_show     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_led      ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_modbus   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_count    ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_flash    ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_uplink   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_wakeup   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_rejoin   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_mode     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_time     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_threshold( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_device   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_test     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_gain     ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_appkey   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_appeui   ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_channel  ( int argc, char *argv[], char **ppcStringReply );
eExecStatus cli_reboot   ( int argc, char *argv[], char **ppcStringReply );


#endif /* APP_CLI_H_ */
