/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	

	Maintainer: Shea Gosnell


*/

#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "debug_uart.h"
#include "lora_sensum.h"
#include "global.h"
#include "delays.h"
#include "watchdog.h"
#include "onewire.h"
#include "flash_map.h"
#include "radio_common.h"
#include "ds18b20.h"

#define NUM_PROBE_ADDRESS_SLOTS 6

static int16_t upper_temperature_threshold      = INT16_MAX;
static int16_t lower_temperature_threshold      = INT16_MIN;
static bool upper_temperature_threshold_enabled = false;
static bool lower_temperature_threshold_enabled = false;
static uint8_t temperature_threshold_wakeups    = 1;

//latch for upper temperature threshold on the custom device for Rayza
static bool ds18b20_latch = false;
//pin changed flag for the on each wakeup condition
static bool pin_changed = false;

//store the current states of the pins, according to the interrupt
static bool input1_state = false;
static bool input2_state = false;
static bool input3_state = false;


typedef enum
{
	RESULT_OK           = 0,
	RESULT_DATA_OPEN    = 1,
	RESULT_DATA_SHORT   = 2,
	RESULT_CRC_ERR      = 3,
}result_status_e;

typedef struct
{
	int16_t temperature;
	result_status_e status;
}ds18b20_result_t;

typedef struct
{
       int16_t temperature[NUM_PROBE_ADDRESS_SLOTS];
       result_status_e status;
}six_ds18b20_result_t;



ds18b20_result_t ds18b20_read      (void);
ds18b20_result_t ds18b20_read_multi(uint64_t address);

uint8_t wakeups_per_ds18b20_threshold = 0;
uint64_t probe_id[NUM_PROBE_ADDRESS_SLOTS] = {0};

unsigned char crc_bits(unsigned char data)
{
	//https://stackoverflow.com/a/36505819
    unsigned char crc = 0;
    if(data & 1)     crc ^= 0x5e;
    if(data & 2)     crc ^= 0xbc;
    if(data & 4)     crc ^= 0x61;
    if(data & 8)     crc ^= 0xc2;
    if(data & 0x10)  crc ^= 0x9d;
    if(data & 0x20)  crc ^= 0x23;
    if(data & 0x40)  crc ^= 0x46;
    if(data & 0x80)  crc ^= 0x8c;
    return crc;
}

uint8_t calc_crc(uint8_t *data, uint8_t size)
{
	//https://stackoverflow.com/a/36505819
    uint8_t crc = 0;
	uint8_t i;
    for ( i = 0; i < size; ++i )
    {
         crc = crc_bits(data[i] ^ crc);
    }
    return crc;
}

void ds18b20_uplink()
{
	ds18b20_result_t result = {0};	
	ds18b20_payload_t payload = {0};
	
	result = ds18b20_read();
	
	payload.members.temperature = result.temperature;
	payload.members.status = result.status;
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	//now form the packet and transmit
	Uplink(payload.payload, DS18B20_SIZE);
}

void ds18b20_uplink_multi()
{
	six_ds18b20_result_t result = {0};
	ds18b20_result_t     temp_result = {0};
	six_ds18b20_payload_t payload = {0};
	uint8_t i = 0;
	
	//iterate through to get all temperatures
	for(i=0;i<NUM_PROBE_ADDRESS_SLOTS;i++)
	{
		temp_result = ds18b20_read_multi(probe_id[i]);
		result.temperature[i] = temp_result.temperature;
		
		if(result.status == RESULT_OK)
		{
			result.status = temp_result.status;
		}
	}
	
	payload.members.Temperature1 = result.temperature[0] & 0x0FFF;
	payload.members.Temperature2 = result.temperature[1] & 0x0FFF;
	payload.members.Temperature3 = result.temperature[2] & 0x0FFF;
	payload.members.Temperature4 = result.temperature[3] & 0x0FFF;
	payload.members.Temperature5 = result.temperature[4] & 0x0FFF;
	payload.members.Temperature6 = result.temperature[5] & 0x0FFF;
	payload.members.owpstat      = result.status;
	
	payload.members.sys_voltage  = fourBit_battery_calculation();
	payload.members.pkt_type     = packet_type_data;
	
	//get the input states
	payload.members.input1 = input1_state;
	payload.members.input2 = input2_state;
	payload.members.input3 = input3_state;
	
	//get the latch state
	payload.members.latch = ds18b20_latch;
	
	//Prints
	Debug_printf("Input 1      :%d\r\n", payload.members.input1);
	Debug_printf("Input 2      :%d\r\n", payload.members.input2);
	Debug_printf("Input 3      :%d\r\n", payload.members.input3);
	Debug_printf("Latch        :%d\r\n", payload.members.latch );
	Debug_printf("Status       :%d\r\n", payload.members.owpstat);
	
	//now form the packet and transmit
	Uplink(payload.payload, THREE_DS18B20_SIZE);
}


ds18b20_result_t ds18b20_read()
{
	ds18b20_result_t result = {0};
	uint8_t data[9];
	uint8_t crc = 0xFF;
	int i;
	int attempts = 3;
	static TimerEvent_t ds18b20ReadTimer;
	
	while(crc != 0x00 && attempts && data[5] != 0xFF && data[7] != 0x10)
	{
		attempts--;
		crc = 0xFF;
		/*
		1) Send reset
		2) Send skip_rom (0xCC)
		3) Send convert_t (0x44)
		4) Slave will drive line low. Wait for line to go high
		5) Send init
		6) Send skip_rom (0xCC)
		7) Send read_scratchpad (0xBE)
		8) Read 9 bytes from the slave
			Byte format will be:
				T_LSB
				T_MSB
				User1
				User2
				Config
				Reserved (0xFF)
				Reserved
				Reserved (0x10)
				CRC
			The CRC is of type CRC-8/MAXIM
				The CRC of the entire 9-byte packet should be 0x00.
		*/
		
		dbg_owp("\r\nReading sensor\r\n");
		await_uart_tx();
		
		OWP_reset_bus();
		OWP_write_byte(0xCC);
		OWP_write_byte(0x44);
		
		start_timeout_timer(&ds18b20ReadTimer, 1000);
		
		//this operation can take up to 750ms, abort after 1 second
		while(!OWP_read_byte())
		{ 
			//abort if timeout expired.
			if(timer_expired(&ds18b20ReadTimer))
			{
				break;
			}
			//intentional delay,
			reset_watchdog();
		}
		stop_timeout_timer(&ds18b20ReadTimer);
		
		OWP_reset_bus();
		OWP_write_byte(0xCC);
		OWP_write_byte(0xBE);
		
		for(i=0;i<9;i++)
		{
			data[i] = OWP_read_byte();
			dbg_owp("data[%d]=%02X\r\n", i, data[i]);
			await_uart_tx();
		}
		
		//should check CRC(/validate data) here
		crc = calc_crc(data,9);
		dbg_owp("CRC:%02X\r\n", crc);
	}
	//if crc != 0x00, then we have invalid data.
	
	//now convert the data to a temperature
	//The two data registers form a 16-bit sign-extended twos-compliment number.
	//This is fixed point, with the four LSbits being 2^-1 -> 2^-4. This means 
	//the result is 16* too large.
	result.temperature = data[0]+(data[1]<<8);
	//conversion for display
	float temp = (float)result.temperature;
	temp = temp/16;
	int32_t toDisplay = (int32_t)(temp *1000);
	
	Debug_printf("Temperature:%d.%04d\r\n", toDisplay/1000, toDisplay%1000);
	await_uart_tx();
	
	result.status = RESULT_OK;
	if(crc != 0x00 || data[5] != 0xFF || data[7] != 0x10)
	{
		result.temperature = 0;
		Debug_printf("DATA_INVALID\r\n");
		if(data[7] == 0xFF)
		{
			//open circuit
			result.status =RESULT_DATA_OPEN;
		}
		else if(data[7] == 0x00)
		{
			//closed circuit
			result.status =RESULT_DATA_SHORT;
		}
		else
		{
			//CRC/validation error
			result.status =RESULT_CRC_ERR;
		}
	}
	
	return result;
}


ds18b20_result_t ds18b20_read_multi(uint64_t address)
{
	ds18b20_result_t result = {0};
	uint8_t data[9];
	uint8_t crc = 0xFF;
	int i;
	int probe_id_iterator = 0;
	int attempts = 3;
	static TimerEvent_t ds18b20ReadTimer;
	
	result.status = RESULT_OK;
	
	crc=0xFF;
	data[5] = 0;
	data[7] = 0;
	attempts = 3;
	while(crc != 0x00 && attempts && data[5] != 0xFF && data[7] != 0x10)
	{
		attempts--;
		crc = 0xFF;
		/*
		1) Send reset
		2) Send skip_rom (0xCC)
		3) Send convert_t (0x44)
		4) Slave will drive line low. Wait for line to go high
		5) Send init
		6) Send skip_rom (0xCC)
		7) Send read_scratchpad (0xBE)
		8) Read 9 bytes from the slave
			Byte format will be:
				T_LSB
				T_MSB
				User1
				User2
				Config
				Reserved (0xFF)
				Reserved
				Reserved (0x10)
				CRC
			The CRC is of type CRC-8/MAXIM
				The CRC of the entire 9-byte packet should be 0x00.
		*/
		
		dbg_owp("\r\nReading sensor\r\n");
		await_uart_tx();
		
		OWP_reset_bus();
		OWP_write_byte(0x55);
		
		//send the device address
		for(probe_id_iterator = 0; probe_id_iterator < 8; probe_id_iterator ++)
		{
			OWP_write_byte((address>>(8*probe_id_iterator))&0xFF);
		}
		
		OWP_write_byte(0x44);
		
		start_timeout_timer(&ds18b20ReadTimer, 1000);
		
		//this operation can take up to 750ms, abort after 1 second
		while(!OWP_read_byte())
		{ 
			//abort if timeout expired.
			if(timer_expired(&ds18b20ReadTimer))
			{
				break;
			}
			//intentional delay,
			reset_watchdog();
		}
		stop_timeout_timer(&ds18b20ReadTimer);
		
		OWP_reset_bus();
		OWP_write_byte(0x55);
		
		//send the device address
		for(probe_id_iterator = 0; probe_id_iterator < 8; probe_id_iterator ++)
		{
			OWP_write_byte((address>>(8*probe_id_iterator))&0xFF);
		}
		
		OWP_write_byte(0xBE);
		
		for(i=0;i<9;i++)
		{
			data[i] = OWP_read_byte();
			dbg_owp("data[%d]=%02X\r\n", i, data[i]);
			await_uart_tx();
		}
		
		//should check CRC(/validate data) here
		crc = calc_crc(data,9);
		dbg_owp("CRC:%02X\r\n", crc);
	}
	//if crc != 0x00, then we have invalid data.
	
	//now convert the data to a temperature
	//The two data registers form a 16-bit sign-extended twos-compliment number.
	//This is fixed point, with the four LSbits being 2^-1 -> 2^-4. This means 
	//the result is 16* too large.
	result.temperature = data[0]+(data[1]<<8);
	//conversion for display
	float temp = (float)result.temperature;
	temp = temp/16;
	int32_t toDisplay = (int32_t)(temp *1000);
	
	Debug_printf("Temperature:%d.%04d\r\n", toDisplay/1000, toDisplay%1000);
	await_uart_tx();
	
	if(crc != 0x00 || data[5] != 0xFF || data[7] != 0x10)
	{
		result.temperature = 0;
		Debug_printf("DATA_INVALID\r\n");
		if(data[7] == 0xFF)
		{
			//open circuit
			result.status =RESULT_DATA_OPEN;
		}
		else if(data[7] == 0x00)
		{
			//closed circuit
			result.status =RESULT_DATA_SHORT;
		}
		else
		{
			//CRC/validation error
			result.status =RESULT_CRC_ERR;
		}
	}
	
	return result;
}


void ds18b20_onWakeup()
{
	ds18b20_result_t result = {0};
	if(wakeup_count % wakeups_per_uplink == 0)
	{
		Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
		ds18b20_uplink();
		return;
	}
	
	//this is only executed if at least one of the thresholds is enabled
	if((wakeup_count % temperature_threshold_wakeups == 0) && (upper_temperature_threshold_enabled || lower_temperature_threshold_enabled))
	{
		Debug_printf("Threshold Check (%d)\r\n", temperature_threshold_wakeups);
		result = ds18b20_read();
		
		if((result.temperature > upper_temperature_threshold) && upper_temperature_threshold_enabled)
		{
			Debug_printf("Upper Threshold exceeded\r\n");
			ds18b20_uplink();
			return;
		}
		
		if((result.temperature < lower_temperature_threshold) && lower_temperature_threshold_enabled)
		{
			Debug_printf("Lower Threshold exceeded\r\n");
			ds18b20_uplink();
			return;
		}
	}
}

void ds18b20_mulit_on_each_wakeup()
{
	if(pin_changed)
	{
		Debug_printf("Interrupt Uplink\r\n");
		//schedule an immediate uplink
		ds18b20_uplink_multi();
		pin_changed = false;
	}
}

void pin_change_irq()
{
	input1_state = HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN);
	input2_state = HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN);
	input3_state = HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN);
	
	Debug_printf("Input Changed: %d%d%d \r\n", input1_state, input2_state, input3_state);
	
	pin_changed = true;
	
	ds18b20_latch = false;
	Debug_printf("Latch Cleared - Pin Change\r\n");
}

//returns true iff this threshold triggered an uplink
bool multi_ds18b20_check_thresholds()
{
	ds18b20_result_t result = {0};	

	//If the temperature returns above 85 Degrees C, then we can clear the latch
	if(ds18b20_latch)
	{
		Debug_printf("Checking to clear latch\r\n");
		
		result = ds18b20_read_multi(probe_id[0]);
		
		//only check upper temperature if latch is clear
		if(result.temperature > upper_temperature_threshold)
		{
			Debug_printf("Upper Threshold exceeded\r\n");
			Debug_printf("Latch Cleared - Upper Threshold\r\n");
			ds18b20_latch = false;
		}
		//we return here, because the alarm cannot be resent while the latch is on.
		return false;
	}

	//if the latch is not enabled, and D3 is high, then we check the
	//temperature for the alarm condition.
	//The alarm condition here is the temperature falling below 83 Degrees C
	if(!input3_state)
	{
		Debug_printf("Checking for low temperature\r\n");
		result = ds18b20_read_multi(probe_id[0]);
		//If we are below the lower threshold, we have to send the alarm and turn the latch on
		if(result.temperature < lower_temperature_threshold)
		{
			Debug_printf("Lower Threshold exceeded\r\n");
			ds18b20_latch = true;
			Debug_printf("Latch Set - Lower Threshold\r\n");
			ds18b20_uplink_multi();
			//return true, beacause we triggered an uplink
			return true;
		}
	}
	else
	{
		Debug_printf("D3 high\r\n");
	}
	return false;


}

void multi_ds18b20_onWakeup()
{
	//if the threshold check doesn't trigger an uplink, we can check for regularly scheduled uplinks.
	if(!multi_ds18b20_check_thresholds())
	{
		if(wakeup_count % wakeups_per_uplink == 0)
		{
			Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
			ds18b20_uplink_multi();
			return;
		}
	}

}


void six_ds18b20_save_config()
{
	//save the id array to a config page in flash
	six_ds18b20_config_extra_page_layout_t config = {0};
	uint8_t i = 0;
	
	for(i=0;i<NUM_PROBE_ADDRESS_SLOTS;i++)
	{
		config.members.id[i] = probe_id[i];
	}

	save_extra_config_page(config.raw_bytes, device_specific_page_2);
	
	ds18b20_save_config();
}

void six_ds18b20_load_config()
{
	//save the id array to a config page in flash
	six_ds18b20_config_extra_page_layout_t config = {0};
	uint8_t i = 0;
	
	load_extra_config_page(config.raw_bytes, device_specific_page_2);
	
	for(i=0;i<NUM_PROBE_ADDRESS_SLOTS;i++)
	{
		probe_id[i] = config.members.id[i];
	}

	ds18b20_load_config();
}

void ds18b20_save_config()
{
	ds18b20_config_page_layout_t config = {0};
	
	config.members.Temperature_threshold_upper.s_threshold = upper_temperature_threshold;
	config.members.Temperature_threshold_upper.enabled     = upper_temperature_threshold_enabled;
	config.members.Temperature_threshold_upper.wakeups     = temperature_threshold_wakeups;
	
	config.members.Temperature_threshold_lower.s_threshold = lower_temperature_threshold;
	config.members.Temperature_threshold_lower.enabled     = lower_temperature_threshold_enabled;
	config.members.Temperature_threshold_lower.wakeups     = 0;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void ds18b20_load_config()
{
	ds18b20_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	
	upper_temperature_threshold          = config.members.Temperature_threshold_upper.s_threshold;
	upper_temperature_threshold_enabled  = config.members.Temperature_threshold_upper.enabled;
	temperature_threshold_wakeups        = config.members.Temperature_threshold_upper.wakeups;

	lower_temperature_threshold          = config.members.Temperature_threshold_lower.s_threshold;
	lower_temperature_threshold_enabled  = config.members.Temperature_threshold_lower.enabled;
	
}

void ds18b20_onDownlink(uint8_t *buffer, uint8_t size)
{
	//downlink_type_threshold_1
	threshold_downlinks_t downlink = {0};
	int i;
	
	Debug_printf("ds18b20 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[THRESHOLD_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.threshold.downlink_type == downlink_type_ds18b20_temperature)
	{
		Debug_printf("Temperature Threshold Downlink\r\n");
		
		Debug_printf("Temperature Upper: %d.%03d\r\n"   ,downlink.threshold.upper/16, ((downlink.threshold.upper*1000)/16)%1000);
		Debug_printf("Temperature Lower: %d.%03d\r\n"   ,downlink.threshold.lower/16, ((downlink.threshold.lower*1000)/16)%1000);
		Debug_printf("Upper Enabled    : %s\r\n"        ,downlink.threshold.enabled_h ? "YES":"NO");
		Debug_printf("Lower Enabled    : %s\r\n"        ,downlink.threshold.enabled_l ? "YES":"NO");
		Debug_printf("Update Upper     : %s\r\n"        ,downlink.threshold.update_h  ? "YES":"NO");
		Debug_printf("Update Lower     : %s\r\n"        ,downlink.threshold.update_l  ? "YES":"NO");
		Debug_printf("Interval         : %d wakeups\r\n",downlink.threshold.period);
		Debug_printf("Update Interval  : %s\r\n"        ,downlink.threshold.period    ? "YES":"NO");
		
		if(downlink.threshold.update_h)
		{
			upper_temperature_threshold = downlink.threshold.upper;
			upper_temperature_threshold_enabled = downlink.threshold.enabled_h;
		}
		
		if(downlink.threshold.update_l)
		{
			lower_temperature_threshold = downlink.threshold.lower;
			lower_temperature_threshold_enabled = downlink.threshold.enabled_l;
		}
		
		if(downlink.threshold.period != 0)
		{
			temperature_threshold_wakeups = downlink.threshold.period;
		}
	}
	
	ds18b20_save_config();
}


void ds18b20_cli_threshold_help()
{
	Debug_printf("Usage: threshold [enable|disable] [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\tEnable or Disable [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold set [threshold] [value]\r\n");
	await_uart_tx();
	Debug_printf("\tset [threshold] trigger value to [value]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	await_uart_tx();
	Debug_printf("\t[value] is a signed integer\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold wakeup [value]\r\n");
	await_uart_tx();
	Debug_printf("\tCheck threshold every [value] wakeups\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold show\r\n");
	await_uart_tx();
	Debug_printf("\tShows the current threshold configuration\r\n");
	await_uart_tx();
}

void ds18b20_cli_threshold(int argc, char *argv[])
{
	//"threshold enable t_upper"
	//"threshold disable t_upper"
	//"threshold set t_upper [value]"
	float value;
	
	if(argc == 1)
	{
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("Upper threshold        :%d.%03d\r\n", upper_temperature_threshold/16, ((upper_temperature_threshold*1000)/16)%1000);
			await_uart_tx();
			Debug_printf("Upper threshold Enabled:%s\r\n", upper_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("lower threshold        :%d.%03d\r\n", lower_temperature_threshold/16, ((lower_temperature_threshold*1000)/16)%1000);
			await_uart_tx();
			Debug_printf("lower threshold Enabled:%s\r\n", lower_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("Checking thresholds every %d wakeups\r\n", temperature_threshold_wakeups);
			await_uart_tx();
			return;
		}
	}
	
	if(argc == 2)
	{
		if(!strcmp(argv[0],"wakeup"))
		{
			uint8_t value = atoi(argv[1]);
			
			if(value <= 0 || value >= 127)
			{
				Debug_printf("Value out of range\r\n");
			}
			
			temperature_threshold_wakeups = value;
			Debug_printf("Check temperature every %d wakeups\r\n", temperature_threshold_wakeups);
			return;
			
		}
		if(!strcmp(argv[0], "enable"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//enable upper
				upper_temperature_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//enable lower
				lower_temperature_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
		}
		if(!strcmp(argv[0], "disable"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//disable upper
				upper_temperature_threshold_enabled = false;
				Debug_printf("Upper threshold Disabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//disable lower
				lower_temperature_threshold_enabled = false;
				Debug_printf("Lower threshold Disabled\r\n");
				return;
			}
		}
	}
	
	if(argc == 3)
	{
		value = atof(argv[2]);
		
		if(!strcmp(argv[0], "set"))
		{
			//options here are upper or lower
			if(!strcmp(argv[1], "t_upper"))
			{
				//set upper
				upper_temperature_threshold = (int)(value*16);
				Debug_printf("Upper threshold set to %d.%03d\r\n", upper_temperature_threshold/16, ((upper_temperature_threshold*1000)/16)%1000);
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//set lower
				lower_temperature_threshold = (int)(value*16);
				Debug_printf("Lower threshold set to %d.%03d\r\n", lower_temperature_threshold/16, ((lower_temperature_threshold*1000)/16)%1000);
				return;
			}
		}
	}
	
	ds18b20_cli_threshold_help();
}

static uint8_t axtoi(char input)
{
	//handle simplest case (number) first.
	if(input >= '0' && input <= '9')
	{
		return input - '0';
	}
	
	input &= ~0x20;
	
	if(input >= 'A' && input <='F')
	{
		return  10 + (input - 'A');
	}
	
	return 0;
}

//assume that input is zero-terminated, ascii encoded, hex
uint64_t process64hex(char* input)
{
	uint64_t result;
	//notice that we are processing ascii encoded hex, so each character we read
	//is 4 bits
	int i = 16;
	//we also make the assumption that there are no characters outside [0-9,A-F]
	while(*input && i)
	{
		result = result << 4;
		result += axtoi(*input);
		input++;
		i--;
	}
	
	return result;
}

void ds18b20_cli_device_help()
{
	Debug_printf("Usage: device show\r\n");
	Debug_printf("\tShows device configuration\r\n");
	Debug_printf("Usage: device probes [slot] [address]\r\n");
	Debug_printf("\tSets the probe in [slot] to [address]\r\n");
	Debug_printf("\tThe slots are used to identify the probes in the data packets\r\n");
	Debug_printf("\tThe probe in the first slot (0) is used for the threshold check\r\n");
}

void ds18b20_cli_device(int argc, char *argv[])
{
	uint64_t address = 0;
	uint8_t slot = 0;
	
	if(argc == 1)
	{
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("|SLOT |      PROBE ID      |\r\n");
			Debug_printf("|-----|--------------------|\r\n");
			await_uart_tx();
			//"|-----|--------------------|"
			//"|  5  | 0xFFFFFFFFFFFFFFFF |"
			for(slot=0;slot<NUM_PROBE_ADDRESS_SLOTS;slot++)
			{
				
				Debug_printf("|  %d  | 0x%08X%08X |\r\n", slot
				                                        , (uint32_t)(probe_id[slot]>>32)
				                                        , (uint32_t)(probe_id[slot] & 0xFFFFFFFF));
				await_uart_tx();
			}
			Debug_printf("\r\n");
		}
	}
	
	//command device probes X Y
	// Used to set probe ID slot. X is slot, Y is 64-Bit address
	// we will need a 16 Hex char input for the address.
	if(argc == 3)
	{
		if(!strcmp(argv[0],"probes"))
		{
			//process argv[1] as the slot
			slot = atoi(argv[1]);
			
			if(slot >= NUM_PROBE_ADDRESS_SLOTS)
			{
				ds18b20_cli_device_help();
				return;
			}
			
			//process argv[2] as the address
			address = process64hex(argv[2]);
			
			Debug_printf("Slot: %d", slot);
			Debug_printf("ID  : 0x%08X%08X\r\n", (uint32_t)(address>>32), (uint32_t)(address & 0xFFFFFFFF));
			
			probe_id[slot] = address;
		}
	}
}

void ds18b20_init(void)
{
	OWP_init(COUNT1_PORT, COUNT1_PIN);
}

void multi_ds18b20_init()
{
	OWP_init(MODBUS_TX_PORT, MODBUS_TX_PIN);
	
	//initialise digital inputs
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	//alternate function doesn't matter, because this is not configured for af.
	
	HW_GPIO_Init(COUNT1_PORT    , COUNT1_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
	HW_GPIO_Init(COUNT3_PORT    , COUNT3_PIN    , &GPIO_InitStruct);	
	
	//set all three inputs to fire an interrupt on change (either edge)
	LL_EXTI_EnableRisingTrig_0_31 (COUNT1_PIN);
	LL_EXTI_EnableFallingTrig_0_31(COUNT1_PIN);
	
	LL_EXTI_EnableRisingTrig_0_31 (COUNT2_PIN);
	LL_EXTI_EnableFallingTrig_0_31(COUNT2_PIN);
	
	LL_EXTI_EnableRisingTrig_0_31 (COUNT3_PIN);
	LL_EXTI_EnableFallingTrig_0_31(COUNT3_PIN);
	
	HW_GPIO_SetIrq(COUNT1_PORT, COUNT1_PIN, 3, pin_change_irq);
	HW_GPIO_SetIrq(COUNT2_PORT, COUNT2_PIN, 3, pin_change_irq);
	HW_GPIO_SetIrq(COUNT3_PORT, COUNT3_PIN, 3, pin_change_irq);
	
	//manually set the upper threshold to 85 Degrees C, and the lower threshold to 83 Degrees C
	//upper_temperature_threshold = 85*16;
	//lower_temperature_threshold = 83*16;
	
	//get current input states
	input1_state = HW_GPIO_Read(COUNT1_PORT, COUNT1_PIN);
	input2_state = HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN);
	input3_state = HW_GPIO_Read(COUNT3_PORT, COUNT3_PIN);
	
	ds18b20_latch = false;
	Debug_printf("Latch Cleared - Startup\r\n");
	pin_changed = false;
	
	
	OWP_getSingleID();
}
