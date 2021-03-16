/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	I2C register level driver for LoRa only implementation project
								Implements I2C flash external periphheral.

	Maintainer: Shea Gosnell


*/


#include "i2c1.h"
#include "i2cLightSensor.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#include "debug_uart.h"
#include "global.h"
#include "delays.h"
#include "flash_map.h"

#include "watchdog.h"
#include "radio_common.h"
#include "sht30.h"

#define SI1133_I2C_ADDR 0x55

//Command register commands
#define RESET_CMD_CTR 0x00 //Resets Response0 command counter to 0
#define RESET_SW      0x01 //Forces a sensor reset, all programmed values will be lost?
#define FORCE         0x11 //Stars a FORCED measurement
#define PAUSE         0x12 //Pauses autonomous measurements
#define START         0x13 //Starts autonomous measurements
#define PARAM_QUERY   0x40 //Read  a parameter, specified by the 6LSB (from parameter table)
#define PARAM_SET     0x80 //Write a parameter, specified by the 6LSB (from parameter table)

//Parameter Table, needs to be indirectly accesses
#define I2C_ADDR      0x00
#define CHAN_LIST     0x01
#define ADCCONFIG0    0x02
#define ADCSENS0      0x03
#define ADCPOST0      0x04
#define MEASCONFIG0   0x05
#define ADCCONFIG1    0x06
#define ADCSENS1      0x07
#define ADCPOST1      0x08
#define MEASCONFIG1   0x09
#define ADCCONFIG2    0x0A
#define ADCSENS2      0x0B
#define ADCPOST2      0x0C
#define MEASCONFIG2   0x0D
#define ADCCONFIG3    0x0E
#define ADCSENS3      0x0F
#define ADCPOST3      0x11
#define MEASCONFIG3   0x12
#define ADCCONFIG4    0x13
#define ADCSENS4      0x14
#define ADCPOST4      0x15
#define MEASCONFIG4   0x16
#define ADCCONFIG5    0x17
#define ADCSENS5      0x18
#define ADCPOST5      0x19
#define MEASCONFIG5   0x1A
#define MEASRATE_H    0x1B
#define MEASRATE_L    0x1C
#define MEASCOUNT0    0x1D
#define MEASCOUNT1    0x1E
#define MEASCOUNT2    0x1F
#define THRESHOLD0_H  0x25
#define THRESHOLD0_L  0x26
#define THRESHOLD1_H  0x27
#define THRESHOLD1_L  0x28
#define THRESHOLD2_H  0x29
#define THRESHOLD2_L  0x2A
#define BURST         0x2B

//I2C Registers, direct access
#define PART_ID       0x00 //Should always read 0x33
#define HW_ID         0x01 //
#define REV_ID        0x02 //
#define INFO0         0x03 //
#define INFO1         0x04 //
#define HOSTIN3       0x07 //
#define HOSTIN2       0x08 //
#define HOSTIN1       0x09 //
#define HOSTIN0       0x0A //Used to write values to the parameter table
#define COMMAND       0x0B //Used to issue commands to the sensor
#define IRQ_ENABLE    0x0F //Set which channels trigger an Interrupt
#define RESPONSE1     0x10 //
#define RESPONSE0     0x11 //Shows device status and command success/error status
#define IRQ_STATUS    0x12 //Shows which channels have completed reads, clears on read
#define HOSTOUT_START 0x13 //Output buffer start
#define HOSTOUT_LIMIT 0x2C //

typedef struct
{
	uint8_t
		running   :1,
		suspend   :1,
		sleep     :1,
		cmd_error :1,
		cmd_count :4;
	bool error_timeout;
}response0_t;

//Command Error Codes
typedef enum
{
	command_error_invalid_command        = 0x0, //The command issues was not recognised by the unit
	command_error_parameter_invalid      = 0x1, //The parameter address was out of range
	command_error_adc_saturation         = 0x2, //The ADC saturated, or the accumulator overflowed
	command_error_output_buffer_overflow = 0x3, //The output buffer overflowed
}command_error_e;

 /********************************************************************
 *Function Prototypes                                                *
 ********************************************************************/
static void init_sensor_for_reading(void);
static void write_parameter(uint8_t parameter, uint8_t value);
static void await_reading(void);
static void start_reading(void);
static void reset_command_counter(void);
static void reset_sensor(void);
static response0_t read_response_register(void);

/********************************************************************
*File Scope Variables                                               *
********************************************************************/
#define HW_GAIN_MIN 0
#define HW_GAIN_MAX 11
#define SW_GAIN_MIN 0
#define SW_GAIN_MAX 7
uint8_t hw_gain    = 5; //RANGE 0-11
uint8_t sw_gain    = 7 ; //RANGE 0-7
uint8_t post_shift = 7 ; //RANGE 0-7

/********************************************************************
 *Functions                                                         *
 *******************************************************************/
static response0_t read_response_register()
{
	static TimerEvent_t write_parameter_timer ;
	uint8_t             tx_data_buffer[1]     ;
	uint8_t             rx_data_buffer[1]     ;
	response0_t         value = {0}           ;
	
	tx_data_buffer[0] = RESPONSE0; 
	
	
	//use a 100 milli-second timeout
	start_timeout_timer(&write_parameter_timer, 100);
	
	do
	{		
		i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 1, 0);
		i2c1_receive_feedback(SI1133_I2C_ADDR, rx_data_buffer, 1, 1);
		
		value.cmd_count = (rx_data_buffer[0] & 0x0F)     ;
		value.cmd_error = (rx_data_buffer[0] & 0x10) >> 4;
		value.running   = (rx_data_buffer[0] & 0x20) >> 5;
		value.sleep     = (rx_data_buffer[0] & 0x30) >> 6;
		value.suspend   = (rx_data_buffer[0] & 0x40) >> 7;
		
		//this would give us ~7 attempts to get the response in the 100ms
		delay_timeout_ms(10);
	}while(value.cmd_count == 0 && value.cmd_error == 0);
	
	if(timer_expired(&write_parameter_timer))
	{
		//let the caller know that the read timedout
		value.error_timeout = 1;
	}
	
	stop_timeout_timer(&write_parameter_timer);

	return value;
}


static void reset_command_counter()
{
	//Reset the command counter to 0
	
	//Issue the RESET_CMD_CTR command to set the command counter to 0
	//read RESPONSE0 to ensure that the counter is set to 0	
	uint8_t tx_data_buffer[2];
	
	//we need to set up COMMAND to contain RESET_CMD_CTR
	tx_data_buffer[0] = COMMAND;
	tx_data_buffer[1] = RESET_CMD_CTR;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 2, 1);
}

static void reset_sensor()
{
	//Reset the sensor to power-on state
	
	//Issue the RESET_SW command to set the command counter to 0
	//read RESPONSE0 to ensure that the counter is set to 0	
	uint8_t tx_data_buffer[2];
	
	//we need to set up COMMAND to contain RESET_CMD_CTR
	tx_data_buffer[0] = COMMAND;
	tx_data_buffer[1] = RESET_SW;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 2, 1);
}

//this function will send the command to start the reading, and ensure that it is
//correctly processed
static void start_reading()
{
	//Sending Command FORCE
	//	Write register 0x0B with value 0x11
	uint8_t tx_data_buffer[2];
	
	//we need to set up COMMAND to contain RESET_CMD_CTR
	tx_data_buffer[0] = COMMAND;
	tx_data_buffer[1] = FORCE;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 2, 1);
	
	//we should probably handle the error/timeout cases here
	read_response_register();
}

//this command will wait for the interrupt line to trigger, and then read the status register
//to ensure that all channels have finished processing
static void await_reading()
{
	//The COUNT2 line should Idle low while we are waiting for data to come in.
	//lets use a ~1 second timeout to wait for the data.
	static TimerEvent_t await_reading_timer;
	
	start_timeout_timer(&await_reading_timer, 1000);
	
	while(!timer_expired(&await_reading_timer) && HW_GPIO_Read(COUNT2_PORT, COUNT2_PIN))
	{
		//delaying here should put us into low power mode, which will save some power while waiting
		delay_timeout_ms(20);
	}
	
	//we should indicate that the timeout happened, instead of the falling edge?
}
 
Si1133_power_state_e get_sensor_power_state()
{
	//read the COMMAND0 register, and return the three MSB
	//we want to & with 0xC0
	response0_t status = {0};
	
	status = read_response_register();
	
	if(status.running)
	{
		return Si1133_power_state_running;
	}
	
	if(status.sleep)
	{
		return Si1133_power_state_sleep;
	}
		
	if(status.suspend)
	{
		return Si1133_power_state_suspend;
	}
	
	return Si1133_power_state_unknown;
}

static void write_parameter(uint8_t parameter, uint8_t value)
{
	response0_t         command_response = {0};
	uint8_t             tx_data_buffer[3]     ;
	//Reset the command counter to 0
	reset_command_counter();
	
	//We need to set up HOSTIN0 to contain the value
	//we need to set up COMMAND to contain PARAM_SET + parameter
	//This can be done as a I2C continious write.
	tx_data_buffer[0] = HOSTIN0;
	tx_data_buffer[1] = value;
	tx_data_buffer[2] = PARAM_SET + parameter;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 3, 1);
	
	//We then need to poll the RESPONSE0 field for the counter incrementing (to 1)
	//this would indicate that the command has been processed.
	//we will use a timeout timer here to ensure that the program doesn't hang
	//waiting for the sensor
	command_response = read_response_register();

	
	//We need to look for RESPONSE0 bit 4, which is the error indicator
	//if RESPONSE0 bit 4 is set, then we need to determine the error code.
	if(command_response.cmd_error == 1)
	{
		//handle the error code?
	}
	
	if(command_response.error_timeout == 1)
	{
		//handle the timeout error?
	}
}

//we cannot gaurentee that there is not a power outage that the board survives while the sensor
//drops out. To mitigate this, we will need to initialise the sensor before each reading
static void init_sensor_for_reading()
{
	i2c1_init();
	uint8_t tx_data_buffer[2];
	
	//Enable Target channels in CHAN_LIST
	//	CHAN_LIST = 0x07
	//This will enable channels 0 1 and 2
	write_parameter(CHAN_LIST, 0x03);
	
	//Set channel 0 to be The small white photodiode
	//	ADCCONFIG0 = 0x0B
	write_parameter(ADCCONFIG0, 0x0D);
		
	//Set channel 1 to be The small IR photodiode
	//	ADCCONFIG1 = 0x00
	write_parameter(ADCCONFIG1, 0x00);
		
	//Set channel 2 to be The UV photodiode
	//	ADCCONFIG2 = 0x19
	//write_parameter(ADCCONFIG2, 0x19);
		
	//Set each channel to Normal Range, with 16 samples accumulation, and HW_GAIN 0
	//	ADCSENS0 = 0x40
	//	ADCSENS1 = 0x40
	//	ADCSENS2 = 0x40
	write_parameter(ADCSENS0, (sw_gain << 4) + hw_gain);
	write_parameter(ADCSENS1, (sw_gain << 4) + hw_gain);
	//write_parameter(ADCSENS2, 0x10);
	//We can change the SW gain later if nessessary by modifying from the range 0x00-0x30
	//while keeping the lower 4 bits at 0
	
	//Set each channel to 24 bit output, no shift out, no threshold
	//	ADCPOST0 = 0x40
	//	ADCPOST1 = 0x40
	//	ADCPOST2 = 0x40
	write_parameter(ADCPOST0, 0x40 + (post_shift << 3));
	write_parameter(ADCPOST1, 0x40 + (post_shift << 3));
	//write_parameter(ADCPOST2, 0x40);
		
	//Set MEASCONFIG for each channel to 0, as we are using FORCED mode
	//	MEASCONFIG0 = 0
	//	MEASCONFIG1 = 0
	//	MEASCONFIG2 = 0
	write_parameter(MEASCONFIG0, 0);
	write_parameter(MEASCONFIG1, 0);
	//write_parameter(MEASCONFIG2, 0);
	

	
	//ensure that the interrupt is enabled for the last channel that we are interesetd in
	tx_data_buffer[0] = IRQ_ENABLE;
	tx_data_buffer[1] = 0x02;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 2, 1);
}


//this actually returns a 24 bit reading, but the next storage class is 32 bits.
Si1133_reading_t read_light_level()
{
	Si1133_reading_t result;
	uint8_t rx_data_buffer[6];
	uint8_t tx_data_buffer[1];
	
	init_sensor_for_reading();
	
	start_reading();
	await_reading();

	//read the data from the output buffer registers
	tx_data_buffer[0] = HOSTOUT_START;
	i2c1_send_feedback(SI1133_I2C_ADDR, tx_data_buffer, 1, 0);
	i2c1_receive_feedback(SI1133_I2C_ADDR, rx_data_buffer, 9, 1);
	
	reset_sensor();
	
	//The data should be in 3 sets of 24 bits (3 x (3 x uint8_t)), with one set per reading
	//first should be the visible light
	//then the infra
	//finally the ultra
	result.visible_reading  = rx_data_buffer[0];
	result.visible_reading  = result.visible_reading << 8;
	result.visible_reading += rx_data_buffer[1];
	result.visible_reading  = result.visible_reading << 8;
	result.visible_reading += rx_data_buffer[2];
	
	result.infra_reading  = rx_data_buffer[3];
	result.infra_reading  = result.infra_reading << 8;
	result.infra_reading += rx_data_buffer[4];
	result.infra_reading  = result.infra_reading << 8;
	result.infra_reading += rx_data_buffer[5];
	
	Debug_printf("\r\nVisible:%08d\r\n", result.visible_reading);
	Debug_printf("Infra  :%08d\r\n", result.infra_reading);

	return result;
}

void continious_read_light_level()
{
	init_light_sensor();
	while(1)
	{
		read_light_level();
		delay_timeout_ms(1000);
	}
}

void light_sensor_uplink()
{
	LIGHT_payload_t payload = {0};
	
	sht30Reading_t ht_reading;
	Si1133_reading_t light_reading = {0};
	//we need to get the SHT30 readings, and the light sensor reading into a packet.
	ht_reading = sht30GetReading();
	light_reading = read_light_level();
	
	//truncate to uint16_t
	if(light_reading.visible_reading > 0xFFFF)
	{
		light_reading.visible_reading = 0xFFFF;
	}
	
	payload.members.Temperature1 = ht_reading.T1;
	payload.members.Temperature2 = ht_reading.T2;
	payload.members.Humidity1    = ht_reading.H1;
	payload.members.Humidity2    = ht_reading.H2;
	payload.members.Light        = light_reading.visible_reading;
	
	Debug_printf("Temperature1: %02d.%02d Deg C\r\n", payload.members.Temperature1/100,payload.members.Temperature1%100);
	Debug_printf("Humidity1   : %02d.%02d %RH\r\n"  , payload.members.Humidity1/100, payload.members.Humidity1%100);
	Debug_printf("Temperature2: %02d.%02d Deg C\r\n", payload.members.Temperature2/100,payload.members.Temperature2%100);
	Debug_printf("Humidity2   : %02d.%02d %RH\r\n"  , payload.members.Humidity2/100, payload.members.Humidity2%100);
	Debug_printf("Light       : %05d\r\n"           , payload.members.Light);
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	Uplink(payload.payload,CO2_SIZE);
}

void init_light_sensor()
{
	//set count2 as an input, and init I2C.
	GPIO_InitTypeDef GPIO_InitStruct;
	/* Digital Input Initialisation */
	GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_LOW;
	
	HW_GPIO_Init(COUNT2_PORT    , COUNT2_PIN    , &GPIO_InitStruct);
}

light_sensor_config_component_t get_light_sensor_config_values( void )
{
	light_sensor_config_component_t config;
	
	config.hw_gain = hw_gain;
	config.sw_gain = sw_gain;
	
	return config;
}
	
void set_light_sensor_config_values(light_sensor_config_component_t config)
{
	//casting the uint8_t variables to int16_t prevents a warning about pointless comparison
	//It is true that the comparison is currently pointless, but if we were to change the
	//definition of HW_GAIN_MIN or SW_GAIN_MIN then we would want to have this comparison in
	//place.
	if((int16_t)config.hw_gain >= HW_GAIN_MIN && config.hw_gain <= HW_GAIN_MAX)
	{
		hw_gain = config.hw_gain;
	}
	
	if((int16_t)config.sw_gain >= SW_GAIN_MIN && config.sw_gain <= SW_GAIN_MAX)
	{
		sw_gain    = config.sw_gain;
		post_shift = config.sw_gain;
	}
}

//this is a weird way of handleing this, but I believe it works for now.
static char extra_command_str[] = "\0ight ";
static void light_sensor_cli_help()
{
	Debug_printf("Usage: device %sgain [value]\r\n", extra_command_str);
	Debug_printf("\tSet the HW gain of the photodiode.\r\n");
	Debug_printf("\t[value] in range %d to %d", HW_GAIN_MIN, HW_GAIN_MAX);
	await_uart_tx();
	Debug_printf("Usage: device %saverage [value]\r\n", extra_command_str);
	Debug_printf("\tSet the SW Gain and Post-Division of the ADC.\r\n");
	Debug_printf("\t[value] in range %d to %d", SW_GAIN_MIN, SW_GAIN_MAX);
	await_uart_tx();
}

void light_sensor_cli_component(int argc, char *argv[])
{
	if(!strcmp(argv[0], "light"))
	{
		extra_command_str[0] = 'l';
		light_sensor_cli(argc-1, argv+1);
		extra_command_str[0] = 0;
	}
}

void light_sensor_cli(int argc, char *argv[])
{
	uint8_t value;
	
	if(argc < 1)
	{
		light_sensor_cli_help();
		return;
	}
	
	if(!strcmp(argv[0], "show"))
	{
		Debug_printf("Light Sensor hw_gain: %d\r\n", hw_gain);
		Debug_printf("Light Sensor sw_gain: %d\r\n", sw_gain);
		return;
	}
	
	if(argc < 2)
	{
		light_sensor_cli_help();
		return;
	}
	
	value = atoi(argv[1]);
	
	//casting the uint8_t variables to int16_t prevents a warning about pointless comparison
	//It is true that the comparison is currently pointless, but if we were to change the
	//definition of HW_GAIN_MIN or SW_GAIN_MIN then we would want to have this comparison in
	//place.
	if(!strcmp(argv[0], "gain"))
	{
		if((int16_t)value >= HW_GAIN_MIN && value <= HW_GAIN_MAX)
		{
			hw_gain = value;
			Debug_printf("hw_gain set to %d\r\n", value);
		}
		else
		{
			light_sensor_cli_help();
		}
		return;
	}
	
	if(!strcmp(argv[0], "average"))
	{
		if((int16_t)value >= SW_GAIN_MIN && value <= SW_GAIN_MAX)
		{
			sw_gain    = value;
			post_shift = value;
			Debug_printf("sw_gain set to %d\r\n", value);
		}
		else
		{
			light_sensor_cli_help();
		}
		return;
	}

	light_sensor_cli_help();
	return;
	
}

