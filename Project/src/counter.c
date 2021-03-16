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


#include "counter.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "lora_sensum.h"
#include "watchdog.h"
#include "global.h"
#include "delays.h"
#include "flash_map.h"
#include "adc.h"
#include "timeServer.h"
#include "../SHELL/app_cli.h"
#include "radio_common.h"
																	
#define SINGLE_COUNT_DELTA_MAX 0x7FF

#ifdef DISABLE_COUNT_DEBUG
	#define dbg_print(...)
#else
	#define dbg_print(...) Debug_printf(__VA_ARGS__)
#endif

#define cli_print(...) Debug_printf(__VA_ARGS__)

void Count2LeakCheck(void);
void Count1LeakCheck(void);

volatile uint8_t wake_via_counters = 0;
volatile uint8_t leak_detected_1 = 1;
volatile uint8_t leak_detected_2 = 1;
volatile bool burst_1_this_hour = 0;
volatile uint8_t burst_detected_1 = 0;
volatile bool burst_2_this_hour = 0;
volatile uint8_t burst_detected_2 = 0;
volatile uint8_t tamper_detected = 0;
volatile uint8_t tamper_detected_delayed = 0;

static bool dir_enabled = false;
//dir default is count up when dir high. Inverted is count up when dir low.
static bool dir_inverted = false;

//configuration value for if internal pullups are enabled, default no
static bool internal_pullup_enabled = false;

/* Leave this here for a tidy-up
typedef struct
{
	uint32_t count;
	uint8_t
		burst_alarm_sent :1,
		burst_detected   :1,
		leak_detected    :1,
		input_state      :1,
		input_changed    :1;
	uint32_t hourly_counts[12];
}count_t;

count_t counts[3] = {{0}};
*/

//used to prevent spamming the alarm packets on every pulse
uint8_t burst1_alarm_sent = 0;
uint8_t burst2_alarm_sent = 0;
uint8_t tamper_alarm_sent = 0;

volatile uint32_t count1 = 0;
volatile uint32_t count2 = 0;
volatile uint32_t count3 = 0;

volatile threeEdgeData_t threeEdgeData = {0};

//store an entire days worth of count1, for history in uplink
//made this 25 for readability later
volatile uint32_t hourly_count1[12] = {0};
volatile uint32_t hourly_count2[12] = {0};

//default 1s debounce
uint32_t debounce_interval       = MIN_DEBOUNCE_INTERVAL;
uint32_t leak_interval           = MIN_LEAK_INTERVAL;
uint16_t maximum_hourly_flow     = MIN_MAX_FLOW;
uint16_t  consecutive_burst_hours = BURST_HOURS_MIN;

volatile bool edge1_debounce_timedout = true;
volatile bool edge2_debounce_timedout = true;
volatile bool edge3_debounce_timedout = true;

void Count1_debounce_timeout()
{
	//timeout before re-activation
	wake_via_counters = 1;

	//iff count1 is high, increment the count
	if(HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN))
	{
		//process incrementing count
		//now handle the count event
		if(dir_enabled == false)
		{
				count1++;
		}
		else
		{
			if(HW_GPIO_Read(COUNT1_DIR_PORT, COUNT1_DIR_PIN))
			{
				if(dir_inverted == false)
				{
					count1++;
				}
				else
				{
					count1--;
				}
			}
			else
			{
				if(dir_inverted == false)
				{
					count1--;
				}
				else
				{
					count1++;
				}
			}
		}
		dbg_print("\r\nCountOne:%d\r\n",count1);
	}
	else
	{
		dbg_print("\r\nCount One Ignored\r\n");
	}
}

void Count2_debounce_timeout()
{
	//timeout before re-activation
	wake_via_counters = 1;

	//iff count2 is high, increment the count
	if(HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN))
	{
		//process incrementing count
		//now handle the count event
		count2++;
		dbg_print("\r\nCount2:%d\r\n",count2);
	}
		else
	{
		dbg_print("\r\nCount 2 Ignored\r\n");
	}
}

void Count3_debounce_timeout()
{
	//timeout before re-activation
	wake_via_counters = 1;

	//iff count2 is high, increment the count
	if(HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN))
	{
		//process incrementing count
		//now handle the count event
		count3++;
		dbg_print("\r\nCount3:%d\r\n",count3);
	}
		else
	{
		dbg_print("\r\nCount 3 Ignored\r\n");
	}
}

void Edge1_debounce_timeout()
{
	edge1_debounce_timedout = true;
}
void Edge2_debounce_timeout()
{
	edge2_debounce_timedout = true;
}
void Edge3_debounce_timeout()
{
	edge3_debounce_timedout = true;
}

static TimerEvent_t count1_debounce_timer;
static TimerEvent_t count2_debounce_timer;
static TimerEvent_t count3_debounce_timer;
static TimerEvent_t edge1_debounce_timer;
static TimerEvent_t edge2_debounce_timer;
static TimerEvent_t edge3_debounce_timer;

void Count1IRQ()
{	

	if(count1_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Count1 debounce timer\r\n");
		TimerInit(&count1_debounce_timer, &Count1_debounce_timeout);
	}
	

	//reset the debounce timer
	TimerStop(&count1_debounce_timer);
	TimerSetValue(&count1_debounce_timer, debounce_interval);
	TimerStart(&count1_debounce_timer);
	
}

void Count1LeakCheck()
{
	//leak detection
	static TimerEvent_t count1LeakTimer;
	
	//if the timeout completed, then we know that we have no leak, and can clear the flag.
	if(timer_expired(&count1LeakTimer))
	{
		leak_detected_1 = 0;
	}
	
	start_timeout_timer(&count1LeakTimer, leak_interval);
}


void SingleCount1IRQ()
{
	Count1LeakCheck();
	Count1IRQ();
}



void Count2IRQ()
{	
	if(count2_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Count2 debounce timer\r\n");
		TimerInit(&count2_debounce_timer, &Count2_debounce_timeout);
	}
	
	//reset the debounce timer
	TimerStop(&count2_debounce_timer);
	TimerSetValue(&count2_debounce_timer, debounce_interval);
	TimerStart(&count2_debounce_timer);
}

void Count2LeakCheck()
{
	//leak detection
	static TimerEvent_t count2LeakTimer;
	
	//if the timeout completed, then we know that we have no leak, and can clear the flag.
	if(timer_expired(&count2LeakTimer))
	{
		leak_detected_2 = 0;
	}
	
	start_timeout_timer(&count2LeakTimer, leak_interval);
}

void SynchronisedCount2IRQ()
{

	Count2LeakCheck();
	Count2IRQ();
}



void Tamper_IRQ()
{
	tamper_detected = 1;
	tamper_detected_delayed = 1;
}

	
void Count3IRQ()
{
	if(count3_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Count3 debounce timer\r\n");
		TimerInit(&count3_debounce_timer, &Count3_debounce_timeout);
	}
	
	//reset the debounce timer
	TimerStop(&count3_debounce_timer);
	TimerSetValue(&count3_debounce_timer, debounce_interval);
	TimerStart(&count3_debounce_timer);
}

void Edge1IRQ()
{	
	if(edge1_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Edge1 debounce timer\r\n");
		TimerInit(&edge1_debounce_timer, &Edge1_debounce_timeout);
	}
	
	dbg_print("\r\nEdge1, %d\r\n",HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN));

	if(edge1_debounce_timedout)
	{
		//reset the debounce timer
		edge1_debounce_timedout = false;
		TimerStop(&edge1_debounce_timer);
		TimerSetValue(&edge1_debounce_timer, debounce_interval);
		TimerStart(&edge1_debounce_timer);
		
		
		wake_via_counters = 1;
		
		
		
		threeEdgeData.input_1_changed = 1;
		threeEdgeData.input_1_state = HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN);
		
		if(threeEdgeData.input_1_state)
		{
			//high level, just had a rising edge
			//log as a count
			Count1IRQ();
		}
		else
		{
			//low level, just had a falling edge

		}
	}
}

void Edge2IRQ()
{
	if(edge2_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Edge2 debounce timer\r\n");
		TimerInit(&edge2_debounce_timer, &Edge2_debounce_timeout);
	}
	
	dbg_print("\r\nEdge2, %d\r\n",HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN));
	
	if(edge2_debounce_timedout)
	{
		//reset the debounce timer
		edge2_debounce_timedout = false;
		TimerStop(&edge2_debounce_timer);
		TimerSetValue(&edge2_debounce_timer, debounce_interval);
		TimerStart(&edge2_debounce_timer);
		
		wake_via_counters = 1;

		threeEdgeData.input_2_changed = 1;
		threeEdgeData.input_2_state = HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN);
		
		if(threeEdgeData.input_2_state)
		{
			//high level, just had a rising edge
			//log as a count
			Count2IRQ();
		}
		else
		{
			//low level, just had a falling edge
		}
	}
}

void Edge3IRQ()
{
	if(edge3_debounce_timer.Callback == NULL)
	{
		dbg_print("Initialising Edge3 debounce timer\r\n");
		TimerInit(&edge3_debounce_timer, &Edge3_debounce_timeout);
	}
	
	dbg_print("\r\nEdge3, %d\r\n",HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN));
	
	if(edge3_debounce_timedout)
	{
		//reset the debounce timer
		edge3_debounce_timedout = false;
		TimerStop(&edge3_debounce_timer);
		TimerSetValue(&edge3_debounce_timer, debounce_interval);
		TimerStart(&edge3_debounce_timer);
		
		wake_via_counters = 1;
		
		threeEdgeData.input_3_changed = 1;
		threeEdgeData.input_3_state = HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN);
		
		if(threeEdgeData.input_3_state)
		{
			//high level, just had a rising edge
			//log as a count
			Count3IRQ();
		}
		else
		{
			//low level, just had a falling edge
		}
	}
}


void counterDownlinks(uint8_t *buffer, uint8_t size)
{
	counter_downlinks_t downlink = {0};
	int i;
	
	Debug_printf("Counter Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[COUNTER_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.params.downlink_type == downlink_type_count_params)
	{
		Debug_printf("Parameters Downlink\r\n");
		
		Debug_printf("Debounce      : %d00 ms\r\n",downlink.params.debounce);
		Debug_printf("Burst         : %d counts\r\n",downlink.params.burst);
		Debug_printf("Burst Hours   : %d hours\r\n",downlink.params.burst_hours);
		Debug_printf("Leak interval : %d ms\r\n",downlink.params.leak);
		
		if(downlink.params.debounce > 0)
		{
			Debug_printf("Updating Debounce\r\n");
			debounce_interval = downlink.params.debounce * 100;
		}
		else
		{
			Debug_printf("Not Updating Debounce\r\n");
		}
		
		if(downlink.params.burst > 0)
		{
			Debug_printf("Updating Burst\r\n");
			maximum_hourly_flow = downlink.params.burst;
		}
		else
		{
			Debug_printf("Not Updating Burst\r\n");
		}
		
		if(downlink.params.burst_hours >= BURST_HOURS_MIN)
		{
			Debug_printf("Updating Burst Hours\r\n");
			consecutive_burst_hours = BURST_HOURS_MIN;
		}
		else
		{
			Debug_printf("Not Updating Burst Hours\r\n");
		}
		
		if(downlink.params.leak >= MIN_LEAK_INTERVAL)
		{
			Debug_printf("Updating Leak\r\n");
			leak_interval = downlink.params.leak;
		}
		else
		{
			Debug_printf("Not Updating Leak\r\n");
		}
	}
	
	if(downlink.params.downlink_type == downlink_type_counts_1_2)
	{
		Debug_printf("Count 1 and 2 downlink\r\n");
		
		Debug_printf("Count 1 :%d\r\n",downlink.counts_1_2.count1);
		Debug_printf("Count 2 :%d\r\n",downlink.counts_1_2.count2);
		Debug_printf("Update Count 1 :%s\r\n", (downlink.counts_1_2.update_count1)? "YES":"NO");
		Debug_printf("Update Count 2 :%s\r\n", (downlink.counts_1_2.update_count2)? "YES":"NO");
		Debug_printf("Invert Dir: %s\r\n", (downlink.counts_1_2.invert_dir)? "YES":"NO");
		Debug_printf("Update Dir: %s\r\n", (downlink.counts_1_2.update_dir)? "YES":"NO");
		
		if(downlink.counts_1_2.update_count1)
		{
			Debug_printf("Updating Count 1\r\n");
			count1 = downlink.counts_1_2.count1;
			
			//update all history pages to equal count1.
			//this erases the history cleanly.
			//Required becuause changing the count invalidates the data.
			for(i=0; i<(sizeof(hourly_count1)/sizeof(uint32_t)); i++)
			{
				hourly_count1[i] = count1;
			}
		}
		else
		{
			Debug_printf("Not Updating Count 1\r\n");
		}
		
		if(downlink.counts_1_2.update_count2)
		{
			Debug_printf("Updating Count 2\r\n");
			count2 = downlink.counts_1_2.count2;
			
			//update all history pages to equal count1.
			//this erases the history cleanly.
			//Required becuause changing the count invalidates the data.
			for(i=0; i<(sizeof(hourly_count2)/sizeof(uint32_t)); i++)
			{
				hourly_count2[i] = count2;
			}
		}
		else
		{
			Debug_printf("Not Updating Count 2\r\n");
		}
		
		if(downlink.counts_1_2.update_dir)
		{
			Debug_printf("Updating Dir\r\n");
			dir_inverted = downlink.counts_1_2.invert_dir;
		}
		else
		{
			Debug_printf("Not Updating Dir\r\n");
		}
		
	}
	
	if(downlink.params.downlink_type == downlink_type_count3)
	{
		Debug_printf("Count 3 Downlink\r\n");
		
		Debug_printf("Count 3 :%d\r\n",downlink.count3.count3);
		Debug_printf("Update Count 3 :%s\r\n", (downlink.count3.update_count3)? "YES":"NO");
		
		if(downlink.count3.update_count3)
		{
			Debug_printf("Updating count3\r\n");
			count3 = downlink.count3.count3;
		}
	}
	
	//because one of config or data must have changed, we should save them
	save_config();
	device.save_data();
}

void two_counter_adc_uplink()
{
	(void) two_counter_adc_uplink;
	int i;
	static uint16_t count1History[4] = {0};
	static uint16_t count2History[4] = {0};
	static uint16_t adcHistory[4] = {0};
	
	//bump each history forward. [2]->[3], [1]->[2], etc
	for(i=3; i>0; i--)
	{
		count1History[i] = count1History[i-1];
		count2History[i] = count2History[i-1];
		adcHistory[i] = adcHistory[i-1];
	}
	
	//add the current readings to the history
	count1History[0] = count1;
	count2History[0] = count2;
	adcHistory[0] = read_adc();
	
	lora_two_countADC_payload_t payload = {.payload = {0}};
	
	payload.members.count1     = count1History[0];
	payload.members.count2     = count2History[0]; 
	payload.members.Adc        = adcHistory[0];
	if(count1History[0]-count1History[1] > 511)
	{
		payload.members.p1delta1 = 511;
	}
	
	//populate history information
	payload.members.p1delta1 = count1History[0]-count1History[1];
	payload.members.p1delta2 = count2History[0]-count2History[1];
	payload.members.p1Adc    = adcHistory[1];
	
	payload.members.p2delta1 = count1History[1]-count1History[2];
	payload.members.p2delta2 = count2History[1]-count2History[2];
	payload.members.p2Adc    = adcHistory[2];
	
	payload.members.p3delta1 = count1History[2]-count1History[3];
	payload.members.p3delta2 = count2History[2]-count2History[3];
	payload.members.p3Adc    = adcHistory[3];
	
	//count 1 history bounds checking
	if(count1History[0]-count1History[1] > 511)
	{
		payload.members.p1delta1 = 511;
	}
	if(count1History[1]-count1History[2] > 511)
	{
		payload.members.p2delta1 = 511;
	}
	if(count1History[2]-count1History[3] > 511)
	{
		payload.members.p3delta1 = 511;
	}
	
	//count 2 history bounds checking
	if(count2History[0]-count2History[1] > 511)
	{
		payload.members.p1delta2 = 511;
	}
	if(count2History[1]-count2History[2] > 511)
	{
		payload.members.p2delta2 = 511;
	}
	if(count2History[2]-count2History[3] > 511)
	{
		payload.members.p3delta2 = 511;
	}
	
	Debug_printf("   Count1:%d\r\n",payload.members.count1);
	Debug_printf("   Count2:%d\r\n",count2History[0]);
	Debug_printf("   ADC   :%d\r\n",adcHistory[0]);
	Debug_printf("P1 delta1:%d\r\n",payload.members.p1delta1);
	Debug_printf("P1 delta2:%d\r\n",payload.members.p1delta2);
	Debug_printf("P1 ADC   :%d\r\n",payload.members.p1Adc);
	Debug_printf("P2 delta1:%d\r\n",payload.members.p2delta1);
	Debug_printf("P2 delta2:%d\r\n",payload.members.p2delta2);
	Debug_printf("P2 ADC   :%d\r\n",payload.members.p2Adc);
	Debug_printf("P3 delta1:%d\r\n",payload.members.p3delta1);
	Debug_printf("P3 delta2:%d\r\n",payload.members.p3delta2);
	Debug_printf("P3 ADC   :%d\r\n",payload.members.p3Adc);
	
	payload.members.pkt_type = packet_type_data;
	payload.members.sys_voltage = fourBit_battery_calculation();
	
	Uplink(payload.payload, TWO_COUNT_ADC_SIZE);
}

void three_counter_uplink()
{
	(void) three_counter_uplink;
	lora_three_count_payload_t count_payload = {.payload={0}};
	
	count_payload.members.count1 = count1;
	count_payload.members.count2 = count2;
	
	Debug_printf("Count3:%d\r\n",count3);
	count_payload.members.count3_lsb = count3 & 0x1;
	count_payload.members.count3_cb  = (count3 >> 1) & 0xFFFF;
	count_payload.members.count3_msb = (count3 >> 17) & 0x3FF;
	
	Debug_printf("Counts:\r\n\tC1:%05d\r\n\tC2:%05d\r\n\tC3:%05d\r\n",count_payload.members.count1,
	                                                                  count_payload.members.count2,
	                                                                  count_payload.members.count3_lsb 
	                                                                  + (count_payload.members.count3_cb<<1)
	                                                                  + (count_payload.members.count3_msb<<17));
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_data;
	
	// Uplink(count_payload.payload, THREE_COUNT_SIZE);
}


void two_counter_synchronised_uplink()
{	
	//perform the leak check
	Count1LeakCheck();
	Count2LeakCheck();
	
	
	lora_two_count_payload_t count_payload = {.payload={0}};
	//uint8_t hour = HW_RTC_getHour();
	#ifdef ACCELERATE_HOURS
		#warning Hourly warnings set to minutes
		uint8_t hour = HW_RTC_GetMinute() %24;
	#else
		uint8_t hour = HW_RTC_GetHour() %24;
	#endif
	
	//we will need to know which period we are uploading for
	
	//we have the periods 0000-0600, 0600-1200, 1200-1800, 1800-2400
	//we will keep the previous day of counts in memory
	
	count_payload.members.Count1 = count1;
	count_payload.members.Count2 = count2;

	
	//leak detection, read the flag, to see if it was cleared in the interrupt
	count_payload.members.leak2 = leak_detected_2;
	count_payload.members.leak1 = leak_detected_1;
	
	//reset the leak detected flag
	leak_detected_1 = 1;
	leak_detected_2 = 1;
	
	//burst detection
	if(burst_detected_2 >= consecutive_burst_hours)
	{
		count_payload.members.burst2 = 1;
		burst_detected_2 = 0;
	}
	
	//burst detection
	if(burst_detected_1 >= consecutive_burst_hours)
	{
		count_payload.members.burst1 = 1;
		burst_detected_1 = 0;
	}
	
	//tamper
	if(tamper_detected_delayed)
	{
		count_payload.members.tamper = 1;
		tamper_detected_delayed = 0;
	}
	
	//clear the alarm sent flags every six hours
	burst2_alarm_sent = 0;
	burst1_alarm_sent = 0;
	tamper_alarm_sent = 0;
	
	//print out the payload values:
	Debug_printf("Count 1: %d\r\n",  count_payload.members.Count1);
	Debug_printf("Count 2: %d\r\n",  count_payload.members.Count2);
	Debug_printf("Leak  1: %s\r\n", (count_payload.members.leak1 ) ?"YES":"NO");
	Debug_printf("Leak  2: %s\r\n", (count_payload.members.leak2 ) ?"YES":"NO");
	Debug_printf("Burst 1: %s\r\n", (count_payload.members.burst1) ?"YES":"NO");
	Debug_printf("Burst 2: %s\r\n", (count_payload.members.burst2) ?"YES":"NO");
	Debug_printf("Tamper : %s\r\n", (count_payload.members.tamper) ?"YES":"NO");
	await_uart_tx();
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_data;
	
	Uplink(count_payload.payload, TWO_COUNT_SIZE);
}

void two_counter_uplink_alarm()
{
	lora_two_count_alarm_t count_payload = {.payload={0}};
	
	if(tamper_detected)
	{
		Debug_printf("Tamper Alarm\r\n");
		tamper_alarm_sent = 1;
		tamper_detected = 0;
	}
	
	if(burst_detected_1 >= consecutive_burst_hours)
	{
		Debug_printf("Burst Alarm 1\r\n");
		burst1_alarm_sent = 1;
		count_payload.members.burst1 = 1;
	}
	
	if(burst_detected_2 >= consecutive_burst_hours)
	{
		Debug_printf("Burst Alarm 2\r\n");
		burst2_alarm_sent = 1;
		count_payload.members.burst2 = 1;
	}
	
	count_payload.members.tamper = tamper_detected;
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_alarm;

	
	Uplink(count_payload.payload, TWO_COUNT_ALARM_SIZE);
}

void single_counter_uplink_alarm()
{
	(void) single_counter_uplink_alarm;
	lora_single_count_alarm_t count_payload = {.payload={0}};
	
	if(tamper_detected)
	{
		Debug_printf("Tamper Alarm\r\n");
		count_payload.members.tamper = 1;
	}
	
	if(burst_detected_1 >= consecutive_burst_hours)
	{
		Debug_printf("Burst Alarm\r\n");
		burst1_alarm_sent = 1;
		count_payload.members.burst = burst_detected_1;
	}
	
	tamper_detected = 0;
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_alarm;

	
	Uplink(count_payload.payload, SINGLE_COUNT_ALARM_SIZE);
}

void threeEdge_alarm()
{
	while(threeEdgeData.input_1_changed || threeEdgeData.input_2_changed || threeEdgeData.input_3_changed)
	{
		Debug_printf("Edge Changed, %d %d %d\r\n", threeEdgeData.input_1_changed,
		                                           threeEdgeData.input_2_changed,
		                                           threeEdgeData.input_3_changed);
		threeEdge_uplink();
	}
	reset_watchdog();
}

void threeEdge_uplink()
{
	(void) threeEdge_uplink;
	
	lora_three_edge_payload_t edge_payload = {0};
	
	edge_payload.members.count3 = count3;
	edge_payload.members.count2 = count2;
	edge_payload.members.count1 = count1;
	
	edge_payload.members.state1 = HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN);
	edge_payload.members.state2 = HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN);
	edge_payload.members.state3 = HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN);
	
	edge_payload.members.trigger1 = threeEdgeData.input_1_changed;
	edge_payload.members.trigger2 = threeEdgeData.input_2_changed;
	edge_payload.members.trigger3 = threeEdgeData.input_3_changed;
	
	//TODO: flash and config satatus need to be implemented
	//TODO: Temperature needs to be implemented
	
	//clear the changed flags
	threeEdgeData.input_1_changed = 0;
	threeEdgeData.input_2_changed = 0;
	threeEdgeData.input_3_changed = 0;
	
	edge_payload.members.sys_voltage = fourBit_battery_calculation();
	edge_payload.members.pkt_type    = packet_type_data;
	
	Uplink(edge_payload.payload, THREE_EDGE_SIZE);
}	

void tamper_detect()
{
	if(tamper_detected)
	{
		
		//because lazy, I am reusing the single counter alarm, as it functionally does what is requred.
		//at some point this may be changed to a unique alarm.
		single_counter_uplink_alarm();
		
		tamper_detected = 0;
		tamper_alarm_sent = 0;
	}
}

void two_counter_alarms()
{
	//at this point we have exceeded the burst allocation, and send an alarm packet
	if(((burst_detected_1 >= consecutive_burst_hours) && !burst1_alarm_sent && burst_1_this_hour)||
       (tamper_detected) ||
	   ((burst_detected_2 >= consecutive_burst_hours) && !burst2_alarm_sent && burst_2_this_hour))
	{
		two_counter_uplink_alarm();
	}
}



void single_counter_on_count()
{
	//check the count against the hours delta.
	//If exceeded, set the flag, and increase the burst count
	
	//get the hour
	uint8_t hour;
	//get the current hour
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		hour = HW_RTC_GetMinute()%24;
	#else
		hour = HW_RTC_GetHour()%24;
	#endif
		
	if(count1 - hourly_count1[hour % 12] > maximum_hourly_flow)
	{
		if(burst_1_this_hour == 0)
		{
			//indicate that the burst should not be triggered again this hour
			burst_1_this_hour = 1;
			//increase the consecutive burst1 hours.
			burst_detected_1++;
			dbg_print("Burst_detected_1: %d\r\n", burst_detected_1);
		}
	}
}

void two_counter_on_count()
{
	//check the count against the hours delta.
	//If exceeded, set the flag, and increase the burst count
	
	//get the hour
	uint8_t hour;
	//get the current hour
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		hour = HW_RTC_GetMinute()%24;
	#else
		hour = HW_RTC_GetHour()%24;
	#endif
		
	if(count2 - hourly_count2[hour % 12] > maximum_hourly_flow)
	{
		if(burst_2_this_hour == 0)
		{
			//indicate that the burst should not be triggered again this hour
			burst_2_this_hour = 1;
			//increase the consecutive burst1 hours.
			burst_detected_2++;
			
			dbg_print("Burst_detected_2: %d\r\n", burst_detected_2);
		}
	}
}


void single_counter_on_hour()
{
	uint8_t hour;
	//get the current hour
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		hour = HW_RTC_GetMinute()%24;
	#else
		hour = HW_RTC_GetHour()%24;
	#endif

	Debug_printf("at hour %d, count1 is %d\r\n", hour, hourly_count1[hour % 12]);
}

void single_counter_alarms()
{
	//at this point we have exceeded the burst allocation, and send an alarm packet
	if(((burst_detected_1 >= consecutive_burst_hours) && !burst1_alarm_sent && burst_1_this_hour)|| (tamper_detected))
	{
		single_counter_uplink_alarm();
	}
}

void two_counter_on_hour()
{
	uint8_t hour;
	//get the current hour
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		hour = HW_RTC_GetMinute()%24;
	#else
		hour = HW_RTC_GetHour()%24;
	#endif

	Debug_printf("at hour %d, count2 is %d\r\n", hour, hourly_count2[hour % 12]);
		
	single_counter_on_hour();
}

void single_counter_simple_uplink()
{
	single_count_data_t count_payload = {.payload={0}};
	
	count_payload.members.Count1 = count1;
	
	//print out the payload values:
	Debug_printf("Payload:\r\n");
	await_uart_tx();
	Debug_printf("\tCount :%d\r\n",count_payload.members.Count1);
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_data2;
	
	Uplink(count_payload.payload, SINGLE_COUNT_DATA_SIZE);
}

//Construct the packet, notice that this one is 12 bytes, as it does not have the standard header
void single_counter_uplink()
{
	
	(void) single_counter_uplink;
	single_count_data_t count_payload = {.payload={0}};
	uint32_t x;
	int offset = 0;
	//uint8_t hour = HW_RTC_getHour();
	#ifdef ACCELERATE_HOURS
		#warning Hourly warnings set to minutes
		uint8_t hour = HW_RTC_GetMinute() %24;
	#else
		uint8_t hour = HW_RTC_GetHour() %24;
	#endif
	
	//we will need to know which period we are uploading for
	
	//we have the periods 0000-0600, 0600-1200, 1200-1800, 1800-2400
	//we will keep the previous day of counts in memory
	
	//uplink for the last complete 6-hour period.
	//if 0 < hour < 6, the last complete cycle was at midnight
	if(hour < 6)
	{
		offset = 18; //hour ending 19, 20 ,12, 22, 23, 24
		count_payload.members.Count1 = hourly_count1[at_midnight]; //last complete 6-hour cycle was at midnight
		count_payload.members.pkt_tod = count_hours18to00;
	}
	//if 6 < hour < 12
	else if(hour < 12)
	{
		offset = 0;  //hour ending 1, 2, 3, 4, 5, 6
		count_payload.members.Count1 = hourly_count1[at_6]; //last complete 6-hour cycle was at 6AM
		count_payload.members.pkt_tod = count_hours00to06;
	}
	//if 12< hour < 18
	else if(hour < 18)
	{
		offset = 6;  //hour ending 7, 8, 9, 10 , 11, 12
		count_payload.members.Count1 = hourly_count1[at_midday]; //last complete 6-hour cycle was at midday
		count_payload.members.pkt_tod = count_hours06to12;
	}
	//if 18 < hour < 24
	else //if(HW_RTC_GetHour() < 24)
	{
		offset = 12; //hour ending 13, 14, 15, 16, 17, 18
		count_payload.members.Count1 = hourly_count1[at_6]; //last complete 6-hour cycle was at 6PM
		count_payload.members.pkt_tod = count_hours12to18;
	}

	//  2, 8, 14, 20              1, 7, 13, 19
	x = hourly_count1[(at_2+offset)%12] - hourly_count1[(at_1+offset)%12];

	if(x <= SINGLE_COUNT_DELTA_MAX)
	{
		count_payload.members.Delta2 = x;
	}
	else
	{
		count_payload.members.Delta2 = SINGLE_COUNT_DELTA_MAX;
	}
	//  3, 9, 15, 21              2, 8, 14, 20
	x = hourly_count1[(at_3+offset)%12] - hourly_count1[(at_2+offset)%12];

	if(x <= SINGLE_COUNT_DELTA_MAX)
	{
		count_payload.members.Delta3 = x;
	}
	else
	{
		count_payload.members.Delta3 = SINGLE_COUNT_DELTA_MAX;
	}
	//  4, 10, 16, 22             3, 9, 15, 21
	x = hourly_count1[(at_4+offset)%12] - hourly_count1[(at_3+offset)%12];

	if(x <= SINGLE_COUNT_DELTA_MAX)
	{
		count_payload.members.Delta4 = x;
	}
	else
	{
		count_payload.members.Delta4 = SINGLE_COUNT_DELTA_MAX;
	}
	//  5, 11, 17, 23             4, 10, 16, 22
	x = hourly_count1[(at_5+offset)%12] - hourly_count1[(at_4+offset)%12];

	if(x <= SINGLE_COUNT_DELTA_MAX)
	{
		count_payload.members.Delta5 = x;
	}
	else
	{
		count_payload.members.Delta5 = SINGLE_COUNT_DELTA_MAX;
	}
	//  6, 12, 18, 24             5, 11, 17, 23
	x = hourly_count1[(at_6+offset)%12] - hourly_count1[(at_5+offset)%12];

	if(x <= SINGLE_COUNT_DELTA_MAX)
	{
		count_payload.members.Delta6 = x;
	}
	else
	{
		count_payload.members.Delta6 = SINGLE_COUNT_DELTA_MAX;
	}
	
	Count1LeakCheck();
	//leak detection, read the flag, to see if it was cleared in the interrupt
	count_payload.members.leak = leak_detected_1;
	//reset the leak detected flag
	leak_detected_1 = 1;
	
	//print out the payload values:
	Debug_printf("Payload:\r\n");
	await_uart_tx();
	Debug_printf("\tCount :%d\r\n", count_payload.members.Count1);
	await_uart_tx();
	Debug_printf("\tDelta2:%d\r\n", count_payload.members.Delta2);
	Debug_printf("\tDelta3:%d\r\n", count_payload.members.Delta3);
	await_uart_tx();
	Debug_printf("\tDelta4:%d\r\n", count_payload.members.Delta4);
	Debug_printf("\tDelta5:%d\r\n", count_payload.members.Delta5);
	Debug_printf("\tDelta6:%d\r\n", count_payload.members.Delta6);
	await_uart_tx();
	Debug_printf("\tLeak  :%d\r\n", count_payload.members.leak);
	await_uart_tx();
	
	count_payload.members.sys_voltage = fourBit_battery_calculation();
	count_payload.members.pkt_type    = packet_type_data;
	
	Uplink(count_payload.payload, SINGLE_COUNT_DATA_SIZE);
}

void cli_count_help()
{
	cli_print("Usage: count clear\r\n");
	await_uart_tx();
	cli_print("\t Clears the counts\r\n");
	await_uart_tx();
	
	cli_print("Usage: count debounce [interval]\r\n");
	await_uart_tx();
	cli_print("\t Sets the debounce to [interval] ms\r\n");
	await_uart_tx();
	
	cli_print("Usage: count pull [enable|disable]\r\n");
	cli_print("\t Enable or disable internal pullup\r\n");
	await_uart_tx();
	
	if(device.cli_commands & cmd_count_leak)
	{
		cli_print("Usage: count leak [interval]\r\n");
		await_uart_tx();
		cli_print("\tSets the leak detection timeout to [interval] ms\r\n");
		await_uart_tx();
	}
	
	if(device.cli_commands & cmd_count_burst)
	{
		cli_print("Usage: count burst [limit]\r\n");
		await_uart_tx();
		cli_print("\tSets the burst detection hourly threshold to [limit] pulses\r\n");
		await_uart_tx();
		
		cli_print("Usage: count burst_hours [hours]\r\n");
		await_uart_tx();
		cli_print("\tSets the burst detection consecutive\r\n");
		await_uart_tx();
		cli_print("\thours requirement to [hours] hours.\r\n");
		await_uart_tx();
	}
	
	if(dir_enabled)
	{
		cli_print("Usage: count invert_dir [true|false]\r\n");
		await_uart_tx();
		cli_print("\t Sets the polarity of the dir pin\r\n");
		cli_print("\t 0 -> Count up when dir high\r\n");
		cli_print("\t 1 -> Count up when dir low\r\n");
		await_uart_tx();
	}
	
	cli_print("Usage: count set [counter] [value]\r\n");
	await_uart_tx();
	cli_print("\t Sets the value of [counter] to [value] ms\r\n");
	await_uart_tx();
	
	cli_print("Usage: count show\r\n");
	await_uart_tx();
	cli_print("\t Displays the current counts\r\n");
	await_uart_tx();
}

//method to handle count configuration on cli
void cli_count_implementation(int argc,   char *argv[])
{ 
	uint32_t debounce = 0;
	uint8_t target;
	uint32_t target_value;
	uint8_t i;
	
	//if one of the three counters is not defined for the current mode, then this command does nothing
	if(device.cli_commands & (cmd_count1 | cmd_count2 | cmd_count3))
	{
		
		if(argc == 1)
		{
			if(!strcmp(argv[0], "clear"))
			{
				count1 = 0;
				count2 = 0;
				count3 = 0;
				
				for(i=0; i<12; i++)
				{
					hourly_count1[i] = 0;
				}
				
				cli_print("Counts cleared\r\n");
				return;
			}
			
			if(!strcmp(argv[0],"show"))
			{
				await_uart_tx();
				
				if(device.cli_commands & cmd_mux_adc)
				{
					uint16_t adc_raw = AdcReadCompensate(ADC2_CH);
					uint16_t mA = ((adc_raw)*0.006105); //(3/4095f)*(1/120)*1000);
					cli_print("4-20mA: %dmA\r\n",mA);
					await_uart_tx();
					
					uint16_t volts = AdcReadCompensate(ADC3_CH)*1.4652;//(3/4095)*1000*2;
					cli_print("0-5V: %dmV\r\n",volts);
					await_uart_tx();
				}
				
				if(device.cli_commands & cmd_count1)
				{
					cli_print("Count1:%d\r\n",count1);
					await_uart_tx();
				}

				cli_print("Debounce:%dms\r\n", debounce_interval);
				await_uart_tx();
				
				if(device.cli_commands & cmd_count_leak)
				{
					cli_print("Leak    :%dms\r\n", leak_interval);
					await_uart_tx();
				}				
				
				if(device.cli_commands & cmd_count2)
				{
					cli_print("Count2:%d\r\n",count2);
					await_uart_tx();
				}
				
				if(device.cli_commands & cmd_count3)
				{
					cli_print("Count3:%d\r\n",count3);
					await_uart_tx();
				}
				
				cli_print("Dir Enabled: %s\r\n", (dir_enabled)? "YES":"NO");
				
				
				if(dir_enabled == true)
				{
					cli_print("Direction Inverted: %s\r\n", (dir_inverted)? "YES":"NO");
					await_uart_tx();
				}
				
				if(device.cli_commands & cmd_count_burst)
				{
					cli_print("Burst   :%d pulses per hour\r\n", maximum_hourly_flow);
					await_uart_tx();
					cli_print("Burst_hr:%d consecutive hours before alarm\r\n", consecutive_burst_hours);
					await_uart_tx();
				}

				cli_print("Internal Pullup %sabled\n\r", internal_pullup_enabled?"En":"Dis");
				await_uart_tx();
				
				
				await_uart_tx();
				return;
			}
		}
		
		if(argc == 2)
		{
			if(!strcmp(argv[0], "pull"))
			{
				if(!strcmp(argv[1], "enable"))
				{
					internal_pullup_enabled = true;
					return;
				}
				
				if(!strcmp(argv[1], "disable"))
				{
					internal_pullup_enabled = false;
					return;
				}
			}
			
			
			if(!strcmp(argv[0],"debounce"))
			{
				debounce = atoi(argv[1]);
				
				if(debounce >= MIN_DEBOUNCE_INTERVAL)
				{
					debounce_interval = debounce;
					cli_print("Debounce interval set to %dms\r\n", debounce_interval);
					return;
				}
			}
			
			if(!strcmp(argv[0],"leak") && (device.cli_commands & cmd_count_leak))
			{
				//re-using the debounce variable, need to change it to something more/less descriptive.
				debounce = atoi(argv[1]);
				
				if(debounce >= MIN_LEAK_INTERVAL)
				{
					leak_interval = debounce;
					cli_print("Leak interval set to %dms\r\n", leak_interval);
					return;
				}
			}
			
			if(!strcmp(argv[0],"burst") && (device.cli_commands & cmd_count_burst))
			{
				//re-using the debounce variable, need to change it to something more/less descriptive.
				debounce = atoi(argv[1]);
				
				if(debounce >= MIN_MAX_FLOW)
				{
					maximum_hourly_flow = debounce;
					cli_print("Hourly burst flow limit set to %d pulses\r\n", maximum_hourly_flow);
					return;
				}
			}
			
			if(!strcmp(argv[0],"burst_hours") &&(device.cli_commands & cmd_count_burst))
			{
				//re-using the debounce variable, need to change it to something more/less descriptive.
				debounce = atoi(argv[1]);
				
				if(debounce >= BURST_HOURS_MIN)
				{
					consecutive_burst_hours = debounce;
					cli_print("Required burst hours set to %d\r\n", consecutive_burst_hours);
					return;
				}
			}
			
			//Only configure dir if dir is enabled
			if(!strcmp(argv[0],"invert_dir") && (dir_enabled == true))
			{
				if(!strcmp(argv[1],"true"))
				{
					dir_inverted = true;
					cli_print("Dir pin inverted\r\n");
					return;
				}
				if(!strcmp(argv[1],"false"))
				{
					dir_inverted = false;
					cli_print("Dir pin not inverted\r\n");
					return;
				}
				
			}
		}
		
		if(argc == 3)
		{
			if(!strcmp(argv[0],"set"))
			{
				target = atoi(argv[1]);
				target_value = atoi(argv[2]);
				
				if(target ==1 && (device.cli_commands & cmd_count1))
				{
					count1 = target_value;
					cli_print("Count%d set to %d\r\n",target, target_value);
					return;
				}
				if(target ==2 && (device.cli_commands & cmd_count2))
				{
					count2 = target_value;
					cli_print("Count%d set to %d\r\n",target, target_value);
					return;
				}
				if(target ==3 && (device.cli_commands & cmd_count3))
				{
					count3 = target_value;
					cli_print("Count%d set to %d\r\n",target, target_value);
					return;
				}
			}
		}
		
		//if we get to this point without calling return, then show the help info
		cli_count_help();
	}
}

//this is a callback from the RTC alarmB interrupt. Don't make it too long
void record_hourly_count()
{
	//static ensures that it is not put on the stack, and is persistant
	static uint32_t previous_hour_count = 0;
	static uint16_t delta = 0;
	
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		uint8_t hour = HW_RTC_GetMinute() % 12;
	#else
		uint8_t hour = HW_RTC_GetHour() % 12;
	#endif
	
	//record the count at this hour
	hourly_count1[hour] = count1;
	
	//take the last hour, to get the delta
	delta = count1-previous_hour_count;
	
	//If the delta has not exceeded the hourly flow limit
	//reset the burst counter
	if(delta < maximum_hourly_flow)
	{
		burst_detected_1 = 0;
	}
	
	//check the leak interval at the hour
	Count1LeakCheck();
	
	burst1_alarm_sent = 0;
	//reset the indicator for the hours burst 1, allow another burst to be captured
	burst_1_this_hour = 0;
	previous_hour_count = count1;
}

//this is a callback from the RTC alarmB interrupt. Don't make it too long
void record_hourly_count2()
{
	//static ensures that it is not put on the stack, and is persistant
	static uint32_t previous_hour_count = 0;
	static uint16_t delta = 0;
	
	#ifdef ACCELERATE_HOURS
		#warning hourly wake is in minutes for testing
		uint8_t hour = HW_RTC_GetMinute() % 12;
	#else
		uint8_t hour = HW_RTC_GetHour() % 12;
	#endif
	
	//record the count at this hour
	hourly_count2[hour] = count2;
	
	//take the last hour, to get the delta
	delta = count2-previous_hour_count;
	
	//If the delta has not exceeded the hourly flow limit
	//reset the burst counter
	if(delta < maximum_hourly_flow)
	{
		burst_detected_2 = 0;
	}
	
	Count2LeakCheck();
	
	//reset the indicator for the hours burst 1, allow another burst to be captured
	burst_2_this_hour = 0;
	
	burst2_alarm_sent = 0;
	previous_hour_count = count2;
	
	record_hourly_count();
}

void init_tamper()
{
		GPIO_InitTypeDef GPIO_InitStruct;
	
		GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
		if(internal_pullup_enabled)
		{
			GPIO_InitStruct.Pull  = GPIO_PULLUP;
		}
		else
		{
			GPIO_InitStruct.Pull  = GPIO_NOPULL;
		}
		GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	
		HW_GPIO_Init(TAMPER_PORT, TAMPER_PIN, &GPIO_InitStruct);
		
		HW_GPIO_SetIrq(TAMPER_PORT    , TAMPER_PIN    ,3,Tamper_IRQ);
}

void init_single_counter()
{
	int i;
	dir_enabled = true;
	GPIO_InitTypeDef GPIO_InitStruct;
	//all count inputs have an external pullup
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	
	//count1 as a counter
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	//count3 as an alarm
	HW_GPIO_Init(TAMPER_PORT, TAMPER_PIN, &GPIO_InitStruct);
	
	//if the device is a single counter, then we want to have the ADC_IN/tamper line be a
	//falling edge interrupt.
	LL_EXTI_DisableRisingTrig_0_31(TAMPER_PIN);
	LL_EXTI_EnableFallingTrig_0_31(TAMPER_PIN);
	
	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,SingleCount1IRQ  );
	HW_GPIO_SetIrq(TAMPER_PORT    , TAMPER_PIN    ,3,Tamper_IRQ);
	
	//count2 as a direction pin, no interrrupt needed
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	HW_GPIO_Init(COUNT1_DIR_PORT    , COUNT1_DIR_PIN    , &GPIO_InitStruct);
	
	
	//pre-initialise the array
	for(i=0;i<12;i++)
	{
		hourly_count1[i] = count1;
		hourly_count2[i] = count2;
	}
	
	//This device relys on transmitting every 6 hours
	//However we will allow setting custom periods so configuration is made easier
}

void init_two_counter_tamper()
{
	int i;
	dir_enabled = false;
	GPIO_InitTypeDef GPIO_InitStruct;
	//all count inputs will have external pull-ups (1M)
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	
	//count1 and 2 as a counter
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
	//count3 as an alarm
	HW_GPIO_Init(TAMPER_PORT, TAMPER_PIN, &GPIO_InitStruct);
	
	//falling edge interrupt for tamper
	LL_EXTI_DisableRisingTrig_0_31(TAMPER_PIN);
	LL_EXTI_EnableFallingTrig_0_31(TAMPER_PIN);
	
	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,SingleCount1IRQ );
	HW_GPIO_SetIrq(COUNT2_PORT    , COUNT2_PIN    ,3,SynchronisedCount2IRQ );
	HW_GPIO_SetIrq(TAMPER_PORT    , TAMPER_PIN    ,3,Tamper_IRQ);
	
	//pre-initialise the array
	for(i=0;i<12;i++)
	{
		hourly_count1[i] = count1;
		hourly_count2[i] = count2;
	}
	
	//This device relys on transmitting every 6 hours
	//However we will allow setting custom periods so configuration is made easier
}

void init_three_counter()
{
	dir_enabled = false;
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	//alternate function doesn't matter, because this is not configured for af.
	
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT3_PORT    , COUNT3_PIN    , &GPIO_InitStruct);		
	
	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,Count1IRQ  );
	HW_GPIO_SetIrq(COUNT2_PORT    , COUNT2_PIN    ,3,Count2IRQ  );
	HW_GPIO_SetIrq(COUNT3_PORT    , COUNT3_PIN    ,3,Count3IRQ  );
}

void init_three_edge_alarm()
{
	dir_enabled = false;
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	//alternate function doesn't matter, because this is not configured for af.
	
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT3_PORT    , COUNT3_PIN    , &GPIO_InitStruct);	

	//we want the alarms to trigger on both edges
	LL_EXTI_EnableFallingTrig_0_31(COUNT1_PIN);
	LL_EXTI_EnableFallingTrig_0_31(COUNT2_PIN);
	LL_EXTI_EnableFallingTrig_0_31(COUNT3_PIN);
	
	//for some reason, we need to re-enable the rising edge on these pins,
	//becuase reasons?
	LL_EXTI_EnableRisingTrig_0_31(COUNT1_PIN);
	LL_EXTI_EnableRisingTrig_0_31(COUNT2_PIN);
	LL_EXTI_EnableRisingTrig_0_31(COUNT3_PIN);
	
	//we need a delay to account for a capacitor on the input pins, before enabling the interrupts.
	delay_timeout_ms(100);
	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,Edge1IRQ  );
	HW_GPIO_SetIrq(COUNT2_PORT    , COUNT2_PIN    ,3,Edge2IRQ  );
	HW_GPIO_SetIrq(COUNT3_PORT    , COUNT3_PIN    ,3,Edge3IRQ  );
}

void init_two_count_adc()
{
	dir_enabled = false;
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	//alternate function doesn't matter, because this is not configured for af.
	
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);		

	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,Count1IRQ  );
	HW_GPIO_SetIrq(COUNT2_PORT    , COUNT2_PIN    ,3,Count2IRQ  );
	
	//analog input initialisation
	init_adc();
}

void save_counter_config()
{
	count_config_page_layout_t config = {0};
	
	//save count config values into the config array
	config.members.count_debounce    = debounce_interval;
	config.members.count_leak        = leak_interval;
	config.members.count_burst       = maximum_hourly_flow;
	config.members.count_burst_hours = consecutive_burst_hours;
	config.members.invert_dir        = dir_inverted;
	config.members.pullup_enabled    = internal_pullup_enabled;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void load_counter_config()
{
	count_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	
	//save count config values into the config array
	debounce_interval        = config.members.count_debounce;
	leak_interval            = config.members.count_leak;
	maximum_hourly_flow      = config.members.count_burst;
	consecutive_burst_hours  = config.members.count_burst_hours;
	dir_inverted             = config.members.invert_dir;
	internal_pullup_enabled = config.members.pullup_enabled;
}

void save_counter_data()
{
	count_data_page_layout_t data = {0};
	
	data.members.count1 = count1;
	data.members.count2 = count2;
	data.members.count3 = count3;
	
	save_data_page(data.raw_bytes);
}

void load_counter_data()
{
	count_data_page_layout_t data = {0};
	load_data_page(data.raw_bytes);
	
	count1 = data.members.count1;
	count2 = data.members.count2;
	count3 = data.members.count3;
}

void three_mux_onDownlink(uint8_t *buffer, uint8_t size)
{

}

void three_mux_load_counter_data (void)
{
	load_counter_data();
}

void three_mux_save_counter_data (void)
{
	save_counter_data();
}

void three_mux_save_config(void)
{
	save_counter_config();
}

void three_mux_load_config(void)
{
	load_counter_config();
}

void init_three_mux_inputs()
{
	dir_enabled = false;
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	if(internal_pullup_enabled)
	{
		GPIO_InitStruct.Pull  = GPIO_PULLUP;
	}
	else
	{
		GPIO_InitStruct.Pull  = GPIO_NOPULL;
	}
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	
	//alternate function doesn't matter, because this is not configured for af.
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	
	//now that the pins are configured as inputs, we need to set up the interrupt
	HW_GPIO_SetIrq(COUNT1_PORT    , COUNT1_PIN    ,3,Count1IRQ  );
	
	//analog input initialisation
	init_adc();
}
void three_mux_uplink()
{
	lora_three_mux_payload_t packet = {.payload={0}};

	packet.members.ADC_420 = AdcReadCompensate(ADC2_CH);
	packet.members.ADC_V = AdcReadCompensate(ADC3_CH);
	
	//calculate the actual voltage from this value using the following formula
	//     3.0*reading
	// ----------------------
	//   4095 * div_ratio
	//
	//Caluculate the div_ratio as
	//      R2
	//  -----------
	//    R1 + R2
	Debug_printf("Reading ADC_420: %d \r\n", packet.members.ADC_420);
	Debug_printf("Reading ADC_V: %d \r\n", packet.members.ADC_V);
	
	packet.members.count1 = count1;
	
	//print out the payload values:
	Debug_printf("Count :%d\r\n",packet.members.count1);
	await_uart_tx();
	Debug_printf("Payload:");
	
	packet.members.sys_voltage = fourBit_battery_calculation();
	packet.members.pkt_type    = packet_type_data;
	
	Uplink(packet.payload, THREE_MUX_SIZE);
}

