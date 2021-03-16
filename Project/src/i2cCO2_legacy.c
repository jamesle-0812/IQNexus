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
#include "i2cCO2.h"
#include "sht30.h"
#include "stm32l0xx.h"                  // Device header
#include "hw.h"

#include "debug_uart.h"
#include "global.h"
#include "delays.h"

#include "i2cLightSensor.h"

#include "watchdog.h"
#include "radio_common.h"

#ifndef DISABLE_CO2_DEBUG
	#define DBG_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define DBG_printf(...)
#endif

#define CO2_I2C_ADDR	 0x68


#define PARAMS_START   0x88
#define PARAMS_END     0x8F

#define TRIGGER_REG    0x93
#define TRIGGER_VAL    0x01

#define WAKE_REG       0x30

#define MODE_REG       0x95

#define MODE_SINGLE    0x01
#define MODE_CONT      0x00

#define CO2_REGISTER_L 0x06
#define CO2_REGISTER_H 0x07

#define CO2_SCR_H      0x82
#define CO2_SCR_L      0x83

#define CAL_TARGET_H   0x84
#define CAL_TARGET_L   0x85

#define CAL_STATUS     0x81


static uint8_t register_backups[PARAMS_END - PARAMS_START +1] = {0};


/********************************************************************
 *Functions                                                         *
 *******************************************************************/

static void enable()
{
	delay_timeout_ms(10);
	LL_GPIO_SetOutputPin(CO2_EN_PORT,CO2_EN_PIN);
}

static void disable()
{
	LL_GPIO_ResetOutputPin(CO2_EN_PORT,CO2_EN_PIN);
}
 
static int send_wakeup()
{
	delay_timeout_ms(40);
	uint8_t data[] = {WAKE_REG};
	int i = 0;
	while(i<10)
	{
		if(i2c1_send_feedback(CO2_I2C_ADDR, data, 1, 0))
		{
			i2c1_receive(CO2_I2C_ADDR, data, 1, 1);
			delay_timeout_ms(12);
			break;
		}
		i++;
		delay_timeout_ms(40);
	}

	DBG_printf("Wake Fails: %d\r\n", i);
	return i;
}

static int write_registers(uint8_t reg, uint8_t *value, uint8_t count)
{
	uint8_t request[count+1];
	int i;

	request[0] = reg;
	
	for(i=0;i< count; i++)
	{
		request[i+1] = value[i];
	}


	int fail = 1;

	while(fail !=0 && fail < 30)
	{
		//send the wakeup
		send_wakeup();
		if(i2c1_send_feedback(CO2_I2C_ADDR, request, count+1, 1) == 0)
		{
			fail ++;
		}
		else
		{
			DBG_printf("\tPASS, WRITE REG %02x\r\n", reg);
			DBG_printf("Write fails: %d\r\n", fail);
			fail = 0;
		}
		await_uart_tx();
	}

	delay_timeout_ms(12);

	return fail;
}
 
static int read_registers(uint8_t reg, uint8_t *rx_data, uint8_t count)
{
	uint8_t tx_data[1];
	tx_data[0] = reg;

	int fail = 1;

	while(fail !=0 && fail < 30)
	{
		//send the wakeup
		send_wakeup();
		if(i2c1_send_feedback(CO2_I2C_ADDR, tx_data, 1, 0) == 0)
		{
			fail++;
		}
		else
		{
			i2c1_receive(CO2_I2C_ADDR, rx_data, count, 1);
			DBG_printf("\tPASS, READ  REG %02x\r\n", reg);
			DBG_printf("Read Fails: %d\r\n", fail);
			fail = 0;
		}
		await_uart_tx();
	}

	delay_timeout_ms(12);

	return fail;
}
 
static int backup_regs()
{
	return read_registers(PARAMS_START,register_backups,PARAMS_END - PARAMS_START+1);
}
 
static int restore_regs()
{
	return write_registers(PARAMS_START, register_backups, PARAMS_END - PARAMS_START+1);
}
 
void legacy_C02_init( void )
{
	GPIO_InitTypeDef GPIO_InitStruct;
	uint8_t tx_data[] = {MODE_SINGLE};


	//Pin configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;

	//EN as push-pull output, so we can turn the CO2 metre on and off
	HW_GPIO_Init(CO2_EN_PORT, CO2_EN_PIN, &GPIO_InitStruct);

	//nRDY as input, so we can read it
	GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
	HW_GPIO_Init(CO2_nRDY_PORT, CO2_nRDY_PIN, &GPIO_InitStruct);

	//Enable high
	enable();


	i2c1_init();

	//required delay
	delay_timeout_ms(40);
	//set to single reading mode
	write_registers(MODE_REG, tx_data, 1);

	//backup the required registers
	backup_regs();

	//Enable low
	disable();
	
	init_light_sensor();
}
 
 
static uint16_t CO2_read( void )
{
	uint8_t tx_data[1] = {TRIGGER_VAL};
	uint8_t rx_data[2] = {0};
	uint16_t return_value = 0;
	
	static TimerEvent_t CO2Timer;
	
	int error = 0;


	//enable high
	enable();
	
	i2c1_init();
	
	//required delay
	delay_timeout_ms(40);
	
	//restore registers
	if(restore_regs() != 0)
	{
		error = 1;
	}
	
	//start measurement
	if(write_registers(TRIGGER_REG, tx_data, 1) != 0)
	{
		error = 1;
	}
	
	start_timeout_timer(&CO2Timer, 100);
	
	//wait for nRDY to rise, indicating that the measurement has started
	while(!HW_GPIO_Read(CO2_nRDY_PORT, CO2_nRDY_PIN))
	{
		reset_watchdog();
		
		if(timer_expired(&CO2Timer))
		{
			DBG_printf("Timeout Rising\r\n");
			error = 1;
			break;
		}
	}
	
	//specified 6 second timeout from wehn the nRDY goes high.
	start_timeout_timer(&CO2Timer, 6000);
	
	//wait for nRDY to fall, indicating that the measurement is complete
	while(HW_GPIO_Read(CO2_nRDY_PORT, CO2_nRDY_PIN))
	{
		reset_watchdog();
		
		//if the timeout has expired, then we should exit to prevent a lockup
		if(timer_expired(&CO2Timer))
		{
			DBG_printf("Timeout Falling\r\n");
			error = 1;
			break;
		}
	}
	stop_timeout_timer(&CO2Timer);
	
	//read the measured value
	if(read_registers(CO2_REGISTER_L, rx_data, 2) != 0)
	{
		error = 1;
	}
	
	DBG_printf("Values: %02X %02X\r\n", rx_data[0], rx_data[1]);
	
	//save the registers
	if(backup_regs() != 0)
	{
		error = 1;
	}
	
	//enable low
	disable();
	
	return_value = (rx_data[0]<<8) + rx_data[1];	
	
	if(error == 0)
		return return_value;
	else
	{
		return 0xFFFF;
	}
}
 
 
void legacy_co2_uplink( void )
{
	co2_payload_t payload = {0};
	sht30Reading_t ht_reading;
	Si1133_reading_t light_reading = {0};
	uint16_t value = CO2_read();
	
	int i = 0;
	
	//try to read the CO2 several times, before quitting
	while(value == 0xFFFF)
	{
		i++;
		value = CO2_read();
		if(i == 10)
			break;
	}
	
	payload.members.CO2 = value;
	
	//now get SHT20 Temperature and Humidity
	//all CO2 devices will have a SHT20 installed.
	
	
	ht_reading = sht30GetReading();
	
	light_reading = read_light_level();
	
	//post-process the light reading onto a suitable range in 8-bits.
	light_reading.visible_reading = light_reading.visible_reading/16;
	if(light_reading.visible_reading > 255)
	{
		light_reading.visible_reading = 255;
	}
	
	
	payload.members.Temperature1 = ht_reading.T1;
	payload.members.Temperature2 = ht_reading.T2;
	payload.members.Humidity1    = ht_reading.H1;
	payload.members.Humidity2    = ht_reading.H2;
	payload.members.Light        = light_reading.visible_reading;
	
	Debug_printf("CO2         : %d ppm\r\n"         , payload.members.CO2);
	Debug_printf("Temperature1: %02d.%02d Deg C\r\n", payload.members.Temperature1/100,payload.members.Temperature1%100);
	Debug_printf("Humidity1   : %02d.%02d %RH\r\n"  , payload.members.Humidity1/100, payload.members.Humidity1%100);
	Debug_printf("Temperature2: %02d.%02d Deg C\r\n", payload.members.Temperature2/100,payload.members.Temperature2%100);
	Debug_printf("Humidity2   : %02d.%02d %RH\r\n"  , payload.members.Humidity2/100, payload.members.Humidity2%100);
	Debug_printf("Light       : %03d\r\n"           , payload.members.Light);
	
	
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type    = packet_type_data;
	
	Uplink(payload.payload,CO2_SIZE);
}
 
static void target_calibration(uint16_t target)
{
	uint8_t command_buffer[10];
	uint16_t value;
	
	enable();
	
	//set the calibration target register
	command_buffer[0] = target >> 8;
	command_buffer[1] = target & 0xFF;
	
	write_registers(CAL_TARGET_H, command_buffer, 2);
	
	//set the calibration mode to target calibration
	command_buffer[0] = 0x7C;
	command_buffer[1] = 0x05;
	
	write_registers(CO2_SCR_H, command_buffer, 2);
	
	read_registers(CO2_SCR_H, command_buffer,4);
	
	DBG_printf("SCR: 0x%02X%02X\r\n", command_buffer[0], command_buffer[1]);
	DBG_printf("TGT: 0x%02X%02X\r\n", command_buffer[2], command_buffer[3]);
	
	//take a reading, also disables the CO2
	value = CO2_read();
	Debug_printf("Reading 1: %d\r\n", value);
	
	enable();
	
	//check that the calibration succeeded.
	
	if(read_registers(CAL_STATUS, command_buffer, 1))
	{	
		if(command_buffer[0] & 0x10)
		{
			Debug_printf("Calibration Complete\r\n");
		}
	}
	
	//take another reading to check.
	value = CO2_read();
	Debug_printf("Reading 2: %d\r\n", value);
	
	enable();
	
	command_buffer[0] = 0;
	write_registers(CAL_STATUS, command_buffer, 1);
	
	//Calibration mode is automatically set to 00 after a calibration!!.
	
	disable();
	
}
 
  
static void co2_cli_device_help()
{
	Debug_printf("Usage: device calib target [value]\r\n");
	Debug_printf("\tCalibrate the CO2 sensor to the environment PPM value.\r\n");
	await_uart_tx();
}

static void co2_cli_calibrate(int argc, char *argv[])
{
	uint16_t target = 0;
	
	if(argc < 2)
	{
		co2_cli_device_help();
		return;
	}
	
	if(!strcmp(argv[0], "target"))
	{
		target = atoi(argv[1]);
		
		target_calibration(target);
		return;
	}
}

void legacy_co2_cli_device(int argc, char *argv[])
{
	
	if(argc < 1)
	{
		co2_cli_device_help();
		return;
	}
	
	if(!strcmp(argv[0], "calib"))
	{
		co2_cli_calibrate(argc-1, argv+1);
		return;
	}
	
	light_sensor_cli_component(argc, argv);

	co2_cli_device_help();
	return;
	
}
 
 
void legacy_co2_onDownlink(uint8_t *buffer, uint8_t size)
{
	uint8_t downlink_type = buffer[0]>>4;
	co2_cal_downlink_t cal_downlink = {0};
	int i;
	
	Debug_printf("CO2 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		if(i < CO2_CAL_DOWNLINK_SIZE +1)
		{
			cal_downlink.payload[CO2_CAL_DOWNLINK_SIZE-i-1] = buffer[i];
		}
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink_type == downlink_type_co2_calib)
	{
		target_calibration(cal_downlink.cal.target);
	}
	
	co2_save_config();
}
 
 
 
 
