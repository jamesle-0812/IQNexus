
#include <string.h>
#include "debug_uart.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#define NUM_COMMANDS 10
#define MAX_NUM_ARGS 4
#define MAX_ARG_LENGTH 10
#define MAX_COMMAND_LENGTH 30

void cli_unlock(char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc);
void cli_null  (char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc);
void cli_led   (char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc);

typedef struct
{
	const char* cmd_name;
	void (*cmd_ptr) (char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc);
} command_registration_t;


command_registration_t cmd_index[NUM_COMMANDS+1] =
{
	{"CLI", cli_unlock},
	{"LED", cli_led},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NULL", cli_null},
	{"NO COMMAND", cli_null},
};

void cli_null(char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc)
{
	//Debug_printf("\r\n>");
}

void cli_led(char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc)
{
			//turn on the LEDs
		
		
		
	int new_state = 0;
	if(argc == 2)
	{
		if(!strcmp(argv[1], "ON"))
		{
			new_state = 1;
		}
		else if(!strcmp(argv[1], "OFF"))
		{
			new_state = 0;
		}
		else if(!strcmp(argv[1], "TOGGLE"))
		{
			new_state = 2;
		}
		else
		{
			Debug_printf("Usage: LED [RED|ORANGE|GREEN] [ON|OFF|TOGGLE]\r\n");
			return;
		}
		//red led
		if(!strcmp(argv[0], "GREEN"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(GPIOB,LL_GPIO_PIN_13);
			else if (new_state ==2)
				LL_GPIO_TogglePin(GPIOB,LL_GPIO_PIN_13);
			else
				LL_GPIO_ResetOutputPin(GPIOB,LL_GPIO_PIN_13);
		}
		//orange led
		if(!strcmp(argv[0], "ORANGE"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(GPIOB,LL_GPIO_PIN_14);
						else if (new_state ==2)
				LL_GPIO_TogglePin(GPIOB,LL_GPIO_PIN_14);
			else
				LL_GPIO_ResetOutputPin(GPIOB,LL_GPIO_PIN_14);
		}
		//green led
		if(!strcmp(argv[0], "RED"))
		{
			if(new_state == 1)
				LL_GPIO_SetOutputPin(GPIOB,LL_GPIO_PIN_15);
			else if (new_state ==2)
				LL_GPIO_TogglePin(GPIOB,LL_GPIO_PIN_15);
			else
				LL_GPIO_ResetOutputPin(GPIOB,LL_GPIO_PIN_15);
		}
	}
	else
	{
		Debug_printf("Usage: LED [RED|ORANGE|GREEN] [ON|OFF]\r\n");
	}
}

int pause_uplink = 0;
void cli_unlock(char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH], int argc)
{
	Debug_printf("Pause uplink ");
	
	if(argc > 0)
	{
		if(!strcmp(argv[0], "ON"))
		{
			//remember strcmp returns 0 if they match
			Debug_printf("Enabled\r\n");
			pause_uplink = 1;
		}
		else if(!strcmp(argv[0], "OFF"))
		{
			Debug_printf("Disabled\r\n");
			pause_uplink = 0;
		}
		else
		{
				Debug_printf("Not changed\r\n");
		}
	}
	else
	{
			Debug_printf("Not changed\r\n");
	}
	
	return;
}

//in order to process a line of command, we need to do a string compare
//and find where in the index the command is
void process_command(char* command, int length)
{
	//variables for parsing commands
	int cmd_idx = 0;
	int cmd_str_idx = 0;
	char cmd_str[length];
	
	//arguments to pass to cli functions
	char argv[MAX_NUM_ARGS][MAX_ARG_LENGTH];
	int argc = 0;
	int argp = 0;
	
	
	//first find the first ' ' character
	while(command[cmd_idx] != ' ')
	{
		cmd_idx ++;
		if(cmd_idx == length)
		{
			//Debug_printf("Reached length\r\n");
			break;
		}
		if(command[cmd_idx] == 0)
		{
			//Debug_printf("Reached Terminator\r\n");
			break;
		}
	}
	
	//at this point we have the index of the first space.
	//now strip out the first 'word' and compare to our array of commands
	while(cmd_str_idx < cmd_idx)
	{
		cmd_str[cmd_str_idx] = command[cmd_str_idx];
		cmd_str_idx++;
	}
	//ensure that the null terminator is in place
	cmd_str[cmd_str_idx] = 0;
	
	//at this point we have the command string, ready for comparison
	//iterate through the array, and find if any strings match
	for(cmd_idx = 0; cmd_idx < NUM_COMMANDS; cmd_idx++)
	{
		if(!strcmp(cmd_str, cmd_index[cmd_idx].cmd_name))
		{
			//strcmp returns 0 if the srtings match
			break;
		}
	}
	
	//cmd_idx now has the index of the command to execute, or NUM_COMMANDS
	
	//now lets separate the args into the vector
	//terminating conditions are:
		//end of input string (0) character
		//exceeded input string count
		//exceeded MAX_NUM_ARGS
	while(command[cmd_str_idx] != 0 && cmd_str_idx < length && argc <= MAX_NUM_ARGS)
	{
		//check if we are going to overflow the argument buffer
		if(argp == MAX_ARG_LENGTH)
		{
			Debug_printf("Overflowed argument\r\n\tARGC:%d\r\n", argc);
			//because of the overflow, we will treat this as a bad command, and not execute
			cmd_idx = NUM_COMMANDS;
			argc = 0;
			break;
		}
		//commands are space delimited
		if(command[cmd_str_idx] == ' ')
		{
			//if we were writing a command to the argv, ensure that is is null terminated
			if(argc > 0)
			{
				argv[argc-1][argp] = 0;
			}
			argp = 0;
			cmd_str_idx++;
			continue;
		}
		
		//if we are at the start of an aggument string, then we should increment the arg count.
		if(argp == 0)
		{
			argc ++;
		}
		
		//copy the character from the input to the argv
		argv[argc-1][argp] = command[cmd_str_idx];
		
		//increment the character pointers
		cmd_str_idx++;
		argp++;
	}
	
	//if we got to the end of the string, or a null terminator, we would not have written that
	//to the argv. Do that now
	if(argp > 0)
	{
		argv[argc-1][argp] = 0;
	}
	
	//at this point we can execute the command
	cmd_index[cmd_idx].cmd_ptr(argv, argc);
	
	if(pause_uplink)
		Debug_printf("\r\n>");
	
	return;
}

void cmd_process_character(char c)
{
		static char command[MAX_COMMAND_LENGTH];
		static int command_index = 0;
		//Debug_printf("\t\tReading Character 0x%02x\r\n", rx_buffer[rx_buffer_read_pos]);
		//filter out unwanted characters here (e.g. \n)
		switch(c)
		{
			case '\n':
			case '\t':
				//these are no action characters
				break;
			case '\b':
			{
				//for a bacspace, we want to remove the last character from the buffer
				command[command_index] = 0;
				//but we do not want to move the pointer back past the beginning of the array
				if(command_index > 0)
				{
					command_index --;
					//if we are in command mode, we can echo characters to the cli
					if(pause_uplink)
					{
						Debug_printf("\b \b");
					}
				}
				break;
			}
			default:
			{	
				//convert lower case characters to upper case
				if(c >= 'a' && c <= 'z')
				{
					c -= 'a'-'A';
				}
				
				//if we are in command mode, we can echo characters to the cli
				if(pause_uplink)
				{
					Debug_printf("%c",c);
				}
				
				//if the last character is a \r, we have a complete line
				if(c == '\r')
				{
					//attatch the null terminator to the string
					command[command_index] = 0;
					//process the command
					process_command(command, command_index);
					//-1 because we increment after this conditional block
					command_index = -1;

				}
				//all other characters are written to the command buffer
				command[command_index] = c;
				command_index++;
				if(command_index == MAX_COMMAND_LENGTH)
				{
					Debug_printf("Max Command Length exceeded\r\n");
					//zero here, as we are after the increment
					command_index = 0;
				}
				break;
			}
		}
}


