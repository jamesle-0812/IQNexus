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


#include "lora_sensum.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "tiny_vsnprintf.h"
#include "lora.h"
#include "at.h"
#include "watchdog.h"
#include "counter.h"
#include "delays.h"
#include "global.h"
#include "flash_map.h"
#include "radio.h"
#include "sx1276.h"
#include "sensum_version.h"
#include "timeServer.h"
#include "radio_common.h"

#define WATCHDOG_RESET_TIMER_PERIOD 100
static TimerEvent_t watchdog_reset_timer;
static uint16_t channel_list[] = {0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

#ifndef DISABLE_LORA_CLASS_DEBUG
	#define LORA_CLASS_printf(...) Debug_printf(__VA_ARGS__)//; await_uart_tx()
#else
	#define LORA_CLASS_printf(...)
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/**
 * @brief LoRaWAN Adaptive Data Rate
 * @note Please note that when ADR is enabled the end-device should be static
 */
#define LORAWAN_ADR_ON                              1

/**
 * When fast wake up is enabled, the mcu wakes up in ~20us and
 * does not wait for the VREFINT to be settled. THis is ok for
 * most of the case except when adc must be used in this case before
 * starting the adc, you must make sure VREFINT is settled
 */
#define ENABLE_FAST_WAKEUP
/*!
 * Number of trials for the join request.
 */
#define JOINREQ_NBTRIALS                            3

/* Private macro -------------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/



/* call back when LoRa has received a frame*/
static void LoraRxData(lora_AppData_t *AppData);
/* call back when LoRa endNode has just joined*/
static void LORA_HasJoined( void );
/* call back when LoRa endNode has just switch the class*/
static void LORA_ConfirmClass ( DeviceClass_t Class );

volatile uint8_t data_received = 0;
volatile uint8_t rx_buffer[MAX_RX_DATA] = {0};
volatile uint8_t rx_data_size = 0;

static void LoraRxData(lora_AppData_t *AppData)
{
	int i = 0;
		
	rx_data_size = AppData->BuffSize;
	
	if(MAX_RX_DATA < AppData->BuffSize)
	{
		Debug_printf("Max Downlink Size exceeded. Truncating to %d bytes\r\n", MAX_RX_DATA);
		Debug_printf("Previous Size was %d\r\n", AppData->BuffSize);
		rx_data_size = MAX_RX_DATA;
	}


	Debug_printf("Recieved Data:");

	for(i=0;i<rx_data_size;i++)
	{
		Debug_printf(" %02X", AppData->Buff[i]);
		rx_buffer[i] = AppData->Buff[i];
	}
	
	Debug_printf("\r\n");
	
	Debug_printf("Data Received on Port %d\r\n", AppData->Port);
	
	if(rx_data_size > 0)
	{
		data_received = 1;
	}
	
	reset_watchdog();
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  Error_Handler();
}
#endif

static void LORA_HasJoined( void )
{
#if( OVER_THE_AIR_ACTIVATION != 0 )
  PRINTF("JOINED\n\r");
#endif
}

static void LORA_ConfirmClass ( DeviceClass_t Class )
{
  LORA_CLASS_printf("switch to class %c done\n\r","ABC"[Class] );
}


/* Private variables ---------------------------------------------------------*/

//we need to look at a less confusing way to word this.
//This one is the configuration from the command line
uint16_t lora_hours_until_rejoin = MIN_HOURS_TO_REJOIN;
//this one is the actual down-counter
uint32_t hours_until_rejoin = MIN_HOURS_TO_REJOIN;
/* load call backs*/
static LoRaMainCallback_t LoRaMainCallbacks = { HW_GetBatteryLevel,
                                                HW_GetTemperatureLevel,
                                                HW_GetUniqueId,
                                                HW_GetRandomSeed,
                                                LoraRxData,
                                               LORA_HasJoined,
                                               LORA_ConfirmClass};

/**
 * Initialises the Lora Parameters
 */
static LoRaParam_t LoRaParamInit = {LORAWAN_ADR_ON,
                                    DR_0,
                                    LORAWAN_PUBLIC_NETWORK,
                                    JOINREQ_NBTRIALS};


void print_lora_radio_information()							
{									
	//print LoRa Chip information.
	//Device EUI
	uint8_t *pt = lora_config_deveui_get();
	Debug_printf("\r\n\r\nEUID:\t%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
					pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7]);
	await_uart_tx();
	//APPKEY
	pt = lora_config_appkey_get();
	
	Debug_printf("APPKEY:\t%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
		pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7], pt[8], pt[9], pt[10], pt[11], pt[12], pt[13], pt[14], pt[15]);
	await_uart_tx();
	//APPEUI
	pt = lora_config_appeui_get();
	Debug_printf("APPEUI:\t%02X%02X%02X%02X%02X%02X%02X%02X\r\n",
					pt[0], pt[1], pt[2], pt[3], pt[4], pt[5], pt[6], pt[7]);
	await_uart_tx();
}
																		
void lora_sleep()
{	
	static TimerEvent_t lora_sleep_timer;
	start_timeout_timer(&lora_sleep_timer,10000);
	
	Debug_printf("Putting Radio to sleep\r\n");
	
	//wait for any on-going operations to finish
	while(isLoRaMacTxBusy() == LORAMAC_STATUS_BUSY)
	{
		reset_watchdog();
			
		if(timer_expired(&lora_sleep_timer))
		{
			//failure condition
			Debug_printf("Timeout\r\n");
			break; //or should this be more drastic, as joining took longer than expected?
		}
	}
	
	stop_timeout_timer(&lora_sleep_timer);
	//then put the radio to sleep
	
	//do not sleep the radio in CLASS_C
	if(!device.lora_class_c)
	{
		Radio.Sleep( );
	}
}

void add_channel(int channel_number)
{
	//get the index in the array
	uint8_t array_index = channel_number / 16;
	
	//get the position in the index
	uint8_t position_index = channel_number % 16;
	
	channel_list[array_index] |= 1 << position_index;
}

void add_channels(uint8_t* list ,uint8_t length)
{
	int i = 0;
	uint8_t array_index;
	uint8_t position_index;
	
	for(i = 0; i< length; i++)
	{
		//get the index in the array
		array_index = list[i] / 16;
		
		//get the position in the index
		position_index = list[i] % 16;
		
		channel_list[array_index] |= (1 << position_index);
	}
}

void remove_channels(uint8_t* list, uint8_t length)
{
	int i = 0;
	uint8_t array_index;
	uint8_t position_index;
	
	for(i = 0; i< length; i++)
	{
		//get the index in the array
		array_index = list[i] / 16;
		
		//get the position in the index
		position_index = list[i] % 16;
		
		channel_list[array_index] &= ~(1 << position_index);
	}
}

void print_active_channels()
{
	int array_index = 0;
	int position_index = 0;
	bool newline = false;
	Debug_printf("Enabled Channels:\r\n");
	await_uart_tx();
	for(array_index=0; array_index < 6; array_index++)
	{
		for(position_index = 0; position_index < 16; position_index++)
		{
			await_uart_tx();
			//suspect that this conditional is the issue
			if(((channel_list[array_index] >> position_index) & 1) == 1)
			{
				Debug_printf("%02d ", (array_index*16)+position_index);
				await_uart_tx();
				newline = true;
			}
		}
		if(newline)
		{
			Debug_printf("\r\n");
			await_uart_tx();
			newline = false;
		}	
	}
	await_uart_tx();
}

	//call the LoRa_ initilisation to reset the SX1276.
void lora_init()
{
	int16_t gain = 0;
	
	if(!internal_NetworkJoinStatus())
	{
		gain = lora_get_antenna_gain();
		Debug_printf("Not Joined, Initialising\r\n");
		LORA_Init(&LoRaMainCallbacks, &LoRaParamInit);
		lora_set_antenna_gain(gain);
		#ifdef REGION_AU915
		{
			Debug_printf("Channel List: 0x%04X%04X%04X%04X%04X%04X\r\n",
				channel_list[5],
				channel_list[4],
				channel_list[3],
				channel_list[2],
				channel_list[1],
				channel_list[0]);
			ChanMaskSetParams_t chan_mask = {0};
			chan_mask.ChannelsMaskIn = channel_list;
			
			chan_mask.ChannelsMaskType = CHANNELS_DEFAULT_MASK;
			RegionChanMaskSet(LORAMAC_REGION_AU915,&chan_mask);
			chan_mask.ChannelsMaskType = CHANNELS_MASK;
			RegionChanMaskSet(LORAMAC_REGION_AU915,&chan_mask);
		}
		#endif
		return;
	}
	

	
	Debug_printf("Joined, no LoRaInit\r\n");

}

static char* getNetworkName(uint32_t networkID)
{	
	static char* TTN_PUBLIC = "TTN - PUBLIC";
	static char* SPARK_ACTILLITY = "SPARK ACTILLITY";
	static char* GEMTEK = "GEMTEK";
	static char* UNKNOWN = "UNKNOWN";
	
	switch(networkID)
	{
		case 19:
			return TTN_PUBLIC;
		case 6291470: 
			return SPARK_ACTILLITY;
		case 12648430:
			return GEMTEK;
		default:
			return UNKNOWN;
	}
}


static bool JoinLoRaNetwork()
{
	MibRequestConfirm_t mibReq;
	uint32_t random_time = 0;
	uint32_t attempt_counter = LORA_JOIN_ATTEMPT_MAX;
	
	//we will be using these to set a timeout limit on the radio TX, to ensure that it does not lock up.
	static TimerEvent_t join_timer;

	#ifndef DISABLE_LORA_DEBUG
		uint16_t uplink_counter = 0;
		uint16_t downlink_counter = 0;
	#endif //DISABLE_LORA_DEBUG

	#ifndef DISABLE_LORA_DEBUG
		mibReq.Type = MIB_UPLINK_COUNTER;
		LoRaMacMibGetRequestConfirm(&mibReq);
		uplink_counter = mibReq.Param.UpLinkCounter;
		
		mibReq.Type = MIB_DOWNLINK_COUNTER;
		LoRaMacMibGetRequestConfirm(&mibReq);
		downlink_counter = mibReq.Param.DownLinkCounter;
		
		Debug_printf("Before Join: U=%d, D=%d\r\n", uplink_counter, downlink_counter);
	#endif //DISABLE_LORA_DEBUG
	
	if(!internal_NetworkJoinStatus())
	{
		//if not, we have to attempt to join
		Debug_printf("Attempting to Join\r\n");
		lora_init();
		//we need a random delay, to ensure that timing co-incidences are broken up.
		//lets aim for between 0 and 10 seconds.
		random_time = rand () % 10000;
		#ifndef DISABLE_LORA_DEBUG
			Debug_printf("Delaying for %d ms\r\n", random_time);
		#endif
		delay_timeout_ms(random_time); 
	}
	
	//check if we are joined
	while(attempt_counter > 0 && !internal_NetworkJoinStatus())
	{
		attempt_counter --;
		
		LORA_Join();
		
		//as long as the MAC is transmitting, we want to be waiting
		//we also need a timeout on this, to ensure that we don't permanantly lock while waiting
			 
		//get the timestamps, ensure that the timeout is set up correctly, 10 Minutes.
		//This allows for channel plans with longer join delays, such as AU915
		start_timeout_timer(&join_timer, 600000);

		while(isLoRaMacTxBusy() == LORAMAC_STATUS_BUSY)
		{
			reset_watchdog();
			start_timeout_timer(&watchdog_reset_timer, WATCHDOG_RESET_TIMER_PERIOD);
			sleep_until_interrupt();
			//check if we are joined
			if(internal_NetworkJoinStatus())
			{
				//if we are, then we can stop waiting
				break;
			}
			
			if(timer_expired(&join_timer))
			{
				//failure condition
				break; //or should this be more drastic, as joining took longer than expected?
			}
		}
		stop_timeout_timer(&join_timer);
		stop_timeout_timer(&watchdog_reset_timer);
	}
	if(!internal_NetworkJoinStatus())
	{
		Debug_printf("Failed to Join\r\n");
		//failed to join
		return 0;
	}

	mibReq.Type = MIB_NET_ID;

	LoRaMacMibGetRequestConfirm( &mibReq );
	
	//if we get to here, we have successfully joined
	//Alert the UART user
	Debug_printf("Joined Network %d (%s)\r\n",mibReq.Param.NetID, getNetworkName(mibReq.Param.NetID));
	//Switch from OTAA to ABP, to reduce the number of joins


	#ifndef DISABLE_LORA_DEBUG
		mibReq.Type = MIB_UPLINK_COUNTER;
		LoRaMacMibGetRequestConfirm(&mibReq);
		uplink_counter = mibReq.Param.UpLinkCounter;
		
		mibReq.Type = MIB_DOWNLINK_COUNTER;
		LoRaMacMibGetRequestConfirm(&mibReq);
		downlink_counter = mibReq.Param.DownLinkCounter;
		

		Debug_printf("After  Join: U=%d, D=%d\r\n", uplink_counter, downlink_counter);
	#endif //DISABLE_LORA_DEBUG

	return 1;
}

void lora_force_rejoin(void)
{
	MibRequestConfirm_t mibReq;
	
	//reset the downcounter
	hours_until_rejoin = lora_hours_until_rejoin;
	//set rejoin


	mibReq.Type = MIB_NETWORK_JOINED;
	mibReq.Param.IsNetworkJoined = 0;
	LoRaMacMibSetRequestConfirm(&mibReq);
}

void lora_manage_rejoin( void )
{
	hours_until_rejoin--;
	
	//if the down-counter has expired,
	if(hours_until_rejoin == 0 && lora_hours_until_rejoin != LORA_REJOIN_LOCKOUT_VALUE)
	{
		lora_force_rejoin();
	}
}


void lora_set_antenna_gain(int16_t gain)
{
	MibRequestConfirm_t MibReq;
	
	float ant_gain = gain;
	ant_gain /= 10;
	
	MibReq.Type = MIB_ANTENNA_GAIN;
	MibReq.Param.AntennaGain = ant_gain;
	
	LoRaMacMibSetRequestConfirm(&MibReq);
}

int16_t lora_get_antenna_gain(void )
{
	MibRequestConfirm_t MibReq;
	
	MibReq.Type = MIB_ANTENNA_GAIN;
	float ant_gain;
	
	LoRaMacMibGetRequestConfirm(&MibReq);
	
	ant_gain = MibReq.Param.AntennaGain;
	
	return (int16_t)(ant_gain*10);
}


static int LoRaFormatTxString(char* output, int output_length,const char *format, ... )
{
	va_list args;
  va_start(args, format);
  uint8_t len=0;

  /*convert into string at buff[0] of length iw*/
	len = tiny_vsnprintf_like(output, output_length, format, args); 
	
  va_end(args);
	
	return len;
}

int process_lora_downlink()
{
	int i;
	uint8_t data[MAX_RX_DATA] = {0};
	if(data_received != 0)
	{
		data_received = 0;
		
		for(i=0;i<rx_data_size; i++)
		{
			data[i] = rx_buffer[i];
		}
		
		//if the received downlink is not the default downlink, then we need to 
		//process the device specific downlink.
		default_downlink(data, rx_data_size);
		
		return transmit_status_received_downlink;
	}
	return transmit_status_success;
}

static transmit_status_e sendData(char *txASCII)
{
	ATEerror_t sendStatus;

	static TimerEvent_t transmit_timer;
	
	//if this starts failing, then we may need to find a way to split the string into smaller parts
	//or add a reset_watchdog() call in tiny_vsnprintf.c
	await_uart_tx();
	Debug_printf("Tx String: ");
	await_uart_tx();
	Debug_printf(txASCII);
	await_uart_tx();
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(!JoinLoRaNetwork())
	{
		//put the radio into low current mode
		lora_sleep();
		return transmit_status_no_join;
	}
	
	
	#ifndef DISABLE_LORA_DEBUG
		Debug_printf("Requesting ACK? %d\r\n", lora_config_reqack_get());
	#endif //DISABLE_LORA_DEBUG
	
	//get the timestamps, ensure that the timeout is set up correctly
	//TODO: Should this be 5 seconds instead of 50? why/why not
	start_timeout_timer(&transmit_timer, 50000);
	reset_watchdog();
	//while the radio is busy performing TX operations
	while( isLoRaMacTxBusy() ==  LORAMAC_STATUS_BUSY)
	{
		//ensure that the device does not reset while waiting
		reset_watchdog();	
		start_timeout_timer(&watchdog_reset_timer, WATCHDOG_RESET_TIMER_PERIOD);
		sleep_until_interrupt();
		//wait for the tx to finish, if it takes longer than 5 seconds, we have an error and should reset everything
		if(timer_expired(&transmit_timer))
		{
			//diagnositc print
			Debug_printf("Tx Timeout Expired, resetting device\r\n");
			//save data to flash, just in case we are a device that cares about losing data.
			//on device reset all volatile memory is reset
			device.save_data();
			//delay to allow last character to be sent
			await_uart_tx();
			delay_timeout_ms(10);
			
			//force a reset to clear all errors
			force_mcu_reset_via_watchdog();
		}
	}
	stop_timeout_timer(&transmit_timer);
	stop_timeout_timer(&watchdog_reset_timer);
	
	#ifdef ENABLE_DEBUG_PINS_TIMERS
	{
		LL_GPIO_SetOutputPin(DEBUG_1_PORT, DEBUG_1_PIN);
		LL_GPIO_SetOutputPin(DEBUG_2_PORT, DEBUG_2_PIN);
		LL_GPIO_SetOutputPin(DEBUG_3_PORT, DEBUG_3_PIN);
	
		delay_timeout_ms(100);
	
		LL_GPIO_ResetOutputPin(DEBUG_1_PORT, DEBUG_1_PIN);
		LL_GPIO_ResetOutputPin(DEBUG_2_PORT, DEBUG_2_PIN);
		LL_GPIO_ResetOutputPin(DEBUG_3_PORT, DEBUG_3_PIN);
		
		delay_timeout_ms(100);
	}
	#endif
	
	#ifdef ENABLE_DEBUG_PINS_TIMERS
	{
		LL_GPIO_SetOutputPin(DEBUG_1_PORT, DEBUG_1_PIN);
	}
	#endif

	
	//we have learned that we cannot trust the at_SendBinary to return at the right time
	sendStatus = at_SendBinary(txASCII);
	
	
	//get the timestamps, ensure that the timeout is set up correctly
	start_timeout_timer(&transmit_timer, 50000);
	//while the radio is busy performing TX operations
	while( isLoRaMacTxBusy() ==  LORAMAC_STATUS_BUSY)
	{
		//ensure that the device does not reset while waiting
		reset_watchdog();
		start_timeout_timer(&watchdog_reset_timer, WATCHDOG_RESET_TIMER_PERIOD);
		sleep_until_interrupt();
		//wait for the tx to finish, if it takes longer than 5 seconds, we have an error and should reset everything
		if(timer_expired(&transmit_timer))
		{
			//diagnositc print
			Debug_printf("Tx Timeout Expired, resetting device\r\n");
			//save data to flash, just in case we are a device that cares about losing data.
			//on device reset all volatile memory is reset
			device.save_data();
			//delay to allow last character to be sent
			await_uart_tx();
			delay_timeout_ms(10);
			//force a reset to clear all errors
			force_mcu_reset_via_watchdog();
		}
	}
	
	Debug_printf("Tx Done\r\n");
	
	stop_timeout_timer(&transmit_timer);
	stop_timeout_timer(&watchdog_reset_timer);

	await_uart_tx();
	
	if(process_lora_downlink() == transmit_status_received_downlink)
	{
		return transmit_status_received_downlink;
	}

	#ifdef ENABLE_DEBUG_PINS_TIMERS
	{
		LL_GPIO_ResetOutputPin(DEBUG_1_PORT, DEBUG_1_PIN);
	}
	#endif
	
	if(sendStatus == AT_OK)
	{
		Debug_printf("Send Success\r\n");
		//reset the LoRa radio
		lora_sleep();
		return transmit_status_success;
	}
	else
	{
		Debug_printf("Send Fail\r\n");
		//reset the LoRa radio
		lora_sleep();
		return transmit_status_no_send;
	}
}

transmit_status_e Lora_Uplink(uint8_t payload[], uint8_t size)
{
	//reset the watchdog on enter, to prevent device reset
	reset_watchdog();
	
	//two ascii bytes per data byte, plus "1:" and a null terminator
	char txASCII[(size*2)+3];
	int i = 0;
	int pos = 0;
	
	//Start
	
	reset_watchdog();
	
	//encode as ascii for transmission
	pos = LoRaFormatTxString(&txASCII[0], sizeof(txASCII), "1:\0");
	

	reset_watchdog();
	for(i=0;i< size;i++)
	{
		pos += LoRaFormatTxString(&txASCII[pos], sizeof(txASCII)-pos, "%02X\0", payload[(size-1)-i]);
		reset_watchdog();
	}
	
	reset_watchdog();
	return sendData(txASCII);
}

void save_lora_config_page(void)
{
	radio_config_page_layout_t config_page = {0};
	for(int i = 0; i< sizeof(config_page.members.join_channel_list)/sizeof(config_page.members.join_channel_list[0]); i++)
	{
		config_page.members.join_channel_list[i] = channel_list[i];
	}
	
	save_extra_config_page(config_page.raw_bytes, radio_config_page);
}

void load_lora_config_page(void)
{
	radio_config_page_layout_t config_page = {0};
	uint16_t mask = 0;
	
	load_extra_config_page(config_page.raw_bytes, radio_config_page);
	
	for(int i = 0; i< sizeof(config_page.members.join_channel_list)/sizeof(config_page.members.join_channel_list[0]); i++)
	{
		channel_list[i] = config_page.members.join_channel_list[i];
		mask |= channel_list[i];
	}
	
	//if we have no channels configured, we enable all channels
	if(mask == 0)
	{
		channel_list[0] = 0xFFFF;
		channel_list[1] = 0x0000;
		channel_list[2] = 0x0000;
		channel_list[3] = 0x0000;
		channel_list[4] = 0x0000;
		channel_list[5] = 0x0000;
	}
}


