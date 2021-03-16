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

#ifndef COUNTER_HEADER
#define COUNTER_HEADER
#include <stdint.h>
#include <stdbool.h>
#include "lora_sensum.h"

#define MIN_DEBOUNCE_INTERVAL 10
#define MIN_LEAK_INTERVAL     10
#define MIN_MAX_FLOW          1
#define BURST_HOURS_MIN       1

typedef enum
{
	at_midnight = 0,  //12=24=0
	at_1,             //1=13=1
	at_2,             //2=14=2
	at_3,             //3=15=3
	at_4,             //4=16=4
	at_5,             //5=17=5
	at_6,             //6=18=6
	at_7,             //7=19=7
	at_8,             //8=20=8
	at_9,             //9=21=9
	at_10,            //10=22=10
	at_11,            //11=23=11
	at_midday = 0,    //12=24=0
}counter_times_e;

typedef struct
{
	uint8_t  input_1_state :1;
	uint8_t  input_2_state :1;
	uint8_t  input_3_state :1;

	uint8_t  input_1_changed :1;
	uint8_t  input_2_changed :1;
	uint8_t  input_3_changed :1;
}threeEdgeData_t;

/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/
extern volatile uint8_t  wake_via_counters;
extern uint32_t debounce_interval;
extern uint32_t leak_interval;
extern uint16_t maximum_hourly_flow;
extern uint16_t  consecutive_burst_hours;


/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
 
 
void init_single_counter(void);
void init_two_counter_tamper(void);
void init_three_counter(void);
void init_three_edge_alarm(void);
void init_two_count_adc(void);
void cli_count_implementation(int argc,   char *argv[]);
void two_counter_adc_uplink(void);
void threeEdge_alarm(void);
void three_counter_uplink( void );
void two_counter_uplink( void );
void tamper_detect(void);
void single_counter_on_count(void);
void single_counter_on_hour(void);
void two_counter_alarms(void);
void two_counter_synchronised_uplink(void);
void two_counter_on_hour(void);
void single_counter_alarms(void);
void single_counter_simple_uplink(void);
void single_counter_uplink(void);
void single_counter_uplink_alarm( void );
void threeEdge_uplink( void );
void record_hourly_count( void );
void record_hourly_count2( void );
void init_tamper( void );

void counterDownlinks(uint8_t *buffer, uint8_t size);
void save_counter_config(void);
void load_counter_config(void);
void save_counter_data(void);
void load_counter_data(void);

void init_three_mux_inputs(void);
void three_mux_uplink(void);
void three_mux_onDownlink(uint8_t *buffer, uint8_t size);
void three_mux_save_config(void);
void three_mux_load_config(void);
void three_mux_load_counter_data (void);
void three_mux_save_counter_data (void);


#endif //COUNTER_HEADER
