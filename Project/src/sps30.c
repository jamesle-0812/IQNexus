/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	I2C register level driver for LoRa only implementation project
								Implements I2C flash external periphheral.

	Created by:	James Le
	Date		 :	Jun 8th, 2020
	Maintainer: Alvaro

*/


#include "i2c1.h"
#include "sps30.h"

#include "stm32l0xx.h"                  // Device header

#include "hw.h"

#include "debug_uart.h"
#include "flash_map.h"
#include "lora_sensum.h"
#include "global.h"
#include "radio_common.h"

#include "delays.h"
#include <math.h>

#ifdef	DISABLE_SPS30_DEBUG
	#define Debug_printf(...)
#endif

#define CRC_POLYNOMIAL 0x131 //P(x)=x^8+x^5+x^4+1 = 100110001

#define SPS30_I2C_ADDR		   0x69

#define SPS30_START_CMD		   0x0010
#define SPS30_STOP_CMD		   0x0104
#define SPS30_READ_READY_CMD	0x0202
#define SPS30_START_ARG		   0x0300
#define SPS30_READ_DATA_CMD	0x0300
#define SPS30_SLEEP_CMD			0x1001
#define SPS30_WAKEUP_CMD		0x1103
#define SPS30_START_FAN_CMD	0x5607
#define SPS30_AUTO_CLEAN_CMD	0x8004
#define SPS30_READ_SN_CMD		0xD033
#define SPS30_READ_VERSION_CMD	0xD100
#define SPS30_READ_STATUS_CMD	0xD206
#define SPS30_CLEAR_STATUS_CMD	0xD210
#define SPS30_RESET_CMD			0xD304
#define SN_MaxSize		   	32

#define SPS30_STATUS_OK		   0
#define SPS30_STATUS_ERROR	   1
#define SPS30_COM_ERROR   	   2

static int16_t upper_temperature_threshold         = INT16_MAX;
static int16_t lower_temperature_threshold         = INT16_MIN;
static bool    upper_temperature_threshold_enabled = false;
static bool    lower_temperature_threshold_enabled = false;
static int16_t upper_humidity_threshold            = INT16_MAX;
static int16_t lower_humidity_threshold            = INT16_MIN;
static bool    upper_humidity_threshold_enabled    = false;
static bool    lower_humidity_threshold_enabled    = false;
static uint16_t upper_mass_concentration_threshold = UINT16_MAX;
static uint16_t lower_mass_concentration_threshold = 0;
static bool upper_mass_concentration_threshold_enabled = false;
static bool lower_mass_concentration_threshold_enabled = false;
static uint8_t threshold_wakeups                   = 1;
static uint8_t auto_cleaning_days						= 1;
uint32_t auto_cleaning_counter							= 24*60;


/*!
 * @brief: 	8-bit Checksum Calculation function with given polynomial,
 * 		  	copied from section 5.2 of SPS30 datasheet
 * @param: 	a pointer off an array
 * 		  	size of the array or length of the piece of array 
 * @return: CRC code
 * @note: 	Tested
 */
static uint8_t CalcCrc(uint8_t *data, uint8_t data_length)
{
	uint8_t crc = 0xFF;
	for (int i = 0; i < data_length; i++)
	{
		crc ^= data[i];
		for (uint8_t bit = 8; bit > 0; --bit)
		{
			if (crc & 0x80)
			{
				crc = (crc << 1) ^ CRC_POLYNOMIAL;
			}
			else
			{
				crc = (crc << 1);
			}
		}
	}
	return crc;
}

/*!
 * @brief: 	Call CRC Calculation function and then compare return value to a given one
 * @param: 	a pointer off an array
 * 		  	size of the array or length of the piece of array
 * 		  	given checksum value to compare
 * @return: CRC code
 * @note: 	Tested
 */
bool validCRC(uint8_t *data, uint8_t data_length, uint8_t checksum)
{
	uint8_t crc = 0xFF;

	crc = CalcCrc(data, data_length);
	
	if (crc != checksum) return false;
	else return true;
}

/*!
 * @brief: 	Sends Start command to go to Measurement-Mode
 * @param: 	None
 * @return: None
 * @note: 	Tested
 */
void sps30_StartMsr(void){

	uint8_t buff[5] = {0};
	
	buff[0] = (uint8_t) (SPS30_START_CMD >> 8);
	buff[1] = (uint8_t) (SPS30_START_CMD & 0xFF);

	buff[2] = (uint8_t) (SPS30_START_ARG >> 8);
	buff[3] = (uint8_t) (SPS30_START_ARG & 0xFF);

	buff[4] = CalcCrc(&buff[2], 2);

	i2c1_send(SPS30_I2C_ADDR, buff, sizeof(buff)/sizeof(*buff), 1);
}

/*!
 * @brief: 	Sends Stop command to stop measurements 
 * 		   and then return to the initial state (Idle-Mode)
 * @param: 	None
 * @return: None
 * @note: 	Tested
 */
void sps30_StopMsr(void){

	uint8_t buff[2] = {0};

	buff[0] = (uint8_t) (SPS30_STOP_CMD >> 8);
	buff[1] = (uint8_t) (SPS30_STOP_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, sizeof(buff)/sizeof(*buff), 1);
}

/*!
 * @brief: 	Send Reset command
 * @param:	None
 * @return: None
 * @note: 	Tested
 */
void sps30_Reset(void){

	uint8_t buff[2] = {0};

	buff[0] = (uint8_t) (SPS30_RESET_CMD >> 8);
	buff[1] = (uint8_t) (SPS30_RESET_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, sizeof(buff)/sizeof(*buff), 1);
}

/*!
 * @brief: 	Check whether SPS30 has done measurements
 * @param: 	None
 * @return: Status of measurements: 1 is done, otherwise 0!
 * @note: 	Tested
 */
uint8_t sps30_ReadRdy(void)
{
	uint8_t buff[3] = {0};

	buff[0] = (uint8_t)(SPS30_READ_READY_CMD >> 8);
	buff[1] = (uint8_t)(SPS30_READ_READY_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, 2 /* sizeof(buff)/sizeof(*buff) */, 1);
	delay_timeout_ms(100);
	i2c1_receive(SPS30_I2C_ADDR, buff, 3 /* sizeof(buff)/sizeof(*buff) */, 1);

	if (validCRC(buff, 2, buff[0 + 2] /* *(buff + 2) */))	// 2 bytes + 1 crc
	{
		Debug_printf("The status of Read data ready is %d!\r\n", buff[1]);
	}
	else
	{
		Debug_printf("The Read data ready is errored!\r\n");
	}

	return buff[1];
}

/*!
 * @brief: 	Read Serial Number of SPS30
 * @param: 	a pointer of an array used to store Serial Number
 * @return: status
 * @note: 	Tested
 */
bool sps30_ReadSN(uint8_t *sn)
{
	bool status = false;
	uint8_t size = SN_MaxSize + (SN_MaxSize / 2); // 2 bytes + 1 crc
	uint8_t buff[size];

	if (!i2c1_init())
	{
		for (uint8_t i = 0; i < SN_MaxSize; i++)
		{
			buff[i] = 0;
		}
	}
	else
	{
		buff[0] = (uint8_t)(SPS30_READ_SN_CMD >> 8);
		buff[1] = (uint8_t)(SPS30_READ_SN_CMD & 0xFF);

		i2c1_send(SPS30_I2C_ADDR, buff, 2, 1);

		i2c1_receive(SPS30_I2C_ADDR, buff, size, 1);

		uint8_t i, j = 0;
		/* Check CRC, replace elements by a spsace if errored & filter CRC out */
		for (i = 0; i < size; i += (2 + 1)) // 2 bytes + 1 crc
		{
			if (validCRC(&buff[i], 2, buff[i + 2]))
			{
				sn[j++] = buff[i];
				sn[j++] = buff[i + 1];
				status = true;
			}
			else
			{
				Debug_printf("The element %d and %d are incorrect!\r\n", j, j + 1);
				sn[j++] = 32;
				sn[j++] = 32;
				status = false;
				break;
				/* break; */
			}
		}
	}
	return status;
}

/*!
 * @brief: 	Read version of SPS30
 * @param: 	None
 * @return: Version of SPS30 (read at hex)
 * @note: 	Tested
 */
uint8_t sps30_ReadVer(void)
{
	uint8_t buff[3] = {0};

	buff[0] = (uint8_t)(SPS30_READ_VERSION_CMD >> 8);
	buff[1] = (uint8_t)(SPS30_READ_VERSION_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, 2 /* sizeof(buff)/sizeof(*buff) */, 1);
	// delay_timeout_ms(100);
	i2c1_receive(SPS30_I2C_ADDR, buff, 3 /* sizeof(buff)/sizeof(*buff) */, 1);

	return ((buff[0] << 8) | buff[1]);
}

/*!
 * @brief: 	Read measurements of SPS30
 * @param: 	None
 * @return: Measurements saved in a struct of 4 unsigned integer 16-bit vairables
 * @note: 	Tested
 */
sps30_reading_sensor sps30_ReadMsr(void)
{
	uint8_t buff[60];

	buff[0] = (uint8_t)(SPS30_READ_DATA_CMD >> 8);
	buff[1] = (uint8_t)(SPS30_READ_DATA_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, 2, 1);

	i2c1_receive(SPS30_I2C_ADDR, buff, sizeof(buff) / sizeof(*buff), 1);

	uint8_t val[40] = {0};
	uint8_t i, j = 0;
	/* Filter out all CRCs */
	for (i = 0; i < sizeof(buff) / sizeof(*buff); i += (2 + 1))	// 2 bytes + 1 crc
	{
		/* Check CRC and mask checksum errors */
		if (validCRC(&buff[i], 2, buff[i + 2]))
		{
			val[j++] = buff[i];
			val[j++] = buff[i + 1];
		}
		else
		{
			Debug_printf("The error is at %d\r\n", j / 4);
			j += 2;
		}
	}

	union {
		uint8_t byte[4];
		uint32_t be_val;
		float le_val;
	} data[10];

	j = 0;
	for (i = 0; i < 10; i++)
	{
		for (uint8_t k = 0; k < 4; k++)
		{
			data[i].byte[k] = val[j++];
		}
	}

	/* Big-Endian -> Little-Endian */
	for (i = 0; i < 10; i++)
	{
		data[i].be_val = SWAP_ENDIAN(data[i].be_val);
	}

	sps30_reading_sensor results = {0};

	i = 0;
	results.MassCon_PM1p0	= data[i++].le_val;
	results.MassCon_PM2p5	= data[i++].le_val;
	results.MassCon_PM4p0	= data[i++].le_val;
	results.MassCon_PM10 	= data[i++].le_val;
	results.NumCon_PM0p5 	= data[i++].le_val;
	results.NumCon_PM1p0 	= data[i++].le_val;
	results.NumCon_PM2p5 	= data[i++].le_val;
	results.NumCon_PM4p0 	= data[i++].le_val;
	results.NumCon_PM10 		= data[i++].le_val;
	results.size 				= data[i++].le_val;

	return results;
}

/*!
 * @brief: 	Starts the fan-cleaning manually for 10 secs. Need to go to Measurement-Mode first.
 * 		  	Recommended to use this mode (read Important notes in section 5.3.5 in datasheet
 * 		  	for more details).
 * @param: 	None
 * @return: None
 * @note:	Not tested yet
 */
void sps30_manual_cleaning(void)
{

	uint8_t buff[2] = {0};

	buff[0] = (uint8_t)(SPS30_AUTO_CLEAN_CMD >> 8);
	buff[1] = (uint8_t)(SPS30_AUTO_CLEAN_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, 2 /* sizeof(buff)/sizeof(*buff) */, 1);
}

/*!
 * @brief: 	Setup auto cleaning for SPS30 		  
 * @param: 	cleaning interval (in seconds)
 * @return: None
 * @note: 	Prototype function, not test yet
 */
void sps30_set_auto_cleaning(uint32_t secs){

	uint8_t buff[8] = {0};

	union{	// another method
		uint32_t value;
		uint8_t	byte[4];
	}interval;

	buff[0] = (uint8_t) (SPS30_AUTO_CLEAN_CMD >> 8);
	buff[1] = (uint8_t) (SPS30_AUTO_CLEAN_CMD & 0xFF);

	//secs = SWAP_ENDIAN(secs);	// Little-endian -> Big-endian
	
	interval.value = SWAP_ENDIAN(secs);	// Little-endian -> Big-endian
	Debug_printf("%08X\r\n", interval.value);
	/* 
	uint8_t j = 0;
	for (uint8_t i = 2; i < sizeof(buff)/sizeof(*buff); i += (2 + 1)){	// 2 bytes + 1 crc
	
		buff[i] = interval.byte[j++];			Debug_printf("%02X ", buff[i]);
		buff[i + 1] = interval.byte[j++];	Debug_printf("%02X ", buff[i + 1]);
		buff[i + 2] = CalcCrc(&buff[i], 2);	Debug_printf("%02X\r\n", buff[i + 2]);
	}
	 */

	buff[2] = interval.value >> 24 & 0xff;       //2 MSB
	buff[3] = interval.value >> 16 & 0xff;       //3 MSB
	buff[4] = CalcCrc(&buff[2], 2); //4 CRC
	buff[5] = interval.value >>8 & 0xff;         //5 LSB
	buff[6] = interval.value & 0xff;             //6 LSB
	buff[7] = CalcCrc(&buff[5], 2); //7 CRC


	/* 
	buff[2] = secs & 0xff; // interval.byte[0];
	buff[3] = (secs >> 8) & 0xff; //	interval.byte[1];
	buff[4] = CalcCrc(&buff[2], 2);
	buff[5] = (secs >> 16) & 0xff; // interval.byte[2];
	buff[6] = (secs >> 24) & 0xff; // interval.byte[3];
	buff[7] = CalcCrc(&buff[5], 2);
	 */
	i2c1_send(SPS30_I2C_ADDR, buff, 8, 1);
}

/*!
 * @brief: 	Read auto cleaning time for SPS30 		  
 * @param: 	None
 * @return: cleaning interval (in seconds)
 * @note: 	Tested
 */
uint32_t sps30_get_auto_cleaning_time(void){

	uint8_t buff[8] = {0};

	union{	// another method
		uint32_t value;
		uint8_t	byte[4];
	}interval;

	buff[0] = (uint8_t) (SPS30_AUTO_CLEAN_CMD >> 8);
	buff[1] = (uint8_t) (SPS30_AUTO_CLEAN_CMD & 0xFF);

	i2c1_send(SPS30_I2C_ADDR, buff, 2 /* sizeof(buff)/sizeof(*buff) */, 1);
	i2c1_receive(SPS30_I2C_ADDR, buff, 6 /* sizeof(buff)/sizeof(*buff) */, 1);
	
	uint8_t j = 0;
	for (uint8_t i = 0; i < 6; i += (2 + 1)){	// 2 bytes + 1 crc
	/* Check CRC and mask checksum errors */
		if (validCRC(&buff[i], 2, buff[i+2])){
			interval.byte[j++] = buff[i];
			interval.byte[j++] = buff[i + 1];
		} else { /* Error code put here */
			Debug_printf("The error is at %d\r\n", j/4);
			j += 2;
		}
	}

	return SWAP_ENDIAN(interval.value);	// Big-endian -> Little-endian
}

sps30_reading_sensor sps30_GetReading(void){

	uint8_t status;
	sps30_reading_sensor data;
	uint8_t Serial_Number[SN_MaxSize] = {0};

	Debug_printf("Firmware version: %02X, ", sps30_ReadVer());
	sps30_ReadSN(Serial_Number);
	Debug_printf("The Serial Number is "); Debug_printf((char *)Serial_Number); Debug_printf(".\r\n");	
	
	/* switch on power for sps module here */
	sps30_StartMsr();
	/*
	 * https://github.com/Sensirion/embedded-sps/issues/15
	 * After the start-up the sensor requires a couple of minutes 
	 * to achieve a stable flow and show the most accurate values.
	 */
	delay_timeout_ms(15000);

	/* 
	 * An updated measurement value is provided every second and the “Data-Ready Flag” is set. 
	 * Refer to section 5.3.4 of SPS30 datasheet
	 * Thus, it should wait for at least a second before reading data ready value.
	 * Otherwise the reading buffer of data ready is error of 255 255 255.
	 */
	do
	{
		delay_timeout_ms(1000);
		status = sps30_ReadRdy();
	} while (!status);
	
	Debug_printf("The status of Read data ready is %d!\r\n", status);

	if (status)
	{
		data = sps30_ReadMsr();
	}
	data.status = status;

	/* switch off power for sps module here */	
	sps30_StopMsr();

	return data;
}

void sps30_uplink( void )
{
	sps30_payload_t payload = {0};
	sps30_reading_sensor data_sps30 = {0};
	sht30Reading_t data_sht30 = {0};

	data_sps30 = sps30_GetReading();

	// payload.members.status = data_sps30.status;
	// payload.members.MassCon_PM1p0 = (uint16_t) round(10*data_sps30.MassCon_PM1p0);
	// payload.members.MassCon_PM2p5 = (uint16_t) round(10*data_sps30.MassCon_PM2p5);
	// payload.members.MassCon_PM4p0 = (uint16_t) round(10*data_sps30.MassCon_PM4p0);
	// payload.members.MassCon_PM10 	= (uint16_t) round(10*data_sps30.MassCon_PM10);
	// /* Unused data, not declared in packet.h
	// payload.members.NumCon_PM0p5 = (uint16_t) round(100*data_sps30.NumCon_PM0p5);
	// payload.members.NumCon_PM1p0 = (uint16_t) round(100*data_sps30.NumCon_PM1p0);
	// payload.members.NumCon_PM2p5 = (uint16_t) round(100*data_sps30.NumCon_PM2p5);
	// payload.members.NumCon_PM4p0 = (uint16_t) round(100*data_sps30.NumCon_PM4p0);
	// payload.members.NumCon_PM10  = (uint16_t) round(100*data_sps30.NumCon_PM10);
	// payload.members.size			  = (uint16_t) round(100*data_sps30.size);
	//  */

	// /* ------------------------------------Debugging------------------------------------ */
	// Debug_printf("Mass Concentration PM1.0 is %d.%d ug/m3!\r\n", payload.members.MassCon_PM1p0/10, payload.members.MassCon_PM1p0%10);
	// Debug_printf("Mass Concentration PM2.5 is %d.%d ug/m3!\r\n", payload.members.MassCon_PM2p5/10, payload.members.MassCon_PM2p5%10);
	// Debug_printf("Mass Concentration PM4.0 is %d.%d ug/m3!\r\n", payload.members.MassCon_PM4p0/10, payload.members.MassCon_PM4p0%10);
	// Debug_printf("Mass Concentration PM10 is %d.%d ug/m3!\r\n", payload.members.MassCon_PM10/10, payload.members.MassCon_PM10%10);

	// /* ------------------------------------SHT30------------------------------------ */	
	// data_sht30 = sht30GetReading();
	// payload.members.TempAve = round((data_sht30.T1/10 + data_sht30.T2/10)/2);
	// payload.members.HumidAve = round((data_sht30.H1/10 + data_sht30.H2/10)/2);

	// Debug_printf("The average of temperature is %d.%d Celcius!\r\n", payload.members.TempAve/10, payload.members.TempAve%10);
	// Debug_printf("The average of humidity is %d.%d!\r\n", payload.members.HumidAve/10, payload.members.HumidAve%10);

	// /* --------------------------Header for sending packet-------------------------- */
	// payload.members.sys_voltage = fourBit_battery_calculation();
	// payload.members.pkt_type    = packet_type_data;

	// Uplink(payload.payload, SPS30_SIZE);
}

void sps30_onWakeup( void )
{
	if(wakeup_count % wakeups_per_uplink == 0)
	{
		Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
		sps30_uplink();		

		if (auto_cleaning_counter != 0){
			auto_cleaning_counter--;
			uint8_t hr = auto_cleaning_counter*(transmit_interval_ms/60000)/60;
			Debug_printf("Auto cleaning is in next %d hours\r\n", hr);
		} else {
			sps30_StartMsr();
			Debug_printf("Start cleaning ...\r\n");
			sps30_manual_cleaning();
			sps30_StopMsr();
			Debug_printf("Cleaning is done\r\n");
			auto_cleaning_counter = auto_cleaning_days*24*60/(transmit_interval_ms/60000);
		}
		return;
	}
	
	//this is only executed if at least one of the thresholds is enabled
	if((wakeup_count % threshold_wakeups == 0) && 
		(upper_temperature_threshold_enabled 			||
		 lower_temperature_threshold_enabled 			||
		 upper_humidity_threshold_enabled    			||
		 lower_humidity_threshold_enabled	 			||
		 upper_mass_concentration_threshold_enabled	||
		 lower_mass_concentration_threshold_enabled))
	{
		Debug_printf("Threshold Check (%d)\r\n", threshold_wakeups);
		sht30Reading_t result = sht30GetReading();
		
		if((result.T1 > upper_temperature_threshold || result.T2 > upper_temperature_threshold) && upper_temperature_threshold_enabled)
		{
			Debug_printf("Upper Threshold exceeded\r\n");
			sps30_uplink();
			return;
		}
		
		if((result.H1 > upper_humidity_threshold || result.H2 > upper_humidity_threshold) && lower_humidity_threshold_enabled)
		{
			Debug_printf("Lower Threshold exceeded\r\n");
			sps30_uplink();
			return;
		}

		sps30_reading_sensor pm = sps30_ReadMsr();

		if((pm.MassCon_PM1p0 > upper_mass_concentration_threshold || 
			 pm.MassCon_PM2p5 > upper_mass_concentration_threshold ||
			 pm.MassCon_PM4p0 > upper_mass_concentration_threshold ||
			 pm.MassCon_PM10 > upper_mass_concentration_threshold) && upper_mass_concentration_threshold_enabled)
		{
			Debug_printf("Upper Threshold exceeded\r\n");
			sps30_uplink();
			return;
		}
	}
}

void sps30_save_config()
{
	sps30_config_page_layout_t config = {0};
	
	config.members.humidity_threshold_upper    			= upper_humidity_threshold           ;
	config.members.humidity_thredhold_lower    			= lower_humidity_threshold           ;
	config.members.temperature_threshold_upper 			= upper_temperature_threshold        ;
	config.members.temperature_threshold_lower 			= lower_temperature_threshold        ;
	config.members.humidity_upper_enabled      			= upper_humidity_threshold_enabled   ;
	config.members.humidity_lower_enabled      			= lower_humidity_threshold_enabled   ;
	config.members.temperature_upper_enabled   			= upper_temperature_threshold_enabled;
	config.members.temperature_lower_enabled   			= lower_temperature_threshold_enabled;
	config.members.threshold_wakeups           			= threshold_wakeups                  ;
	config.members.mass_concentration_threshold_upper 	= upper_mass_concentration_threshold;
	config.members.mass_concentration_threshold_lower 	= lower_mass_concentration_threshold;
	config.members.mass_concentration_upper_enabled 	= upper_mass_concentration_threshold_enabled;
	config.members.mass_concentration_lower_enabled 	= lower_mass_concentration_threshold_enabled;
	config.members.auto_cleaning_time						= auto_cleaning_days;
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void sps30_load_config()
{
	sps30_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	

	upper_humidity_threshold            		= config.members.humidity_threshold_upper   ;
	lower_humidity_threshold            		= config.members.humidity_thredhold_lower   ;
	upper_temperature_threshold         		= config.members.temperature_threshold_upper;
	lower_temperature_threshold         		= config.members.temperature_threshold_lower;
	upper_humidity_threshold_enabled    		= config.members.humidity_upper_enabled     ;
	lower_humidity_threshold_enabled    		= config.members.humidity_lower_enabled     ;
	upper_temperature_threshold_enabled 		= config.members.temperature_upper_enabled  ;
	lower_temperature_threshold_enabled 		= config.members.temperature_lower_enabled  ;
	upper_mass_concentration_threshold  		= config.members.mass_concentration_threshold_upper;
	lower_mass_concentration_threshold 			= config.members.mass_concentration_threshold_lower;
	upper_mass_concentration_threshold_enabled = config.members.mass_concentration_upper_enabled;
	lower_mass_concentration_threshold_enabled = config.members.mass_concentration_lower_enabled;
	threshold_wakeups                   		= config.members.threshold_wakeups			  ;
	auto_cleaning_days 								= config.members.auto_cleaning_time				
	;

}

/* 
void sps30_onDownlink(uint8_t *buffer, uint8_t size)
{
	//downlink_type_threshold_1
	threshold_downlinks_t downlink = {0};
	int i;
	
	Debug_printf("SPS30 Downlink Function\r\n");
	Debug_printf("Received Data: ");
	await_uart_tx();
	
	for(i=0;i<size;i++)
	{
		Debug_printf(" %02X", buffer[i]);
		downlink.payload[THRESHOLD_DOWNLINK_SIZE-i-1] = buffer[i];
	}
	
	Debug_printf("\r\n");
	await_uart_tx();
	
	if(downlink.threshold.downlink_type == downlink_type_sps30_temperature)
	{
		Debug_printf("Temperature Threshold Downlink\r\n");
		
		Debug_printf("Temperature Upper: %d.%02d\r\n"   ,downlink.threshold.upper/100, downlink.threshold.upper%100);
		Debug_printf("Temperature Lower: %d.%02d\r\n"   ,downlink.threshold.lower/100, downlink.threshold.lower%100);
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
			threshold_wakeups = downlink.threshold.period;
		}
	}
	
	if(downlink.threshold.downlink_type == downlink_type_sps30_humidity)
	{
		Debug_printf("Humidity Threshold Downlink\r\n");
		
		Debug_printf("Humidity Upper: %d.%02d\r\n"      ,downlink.threshold.upper/100, downlink.threshold.upper%100);
		Debug_printf("Humidity Lower: %d.%02d\r\n"      ,downlink.threshold.lower/100, downlink.threshold.lower%100);
		Debug_printf("Upper Enabled    : %s\r\n"        ,downlink.threshold.enabled_h ? "YES":"NO");
		Debug_printf("Lower Enabled    : %s\r\n"        ,downlink.threshold.enabled_l ? "YES":"NO");
		Debug_printf("Update Upper     : %s\r\n"        ,downlink.threshold.update_h  ? "YES":"NO");
		Debug_printf("Update Lower     : %s\r\n"        ,downlink.threshold.update_l  ? "YES":"NO");
		Debug_printf("Interval         : %d wakeups\r\n",downlink.threshold.period);
		Debug_printf("Update Interval  : %s\r\n"        ,downlink.threshold.period    ? "YES":"NO");
		
		if(downlink.threshold.update_h)
		{
			upper_humidity_threshold = downlink.threshold.upper;
			upper_humidity_threshold_enabled = downlink.threshold.enabled_h;
		}
		
		if(downlink.threshold.update_l)
		{
			lower_humidity_threshold = downlink.threshold.lower;
			lower_humidity_threshold_enabled = downlink.threshold.enabled_l;
		}
		
		if(downlink.threshold.period != 0)
		{
			threshold_wakeups = downlink.threshold.period;
		}
	}

	if(downlink.threshold.downlink_type == downlink_type_sps30_mass_concentration)
	{
		Debug_printf("Mass Concentration Threshold Downlink\r\n");
		
		Debug_printf("Mass Concentration Upper: %d.%02d\r\n"   ,downlink.threshold.upper/10, downlink.threshold.upper%10);
		Debug_printf("Mass Concentration Lower: %d.%02d\r\n"   ,downlink.threshold.lower/10, downlink.threshold.lower%10);
		Debug_printf("Upper Enabled    : %s\r\n"        ,downlink.threshold.enabled_h ? "YES":"NO");
		Debug_printf("Lower Enabled    : %s\r\n"        ,downlink.threshold.enabled_l ? "YES":"NO");
		Debug_printf("Update Upper     : %s\r\n"        ,downlink.threshold.update_h  ? "YES":"NO");
		Debug_printf("Update Lower     : %s\r\n"        ,downlink.threshold.update_l  ? "YES":"NO");
		Debug_printf("Interval         : %d wakeups\r\n",downlink.threshold.period);
		Debug_printf("Update Interval  : %s\r\n"        ,downlink.threshold.period    ? "YES":"NO");
		
		if(downlink.threshold.update_h)
		{
			upper_mass_concentration_threshold = downlink.threshold.upper;
			upper_mass_concentration_threshold_enabled = downlink.threshold.enabled_h;
		}
		
		if(downlink.threshold.update_l)
		{
			lower_mass_concentration_threshold = downlink.threshold.lower;
			lower_mass_concentration_threshold_enabled = downlink.threshold.enabled_l;
		}
		
		if(downlink.threshold.period != 0)
		{
			threshold_wakeups = downlink.threshold.period;
		}
	}
	
	sps30_save_config();
}
 */

void sps30_cli_threshold_help()
{
	Debug_printf("Usage: threshold [enable|disable] [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\tEnable or Disable [threshold]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	Debug_printf("\t\th_upper\r\n");
	Debug_printf("\t\th_lower\r\n");
	Debug_printf("\t\tpm_upper\r\n");
	Debug_printf("\t\tpm_lower\r\n");
	await_uart_tx();
	
	Debug_printf("Usage: threshold set [threshold] [value]\r\n");
	await_uart_tx();
	Debug_printf("\tset [threshold] trigger value to [value]\r\n");
	await_uart_tx();
	Debug_printf("\t[threshold] can be one of:\r\n");
	Debug_printf("\t\tt_upper\r\n");
	Debug_printf("\t\tt_lower\r\n");
	Debug_printf("\t\th_upper\r\n");
	Debug_printf("\t\th_lower\r\n");
	Debug_printf("\t\tpm_upper\r\n");
	Debug_printf("\t\tpm_lower\r\n");
	Debug_printf("\t\tauto-cleaning\r\n");
	await_uart_tx();
	Debug_printf("\t[value] is an integer\r\n");
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

void sps30_cli_threshold(int argc, char *argv[])
{
	//"threshold enable t_upper"
	//"threshold disable t_upper"
	//"threshold set t_upper [value]"
	float value;
	
	if(argc == 1)
	{
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("Upper temperature        :%d.%02d\r\n"   , upper_temperature_threshold/100, upper_temperature_threshold%100);
			await_uart_tx();
			Debug_printf("Upper temperature Enabled:%s\r\n"        , upper_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();

			Debug_printf("lower temperature        :%d.%03d\r\n"   , lower_temperature_threshold/100, lower_temperature_threshold%100);
			await_uart_tx();
			Debug_printf("lower temperature Enabled:%s\r\n"        , lower_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();

			Debug_printf("Upper humidity           :%d.%02d\r\n"   , upper_humidity_threshold/100, upper_humidity_threshold%100);
			await_uart_tx();
			Debug_printf("Upper humidity Enabled   :%s\r\n"        , upper_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();

			Debug_printf("lower humidity           :%d.%03d\r\n"   , lower_humidity_threshold/100, lower_humidity_threshold%100);
			await_uart_tx();
			Debug_printf("lower humidity Enabled   :%s\r\n"        , lower_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();

			Debug_printf("Checking thresholds every %d wakeups\r\n", threshold_wakeups);
			await_uart_tx();

			Debug_printf("Upper Mass Concentration :%d.%03d\r\n"   ,  upper_mass_concentration_threshold/10, upper_mass_concentration_threshold%10);
			await_uart_tx();
			Debug_printf("Upper Mass Concentration Enabled   :%s\r\n"        , upper_mass_concentration_threshold_enabled?"YES":"NO");
			await_uart_tx();
			
			Debug_printf("lower Mass Concentration :%d.%03d\r\n"   , lower_mass_concentration_threshold/10, lower_mass_concentration_threshold%10);
			await_uart_tx();
			Debug_printf("lower Mass Concentration Enabled :%s\r\n"        , lower_mass_concentration_threshold_enabled?"YES":"NO");

			Debug_printf("Auto Cleaning is on every %d days\r\n"   ,auto_cleaning_days);
			await_uart_tx();
			
			return;
		}
	}
	
	if(argc == 2)
	{
		if(!strcmp(argv[0],"wakeup"))
		{
			uint8_t value = atoi(argv[1]);
			
			if(value <= 0 || value >= 255)
			{
				Debug_printf("Value out of range\r\n");
			}
			
			threshold_wakeups = value;
			Debug_printf("Check thresholds every %d wakeups\r\n", threshold_wakeups);
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
			//options here are upper or lower
			if(!strcmp(argv[1], "h_upper"))
			{
				//enable upper
				upper_humidity_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//enable lower
				lower_humidity_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "pm_upper"))
			{
				//enable upper
				upper_mass_concentration_threshold_enabled = true;
				Debug_printf("Upper Mass Concentration Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "pm_lower"))
			{
				//enable lower
				lower_mass_concentration_threshold_enabled = true;
				Debug_printf("Lower Mass Concentration Enabled\r\n");
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
			if(!strcmp(argv[1], "pm_upper"))
			{
				//disable upper
				upper_mass_concentration_threshold_enabled = false;
				Debug_printf("Upper Mass Concentration Disable\r\n");
				return;
			}
			if(!strcmp(argv[1], "pm_lower"))
			{
				//disable lower
				lower_mass_concentration_threshold_enabled = false;
				Debug_printf("Lower Mass Concentration Disable\r\n");
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
				upper_temperature_threshold = (int16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", upper_temperature_threshold/100, upper_temperature_threshold%100);
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//set lower
				lower_temperature_threshold = (int16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", lower_temperature_threshold/100, lower_temperature_threshold%100);
				return;
			}
			//options here are upper or lower
			if(!strcmp(argv[1], "h_upper"))
			{
				//set upper
				upper_humidity_threshold = (uint16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", upper_humidity_threshold/100, upper_humidity_threshold%100);
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//set lower
				lower_humidity_threshold = (uint16_t)(value*100);
				Debug_printf("Upper threshold set to %d.%02d\r\n", lower_humidity_threshold/100, lower_humidity_threshold%100);
				return;
			}
			if(!strcmp(argv[1], "pm_upper"))
			{
				//set upper
				upper_mass_concentration_threshold = (uint16_t)(value*10);
				Debug_printf("Upper threshold set to %d.%02d\r\n", upper_mass_concentration_threshold/10, upper_mass_concentration_threshold%10);
				return;
			}
			if(!strcmp(argv[1], "pm_lower"))
			{
				//set lower
				lower_mass_concentration_threshold = (uint16_t)(value*10);
				Debug_printf("Lower threshold set to %d.%02d\r\n", lower_mass_concentration_threshold/10, lower_mass_concentration_threshold%10);
				return;
			}
			if(!strcmp(argv[1], "auto-cleaning"))
			{
				//set auto-cleaning by days
				auto_cleaning_days = value;
				auto_cleaning_counter = (uint32_t) (auto_cleaning_days*24*60/(transmit_interval_ms/60000));
				Debug_printf("Auto cleaning is set to every %d days\r\n", auto_cleaning_counter*(transmit_interval_ms/60000)/(60*24));
				return;
			}
		}
	}
	
	sps30_cli_threshold_help();
}

