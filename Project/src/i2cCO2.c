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
#include "flash_map.h"

#include "i2cLightSensor.h"

#include "watchdog.h"
#include "radio_common.h"

#ifndef DISABLE_CO2_L2_DEBUG
	#define DBG_CO2_L2_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define DBG_CO2_L2_printf(...)
#endif

#ifndef DISABLE_CO2_DEBUG
	#define DBG_CO2_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define DBG_CO2_printf(...)
#endif

#define MAX_RETRIES 3
#define MAX_WRITE_RETRIES 10
#define MAX_READ_RETRIES  10
#define MAX_WAKE_RETRIES  10

#define ABC_MAX_TARGET    10000
#define ABC_MIN_TARGET    400
#define ABC_DEFAULT_TIME  180 //hours, as per manufacturer recommendation.

#define CO2_I2C_ADDR   0x68

//CO2 V3 Sensor
//Read-only
#define CO2_REGISTER_H        0x06
#define CO2_REGISTER_L        0x07
#define CO2_TEMPERATURE_H     0x08
#define CO2_TEMPERATURE_L     0x09
#define CO2_MEAS_COUNT        0x0D
#define CO2_MEAS_CYCLE_TIME_H 0x0E
#define CO2_MEAS_CYCLE_TIME_L 0x0F
#define CO2_UNFILTERED_H      0x10
#define CO2_UNFILTERED_L      0x11
#define CO2_ID_1              0x3A
#define CO2_ID_2              0x3B
#define CO2_ID_3              0x3C
#define CO2_ID_4              0x3D
//Read/Write
#define CO2_CAL_STATUS        0x81
#define CO2_CAL_COMMAND_H     0x82
#define CO2_CAL_COMMAND_L     0x83
#define CO2_CAL_TARGET_H      0x84
#define CO2_CAL_TARGET_L      0x85
#define CO2_OVERRIDE_H        0x86
#define CO2_OVERRIDE_L        0x87

#define CO2_START_REG         0xC3
#define CO2_ABC_TIME_H        0xC4
#define CO2_ABC_TIME_L        0xC5
#define CO2_ABC_PARAM_START   0xC6
#define CO2_ABC_PARAM_STOP    0xDB

#define CO2_MODE_REG          0x95
#define CO2_MEAS_PERIOD_H     0x96
#define CO2_MEAS_PERIOD_L     0x97
#define CO2_NUM_SAMPLE_H      0x98
#define CO2_NUM_SAMPLE_L      0x99
#define CO2_ABC_PERIOD_H      0x9A
#define CO2_ABC_PERIOD_L      0x9B
#define CO2_CLEAR_ERROR       0x9D
#define CO2_ABC_TARGET_H      0x9E
#define CO2_ABC_TARGET_L      0x9F
#define CO2_STATIC_IIR_PARAM  0xA1
#define CO2_SCR               0xA3
#define CO2_METER_CONTROL     0xA5
#define CO2_I2C_ADDR_REG      0xA7

//the register backups should have the following registers
//  (N/A)       The Literal Value 0xC3, base address for the read.
//  (0xC3)      The Start Measurement Register
//  (0xC4,0xC5 )The Time Register
//  (0xC6->0xDB)The Parameter Registers.

static uint8_t register_backups[CO2_ABC_PARAM_STOP - CO2_START_REG +2] = {0};
static volatile uint16_t abc_hours = 0;

static uint16_t abc_target = ABC_MIN_TARGET;
static uint16_t abc_period = ABC_DEFAULT_TIME;


static CO2_reading_t CO2_read( void );

/********************************************************************
 *Functions                                                         *
 *******************************************************************/

static void enable()
{
	LL_GPIO_SetOutputPin(CO2_EN_PORT,CO2_EN_PIN);
	delay_timeout_ms(36);
}

static void disable()
{
	LL_GPIO_ResetOutputPin(CO2_EN_PORT,CO2_EN_PIN);
}

/*
 The Write registers command takes an array as argumnet.
	The arrays first element MUST be the register address to start at.

	Count should be the total size of the ARRAY.

*/
static int write_registers(uint8_t *regs, uint8_t count)
{
	//we always expect one failure.
	//therefore set to -1 to ignore the expected failure.
	int fail = -1;
	DBG_CO2_L2_printf("WRITE REG %02x\r\n", regs[0]);

	while(fail <= MAX_WRITE_RETRIES)
	{
		//First write should fail because NACK
		//Second will succeed.
		if(i2c1_send_feedback(CO2_I2C_ADDR, regs, count, 1))
		{
			DBG_CO2_L2_printf("PASS\r\n");
			break;
		}
		fail ++;
	}
	
	if(fail>0)
	{
		DBG_CO2_printf("Write Fail count:%d\r\n", fail);
	}

	return (fail<MAX_RETRIES);
}
 
/*
	Read Registers
		This command uses a repeated start to separate the start and stop conditions.
*/
static int read_registers(uint8_t reg, uint8_t *rx_data, uint8_t count)
{
	uint8_t tx_data[1];
	int i;

	tx_data[0] = reg;

	//we expect a single failure for waking the sensor up, so set fail to -1 to compensate.
	int fail = -1;
	
	while(fail <= MAX_READ_RETRIES)
	{
		if(i2c1_send_feedback(CO2_I2C_ADDR, tx_data, 1, 0))
		{
			if(i2c1_receive_feedback(CO2_I2C_ADDR, rx_data, count, 1))
			{
				DBG_CO2_L2_printf("PASS, READ  REG %02x\r\n", reg);
				break;
			}
		}
		fail++;
	}

	if(fail > 0)
	{
		DBG_CO2_printf("Read Fail count:%d\r\n", fail);
	}
	
	DBG_CO2_printf("Read Data:");
	for(i=0;i<count;i++)
	{
		DBG_CO2_printf(" %02X", rx_data[i]);
	}
	DBG_CO2_printf("\r\n");

	return (fail <MAX_RETRIES);
}

static int read_registers_secure(uint8_t reg, uint8_t *rx_data, uint8_t count, uint8_t ignored_bytes)
{
	uint8_t compare[count];
	uint8_t shadow[count];
	uint8_t i = 0;
	
	if(!read_registers(reg, shadow, count))
	{
		return 0;
	}
	if(!read_registers(reg, compare, count))
	{
		return 0;
	}	
	for(i=0;i<count-ignored_bytes;i++)
	{
		if(shadow[i] != compare[i])
		{
			//reads did not match, fail.
			return 0;
		}
	}
	
	//if we got the read successfully, then we can copy the data into the result register.
	for(i=0;i<count;i++)
	{
		rx_data[i] = shadow[i];
	}
	
	return 1;
}

static int write_registers_secure(uint8_t *regs, uint8_t count, uint8_t ignored_bytes)
{
	(void)write_registers_secure;
	//we need to offset here for the register being in the write data as the first byte
	uint8_t compare[count];
	uint8_t i = 0;
	
	if(write_registers(regs, count) == 0)
	{
		return 0;
	}
	if(read_registers(regs[0], compare, count-1) == 0)
	{
		return 0;
	}
	
	for(i=0;i<count-ignored_bytes;i++)
	{
		if(regs[i+1] != compare[i])
		{
			
			//read did not match write, fail.
			return 0;
		}
	}
	
	return 1;
}
 
static int backup_regs()
{
	uint8_t return_value = 0;
	uint16_t time_shadow = 0;
	//Read all data that should be in the backup register.
	return_value = read_registers_secure(CO2_ABC_TIME_H, register_backups+2, CO2_ABC_PARAM_STOP - CO2_ABC_TIME_H +1, 0);
	
	if(return_value)
	{
		//update the time in the abc_time variable
		time_shadow = (register_backups[2]<<8) + register_backups[3];
		abc_hours = time_shadow;
	}
	
	return return_value;
}
 
static int start_measurement()
{
	//this replaces the old restore_regs function,
	//because the entire block can be done in one transaction.
	//First ensure that the first member of the array is the address
	register_backups[0] = CO2_START_REG;
	//The start_reg should have value 1 to trigger the measurement.
	register_backups[1] = 1;
	//We need to write the ABC Hours into the register
	register_backups[2] = abc_hours;
	register_backups[3] = abc_hours;
	//The rest of the values are left over from backup_regs, which is intended.
	return write_registers(register_backups, CO2_ABC_PARAM_STOP - CO2_START_REG +2);
}

void CO2_hourly_interrupt()
{
	//this interrupt serves to keep track of the ABC time.
	//The variable for the ABC time should be reset to 0 whenever a device init is run
	abc_hours++;
}

static void set_abc(uint16_t period, uint16_t target)
{
	if(target >= ABC_MIN_TARGET && target <= ABC_MAX_TARGET)
	{		
		abc_target = target;
	}
	
	if(period)
	{
		abc_period = ABC_DEFAULT_TIME;
	}
	else
	{
		abc_period = 0;
	}
}

static void target_calibration(uint16_t target)
{
	uint8_t command_buffer[10];
	CO2_reading_t value;
	
	enable();
	
	//set the calibration target register
	command_buffer[0] = CO2_CAL_TARGET_H;
	command_buffer[1] = target >> 8;
	command_buffer[2] = target & 0xFF;
	
	write_registers(command_buffer, 3);
	
	//set the calibration mode to target calibration
	command_buffer[0] = CO2_CAL_COMMAND_H;
	command_buffer[1] = 0x7C;
	command_buffer[2] = 0x05;
	
	write_registers(command_buffer, 3);
	
	//take a reading, also disables the CO2
	value = CO2_read();
	Debug_printf("Reading 1: %d\r\n", value.value);
	
	enable();
	
	disable();
	
	//check that the calibration succeeded.
	
	if(read_registers(CO2_CAL_STATUS, command_buffer, 1))
	{	
		if(command_buffer[0] & 0x10)
		{
			Debug_printf("Calibration Complete\r\n");
		}
	}
	
	//take another reading to check.
	value = CO2_read();
	Debug_printf("Reading 2: %d\r\n", value.value);
	
	enable();
	
	command_buffer[0] = CO2_CAL_STATUS;
	command_buffer[1] = 0;
	write_registers(command_buffer, 2);
	
	//Calibration mode is automatically set to 00 after a calibration!!.
	
	disable();
	
}

static void reset_calibration_to_factory(void)
{
	uint8_t command_buffer[10];
	CO2_reading_t value;
	
	i2c1_init();
	//Enable high
	enable();
	
	//send the calibration command
	command_buffer[0] = CO2_CAL_COMMAND_H;
	command_buffer[1] = 0x7C;
	command_buffer[2] = 0x02;
	
	write_registers(command_buffer, 3);
	
	//take a reading, also disables the CO2
	value = CO2_read();
	Debug_printf("Reading 1: %d\r\n", value.value);
	
	enable();
	
	//check that the calibration succeeded.
	
	if(read_registers(CO2_CAL_STATUS, command_buffer, 1))
	{	
		if(command_buffer[0] & 0x04)
		{
			Debug_printf("Calibration Complete\r\n");
		}
	}
	
	//take another reading to check.
	value = CO2_read();
	Debug_printf("Reading 2: %d\r\n", value.value);
	
	enable();
	
	command_buffer[0] = CO2_CAL_STATUS;
	command_buffer[1] = 0;
	write_registers(command_buffer, 2);
	read_registers(CO2_CAL_STATUS, command_buffer, 1);
	
	disable();
}
 
void C02_init( void )
{
	GPIO_InitTypeDef GPIO_InitStruct;
	
	uint8_t single_command_regs[2];
	uint8_t defaults_1[10];
	uint8_t defaults_2[8];
	uint8_t defaults_3[4];


	//Pin configuration
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;

	//EN as push-pull output, so we can turn the CO2 metre on and off
	HW_GPIO_Init(CO2_EN_PORT, CO2_EN_PIN, &GPIO_InitStruct);

	//nRDY as input, so we can read it
	GPIO_InitStruct.Mode      = GPIO_MODE_INPUT;
	HW_GPIO_Init(CO2_nRDY_PORT, CO2_nRDY_PIN, &GPIO_InitStruct);

	i2c1_init();
	//Enable high
	enable();
	
	//First register to write
	defaults_1[0] = CO2_CAL_STATUS;
	//calibration status default value 0
	defaults_1[1] = 0;
	//Calibration Command restore factory calibration
	//defaults_1[2] = 0x7C;
	//defaults_1[3] = 0x02;
	defaults_1[2] = 0x00;
	defaults_1[3] = 0x00;
	//Set calibration command to 400, even though we don't use the "target Calibration"
	defaults_1[4] = 400>>8;
	defaults_1[5] = 400&0xFF;
	//CO2 Override value needs to be at datasheet-default value 32767
	defaults_1[6] = 32767 >>8;
	defaults_1[7] = 32767 & 0xFF;
	//We will also set the ABC time to 0
	defaults_1[8] = 0;
	defaults_1[9] = 0;
	
	write_registers(defaults_1, 10);
	
	//Now for the second half of the calibration
	//Registers 0x95->0x9F

	//Starting from the mode register
	defaults_2[0] = CO2_MODE_REG;
	//mode register value is 1 for single-shot mode
	defaults_2[1] = 1;
	//measurement period default value is 16
	defaults_2[2] = 0;
	defaults_2[3] = 16;
	//Number of samples default to 8
	defaults_2[4] = 0;
	defaults_2[5] = 8;
	//ABC_PERIOD is either 180 or 0, depending on if enabled, configured elsewhere.
	defaults_2[6] = abc_period>>8;
	defaults_2[7] = abc_period & 0xFF;

	write_registers(defaults_2, 8);
	delay_timeout_ms(100);
	
	defaults_3[0] = 0x9D;
	//clear error status by writing to any number
	defaults_3[1] = 0xFF;
	//abc target is defined elsewhere, write it here
	defaults_3[2] = abc_target>>8;
	defaults_3[3] = abc_target & 0xFF;
	
	write_registers(defaults_3, 4);
	
	delay_timeout_ms(100);
	
	
	
	//now that all defaults are written, issue a reset command, so they all take
	//this is nessessary because some parameters require a reset, as per datasheet.

	
	single_command_regs[0] = CO2_SCR;
	single_command_regs[1] = 0xFF;
	
	write_registers(single_command_regs, 2);
	
	//set the ABC hours to 0
	abc_hours = 0;

	delay_timeout_ms(100);
	
	disable();
	delay_timeout_ms(10);
	enable();
	
	//write the start reading command register
	single_command_regs[0] = CO2_START_REG;
	single_command_regs[1] = 1;
	
	write_registers(single_command_regs, 2);
	
	delay_timeout_ms(100);
	
	//now we can skip waiting for the rdy line, and go straight to storing the state.
	backup_regs();
	
	delay_timeout_ms(100);
	
	//also perform a read on the ABC_PERIOD Register, to see that writes are working.
	read_registers(CO2_ABC_TARGET_H, single_command_regs, 2);
	
	//set the ABC hours to 0
	abc_hours = 0;
	
	disable();
	delay_timeout_ms(10);
	
	init_light_sensor();
}
 
 
static CO2_reading_t CO2_read( void )
{
	uint8_t rx_data[2] = {0};
	CO2_reading_t return_value = {0};
	
	static TimerEvent_t CO2Timer;
	static TimerEvent_t wake_timer;
	
	i2c1_init();

	enable();
	
	//Restore REGS and START MEASUREMENT are a single command
	if(start_measurement() == 0)
	{
		//restore regs/start measurement failed
		DBG_CO2_printf("Restore Regs and Start Failed\r\n");
		return_value.co2_error_restore_fail = 1;
		return_value.co2_error_start_fail   = 1;
		return return_value;
	}
	
	
	
	//wait for nRDY to rise, indicating that the measurement has started
	start_timeout_timer(&CO2Timer, 100);
	while(!HW_GPIO_Read(CO2_nRDY_PORT, CO2_nRDY_PIN))
	{
		reset_watchdog();
		
		if(timer_expired(&CO2Timer))
		{
			DBG_CO2_printf("Timeout Rising\r\n");
			return_value.co2_error_timeout_rise = 1;
			return_value.co2_error = 1;
			break;
		}
		start_timeout_timer(&wake_timer, 10);
		sleep_until_interrupt();
	}
	
	//specified 6 second timeout from wehn the nRDY goes high.
	start_timeout_timer(&CO2Timer, 6000);
	stop_timeout_timer(&wake_timer);
	
	//wait for nRDY to fall, indicating that the measurement is complete
	while(HW_GPIO_Read(CO2_nRDY_PORT, CO2_nRDY_PIN))
	{
		reset_watchdog();
		
		//if the timeout has expired, then we should exit to prevent a lockup
		if(timer_expired(&CO2Timer))
		{
			DBG_CO2_printf("Timeout Falling\r\n");
			return_value.co2_error_timeout_fall = 1;
			return_value.co2_error = 1;
			break;
		}
		start_timeout_timer(&wake_timer, 10);
		sleep_until_interrupt();
	}
	stop_timeout_timer(&CO2Timer);
	stop_timeout_timer(&wake_timer);
	
	//read the measured value
	if(read_registers_secure(CO2_REGISTER_H, rx_data, 2, 0) == 0)
	{
		DBG_CO2_printf("Failed to read value\r\n");
		return_value.co2_error_read_fail = 1;
		return_value.co2_error = 1;
	}
	
	DBG_CO2_printf("Values: %02X %02X\r\n", rx_data[0], rx_data[1]);
	
	//save the registers
	if(backup_regs() == 0)
	{
		DBG_CO2_printf("Failed to backup registers\r\n");
		return_value.co2_error_backup_fail = 1;
		return_value.co2_error = 1;
	}
	
	//enable low
	disable();
	
	return_value.value = (rx_data[0]<<8) + rx_data[1];	
	
	return return_value;
}
 
 
void co2_uplink( void )
{
	co2_payload_t payload = {0};
	co2_error_payload_t error = {0};
	Si1133_reading_t light_reading = {0};
	sht30Reading_t ht_reading;
	CO2_reading_t reading = CO2_read();
	
	int i = 0;
	
	//try to read the CO2 several times, before quitting
	while(reading.value >10000 || reading.value < 400 || reading.co2_error )
	{
		i++;
		reading = CO2_read();
		if(i == MAX_RETRIES)
			break;
		delay_timeout_ms(20);
	}
	
	//this is the error case, uplink an error packet
	if(reading.value >10000 || reading.value < 400 || reading.co2_error)
	{
		error.members.co2_error_restore_fail  = reading.co2_error_restore_fail;
		error.members.co2_error_start_fail    = reading.co2_error_start_fail  ;
		error.members.co2_error_timeout_rise  = reading.co2_error_timeout_rise;
		error.members.co2_error_timeout_fall  = reading.co2_error_timeout_fall;
		error.members.co2_error_read_fail     = reading.co2_error_read_fail   ;
		error.members.co2_error_backup_fail   = reading.co2_error_backup_fail ;
		error.members.co2_error_value_H       = reading.value >10000;
		error.members.co2_error_value_L       = reading.value < 400;
		
		Debug_printf("CO2 Error Uplink \r\n");
		Debug_printf("\tRestore Fail   : %d\r\n", error.members.co2_error_restore_fail);
		Debug_printf("\tStart Fail     : %d\r\n", error.members.co2_error_start_fail  );
		Debug_printf("\tTimeout Rising : %d\r\n", error.members.co2_error_timeout_rise);
		Debug_printf("\tTimeout Falling: %d\r\n", error.members.co2_error_timeout_fall);
		Debug_printf("\tRead Fail      : %d\r\n", error.members.co2_error_read_fail   );
		Debug_printf("\tBackup Fail    : %d\r\n", error.members.co2_error_backup_fail );
		Debug_printf("\tValue High Lim : %d\r\n", error.members.co2_error_value_H     );
		Debug_printf("\tValue Low  Lim : %d\r\n", error.members.co2_error_value_L     );
		
		error.members.sys_voltage = fourBit_battery_calculation();
		error.members.pkt_type    = packet_type_error;
		
		Uplink(error.payload,CO2_ERROR_SIZE);
	}
	else
	{
		payload.members.CO2 = reading.value;
		
		//now get SHT30 Temperature and Humidity
		//all CO2 devices will have a SHT30 installed.
		
		DBG_CO2_L2_printf("Exit CO2, value %d\r\n", reading.value);
		await_uart_tx();
		delay_timeout_ms(50);
		ht_reading = sht30GetReading();
		DBG_CO2_L2_printf("Exit SHT30\r\n");
		await_uart_tx();
		
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
}
 

void co2_save_config()
{
	co2_config_page_layout_t config = {0};
	
	config.members.abc_period = abc_period;
	config.members.abc_target = abc_target;
	
	config.members.light_config = get_light_sensor_config_values();
	
	config.members.flag = 0xBE;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void co2_load_config()
{
	co2_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	
	//we are using the flag to ensure that first load doesn't
	//override the abc values.
	if(config.members.flag == 0xBE)
	{
		abc_period = config.members.abc_period;
		abc_target = config.members.abc_target;
		set_light_sensor_config_values(config.members.light_config);
	}
	
}
 
static void co2_cli_device_help()
{
	Debug_printf("Usage: device show\r\n");
	Debug_printf("\tShows device configuration\r\n");
	await_uart_tx();
	Debug_printf("Usage: device abc [target]\r\n");
	Debug_printf("\tSets the CO2 ABC Target, default 400 (ppm)\r\n");
	await_uart_tx();
	Debug_printf("Usage: device abc [enable|disable]\r\n");
	Debug_printf("\tEnables or Disables ABC\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: device calib target [value]\r\n");
	Debug_printf("\tCalibrate the CO2 sensor to the environment PPM value.\r\n");
	await_uart_tx();
}

static void co2_cli_calibrate(int argc, char *argv[])
{
	uint16_t target = 0;
	if(argc <1)
	{
		co2_cli_device_help();
		return;
	}
	
	if(!strcmp(argv[0], "default"))
	{
		reset_calibration_to_factory();
		return;
	}
	
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

void co2_cli_device(int argc, char *argv[])
{
	uint16_t target;
	
	if(argc < 1)
	{
		co2_cli_device_help();
		return;
	}
	
	if(!strcmp(argv[0], "show"))
	{
		//Print out device parameters
		Debug_printf("ABC %sabled\r\n", abc_period?"En":"Dis");
		if(abc_period)
		{
			Debug_printf("ABC Period %d hours\r\n", abc_period);
			Debug_printf("ABC Target %d ppm\r\n", abc_target);
		}
		
		return;
	}
	
	if(!strcmp(argv[0], "calib"))
	{
		co2_cli_calibrate(argc-1, argv+1);
		return;
	}
	
	light_sensor_cli_component(argc, argv);
	
	if(argc < 2)
	{
		co2_cli_device_help();
		return;
	}
	
	if(!strcmp(argv[0], "en"))
	{
		if(!strcmp(argv[1], "on"))
		{
			i2c1_init();
			enable();
			delay_timeout_ms(40);
		}
		else
		{
			disable();
		}
	}
	
	//for these ABC commands, the effect should take next time device.init() is called,
	//which should be when the "run" or "uplink" commands are run.
	if(!strcmp(argv[0], "abc"))
	{
		i2c1_init();
		enable();
		
		if(!strcmp(argv[1], "enable"))
		{
			set_abc(ABC_DEFAULT_TIME, abc_target);
		}
		else if(!strcmp(argv[1], "disable"))
		{
			set_abc(0, abc_target);
		}
		else
		{
			//Configure ABD baseline/Target.
			target = atoi(argv[1]);
			
			set_abc(abc_period, target);
		}
		
		disable();
		
		return;
	}

	co2_cli_device_help();
	return;
	
}
 
 
void co2_onDownlink(uint8_t *buffer, uint8_t size)
{
	uint8_t downlink_type = buffer[0]>>4;
	co2_abc_downlink_t abc_downlink = {0};
	co2_cal_downlink_t cal_downlink = {0};
	int i;
	
	Debug_printf("CO2 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		if(i < CO2_ABC_DOWNLINK_SIZE + 1)
		{
			abc_downlink.payload[CO2_ABC_DOWNLINK_SIZE-i-1] = buffer[i];
		}
		if(i < CO2_CAL_DOWNLINK_SIZE +1)
		{
			cal_downlink.payload[CO2_CAL_DOWNLINK_SIZE-i-1] = buffer[i];
		}
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink_type == downlink_type_co2_abc)
	{
		set_abc(abc_downlink.abc.abc_enabled, abc_downlink.abc.abc_target);
		C02_init();
	}
	
	if(downlink_type == downlink_type_co2_calib)
	{
		target_calibration(cal_downlink.cal.target);
	}
	
	co2_save_config();
}
 
 
 
 
 
