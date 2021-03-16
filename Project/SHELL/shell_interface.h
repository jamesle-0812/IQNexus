/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           


*/

#ifndef SHELL_INTERFACE_HEADER
#define SHELL_INTERFACE_HEADER
#include "debug_uart.h"

#define cli_uart_init()													Debug_init()
#define cli_output_data(data, length) 					Debug_AddToWriteBuffer(data, length)
#define bufferDataLength()                      Debug_getRxDataLength()
#define bufferGetFromFront()										Debug_getChar()
#define bufferAddDataToEnd()										Debug_AddToWriteBuffer(data, length)
#define uart_start_tx()													//no definition here, as the tx is started by writing to the buffer

#endif //SHELL_INTERFACE_HEADER
