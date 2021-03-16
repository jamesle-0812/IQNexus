/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	Public I2C1 header for SenDeca project

	Maintainer: Shea Gosnell


*/



#ifndef I2C_HEADER_PUBLIC
#define I2C_HEADER_P
#include <stdint.h>
#include <stdbool.h>

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
 

 /********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
bool i2c1_init( void );
void i2c1_send(uint8_t slave_address, uint8_t *data, int data_length, int send_stop);
int i2c1_send_feedback(uint8_t slave_address, uint8_t *data, int data_length, int send_stop);
void i2c1_receive(uint8_t slave_address, uint8_t *data, int data_length, int send_stop);
int i2c1_receive_feedback(uint8_t slave_address, uint8_t *data, int data_length, int send_stop);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //I2C_HEADER_PUBLIC


