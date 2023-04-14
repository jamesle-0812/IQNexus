/*
 *   _____             _____                 
 *  / ____|           / ____|                
 * | (___   ___ _ __ | (___  _   _ _ __ ___  
 *  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
 *  ____) |  __/ | | |____) | |_| | | | | | |
 * |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
 * 
 * 
 * Description : Device driver for the LoRa chip
 * Created by  : James Le
 * Date        : 29/06/2020
 * Maintainer  : Shea Gosnell 
 * 
 */

/**********************************************************************************************************************/
/* header files */
/**********************************************************************************************************************/
#include <stdint.h>
#include "stm32l0xx.h"                  // Device header
#include "hw.h"
#include "global.h"
#include "debug_uart.h"
#include "flash_map.h"
#include "radio_common.h"

#include "i2c1.h"
#include "bme680.h"
#include "../Middlewares/Third_Party/bsec/bsec_integration.h"
#include "../Middlewares/Third_Party/bsec/bsec_serialized_configurations_iaq.h"
#include "../Middlewares/Third_Party/bsec/bme680_api.h"
#include "sht30.h"
#include "i2cLightSensor.h"
#include "i2cCO2.h"
#include "delays.h"
#include "math.h"


#define PRESSURE_OFFSET		80000

#ifdef DISABLE_BME680_DEBUG
	#define Debug_printf(...)
#endif

static int16_t upper_temperature_threshold         = INT16_MAX;
static int16_t lower_temperature_threshold         = INT16_MIN;
static bool    upper_temperature_threshold_enabled = false;
static bool    lower_temperature_threshold_enabled = false;

static int16_t upper_humidity_threshold            = INT16_MAX;
static int16_t lower_humidity_threshold            = INT16_MIN;
static bool    upper_humidity_threshold_enabled    = false;
static bool    lower_humidity_threshold_enabled    = false;

static uint16_t upper_Pressure_threshold			= UINT16_MAX;
static uint16_t lower_Pressure_threshold			= 0;
static bool 	upper_Pressure_threshold_enabled	= false;
static bool		lower_Pressure_threshold_enabled	= false;

static uint16_t upper_CO2_threshold					= 16380;
static uint16_t lower_CO2_threshold					= 0;
static bool 	upper_CO2_threshold_enabled			= false;
static bool		lower_CO2_threshold_enabled			= false;
/* 
static uint16_t upper_bVOC_threshold					= 4096;
static uint16_t lower_bVOC_threshold					= 0;
static bool 	 upper_bVOC_threshold_enabled			= false;
static bool 	 lower_bVOC_threshold_enabled			= false;
 */
static uint16_t upper_IAQ_threshold					= 500;
static uint16_t lower_IAQ_threshold					= 0;
static bool		upper_IAQ_threshold_enabled			= false;
static bool		lower_IAQ_threshold_enabled			= false;

static uint8_t threshold_wakeups                   = 1;
/**********************************************************************************************************************/
/* functions */
/**********************************************************************************************************************/

bme680Reading_t bme680_data = {0};
uint8_t state_saved[BSEC_MAX_STATE_BLOB_SIZE];

//This function will get the temperature from the sht30, using the hold-master method
bme680Reading_t bme680GetReading()
{
	bme680Reading_t results = {0};
	uint8_t data_to_send[] = {0xD0};
	uint8_t data_send_length = 1;
	uint8_t data_to_read[6] = {0};
	uint8_t data_read_length = 1;
	int16_t temp;
	
	//request temperature data from the sht30
	//set the i2c peripheral to the default state
	if(!i2c1_init())
	{
		//if we could not initialise the I2C, then we cannot read the sensors.
		for(temp = 0; temp < data_read_length; temp++)
		{
			data_to_read[temp] = 0;
		}
	}
	else
	{
		i2c1_send(BME680_I2C_ADDR_PRIMARY, data_to_send, data_send_length, 0);
		i2c1_receive(BME680_I2C_ADDR_PRIMARY, data_to_read, data_read_length,1);
		results.chip_id = data_to_read[0];
	}
	return results;
}

int8_t user_i2c_read(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
    int8_t rslt = 0; /* Return 0 for Success, non-zero for failure */

	/*
     * The parameter dev_id can be used as a variable to store the I2C address of the device
     */
		i2c1_send(dev_id, &reg_addr, 1, 1);
		i2c1_receive(dev_id, reg_data, len, 1);

	/*
    * DATA: on the bus should be like
    * |------------+---------------------|
    * | I2C action | Data                |
    * |------------+---------------------|
    * | Start      | -                   |
    * | Write      | (reg_addr)          |
    * | Stop       | -                   |
    * | Start      | -                   |
    * | Read       | (reg_data[0])       |
    * | Read       | (....)              |
    * | Read       | (reg_data[len - 1]) |
    * | Stop       | -                   |
    * |------------+---------------------|
    */

    return rslt;
}

int8_t user_i2c_write(uint8_t dev_id, uint8_t reg_addr, uint8_t *reg_data, uint16_t len)
{
	int8_t rslt = 0; /* Return 0 for Success, non-zero for failure */
	uint8_t data[50];
	int8_t x;

	data[0] = reg_addr;
	for (x = 0; x < len; x++)
		data[x + 1] = reg_data[x];

	/*
     * The parameter dev_id can be used as a variable to store the I2C address of the device
     */
	i2c1_send(dev_id, data, len + 1, 1);

	/*
    * Data on the bus should be like
    * |------------+---------------------|
    * | I2C action | Data                |
    * |------------+---------------------|
    * | Start      | -                   |
    * | Write      | (reg_addr)          |
    * | Write      | (reg_data[0])       |
    * | Write      | (....)              |
    * | Write      | (reg_data[len - 1]) |
    * | Stop       | -                   |
    * |------------+---------------------|
    */

	return rslt;
}

void user_delay_ms(uint32_t period)
{
	/*
     * Return control or wait,
     * for a period amount of milliseconds
     */
	delay_timeout_ms(period);
}

/*!
 * @brief           Write operation in either I2C or SPI
 *
 * param[in]        dev_addr        I2C or SPI device address
 * param[in]        reg_addr        register address
 * param[in]        reg_data_ptr    pointer to the data to be written
 * param[in]        data_len        number of bytes to be written
 *
 * @return          result of the bus communication function
 */
int8_t bus_write(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data_ptr, uint16_t data_len)
{
   // ...
   // Please insert system specific function to write to the bus where BME680 is connected
   // ...
   user_i2c_write(dev_addr, reg_addr, reg_data_ptr, data_len);

   return 0;
}

/*!
 * @brief           Read operation in either I2C or SPI
 *
 * param[in]        dev_addr        I2C or SPI device address
 * param[in]        reg_addr        register address
 * param[out]       reg_data_ptr    pointer to the memory to be used to store the read data
 * param[in]        data_len        number of bytes to be read
 *
 * @return          result of the bus communication function
 */
int8_t bus_read(uint8_t dev_addr, uint8_t reg_addr, uint8_t *reg_data_ptr, uint16_t data_len)
{
   // ...
   // Please insert system specific function to read from bus where BME680 is connected
   // ...

   user_i2c_read(dev_addr, reg_addr, reg_data_ptr, data_len);

   return 0;
}

/*!
 * @brief           System specific implementation of sleep function
 *
 * @param[in]       t_ms    time in milliseconds
 *
 * @return          none
 */
void user_sleep(uint32_t t_ms)
{
	// ...
	// Please insert system specific function sleep or delay for t_ms milliseconds
	// ...
	delay_timeout_ms(t_ms);
}

void Reset_RTC(void)
{

	/* Enter init mode */
	LL_RTC_DisableWriteProtection(RTC);
	LL_RTC_EnterInitMode(RTC);

	//RTC, WEEKDAY, DAY, MONTH, YEAR
	LL_RTC_DATE_Config(RTC, LL_RTC_WEEKDAY_SATURDAY, __LL_RTC_CONVERT_BIN2BCD(1),
							 __LL_RTC_CONVERT_BIN2BCD(1), __LL_RTC_CONVERT_BIN2BCD(0));

	//RTC, FORMAT, HOURS, MINUTES, SECONDS
	LL_RTC_TIME_Config(RTC, LL_RTC_TIME_FORMAT_AM_OR_24, 0, 0, 0);

	/* Exit init mode */
	LL_RTC_DisableInitMode(RTC);
	LL_RTC_EnableWriteProtection(RTC);
}

/*!
 * @brief           Capture the system time in microseconds
 *
 * @return          system_current_time    current system timestamp in microseconds
 */
int64_t get_timestamp_us()
{
   // ...
   // Please insert system specific function to retrieve a timestamp (in microseconds)
   // ...
	int64_t system_current_time = 0;

	system_current_time = (HW_RTC_Tick_to_ms(HW_RTC_GetCurrentTime())) * 1000;

	return system_current_time;
}

/*!
 * @brief           Handling of the ready outputs
 *
 * @param[in]       timestamp       time in nanoseconds
 * @param[in]       iaq             IAQ signal
 * @param[in]       iaq_accuracy    accuracy of IAQ signal
 * @param[in]       temperature     temperature signal
 * @param[in]       humidity        humidity signal
 * @param[in]       pressure        pressure signal
 * @param[in]       raw_temperature raw temperature signal
 * @param[in]       raw_humidity    raw humidity signal
 * @param[in]       gas             raw gas sensor signal
 * @param[in]       bsec_status     value returned by the bsec_do_steps() call
 *
 * @return          none
 */
void output_ready(int64_t timestamp, float iaq, uint8_t iaq_accuracy, float temperature, float humidity,
     float pressure, float raw_temperature, float raw_humidity, float gas, bsec_library_return_t bsec_status,
     float static_iaq, float co2_equivalent, float breath_voc_equivalent)
{
   // ...
   // Please insert system specific code to further process or display the BSEC outputs
   // ...

   bme680_data.bme_IAQ = (uint16_t)iaq;
   bme680_data.bme_IAQ_Accuracy = iaq_accuracy;
	bme680_data.bme_Pressure = (uint16_t)pressure;
	bme680_data.bme_CO2 = (uint16_t)co2_equivalent;

	Debug_printf("DATA:%u,",(uint32_t)(timestamp/1000000));
	Debug_printf("%d,",(int)iaq);
	Debug_printf("%d,",(int)iaq_accuracy);
	Debug_printf("%d,",(int)temperature);
	Debug_printf("%d,",(int)humidity);
//	Debug_printf("%d,",(int)pressure);
//	Debug_printf("%d,",(int)raw_temperature);
//	Debug_printf("%d,",(int)raw_humidity);
//	Debug_printf("%d,",(int)gas);
//	Debug_printf("%d,",(int)bsec_status);
	Debug_printf("%d,",(int)static_iaq);
	Debug_printf("%d,",(int)(co2_equivalent));
//	Debug_printf("%d\n\r",(int)((breath_voc_equivalent)*100));
	Debug_printf("\n\r");

	await_uart_tx();
}

/*!
 * @brief           Load previous library state from non-volatile memory
 *
 * @param[in,out]   state_buffer    buffer to hold the loaded state string
 * @param[in]       n_buffer        size of the allocated state buffer
 *
 * @return          number of bytes copied to state_buffer
 * @note            Prototype, not tested yet!
 */
uint32_t state_load(uint8_t *state_buffer, uint32_t n_buffer)
{
   // ...
   // Load a previous library state from non-volatile memory, if available.
   //
   // Return zero if loading was unsuccessful or no state was available, 
   // otherwise return length of loaded state string.
   // ...

	for (uint32_t i = 0; i < n_buffer; i++)
	{
		state_buffer[i] = state_saved[i];
	}

	return 0;
}

/*!
 * @brief           Save library state to non-volatile memory
 *
 * @param[in]       state_buffer    buffer holding the state to be stored
 * @param[in]       length          length of the state string to be stored
 *
 * @return          none
 * @note            Prototype, not tested yet
 */
void state_save(const uint8_t *state_buffer, uint32_t length)
{
	// ...
	// Save the string some form of non-volatile memory, if possible.
	// ...

	for (uint32_t i = 0; i < length; i++)
	{
		state_saved[i] = state_buffer[i];
	}
}

/*!
 * @brief           Load library config from non-volatile memory
 *
 * @param[in,out]   config_buffer    buffer to hold the loaded state string
 * @param[in]       n_buffer        size of the allocated state buffer
 *
 * @return          number of bytes copied to config_buffer
 */
uint32_t config_load(uint8_t *config_buffer, uint32_t n_buffer)
{
	// ...
	// Load a library config from non-volatile memory, if available.
	//
	// Return zero if loading was unsuccessful or no config was available,
	// otherwise return length of loaded config string.
	// ...

	memcpy(config_buffer, bsec_config_iaq, sizeof(bsec_config_iaq));
	return 0;
}

void bme680_bsec_init()
{
	return_values_init ret;

	/* Call to the function which initializes the BSEC library 
    * Switch on low-power mode and provide no temperature offset */
	C02_init();

	ret = bsec_iot_init(BSEC_SAMPLE_RATE_ULP, 0.0f, bus_write, bus_read, user_sleep, state_load, config_load);
	if (ret.bme680_status)
	{
		/* Could not intialize BME680 */
		Debug_printf("Could not intialize BME680 %d\r\n", ret.bme680_status);
		return;
	}
	else if (ret.bsec_status)
	{
		/* Could not intialize BSEC library */
		Debug_printf("Could not intialize BSEC %d\r\n", ret.bsec_status);
		return;
	}
}

void service_bme680(void)
{
	bsec_iot_loop(user_sleep, get_timestamp_us, output_ready, state_save, 10000);
}

void bme680_uplink(void)
{
	static bool bme680_run = true;

	bme680_payload_t payload = {0};
	sht30Reading_t sht30_data = {0};
	Si1133_reading_t light_reading = {0};

	/********************************** Reading CO2 **********************************/
	CO2_reading_t CO2_value = CO2_read();
	payload.members.CO2 = CO2_value.value;
	/*********************************** Done CO2 ***********************************/

	/********************************* Reading SHT30 *********************************/
	sht30_data = sht30GetReading();
	payload.members.Temperature1 = sht30_data.T1 / 10;
	payload.members.Temperature2 = sht30_data.T2 / 10;
	payload.members.Humidity1 = round(sht30_data.H1 / 100);
	payload.members.Humidity2 = round(sht30_data.H2 / 100);
	/************************************* Done *************************************/

	/***************************** Reading light sensor *****************************/
	light_reading = read_light_level();

	// post-process the light reading onto a suitable range in 8-bits.
	light_reading.visible_reading = light_reading.visible_reading / 16;
	if (light_reading.visible_reading > 255)
	{
		light_reading.visible_reading = 255;
	}
	payload.members.Light = (uint8_t)light_reading.visible_reading;
	/************************************* Done *************************************/

	if (bme680_run)
	{
		bme680_run = false;
		service_bme680();
	}

	payload.members.IAQ = (uint16_t)round(bme680_data.bme_IAQ);
	payload.members.IAQ_Accuracy = bme680_data.bme_IAQ_Accuracy;
	payload.members.Pressure = (uint16_t)(bme680_data.bme_Pressure - PRESSURE_OFFSET);

	/********************************** Debugging **********************************/
	Debug_printf("Temperature 1 : %d.%d\r\n", (payload.members.Temperature1) / 10,
					 (payload.members.Temperature1) % 10);
	Debug_printf("Temperature 2 : %d.%d\r\n", (payload.members.Temperature2) / 10,
					 (payload.members.Temperature2) % 10);

	Debug_printf("Humidity 1    : %d\r\n", payload.members.Humidity1);
	Debug_printf("Humidity 2    : %d\r\n", payload.members.Humidity2);

	Debug_printf("Pressure is   : %d\r\n", payload.members.Pressure + PRESSURE_OFFSET);
	Debug_printf("IAQ is        : %d\r\n", payload.members.IAQ);
	Debug_printf("IAQ accuracy  : %d\r\n", payload.members.IAQ_Accuracy);
	Debug_printf("CO2 is        : %d\r\n", payload.members.CO2);
	Debug_printf("Visual reading: %d\r\n", payload.members.Light);
	/********************************** Debugging **********************************/

	payload.members.Reserved = 0;
	payload.members.Reserved1 = 0;
	payload.members.sys_voltage = fourBit_battery_calculation();
	payload.members.pkt_type = packet_type_data;

	Uplink(payload.payload, BME680_SIZE);
}

void bme680_onWakeup( void )
{
	if(wakeup_count % wakeups_per_uplink == 0)
	{
		Debug_printf("Regular Uplink (%d)\r\n", wakeups_per_uplink);
		bme680_uplink();
		return;
	}
	
	//this is only executed if at least one of the thresholds is enabled
	if((wakeup_count % threshold_wakeups == 0) && 
		(upper_temperature_threshold_enabled ||
		 lower_temperature_threshold_enabled ||
		 upper_humidity_threshold_enabled    ||
		 lower_humidity_threshold_enabled	 ||
		 upper_Pressure_threshold_enabled	 ||
		 lower_Pressure_threshold_enabled	 ||
		 upper_CO2_threshold_enabled			||
		 lower_CO2_threshold_enabled			||
		 upper_IAQ_threshold_enabled			||
		 lower_IAQ_threshold_enabled))
	{
		Debug_printf("Threshold Check (%d)\r\n", threshold_wakeups);
		sht30Reading_t sht30_result = sht30GetReading();
		Si1133_reading_t light_result = read_light_level();
		CO2_reading_t CO2_value = CO2_read();

		if (((sht30_result.T1 / 10) > upper_temperature_threshold ||
			  (sht30_result.T2 / 10) > upper_temperature_threshold) &&
			 upper_temperature_threshold_enabled)
		{
			Debug_printf("Upper Temperature Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if (((sht30_result.H1 / 100) > upper_humidity_threshold ||
			  (sht30_result.H2 / 100) > upper_humidity_threshold) &&
			 upper_humidity_threshold_enabled)
		{
			Debug_printf("Lower Humidity Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((bme680_data.bme_IAQ > upper_IAQ_threshold) &&
			 upper_IAQ_threshold_enabled)
		{
			Debug_printf("Upper IAQ Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((bme680_data.bme_IAQ < lower_IAQ_threshold) &&
			 lower_IAQ_threshold_enabled)
		{
			Debug_printf("Lower IAQ Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((CO2_value.value > upper_CO2_threshold) &&
			 upper_CO2_threshold_enabled)
		{
			Debug_printf("Upper CO2 Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((CO2_value.value < lower_CO2_threshold) &&
			 lower_CO2_threshold_enabled)
		{
			Debug_printf("Lower CO2 Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((bme680_data.bme_Pressure > upper_Pressure_threshold) &&
			 upper_temperature_threshold_enabled)
		{
			Debug_printf("Upper Pressure Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

		if ((bme680_data.bme_Pressure < lower_Pressure_threshold) &&
			 lower_temperature_threshold_enabled)
		{
			Debug_printf("Lower Pressure Threshold exceeded\r\n");
			bme680_uplink();
			return;
		}

	}
}

void bme680_save_config(void)
{
	bme680_config_page_layout_t config = {0};
	
	config.members.humidity_threshold_upper    = upper_humidity_threshold           ;
	config.members.humidity_thredhold_lower    = lower_humidity_threshold           ;
	config.members.temperature_threshold_upper = upper_temperature_threshold        ;
	config.members.temperature_threshold_lower = lower_temperature_threshold        ;
	config.members.humidity_upper_enabled      = upper_humidity_threshold_enabled   ;
	config.members.humidity_lower_enabled      = lower_humidity_threshold_enabled   ;
	config.members.temperature_upper_enabled   = upper_temperature_threshold_enabled;
	config.members.temperature_lower_enabled   = lower_temperature_threshold_enabled;

	config.members.pressure_threshold_upper    = upper_Pressure_threshold			;
	config.members.pressure_threshold_lower    = lower_Pressure_threshold			;
	config.members.co2_threshold_upper		   = upper_CO2_threshold				;
	config.members.co2_threshold_lower		   = lower_CO2_threshold				;
	config.members.iaq_threshold_upper		   = upper_IAQ_threshold				;
	config.members.iaq_threshold_lower		   = lower_IAQ_threshold				;
	config.members.pressure_upper_enabled	   = upper_Pressure_threshold_enabled	;
	config.members.pressure_lower_enabled	   = lower_Pressure_threshold_enabled	;
	config.members.co2_upper_enabled		   = upper_CO2_threshold_enabled		;
	config.members.co2_lower_enabled		   = lower_CO2_threshold_enabled		;
	config.members.iaq_upper_enabled		   = upper_IAQ_threshold_enabled		;
	config.members.iaq_lower_enabled		   = lower_IAQ_threshold_enabled		;
		
	config.members.threshold_wakeups           = threshold_wakeups                  ;

	co2_save_config();
	
	save_extra_config_page(config.raw_bytes, device_specific_page_1);
}

void bme680_load_config(void)
{
	bme680_config_page_layout_t config = {0};
	load_extra_config_page(config.raw_bytes, device_specific_page_1);
	

	upper_humidity_threshold            = config.members.humidity_threshold_upper   ;
	lower_humidity_threshold            = config.members.humidity_thredhold_lower   ;
	upper_temperature_threshold         = config.members.temperature_threshold_upper;
	lower_temperature_threshold         = config.members.temperature_threshold_lower;
	upper_humidity_threshold_enabled    = config.members.humidity_upper_enabled     ;
	lower_humidity_threshold_enabled    = config.members.humidity_lower_enabled     ;
	upper_temperature_threshold_enabled = config.members.temperature_upper_enabled  ;
	lower_temperature_threshold_enabled = config.members.temperature_lower_enabled  ;

	upper_Pressure_threshold			= config.members.pressure_threshold_upper	;
	lower_Pressure_threshold			= config.members.pressure_threshold_lower	;
	upper_CO2_threshold					= config.members.co2_threshold_upper		;
	lower_CO2_threshold					= config.members.co2_threshold_lower		;
	upper_IAQ_threshold					= config.members.iaq_threshold_upper		;
	lower_IAQ_threshold					= config.members.iaq_threshold_lower		;

	upper_Pressure_threshold_enabled	= config.members.pressure_upper_enabled	    ;
	lower_Pressure_threshold_enabled	= config.members.pressure_lower_enabled	    ;
	upper_CO2_threshold_enabled			= config.members.co2_upper_enabled			;
	lower_CO2_threshold_enabled			= config.members.co2_lower_enabled			;
	upper_IAQ_threshold_enabled			= config.members.iaq_upper_enabled			;
	lower_IAQ_threshold_enabled			= config.members.iaq_lower_enabled			;

	threshold_wakeups                   = config.members.threshold_wakeups          ;

	co2_load_config();
}

void bme680_onDownlink(uint8_t *buffer, uint8_t size)
{
	co2_onDownlink(buffer, size);
}

void bme680_cli_threshold_help()
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
	Debug_printf("\t\tPa_upper\r\n");
	Debug_printf("\t\tPa_lower\r\n");
	Debug_printf("\t\tCO2_upper\r\n");
	Debug_printf("\t\tCO2_lower\r\n");
	/* 
	Debug_printf("\t\tbVOC_upper\r\n");
	Debug_printf("\t\tbVOC_lower\r\n");
	 */
	Debug_printf("\t\tIAQ_upper\r\n");
	Debug_printf("\t\tIAQ_lower\r\n");
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
	Debug_printf("\t\tPa_upper\r\n");
	Debug_printf("\t\tPa_lower\r\n");
	Debug_printf("\t\tCO2_upper\r\n");
	Debug_printf("\t\tCO2_lower\r\n");
	/* 
	Debug_printf("\t\tbVOC_upper\r\n");
	Debug_printf("\t\tbVOC_lower\r\n");
	 */
	Debug_printf("\t\tIAQ_upper\r\n");
	Debug_printf("\t\tIAQ_lower\r\n");
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

void bme680_cli_threshold(int argc, char *argv[])
{
	//"threshold enable t_upper"
	//"threshold disable t_upper"
	//"threshold set t_upper [value]"
	float value;
	
	if(argc == 1)
	{
		if(!strcmp(argv[0], "show"))
		{
			Debug_printf("Upper temperature         :%d.%2d\r\n"   , upper_temperature_threshold/10, upper_temperature_threshold%10);
			await_uart_tx();    
			Debug_printf("Upper temperature Enabled :%s\r\n"        , upper_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("lower temperature         :%d.%d\r\n"   , lower_temperature_threshold/10, lower_temperature_threshold%10);
			await_uart_tx();    
			Debug_printf("lower temperature Enabled :%s\r\n"        , lower_temperature_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("Upper humidity            :%d\r\n"   , upper_humidity_threshold);
			await_uart_tx();    
			Debug_printf("Upper humidity Enabled    :%s\r\n"        , upper_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("lower humidity            :%d\r\n"   , lower_humidity_threshold);
			await_uart_tx();    
			Debug_printf("lower humidity Enabled    :%s\r\n"        , lower_humidity_threshold_enabled?"YES":"NO");
			await_uart_tx();    
    
			Debug_printf("Upper Pressure            :%d\r\n"		, upper_Pressure_threshold);
			await_uart_tx();    
			Debug_printf("Upper Pressure Enabled    :%s\r\n"		, upper_Pressure_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("lower Pressure            :%d\r\n"		, lower_Pressure_threshold);
			await_uart_tx();    
			Debug_printf("lower Pressure Enabled    :%s\r\n"		, lower_Pressure_threshold_enabled?"YES":"NO");
			await_uart_tx();    
    
			Debug_printf("Upper CO2                 :%d\r\n"		, upper_CO2_threshold);
			await_uart_tx();    
			Debug_printf("Upper CO2 Enabled         :%s\r\n"		, upper_CO2_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("lower CO2                 :%d\r\n"		, lower_CO2_threshold);
			await_uart_tx();    
			Debug_printf("lower CO2 Enabled         :%s\r\n"		, lower_CO2_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			/*  
			Debug_printf("Upper bVOC					:%d\r\n"		, upper_bVOC_threshold);
			await_uart_tx();
			Debug_printf("Upper bVOC Enabled			:%s\r\n"		, upper_bVOC_threshold_enabled?"YES":"NO");
			await_uart_tx();
			Debug_printf("lower bVOC					:%d\r\n"		, lower_bVOC_threshold);
			await_uart_tx();
			Debug_printf("lower bVOC Enabled			:%s\r\n"		, lower_bVOC_threshold_enabled?"YES":"NO");
			await_uart_tx();
			 */
			Debug_printf("Upper IAQ                 :%d\r\n"	    , upper_IAQ_threshold);
			await_uart_tx();    
			Debug_printf("Upper IAQ Enabled         :%s\r\n"		, upper_IAQ_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			Debug_printf("lower IAQ                 :%d\r\n"	    , lower_IAQ_threshold);
			await_uart_tx();    
			Debug_printf("lower IAQ Enabled         :%s\r\n"		, lower_IAQ_threshold_enabled?"YES":"NO");
			await_uart_tx();    
			
			Debug_printf("Checking thresholds every %d wakeups\r\n", threshold_wakeups);
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
			if(!strcmp(argv[1], "Pa_upper"))
			{
				//enable upper
				upper_Pressure_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "Pa_lower"))
			{
				//enable lower
				lower_Pressure_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "CO2_upper"))
			{
				//enable upper
				upper_CO2_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "CO2_lower"))
			{
				//enable lower
				lower_CO2_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			/* 
			if(!strcmp(argv[1], "bVOC_upper"))
			{
				//enable upper
				upper_bVOC_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "bVOC_lower"))
			{
				//enable lower
				lower_bVOC_threshold_enabled = true;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			 */
			if(!strcmp(argv[1], "IAQ_upper"))
			{
				//enable upper
				upper_IAQ_threshold_enabled = true;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "IAQ_lower"))
			{
				//enable lower
				lower_IAQ_threshold_enabled = true;
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
			if(!strcmp(argv[1], "h_upper"))
			{
				//disable upper
				upper_humidity_threshold_enabled = false;
				Debug_printf("Upper threshold Disabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//disable lower
				lower_humidity_threshold_enabled = false;
				Debug_printf("Lower threshold Disabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "Pa_upper"))
			{
				//enable upper
				upper_Pressure_threshold_enabled = false;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "Pa_lower"))
			{
				//enable lower
				lower_Pressure_threshold_enabled = false;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "CO2_upper"))
			{
				//enable upper
				upper_CO2_threshold_enabled = false;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "CO2_lower"))
			{
				//enable lower
				lower_CO2_threshold_enabled = false;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			/* 
			if(!strcmp(argv[1], "bVOC_upper"))
			{
				//enable upper
				upper_bVOC_threshold_enabled = false;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "bVOC_lower"))
			{
				//enable lower
				lower_bVOC_threshold_enabled = false;
				Debug_printf("Lower threshold Enabled\r\n");
				return;
			}
			 */
			if(!strcmp(argv[1], "IAQ_upper"))
			{
				//enable upper
				upper_IAQ_threshold_enabled = false;
				Debug_printf("Upper threshold Enabled\r\n");
				return;
			}
			if(!strcmp(argv[1], "IAQ_lower"))
			{
				//enable lower
				lower_IAQ_threshold_enabled = false;
				Debug_printf("Lower threshold Enabled\r\n");
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
				upper_temperature_threshold = (int16_t)(value*10);
				Debug_printf("Upper threshold set to %d.%d\r\n", upper_temperature_threshold/10, upper_temperature_threshold%10);
				return;
			}
			if(!strcmp(argv[1], "t_lower"))
			{
				//set lower
				lower_temperature_threshold = (int16_t)(value*10);
				Debug_printf("Upper threshold set to %d.%d\r\n", lower_temperature_threshold/10, lower_temperature_threshold%10);
				return;
			}
			//options here are upper or lower
			if(!strcmp(argv[1], "h_upper"))
			{
				//set upper
				upper_humidity_threshold = (uint16_t)(value);
				Debug_printf("Upper threshold set to %d\r\n", upper_humidity_threshold);
				return;
			}
			if(!strcmp(argv[1], "h_lower"))
			{
				//set lower
				lower_humidity_threshold = (uint16_t)(value);
				Debug_printf("Upper threshold set to %d\r\n", lower_humidity_threshold);
				return;
			}
			if(!strcmp(argv[1], "Pa_upper"))
			{
				//enable upper
				upper_Pressure_threshold = value - PRESSURE_OFFSET;
				Debug_printf("Upper threshold set to %d\r\n", upper_Pressure_threshold + PRESSURE_OFFSET);
				return;
			}
			if(!strcmp(argv[1], "Pa_lower"))
			{
				//enable lower
				lower_Pressure_threshold = value;
				Debug_printf("Lower threshold set to %d\r\n", lower_Pressure_threshold);
				return;
			}
			if(!strcmp(argv[1], "CO2_upper"))
			{
				//enable upper
				upper_CO2_threshold = value;
				Debug_printf("Upper threshold set to %d\r\n", upper_CO2_threshold);
				return;
			}
			if(!strcmp(argv[1], "CO2_lower"))
			{
				//enable lower
				lower_CO2_threshold = value;
				Debug_printf("Lower threshold set to %d\r\n", lower_CO2_threshold);
				return;
			}
			/* 
			if(!strcmp(argv[1], "bVOC_upper"))
			{
				//enable upper
				upper_bVOC_threshold = value;
				Debug_printf("Upper threshold set to %d\r\n", upper_bVOC_threshold);
				return;
			}
			if(!strcmp(argv[1], "bVOC_lower"))
			{
				//enable lower
				lower_bVOC_threshold = value;
				Debug_printf("Lower threshold set to %d\r\n", lower_bVOC_threshold);
				return;
			}
			 */
			if(!strcmp(argv[1], "IAQ_upper"))
			{
				//enable upper
				upper_IAQ_threshold = value;
				Debug_printf("Upper threshold set to %d\r\n", upper_IAQ_threshold);
				return;
			}
			if(!strcmp(argv[1], "IAQ_lower"))
			{
				//enable lower
				lower_IAQ_threshold = value;
				Debug_printf("Lower threshold set to %d\r\n", lower_IAQ_threshold);
				return;
			}
		}
	}
	
	bme680_cli_threshold_help();
}

/*! @}*/

