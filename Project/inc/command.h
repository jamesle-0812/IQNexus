/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Cli driver header

	Maintainer: Shea Gosnell

*/
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMAND_H__
#define __COMMAND_H__

void process_command(char* command, int length);
void cmd_process_character(char c);

extern int pause_uplink;
	
#endif //__COMMAND_H__


