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

#ifndef GLOBAL_HEADER
#define GLOBAL_HEADER
#include <stdint.h>
#include <stdbool.h>

/********************************************************************
 *Debug Print Enable Defines                                        *
 ********************************************************************/
 
 /*******************************************************************
 *Project Defines                                                   *
 ********************************************************************/
//We define the HW version as a compiler argument now.
//T5is simplifies the build-release process.
//Look to the toolbar in keil to change configuration

#ifdef HW_1_0
	#define HW_MAJOR 1
	#define HW_MINOR 0
#endif

#ifdef HW_1_1
	#define HW_MAJOR 1
	#define HW_MINOR 1
#endif

#ifdef HW_1_2
	#define HW_MAJOR 1
	#define HW_MINOR 2
#endif

#ifdef HW_1_3
	#define HW_MAJOR 1
	#define HW_MINOR 3
#endif
//add to this for each hardware version, in order to check that they are mutally exclusive
#if defined(HW_1_0) +\
    defined(HW_1_1) +\
	defined(HW_1_2) +\
	defined(HW_1_3) +\
		0!= 1
	#error DEFINE ONE VERSION
#endif

#ifdef REGION_AS923
	#define RADIO_STRING "LoRa AS923"
#endif
#ifdef REGION_AU915
	#define RADIO_STRING "LoRa AU915"
#endif
#ifdef REGION_CN470
	#define RADIO_STRING "LoRa CN470"
#endif
#ifdef REGION_CN779
	#define RADIO_STRING "LoRa CN779"
#endif
#ifdef REGION_EU433
	#define RADIO_STRING "LoRa EU433"
#endif
#ifdef REGION_EU868
	#define RADIO_STRING "LoRa EU868"
#endif
#ifdef REGION_IN865
	#define RADIO_STRING "LoRa IN865"
#endif
#ifdef REGION_KR920
	#define RADIO_STRING "LoRa KR920"
#endif
#ifdef REGION_US915
	#define RADIO_STRING "LoRa US915"
#endif
#ifdef REGION_US915_HYBRID
	#define RADIO_STRING "LoRa US915-Hybrid"
#endif
#ifdef RADIO_SIGFOX_AT
	#define RADIO_STRING "SIGFOX AT"
#endif

#if defined(REGION_AS923)        +\
    defined(REGION_AU915)        +\
	defined(REGION_CN470)        +\
	defined(REGION_CN779)        +\
    defined(REGION_EU433)        +\
	defined(REGION_EU868)        +\
	defined(REGION_IN865)        +\
    defined(REGION_KR920)        +\
	defined(REGION_US915)        +\
	defined(REGION_US915_HYBRID) +\
	defined(RADIO_SIGFOX_AT)     +\
		0!= 1
	#error DEFINE ONE RADIO
#endif

//We define the radio as a compiler argument now.
//T5is simplifies the build-release process.
//Look to the toolbar in keil to change configuration

//add to this for each hardware version, in order to check that they are mutally exclusive
#if defined(RAIDO_LORA_INTERNAL) +\
    defined(RADIO_SIGFOX_AT)     +\
		0!= 1
	#error DEFINE ONE RADIO
#endif



//	Here are some definitions for a static assert, which will cause compile failure, if conditions are not met
//	thank https://stackoverflow.com/a/42966734
//	This STATIC assert will cause a compile time error if "condition" is not true
//	This has the advantage of being a typedef, which will not add to program memory usage.
#define STATIC_ASSERT(condition) typedef char p__LINE__[ (condition) ? 1 : -1];
//thank https://cboard.cprogramming.com/c-programming/102743-sizeof-struct-element.html#post750542
  /* Use this to cleanly access the internal member of a struct. */
#define MEMBER(outer,inner) (((outer*)0)->inner)
//STATIC_ASSERT((sizeof(MEMBER(sht30_config_page_layout_t,members)) == CONFIG_SIZE))
#define PACKED __attribute__((packed, aligned(1)))
/********************************************************************
 *Global Variables                                                  *
 ********************************************************************/
//  #define DISABLE_FIRMWARE_READOUT
 #define DISABLE_I2C_DEBUG
 #define DISABLE_I2C_DATA_DEBUG
 #define DISABLE_FLASH_DEBUG
 #define DISABLE_COUNT_DEBUG
 #define DISABLE_LORA_DEBUG
 #define DISABLE_SEND_DEBUG
 #define DISABLE_REGION_SPECIFIC_DEBUG
 #define DISABLE_MODBUS_DEBUG
 #define DISABLE_CO2_DEBUG
 #define DISABLE_CO2_L2_DEBUG
//  #define DISABLE_ALARM_DEBUG
 #define DISABLE_TIMER_DEBUG
 #define DISABLE_SX1276_DEBUG
 #define DISABLE_BUFFER_TRACE
 #define DISABLE_OWP_DEBUG
 #define DISABLE_SIGFOX_DEBUG
 #define DISABLE_LORA_CLASS_DEBUG
 #define DISABLE_LORA_CLASSC_DEBUG
//  #define	DISABLE_SPS30_DEBUG
 
// #define DISABLE_RADIO
// #define ENABLE_DEBUG_PINS_TIMERS
 
// #define ACCELERATE_HOURS
// #define DISABLE_BOOT_LED
// #define TIME_NEAR_OVERFLOW_AT_BOOT
 /*******************************************************************
 *Global Enums (for main.c)                                         *
 ********************************************************************/
#define DEVICE_UNINITIALISED  0xFF
#define DEVICE_UNCONFIGURED   0x00
 
  
 typedef enum
 {
	 test_pass = 0,
	 test_fail = 1
 }test_status_e;
 
 //This enum is used to prevent legacy counter mode on any device which is running 
 //firmware intended for HW 1.0 or 1.1
 typedef enum
 {
	 legacy_counters_enabled,
	 legacy_counters_disabled,
 }legacy_counters_e;
 
 typedef struct
 {
	 void          (* on_each_wakeup)           ();
	 void          (* on_scheduled_wakeup)      ();
	 void          (* on_hourly_alarm)          ();
	 void          (* on_hourly_alarm_interrupt)();
	 void          (* on_count_wakeup)          ();
	 void          (* on_alarm)                 ();
	 void          (* init)                     ();
	 test_status_e (* test_peripheral)          ();
	 void          (* send_data)                ();
	 void          (* on_downlink)              (uint8_t *buffer, uint8_t size);
	 void          (* save_config)              ();
	 void          (* save_data)                ();
	 void          (* load_config)              ();
	 void          (* load_data)                ();
	 void          (* cli_set_thresholds)       (int argc, char *argv[]);
	 void          (* cli_device_specific)      (int argc, char *argv[]);
	 char           * mode_name;
	 bool             legacy_counter;
	 bool             lora_class_c;
	 uint64_t         cli_commands; //If we end up with more than 64 command options, then consider using
 }sensum_device_callback_t;       //a packed bitfield struct here, to extend the range.
 
 extern uint8_t device_mode;
 extern sensum_device_callback_t device;
 extern uint32_t transmit_interval_ms;
 extern uint8_t wakeups_per_uplink;
 extern uint64_t wakeup_count;
 extern bool RTC_modified;
 
 //external function from main, which is used to update device behaviour
 void update_device_behaviour(void);
 void list_device_modes(void);
 int get_number_device_modes(void);
 char* get_string_from_mode_number(uint8_t number);
 
 void sleep_until_interrupt(void);

 //seems weird to have this include at the end of the file, but pins depends on the contents of global
 // TODO: Tidy this up at some point
 #include "pins.h"
#endif //GLOBAL_HEADER
