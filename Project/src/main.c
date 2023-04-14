 /*
 / _____)             _              | |
( (____  _____ ____ _| |_ _____  ____| |__
 \____ \| ___ |    (_   _) ___ |/ ___)  _ \
 _____) ) ____| | | || |_| ____( (___| | | |
(______/|_____)_|_|_| \__)_____)\____)_| |_|
    (C)2013 Semtech

Description: Generic lora driver implementation

License: Revised BSD License, see LICENSE.TXT file include in the project

Maintainer: Miguel Luis, Gregory Cristian and Wael Guibene
*/
/******************************************************************************
 * @file    main.c
 * @author  MCD Application Team
 * @version V1.1.4
 * @date    08-January-2018
 * @brief   this is the main!
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2017 STMicroelectronics International N.V.
 * All rights reserved.</center></h2>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions are met:
 *
 * 1. Redistribution of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of STMicroelectronics nor the names of other
 *    contributors to this software may be used to endorse or promote products
 *    derived from this software without specific written permission.
 * 4. This software, including modifications and/or derivative works of this
 *    software, must execute solely and exclusively on microcontroller or
 *    microprocessor devices manufactured by or for STMicroelectronics.
 * 5. Redistribution and use of this software other than as permitted under
 *    this license is void and will automatically terminate your rights under
 *    this license.
 *
 * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
 * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT
 * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "hw.h"
#include "low_power_manager.h"
#include "lora.h"
#include "timeServer.h"
#include "version.h"
#include "debug_uart.h"
#include "sht20.h"
#include "sht30.h"
#include "i2cCO2.h"
#include "i2cFlash.h"
#include "modbus_scl61d5.h"
#include "modbus_generic.h"
#include "uart_soil_probe.h"
#include "lora_sensum.h"
#include "../SHELL/UsartShell.h"
#include "../SHELL/app_cli.h"
#include "counter.h"
#include "flash_map.h"
#include "ds18b20.h"
#include "2I2O.h"
#include "i2cLightSensor.h"

#include "global.h"
#include "sensum_version.h"
#include "watchdog.h"
#include "delays.h"
#include "adc.h"
#include "conf_bits.h"

#include "LoRaMac.h"
#include "lc_sensor.h"
#include "timeServer.h"

#include "at.h"

#include "sigfox_sensum.h"
#include "radio_common.h"


#ifndef DISABLE_ALARM_DEBUG
	#define alarm_printf(...) Debug_printf(__VA_ARGS__)
#else
	#define alarm_printf(...)
#endif


#define RSSI_THRESHOLD  //TODO

static int watchdog_was_reset_source = 0;
uint8_t device_mode = DEVICE_UNCONFIGURED;
bool RTC_modified = false;

void no_action()
{
	//no function, used for devices that don't implement some callbacks
}

test_status_e no_test()
{
	return test_pass;
}

void no_cli(int argc, char *argv[])
{
	//no function
}

void no_downlink_action(uint8_t *buffer, uint8_t size)
{
	//no function, used for devices that don't implement specific downlinks
}

//TODO: Add functionality for testing peripherals

sensum_device_callback_t device_modes[] = 
{
	{//0  Unconfigured
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&no_action,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&no_action,
		.test_peripheral           =&no_test,
		.send_data                 =&sendStartupPacket,
		.on_downlink               =&no_downlink_action,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Unconfigured",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},
	
	{//1  Three Counter
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&three_counter_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_three_counter,
		.test_peripheral           =&no_test,
		.send_data                 =&three_counter_uplink,
		.on_downlink               =&counterDownlinks, 
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Three Counter",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1 |
                                     cmd_count2 |
                                     cmd_count3,
	},
	
	{//2  Generic Modbus
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&genric_modbus_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&modbus_init,
		.test_peripheral           =&no_test,
		.send_data                 =&genric_modbus_uplink,
		.on_downlink               =&genericModbus_onDownlink,
		.save_config               =&save_generic_modbus_config,
		.save_data                 =&no_action,
		.load_config               =&load_generic_modbus_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Generic Modbus",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_modbus,
	},                             
	
	{//3  Two Counter with Tamper
		.on_each_wakeup            =&two_counter_alarms,
		.on_scheduled_wakeup       =&two_counter_synchronised_uplink,
		.on_hourly_alarm           =&two_counter_on_hour,
		.on_hourly_alarm_interrupt =&record_hourly_count2,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_two_counter_tamper,
		.test_peripheral           =&no_test,
		.send_data                 =&two_counter_synchronised_uplink,
		.on_downlink               =&counterDownlinks,
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Two Counter with Tamper",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1 |
		                             cmd_count2 |
		                             cmd_count_burst |
		                             cmd_count_leak,
	},                             
	
	{//4  SHT20                       
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&sht20_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&no_action, //SHT20 and SHT30 have no init
		.test_peripheral           =&no_test, //as it is not required
		.send_data                 =&sht20_uplink,
		.on_downlink               =&no_downlink_action,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="SHT20",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_thresholds,
	},                             
	
	{//5  Single Counter              
		.on_each_wakeup            =&single_counter_alarms,
		.on_scheduled_wakeup       =&single_counter_uplink,
		.on_hourly_alarm           =&single_counter_on_hour,
		.on_hourly_alarm_interrupt =&record_hourly_count,
		.on_count_wakeup           =&single_counter_on_count,
		.on_alarm                  =&no_action,
		.init                      =&init_single_counter,
		.test_peripheral           =&no_test,
		.send_data                 =&single_counter_uplink,
		.on_downlink               =&counterDownlinks,
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Single Counter",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1      |
		                             cmd_count_burst |
		                             cmd_count_leak,
	},                             
	
	{//6  Three Edge Alarm            
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&threeEdge_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&threeEdge_alarm,
		.on_alarm                  =&no_action,
		.init                      =&init_three_edge_alarm,
		.test_peripheral           =&no_test,
		.send_data                 =&threeEdge_uplink,
		.on_downlink               =&counterDownlinks,
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Three Edge Alarm",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1 |
		                             cmd_count2 |
		                             cmd_count3,
	},                             
	
	{//7  Soil Probe                  
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&probe_sendData,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&probe_init,
		.test_peripheral           =&test_probe,
		.send_data                 =&probe_sendData,
		.on_downlink               =&no_downlink_action,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Soil Probe",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},                             
	
	{//8 Mux device, digital, analog and 4-20mA
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&three_mux_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_three_mux_inputs,
		.test_peripheral           =&no_test,
		.send_data                 =&three_mux_uplink,
		.on_downlink               =&three_mux_onDownlink,
		.save_config               =&three_mux_save_config,
		.save_data                 =&three_mux_save_counter_data,
		.load_config               =&three_mux_load_config,
		.load_data                 =&three_mux_load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="3MUX",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1|cmd_mux_adc,
	},                             
	
	{//9  SHT30                       
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&sht30_onWakeup,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&no_action,//SHT20 and SHT30 have no init
		.test_peripheral           =&no_test,//as it is not requred
		.send_data                 =&sht30_uplink,
		.on_downlink               =&sht32_onDownlink,
		.save_config               =&sht30_save_config,
		.save_data                 =&no_action,
		.load_config               =&sht30_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&sht30_cli_threshold,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="SHT30",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_thresholds,
	},                             
	
	{//10 Dual Counter with ADC       
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&two_counter_adc_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_two_count_adc,
		.test_peripheral           =&no_test,
		.send_data                 =&two_counter_adc_uplink,
		.on_downlink               =&counterDownlinks,
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Dual Counter with ADC",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1 |
		                             cmd_count2,
	},                             
	
	{//11 Two I/O             
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&twoIO_uplink_state,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&twoIO_init,
		.test_peripheral           =&no_test,
		.send_data                 =&twoIO_uplink_state,
		.on_downlink               =&twoIO_onDownlink,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&twoIO_cli_threshold,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="2I2O",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =true,
		.cli_commands              = 0,
	}, 	                         
	
	{//12 CO2                         
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&co2_uplink,
		.on_hourly_alarm           =&CO2_hourly_interrupt,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&C02_init,
		.test_peripheral           =&no_test,
		.send_data                 =&co2_uplink,
		.on_downlink               =&co2_onDownlink,
		.save_config               =&co2_save_config,
		.save_data                 =&no_action,
		.load_config               =&co2_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&co2_cli_device,
		.mode_name                 ="CO2",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},        

	{//13 DS18B20                         
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&ds18b20_onWakeup,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&ds18b20_init,
		.test_peripheral           =&no_test,
		.send_data                 =&ds18b20_uplink,
		.on_downlink               =&ds18b20_onDownlink,
		.save_config               =&ds18b20_save_config,
		.save_data                 =&no_action,
		.load_config               =&ds18b20_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&ds18b20_cli_threshold,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="DS18B20",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_thresholds,
	},    
	{//14 Single Counter                       
		.on_each_wakeup            =&single_counter_alarms,
		.on_scheduled_wakeup       =&single_counter_simple_uplink,
		.on_hourly_alarm           =&single_counter_on_hour,
		.on_hourly_alarm_interrupt =&record_hourly_count,
		.on_count_wakeup           =&single_counter_on_count,
		.on_alarm                  =&no_action,
		.init                      =&init_single_counter,
		.test_peripheral           =&no_test,
		.send_data                 =&single_counter_simple_uplink,
		.on_downlink               =&counterDownlinks,
		.save_config               =&save_counter_config,
		.save_data                 =&save_counter_data,
		.load_config               =&load_counter_config,
		.load_data                 =&load_counter_data,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Single Counter Simplified",
		.legacy_counter            =legacy_counters_disabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_count1      |
		                             cmd_count_burst |
		                             cmd_count_leak,
	}, 	
	{//15 SCL61D5 Water meter         
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&scl61d5_wakeup,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&modbus_init,
		.test_peripheral           =&no_test,
		.send_data                 =&scl61d5_uplink,
		.on_downlink               =&scl61d5Downlink,
		.save_config               =&scl61d5_save_config,
		.save_data                 =&no_action,
		.load_config               =&scl61d5_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="SCL-61D5 Water meter",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	}, 
	{//16 MultiDS18B20
		.on_each_wakeup            =&ds18b20_mulit_on_each_wakeup,
		.on_scheduled_wakeup       =&multi_ds18b20_onWakeup,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&multi_ds18b20_init,
		.test_peripheral           =&no_test,
		.send_data                 =&multi_ds18b20_onWakeup,
		.on_downlink               =&ds18b20_onDownlink,
		.save_config               =&six_ds18b20_save_config,
		.save_data                 =&no_action,
		.load_config               =&six_ds18b20_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&ds18b20_cli_threshold,
		.cli_device_specific       =&ds18b20_cli_device,
		.mode_name                 ="Multi DS18B20",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = cmd_thresholds,
	},
	{//17 Three ADC
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&three_adc_send,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_three_adc,
		.test_peripheral           =&no_test,
		.send_data                 =&three_adc_send,
		.on_downlink               =&no_downlink_action,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="Three ADC",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},
	
	{//18 OLD CO2                         
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&legacy_co2_uplink,
		.on_hourly_alarm           =&CO2_hourly_interrupt,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&legacy_C02_init,
		.test_peripheral           =&no_test,
		.send_data                 =&legacy_co2_uplink,
		.on_downlink               =&legacy_co2_onDownlink,
		.save_config               =&co2_save_config,
		.save_data                 =&no_action,
		.load_config               =&co2_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&legacy_co2_cli_device,
		.mode_name                 ="LEGACY CO2",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},
	
	{//19 TEmperature Humidity Light                       
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&light_sensor_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&init_light_sensor,
		.test_peripheral           =&no_test,
		.send_data                 =&light_sensor_uplink,
		.on_downlink               =&no_downlink_action,
		.save_config               =&co2_save_config,
		.save_data                 =&no_action,
		.load_config               =&co2_load_config,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&light_sensor_cli,
		.mode_name                 ="SHT30+Light",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},

	{//20 LC Sensor
		.on_each_wakeup            =&no_action,
		.on_scheduled_wakeup       =&lc_sensor_uplink,
		.on_hourly_alarm           =&no_action,
		.on_hourly_alarm_interrupt =&no_action,
		.on_count_wakeup           =&no_action,
		.on_alarm                  =&no_action,
		.init                      =&lc_sensor_init,
		.test_peripheral           =&no_test,
		.send_data                 =&lc_sensor_uplink,
		.on_downlink               =&no_downlink_action,
		.save_config               =&no_action,
		.save_data                 =&no_action,
		.load_config               =&no_action,
		.load_data                 =&no_action,
		.cli_set_thresholds        =&no_cli,
		.cli_device_specific       =&no_cli,
		.mode_name                 ="LC sensor",
		.legacy_counter            =legacy_counters_enabled,
		.lora_class_c              =false,
		.cli_commands              = 0,
	},  
};

sensum_device_callback_t device = {0};

uint32_t transmit_interval_ms = 30000;
uint8_t  wakeups_per_uplink = 1;
uint64_t wakeup_count = 0;

void list_device_modes()
{
	uint8_t i;
	for(i=1; i<get_number_device_modes();i++)
	{
		Debug_printf("\t%3d: %s\r\n",i,device_modes[i].mode_name);
		await_uart_tx();
	}
}

int get_number_device_modes()
{
	return sizeof(device_modes)/sizeof(sensum_device_callback_t);
}

char* get_string_from_mode_number(uint8_t number)
{
	if(number < get_number_device_modes())
	{
		return device_modes[number].mode_name;
	}
	else 
	{
		return device_modes[0].mode_name;
	}
}

void update_device_behaviour()
{
	//If we are running firmware for boards where the counts intefere with the
	//radio interrupts, then we need to reject the change into unsupported modes.
	#if defined(HW_1_0) +\
		defined(HW_1_1) +\
		defined(HW_1_2)
	{
	#ifdef RAIDO_LORA_INTERNAL
		static uint8_t shadow_device_mode = 0;
		if(device_modes[device_mode].legacy_counter == legacy_counters_disabled)
		{
			Debug_printf("%s not supported on HW%d.%d\r\n", device_modes[device_mode].mode_name, HW_MAJOR, HW_MINOR);
			device_mode = shadow_device_mode;
			return;
		}
		shadow_device_mode = device_mode;
	#endif
	}
	#endif

	device.on_each_wakeup            = device_modes[device_mode].on_each_wakeup;
	device.on_scheduled_wakeup       = device_modes[device_mode].on_scheduled_wakeup;
	device.on_hourly_alarm           = device_modes[device_mode].on_hourly_alarm;
	device.on_hourly_alarm_interrupt = device_modes[device_mode].on_hourly_alarm_interrupt;
	device.on_count_wakeup           = device_modes[device_mode].on_count_wakeup;
	device.on_alarm                  = device_modes[device_mode].on_alarm;
	device.init                      = device_modes[device_mode].init;
	device.test_peripheral           = device_modes[device_mode].test_peripheral;
	device.send_data                 = device_modes[device_mode].send_data;
	device.on_downlink               = device_modes[device_mode].on_downlink;
	device.save_config               = device_modes[device_mode].save_config;
	device.save_data                 = device_modes[device_mode].save_data;
	device.load_config               = device_modes[device_mode].load_config;
	device.load_data                 = device_modes[device_mode].load_data;
	device.cli_set_thresholds        = device_modes[device_mode].cli_set_thresholds;
	device.cli_device_specific       = device_modes[device_mode].cli_device_specific;
	device.mode_name                 = device_modes[device_mode].mode_name;
	device.legacy_counter            = device_modes[device_mode].legacy_counter;
	device.lora_class_c              = device_modes[device_mode].lora_class_c;
	device.cli_commands              = device_modes[device_mode].cli_commands;
	
	//set the uplink wakeups to a suitable value.
	if(!(device.cli_commands & cmd_thresholds))
	{
		wakeups_per_uplink = 1;
	}
#ifndef DISABLE_RADIO
#ifdef RAIDO_LORA_INTERNAL
	if(device.lora_class_c)
	{
		LORA_RequestClass(CLASS_C);
	}
	else
	{
		LORA_RequestClass(CLASS_A);
	}
#endif
#endif
	device.init();
}
                                    
/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Main program
 * @param  None
 * @retval None
 */

/* Implementation of the HAL_Init() using LL functions */
void HW_Main_Init()
{
  /* Configure Buffer cache, Flash prefetch,  Flash preread */
#if (BUFFER_CACHE_DISABLE != 0)
  LL_FLASH_EnableBuffers();
#endif /* BUFFER_CACHE_DISABLE */

#if (PREREAD_ENABLE != 0)
  LL_FLASH_EnablePreRead();
#endif /* PREREAD_ENABLE */

#if (PREFETCH_ENABLE != 0)
  LL_FLASH_EnablePrefetch();
#endif /* PREFETCH_ENABLE */

  /*
   * Init the low level hardware
   * - Power clock enable
   * - Disable PVD
   * - Enable the Ultra Low Power mode
   * - Support DBG mode
   * - Take into account Fast Wakeup Mode
   * - Initialize GPIO
   */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_DisablePVD();      /* Disable the Power Voltage Detector */
  LL_PWR_EnableUltraLowPower();   /* Enables the Ultra Low Power mode */
  LL_FLASH_EnableSleepPowerDown();

  /*
   * In debug mode, e.g. when DBGMCU is activated, Arm core has always clocks
   * And will not wait that the FLACH is ready to be read. It can miss in this
   * case the first instruction. To overcome this issue, the flash remain clcoked during sleep mode
   */
  DBG(LL_FLASH_DisableSleepPowerDown(); );

#ifdef ENABLE_FAST_WAKEUP
  LL_PWR_EnableFastWakeUp();
#else
  LL_PWR_DisableFastWakeUp();
#endif

  HW_GpioInit();
	

}

static TimerEvent_t sleep_timer;
uint8_t wake__flag = 0;
void wake_flag_set()
{
	if(!RTC_modified)
	{
		wake__flag = 1;
		//Reset the timer here, to prevent timing drift during operation
		TimerStart(&sleep_timer);
	}
}


//sleeps until <wakeup_time_ms> milliseconds have passed since last wakeup
//Note that this is from last wakeup, not from now.
static void Sleep(uint32_t wakeup_time_ms)
{	
	static uint32_t previous_wakeup_time_ms = 0;
	
	//if there was a change in sleep time, then we need to update the alarm to
	//accomodate.
	//What this achieves is ensuring that the wake alarm is set on wake instead of
	//on sleep, as long as the sleep time length does not change.
	//I.E. the first transmission after the startup packet happens at a fixed 30 seconds
	//after the startup packet. We don't want the packet after THAT one to be in 30 seconds.
	
	//We also want to reset the alarm is the RTC has been modified. This will ensure
	//that the wakeup trigger has not been skipped, and that the trigger is not set
	//too far in the future.
	if(previous_wakeup_time_ms != wakeup_time_ms || RTC_modified)
	{
		
		//this is a cheap do-once type setup
		if(previous_wakeup_time_ms == 0)
		{
			//this needs to happen only once, to ensure that the timer
			//is not put into the queue multiple times.
			TimerInit(&sleep_timer, &wake_flag_set);	
			alarm_printf("Initialising timer\r\n");
		}
		
		alarm_printf("Resetting Alarm for %d seconds\r\n", wakeup_time_ms/1000);
		TimerStop(&sleep_timer);
		TimerSetValue(&sleep_timer, wakeup_time_ms);
		TimerStart(&sleep_timer);
		
		wake__flag = 0;
		RTC_modified = false;
	}
	
	previous_wakeup_time_ms = wakeup_time_ms;
	alarm_printf("Current Time:%u\r\n", HW_RTC_GetTimerValue());
	await_uart_tx();
	//set this to 0, to ensure that we do not wake immediatly.

	
	if(wake__flag)
	{
		alarm_printf("Arrived at sleep after alarm triggered\r\n");
		await_uart_tx();
		
		TimerStop(&sleep_timer);
		TimerSetValue(&sleep_timer, wakeup_time_ms);
		TimerStart(&sleep_timer);
		//note that firing this will not reset the wake flag.
		//we are essentially skipping over the sleep loop.
	}

	
	while(!wake__flag)
	{
		//check the wake-up sources, to do data saving and the such
	#ifndef DISABLE_RADIO
	#ifdef RAIDO_LORA_INTERNAL
		process_lora_downlink();
	#endif
	#endif
		//counters
		if(wake_via_counters)
		{
			#ifndef DISABLE_COUNT_DEBUG
			Debug_printf("Wake via counters\r\n");
			#endif
			wake_via_counters = 0;
			//save the updated counts, so the count is not lost on power off
			//the save data is here to ensure that it does not occur while the Uc is doing
			//someting important. This is to keep in line with the principle of keeping the
			//interrupts short.
			device.save_data();
			reset_watchdog();
			
			device.on_count_wakeup();
		}
		
		if(wake_via_rtc_b)
		{
			alarm_printf("wakeup hourly alarm\r\n");
			wake_via_rtc_b = 0;
			
			device.on_hourly_alarm();
		}
		
		device.on_each_wakeup();
		
		reset_watchdog();
		
		//ensure that all characters in the TX buffer are sent.
		await_uart_tx();
		delay_timeout_ms(10);
		// LL_GPIO_SetPinMode(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN, LL_GPIO_MODE_ANALOG);
		//we are going to disable the interrupts here, before the check for the wake flag
		//this should help prevent race conditions.
		// DISABLE_IRQ(); // causes interrupts to be disabled by setting PRIMASK.
		//check RTC last, if we are awoken by the RTC, then exit the sleep loop.
		//This should be redundant, because the check_timeout should fail if the RTC alarm has gone off.
		if(wake__flag)
		{
			//ensure to re-enable the IRQ here
			// ENABLE_IRQ();
			// __ISB();	// needed if there is a need to recognize a pending interrupt request immediately
			alarm_printf("wakeup_time_ms via Alarm\r\n");
			break;
		}
		
		disable_Debug();
		
		//sleep
		LPM_EnterStopMode();

		//wake up
		LPM_ExitStopMode();
		/* Enable GPIOs clock */
		//also re-enable the IRQ after Interrupt wake.
		// ENABLE_IRQ(); // cause interrupts to be enabled by clearing PRIMASK
		// __ISB();	// needed if there is a need to recognize a pending interrupt request immediately
		/* 
		 * this is where the IRQ is actually executed after the wake-up and restore clock signals
		 * The problem is SLEEP-ON-EXIT is enabled inside the RTC_Irq, leading to the MCU goes to sleep
		 * without the process of disabling unused parts. The result is the MCU goes to sleep with high current consumption.
		 */
		delay_timeout_ms(10);
		Debug_init();
		alarm_printf("waking up\r\n");
	}
	// LL_GPIO_SetPinMode(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN, LL_GPIO_MODE_OUTPUT);
	alarm_printf("Setting alarm for %d seconds\r\n", wakeup_time_ms/1000);
	wake__flag = 0;
	alarm_printf("Time at wake:%d\r\n", HW_RTC_GetTimerValue());
	//now print out the RTC time to confirm
	alarm_printf("Current Time:%u\r\n", HW_RTC_GetTimerValue());
	await_uart_tx();
}

static void print_startup_info()
{
	/* Configure the Lora Stack*/
#ifndef	DISABLE_RADIO
	radio_init();
#endif
	//Print out the version information
	Debug_printf("HW VERSION: %d.%d\r\n", HW_MAJOR, HW_MINOR);
	Debug_printf("SW VERSION: %d\r\n", VERSION_NUMBER);
#ifndef	DISABLE_RADIO
	Debug_printf("Radio     : ");
	Debug_printf(RADIO_STRING);
	Debug_printf("\r\n");
#endif
	Debug_printf("GIT_HASH  : %s\r\n", VERSION_HASH);
	Debug_printf("GIT_BRANCH: %s\r\n", VERSION_BRANCH);
	//for some reason a long version message will send the unit into a bootloop
	Debug_printf("MESSAGE   : ", VERSION_MESSAGE);
	Debug_printf(VERSION_MESSAGE);
	Debug_printf("\r\n");
#ifndef	DISABLE_RADIO
	Print_radio_information();
#endif
	//print out the operation mode
	Debug_printf("Device Mode: %s\r\n", device.mode_name);
	await_uart_tx();
	
	//we also want to print tx intervals/scheduled times at some point
	Debug_printf("Transmit interval: %d minutes\r\n", (transmit_interval_ms/60000)*wakeups_per_uplink);
}

void cli_timeout_timer_event()
{
	//Debug_disableCli(1);
}

void main_handle_cli()
{
	int i = 0;
	static TimerEvent_t cli_timeout_timer;
	
	//check for changing seconds, to manage the count-down
	//this will be unaffected by timer overflow, as it only returns 0-60 anyway.
	uint8_t previous_second = HW_RTC_GetSecond();
	
	//we need a 1 second timeout, and a 5 minute timeout
	//initialise a timer
	TimerInit(&cli_timeout_timer, &cli_timeout_timer_event);
	TimerStop(&cli_timeout_timer);
	TimerSetValue(&cli_timeout_timer, 300000);
	TimerStart(&cli_timeout_timer);
	
	Debug_printf("Press any key to enter CLI within the next 30 seconds\r\n");
	await_uart_tx();
	//allocate 30 seconds on boot for a user to activate the cli
	//clear the cliActive flag, without disabling the UART-RX
	Debug_disableCli(0);
	
	for(i=30;i>0;i--)
	{
		Debug_printf("%02d...\r\n",i);
		await_uart_tx();
		while(previous_second == HW_RTC_GetSecond())
		{
			await_uart_tx();
			while(Debug_cliActive())
			{
				await_uart_tx();
				if(vComShellTask())
				{
					//update timeout to 5 minutes
					TimerReset(&cli_timeout_timer);
				}
				
				//if the timeout expired, and we haven't just finished processing a command
				//then disable the CLI
				if(timer_expired(&cli_timeout_timer))
				{
					Debug_disableCli(1);
				}
				
				//if we have entered and exited the cli, then we can resume without
				//completing the timeout
				if(!Debug_cliActive())
				{
					i = 0;
				}
			}
		}
		previous_second = HW_RTC_GetSecond();
	}
	
	//ensure that the timer is removed from the linked-list before sending it out of scope.
	TimerStop(&cli_timeout_timer);
	//reload the config and data after the cli, in case there were changes made
	load_config();
	device.load_data();
}


static void init()
{
	int error = 0;
	/* STM32 HAL library initialization*/
	HW_Main_Init();



	/* Configure the system clock*/
	SystemClock_Config();

	/* Configure the hardware*/
	HW_Init();

	/* Configure Debug mode */
	DBG_Init();
	

	
	init_watchdog();
	

	
	/*Disable standby mode*/
	LPM_SetOffMode(LPM_APPLI_Id, LPM_Disable);
	

	reset_watchdog();
	//Debug_printf("AT interface");
	/* USER CODE END 1 */
	
	//Init the RTC
	HW_RTC_Init();	
	reset_watchdog();

	
	//initialise the random seed
	srand( HW_GetRandomSeed() ^ HW_RTC_GetTimerValue() );
	
	reset_watchdog();
	
	//Initialise the debug uart
	Debug_init();
	
	if(watchdog_was_reset_source)
	{
		Debug_printf("\r\n\t\t\tReset\r\n");
	}
		
	reset_watchdog();
	
	#ifdef DISABLE_FIRMWARE_READOUT
		//Ensure that the firmware cannot be read out from the uC.
		disable_fw_readout();
	#endif

	reset_watchdog();
	//initialise the LEDs, PB15,14,13
	GPIO_InitTypeDef GPIO_InitStruct;

	/* LED Initialisation  */
	GPIO_InitStruct.Mode      = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_MEDIUM;
	GPIO_InitStruct.Alternate = GPIO_AF6_LPUART1;

	HW_GPIO_Init(LED_RED_PORT, LED_RED_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(LED_ORANGE_PORT, LED_ORANGE_PIN, &GPIO_InitStruct);
	HW_GPIO_Init(LED_GREEN_PORT, LED_GREEN_PIN, &GPIO_InitStruct);
	
	
	//turn the load on for now
	HW_GPIO_Init(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN, &GPIO_InitStruct);
	LL_GPIO_SetOutputPin(PER_SUPPLY_ENABLE_PORT, PER_SUPPLY_ENABLE_PIN);

	/* USER CODE END 1 */
#ifndef	DISABLE_RADIO
	/* Configure the Lora Stack*/
	radio_init();
#endif
	reset_watchdog();
	do
	{
		if(error == 1)
		{
			
			//reset the LED's, to save battery
			LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
			LL_GPIO_ResetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
			LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
			//alert that we are entering sleep
			Debug_printf("Sleep\r\n\r\n");

			reset_watchdog();
			Sleep(transmit_interval_ms);
		}
		
		#ifndef DISABLE_BOOT_LED
		{
			//turn all led's on, if the device is reset via anything but the watchdog
			if(!watchdog_was_reset_source)
			{
				LL_GPIO_SetOutputPin(LED_RED_PORT,LED_RED_PIN);
				LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				LL_GPIO_SetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
			}
		}
		#endif
		
		//Put this here to ensure that there is some feedback while waiting for the flash data page seek
		Debug_printf("\r\n\r\n\r\nBOOT\r\n");
		await_uart_tx();
		reset_watchdog();
		

		#ifdef TIME_NEAR_OVERFLOW_AT_BOOT
		{
			if(!watchdog_was_reset_source)
			{	
				//after initialising the RTC, set it to a date/time that will cause a uint32 overflow on the MS range
				//we want a value near 4294967295, maybe 5 minutes before, to allow for startup time
				
				/* Enter init mode */
				LL_RTC_DisableWriteProtection(RTC);
				LL_RTC_EnterInitMode(RTC);

				//RTC, WEEKDAY, DAY, MONTH, YEAR
				LL_RTC_DATE_Config(RTC, LL_RTC_WEEKDAY_FRIDAY, __LL_RTC_CONVERT_BIN2BCD(18),__LL_RTC_CONVERT_BIN2BCD(2), __LL_RTC_CONVERT_BIN2BCD(0));
				//RTC, FORMAT, HOURS, MINUTES, SECONDS
				LL_RTC_TIME_Config(RTC, LL_RTC_TIME_FORMAT_AM_OR_24, 19, 00, 47);

				/* Exit init mode */
				LL_RTC_DisableInitMode(RTC);
				LL_RTC_EnableWriteProtection(RTC);
					
				//now print out the RTC time to confirm
				Debug_printf("Current Time:%u\r\n", HW_RTC_GetTimerValue());
				await_uart_tx();
			}
		}
		#endif
		await_uart_tx();
		//load configuration and data before entering CLI, so the values can be observed.
		//At this point we do not have a configuration, so we would have undefined behaviour using device.load_config().
		//Therefore we call load_config_default.
		load_gloabl_config_page();
		reset_watchdog();
		if(device_mode == DEVICE_UNINITIALISED)
		{
			await_uart_tx();
			flash_erase_all();
			await_uart_tx();
			load_gloabl_config_page();
			load_config();
			await_uart_tx();
		}
		reset_watchdog();
		//Once we have the current device mode, we can load the configuration using the device specific implementation
		load_config();
		device.load_data();
		reset_watchdog();

		//at this point, set the (initial) device type
		update_device_behaviour();
		//print generic startup info
		print_startup_info();
		
		
		
		if(!watchdog_was_reset_source && error == 0)
		{
			//if the reset source was not the watchdog, then we can do the cli.
			main_handle_cli();
		}
		else
		{
			Debug_printf("WDT_RESET - Bypassing CLI\r\n");
			await_uart_tx();
		}
		//ensure that the UART RX is disabled, and that the UART is disabled in sleep
		Debug_disableCli(1);
		Debug_printf("\r\nCLI Locked\r\n");
		await_uart_tx();
		
		//we would reset this at the top of the loop, but we need it to prevent entering the CLI on subsequent attempts
		error = 0;
		
		update_device_behaviour();
	#ifndef	DISABLE_RADIO
		//all devices need to send the startup packet
		sendStartupPacket();
	#endif

		
		#ifndef DISABLE_BOOT_LED
		{
				// LED_RED_PIN LED_RED_PORT LED_ORANGE_PIN LED_ORANGE_PORT LED_GREEN_PIN LED_GREEN_PORT
				// LL_GPIO_SetOutputPin(
				// LL_GPIO_ResetOutputPin(
				
				//on battery-insert/reset boot, check that the system is functioning correctly
				//use the LED's to indicate otherwise
				if(device_mode == DEVICE_UNCONFIGURED)
				{
					//device not configured, R=0 O=1, G=1
					LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
					LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
					LL_GPIO_SetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
				
					Debug_printf("Device Unconfigured\r\n");
					error = 1;
					//display the error code for 1 minute
					delay_timeout_ms(60000);
					continue;
				}
				
				//check flash (exist? bad-block?)
				
				
				if(device.test_peripheral() != test_pass)
				{
					Debug_printf("Peripheral Test Failed\r\n");
					//display an error code
					LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
					LL_GPIO_SetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
					LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
					//indicate that an error occured
					error = 1;
					//hold for 1 minute
					delay_timeout_ms(60000);
					continue;
				}
	}
	#endif

		
	#ifndef DISABLE_BOOT_LED
	{
		#ifndef	DISABLE_RADIO
			//check that the startup packet has received the join packet
			//if we have not joined the network, we know something is wrong
			if(!radio_joined())
			{
				//no join, set the LED's, and wait for user intervention
				//ROG=100
				LL_GPIO_SetOutputPin(LED_RED_PORT,LED_RED_PIN);
				LL_GPIO_ResetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
				LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
				
				Debug_printf("Failed to Join\r\n");
				error = 1;
				//display the error code for 1 minute
				delay_timeout_ms(60000);
				continue;
			}
		#endif
		//at this point, we know that the device is OK, so turn off all led's
		LL_GPIO_ResetOutputPin(LED_RED_PORT,LED_RED_PIN);
		LL_GPIO_ResetOutputPin(LED_ORANGE_PORT,LED_ORANGE_PIN);
		LL_GPIO_ResetOutputPin(LED_GREEN_PORT,LED_GREEN_PIN);
	}
	#endif
	}while(error ==1);
	
	//if we are a single counter, then we are going to want to transmit
	//history with our packets. To do this, we need to wake up each hour to take a reading
	//Other devices are going to use this to establish when they should be re-enabling the 
	//LoRa join request
	HW_RTC_StartHourlyAlarm();
	Debug_printf("Hourly alarm enabled\r\n");
	await_uart_tx();
	
	//alert that we are entering sleep
	Debug_printf("Sleep\r\n\r\n");
	//sleep for 30 seconds, and then send the first data packet
	#ifdef DISABLE_FIRMWARE_READOUT
		Sleep(30000);
	#else
		Sleep(3000);
	#endif
}



//Main Loop of program
//intended functionality goes here
static void program_loop()
{	

	
	//on wake, enter the infinite loop
	while(1)
	{
		//alert that we are awake
		Debug_printf("\r\nWake\r\n");
		await_uart_tx();
		
		//if the device is unconfigured, we should try to recover the configuration from flash
		if(device_mode == DEVICE_UNCONFIGURED)
		{
			//try to load variables
			load_config();
			
			//if recovery fails, reboot, see if that helps
			if(device_mode == DEVICE_UNCONFIGURED)
			{				
				Debug_printf("\r\nUnconfigured, resetting\r\n");
				await_uart_tx();
				force_mcu_reset_via_watchdog();
			}
		}
		

		device.on_scheduled_wakeup();
		wakeup_count++;
		
		//alert that we are entering sleep
		Debug_printf("Sleep\r\n\r\n");

		reset_watchdog();
		Sleep(transmit_interval_ms);
	}
}

void sleep_until_interrupt()
{
       //sleep
       //cannot use stop mode, as this would turn off the SPI clock
       LPM_EnterSleepMode();
}



int main(void)
{

	if(watchdog_reset_occured())
	{
		watchdog_was_reset_source = 1;
	}
	clear_reset_source();
	
	init();
	
	
	program_loop();

}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
