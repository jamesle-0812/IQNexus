/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	I2C register level header for LoRa only implementation project
								Implements I2C flash external periphheral.

	Maintainer: Shea Gosnell


*/



#ifndef FLASH_MAP_HEADER
#define FLASH_MAP_HEADER
#include <stdint.h>

#include "modbus_generic.h"
#include "global.h"
#define PACKED __attribute__((packed, aligned(1)))

#define CONFIG_SIZE 22 //Bytes
#define DATA_SIZE   60 //Bytes
#define PAGE_SIZE   64 //Bytes

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/

	//have 255 bits to mark which bits of the flash are known to be good
	//thats 32 bytes
	//or 8x uint32_t.
	//or 16x uint16_t
 
 typedef enum
 {
	 config_page = 0,
	 device_specific_page_1,
	 device_specific_page_2,
	 device_specific_page_3,
	 radio_config_page,
	 data_page_start, //pages 5-255 are for wear-leveling on data
	 data_page_end = 255,
 }flash_page_allocation_e;
 
#pragma push
#pragma anon_unions
typedef struct
{
	uint8_t
		wakeups :7,
		enabled :1;
	union
	{
		uint16_t u_threshold;
		int16_t  s_threshold;
	}PACKED;
}PACKED threshold_data_t;

#pragma pop
 

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint8_t  device_mode;                //01 Byte   total 1
		uint32_t wakeup_interval_ms;         //04 bytes  total 5
		uint16_t lora_hours_rejoin;          //02 bytes  total 7
		uint8_t  uplink_wakeups;             //01 byte   total 8
		uint8_t  appeui[8];                  //08 byte   total 16
		uint8_t  appkey[16];                 //16 Bytes  total 32
		uint32_t bad_block_marker[8];        //32 Bytes  toatl 64
	}PACKED members;
}config_page_base_layout_t;
STATIC_ASSERT((sizeof(MEMBER(config_page_base_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint16_t join_channel_list[6]; //12 Bytes  total 12
		uint8_t  reserved[52];         //52 Bytes  total 64
	}PACKED members;
}radio_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(radio_config_page_layout_t,members)) == PAGE_SIZE));


typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		threshold_data_t Temperature_threshold_upper;          //3 Bytes Total 3
		threshold_data_t Temperature_threshold_lower;          //3 Bytes Total 6
		uint8_t               reserved[PAGE_SIZE-6];
	}PACKED members;
}ds18b20_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(ds18b20_config_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint16_t humidity_threshold_upper;
		uint16_t humidity_thredhold_lower;
		int16_t  temperature_threshold_upper;
		int16_t  temperature_threshold_lower;
		uint8_t
			humidity_upper_enabled    :1,
			humidity_lower_enabled    :1,
			temperature_upper_enabled :1,
			temperature_lower_enabled :1,
			reserved                  :4;
		uint8_t threshold_wakeups;
		uint8_t reserved2[PAGE_SIZE-10];
	}PACKED members;
	
}sht30_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(sht30_config_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		modbus_register_t     modbus_slots[8];              //03 bytes  total 3
		uint16_t               write_slots[8];
	}PACKED members;
}generic_modbus_config_extra_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_config_extra_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint8_t
			reserved    :7,
			adc_enabled :1;
		uint8_t reserved2[PAGE_SIZE-1];
	}PACKED members;
}generic_modbus_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_config_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint64_t id[6];
		uint8_t reserved[16];
	}PACKED members;
}six_ds18b20_config_extra_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_config_extra_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint32_t              count_debounce;             //04 bytes  total 4
		uint32_t              count_leak;                 //04 bytes  total 8
		uint16_t              count_burst;                //02 bytes  total 10
		uint8_t               count_burst_hours;          //01 bytes  total 11
		uint8_t               invert_dir :1,
		                      pullup_enabled :1,
		                      reserved1  :6;              //01 bytes  total 12
		uint8_t               reserved[PAGE_SIZE-12];
	}PACKED members;
}count_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(count_config_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint16_t reads_per_uplink;
		uint8_t  device_address;
		uint8_t               reserved[PAGE_SIZE-3];
	}PACKED members;
}scl61d5_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(scl61d5_config_page_layout_t,members)) == PAGE_SIZE));

typedef struct
{
	uint8_t
		hw_gain :4,
		sw_gain :3;
}PACKED light_sensor_config_component_t;
typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint8_t flag;
		uint16_t abc_period;
		uint16_t abc_target;
		light_sensor_config_component_t light_config;
		uint8_t  reserved[PAGE_SIZE-6];
	}PACKED members;
}co2_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(co2_config_page_layout_t,members)) == PAGE_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint32_t write_count;
		uint8_t  mode_specific[DATA_SIZE];
	}PACKED members;
}data_page_base_layout_t;

typedef union
{
	uint8_t raw_bytes[DATA_SIZE];
	struct
	{
		uint32_t count1; //4 bytes
		uint32_t count2; //8 bytes
		uint32_t count3; //12 bytes
		uint8_t reserved[DATA_SIZE-12];
	}PACKED members;
}count_data_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(count_data_page_layout_t,members)) == DATA_SIZE));

typedef union
{
	uint8_t raw_bytes[PAGE_SIZE];
	struct
	{
		uint8_t	humidity_threshold_upper;
		uint8_t	humidity_thredhold_lower;
		int16_t  temperature_threshold_upper;
		int16_t  temperature_threshold_lower;
		uint16_t	pressure_threshold_upper;
		uint16_t	pressure_threshold_lower;
		uint16_t co2_threshold_upper		;
		uint16_t co2_threshold_lower		;
		uint16_t	iaq_threshold_upper		;
		uint16_t	iaq_threshold_lower		;
		light_sensor_config_component_t light_config;
		uint8_t 	flag;
		uint16_t abc_period;
		uint16_t abc_target;		
		uint16_t
			humidity_upper_enabled   	:1,
			humidity_lower_enabled   	:1,
			temperature_upper_enabled	:1,
			temperature_lower_enabled	:1,
			pressure_upper_enabled		:1,
			pressure_lower_enabled		:1,
			co2_upper_enabled				:1,
			co2_lower_enabled				:1,
			iaq_upper_enabled				:1,
			iaq_lower_enabled				:1,
			reserved                 	:6;
		uint8_t threshold_wakeups;
		uint8_t reserved2[PAGE_SIZE-27];
	}PACKED members;
	
}bme680_config_page_layout_t;
STATIC_ASSERT((sizeof(MEMBER(bme680_config_page_layout_t,members)) == PAGE_SIZE));
 
/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/
void save_global_config_page( void );
void load_gloabl_config_page( void );
void save_extra_config_page ( uint8_t mode_specific[static PAGE_SIZE], uint8_t page);
void load_extra_config_page ( uint8_t mode_specific[static PAGE_SIZE], uint8_t page);
void save_data_page         ( uint8_t data[static DATA_SIZE]);
void load_data_page         ( uint8_t data[static DATA_SIZE]);
void save_config_default    ( void );
void load_config_default    ( void );

void save_config( void );
void load_config( void );

void cli_flash_implementation( int argc, char *argv[]);
 
 /********************************************************************
 *Global Variables                                                  *
 ********************************************************************/





#endif //FLASH_MAP_HEADER


