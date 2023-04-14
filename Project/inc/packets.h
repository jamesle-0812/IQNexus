/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: Physical pins definitions for the project

	Maintainer: Shea Gosnell

*/

#ifndef PACKETS_HEADER
#define PACKETS_HEADER


#include "global.h"

/********************************************************************
 *Global Defines                                                    *
 ********************************************************************/
#define MAX_RX_DATA 8 //maximum downlink of 8 bytes, dictated by sigfox

/********************************************************************
 *Global ENUMS                                                      *
 ********************************************************************/

typedef enum//max is 16
{
	packet_type_boot,      //0
	packet_type_status,    //1
	packet_type_data,      //2
	packet_type_summary,   //3
	packet_type_alarm,     //4
	packet_type_error,     //5
	packet_type_data2,     //6
	packet_type_downlink_response = 15,
}packet_type_e;

typedef enum
{
	probe_packet_type_temperature = 2,
	probe_packet_type_humidity    = 6,
}probe_packet_type_e;

typedef enum
{
	transmit_status_success,
	transmit_status_no_join,
	transmit_status_no_send,
	transmit_status_received_downlink,
}transmit_status_e;
 
typedef enum //max is 16
{
	downlink_type_generic_config = 0,
}downlink_type_e;

typedef enum //max 16
{
	downlink_type_count_params = 1,
	downlink_type_counts_1_2 = 2,
	downlink_type_count3 = 3,
}counter_downlink_e;

typedef enum
{
	downlink_type_sht30_temperature = 1,
	downlink_type_sht30_humidity = 2,
}sht32_downlink_e;

typedef enum
{
	downlink_type_co2_abc   = 1,
	downlink_type_co2_calib = 2,
	downlink_type_co2_factory_calib = 3,
}co2_downlink_e;
/* 
typedef enum
{
	downlink_type_bme680_generic	= 1,
}bme680_downlink_e;
 */
typedef enum
{
	downlink_type_ds18b20_temperature = 1,
}ds18b20_downlink_e;

typedef enum
{
	downlink_type_twoIO = 1,
}twoIO_downlink_e;

typedef enum
{
	downlink_type_generic_modbus_register = 1,
	downlink_type_generic_modbus_write    = 2,
	downlink_type_generic_modbus_adc      = 3,
}modbus_downlink_e;

typedef enum
{
	downlink_type_scl61d5_config = 1,
}scl61d5_downlink_e;

typedef enum
{
	count_hours00to06 = 0, //00
	count_hours06to12 = 1, //01
	count_hours12to18 = 2, //10
	count_hours18to00 = 3, //11
}lora_single_count_type_e;



/********************************************************************
 *UPLINKS                                                           *
 ********************************************************************/

#define STARTUP_PACKET_SIZE 10
typedef  union
{
	uint8_t payload[STARTUP_PACKET_SIZE];
	struct
	{
		uint8_t Hash[3];  //9 Bytes
		uint8_t HW_Minor; //6 Bytes
		uint8_t HW_Major; //5 Bytes
		uint32_t          //32 bits 4 Bytes
			device_mode   :8, //32
			second        :6, //14
			minute        :6, //18
			hour          :5, //12
			reserved      :7; //7 //was packet type and voltage
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}startup_packet_t;
STATIC_ASSERT((sizeof(MEMBER(startup_packet_t,members)) == STARTUP_PACKET_SIZE));

/****************************************************************************/

#define THREE_ADC_SIZE 7
typedef union
{
	uint8_t payload[THREE_ADC_SIZE];
	struct
	{
		uint16_t reading3;
		uint16_t reading2;
		uint16_t reading1;
		uint8_t
		    sys_voltage	    :4,
			pkt_type	    :4;
	}PACKED members;
}lora_three_adc_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_three_adc_payload_t,members)) == THREE_ADC_SIZE));

/****************************************************************************/

#define PROBE_HUMIDITY_SIZE 7
typedef union
{
	uint8_t payload[PROBE_HUMIDITY_SIZE];
	struct
	{
		uint8_t H6;
		uint8_t H5;
		uint8_t H4;
		uint8_t H3;
		uint8_t H2;
		uint8_t H1;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_probe_humidity_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_probe_humidity_payload_t,members)) == PROBE_HUMIDITY_SIZE));

#define PROBE_TEMPERATURE_SIZE 8
typedef union
{
	uint8_t payload[PROBE_TEMPERATURE_SIZE];
	struct
	{
		int8_t Tambient;
		int8_t T6;
		int8_t T5;
		int8_t T4;
		int8_t T3;
		int8_t T2;
		int8_t T1;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_probe_temperature_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_probe_temperature_payload_t,members)) == PROBE_TEMPERATURE_SIZE));

#define PROBE_ERROR_SIZE 2
typedef union
{
	uint8_t payload[PROBE_ERROR_SIZE];
	struct
	{
		uint8_t
			reserved  :6, // 8
			no_probe  :1, // 2
			crc_error :1; // 1
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_probe_error_t;
STATIC_ASSERT((sizeof(MEMBER(lora_probe_error_t,members)) == PROBE_ERROR_SIZE));

/****************************************************************************/

#define TWO_COUNT_ADC_SIZE 21
typedef union
{
uint8_t payload[TWO_COUNT_ADC_SIZE];
struct
{
	uint32_t
		p3Adc        :12, //32 bits - 20 Bytes
		p3delta2     :9 , //20 bits
		p3delta1     :9 , //11 bits
		reserved2    :2 ; //2  bits
	uint64_t
		reserved1    :1 , //64 bits - 16 bytes
		p2Adc        :12, //63 bits
		p2delta2     :9 , //51 bits
		p2delta1     :9 , //42 bits
		p1Adc        :12, //33 bits
		p1delta2     :9 , //21 bits
		Adc          :12; //12 bits
	uint64_t
		reserved     :1 , //64 bits - 8 bytes
		p1delta1     :9 , //63 bits
		count2       :27, //54 bits
		count1       :27; //27 bits
	uint8_t
		sys_voltage	:4,
		pkt_type	:4;
	}PACKED members;
}lora_two_countADC_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_two_countADC_payload_t,members)) == TWO_COUNT_ADC_SIZE));

/****************************************************************************/

#define GENERIC_MODBUS_SIZE 12
typedef union
{
	uint8_t payload[GENERIC_MODBUS_SIZE];
	struct
	{
		uint16_t Modbus[5]; //8
		uint16_t
			sequence_number :8,
		    sys_voltage	    :4,
			pkt_type	    :4;
	}PACKED members;
}lora_generic_modbus_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_generic_modbus_payload_t,members)) == GENERIC_MODBUS_SIZE));

/****************************************************************************/

#define CO2_SIZE 12
typedef union
{
	uint8_t payload[CO2_SIZE];
	struct
	{
		uint8_t  Light;        //11 Bytes
		uint16_t Humidity2;    //10 Bytes
		int16_t  Temperature2; //8 Bytes
		uint16_t Humidity1;    //6 Bytes
		int16_t  Temperature1; //4 Bytes
		uint16_t CO2;         //2 BYtes
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}co2_payload_t;
STATIC_ASSERT((sizeof(MEMBER(co2_payload_t,members)) == CO2_SIZE));

#define CO2_ERROR_SIZE 2
typedef union
{
	uint8_t payload[CO2_SIZE];
	struct
	{
		uint8_t
			co2_error_restore_fail :1,
			co2_error_start_fail   :1,
			co2_error_timeout_rise :1,
			co2_error_timeout_fall :1,
			co2_error_read_fail    :1,
			co2_error_backup_fail  :1,
			co2_error_value_H      :1,
			co2_error_value_L      :1;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}co2_error_payload_t;
STATIC_ASSERT((sizeof(MEMBER(co2_error_payload_t,members)) == CO2_ERROR_SIZE));

/****************************************************************************/

#define LIGHT_SIZE 11
typedef union
{
	uint8_t payload[LIGHT_SIZE];
	struct
	{
		uint16_t Humidity2;    //10 Bytes
		int16_t  Temperature2; //8 Bytes
		uint16_t Humidity1;    //6 Bytes
		int16_t  Temperature1; //4 Bytes
		uint16_t Light;        //11 Bytes
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}LIGHT_payload_t;
STATIC_ASSERT((sizeof(MEMBER(LIGHT_payload_t,members)) == LIGHT_SIZE));

/****************************************************************************/

#define SCL61D5_SIZE 12
typedef union
{
	uint8_t payload[SCL61D5_SIZE];
	struct
	{
		float watermark_high;
		float watermark_low;
		uint32_t 
			high_tod    :12,
			low_tod     :12,
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}scl61d5_payload_t;
STATIC_ASSERT((sizeof(MEMBER(scl61d5_payload_t,members)) == SCL61D5_SIZE));

/****************************************************************************/

#define THREE_COUNT_SIZE 12
typedef union
{
	uint8_t payload[THREE_COUNT_SIZE];
	struct
	{
		uint8_t
			reserved   :7,
			count3_lsb :1;
		uint16_t
			count3_cb; //6 bytes
		uint64_t
			count3_msb :10,
			count2     :27, //4 bytes
			count1     :27; //2 bytes
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_three_count_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_three_count_payload_t,members)) == THREE_COUNT_SIZE));

/****************************************************************************/

#define TWO_COUNT_SIZE 10
typedef union
{
	uint8_t payload[TWO_COUNT_SIZE];
	struct
	{
		uint8_t
			reserved :3, //8  - 9 Bytes
			tamper   :1, //5
			burst1   :1, //4
			burst2   :1, //3
			leak1    :1, //2
			leak2    :1; //1
		uint32_t Count2; //32 - 8 bytes
		uint32_t Count1; //32 - 4 bytes
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_two_count_payload_t;
STATIC_ASSERT((sizeof(MEMBER(lora_two_count_payload_t,members)) == TWO_COUNT_SIZE));

/****************************************************************************/

#define THREE_EDGE_SIZE 9
typedef union
{
	uint8_t payload[THREE_EDGE_SIZE];
	struct
	{
		uint8_t  temperature;
		uint16_t count3;
		uint16_t count2;
		uint16_t count1;
		uint16_t
			state1        :1,
			state2        :1,
			state3        :1,
			trigger1      :1,
			trigger2      :1,
			trigger3      :1,
			flash_status  :1,
			config_status :1,
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_three_edge_payload_t;
 STATIC_ASSERT((sizeof(MEMBER(lora_three_edge_payload_t,members)) == THREE_EDGE_SIZE));

/****************************************************************************/

#define THREE_MUX_SIZE 9
typedef union
{
	uint8_t payload[THREE_MUX_SIZE];
	struct
	{
		uint16_t ADC_V;
		uint16_t ADC_420;
		uint32_t count1;
		uint8_t
	    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_three_mux_payload_t;
 STATIC_ASSERT((sizeof(MEMBER(lora_three_mux_payload_t,members)) == THREE_MUX_SIZE));


/****************************************************************************/

//with a larger packet like this, the concept of a bitfield kind of falls apart
//We need to ensure that no fields cross a 32-bit boundary, otherwise we will 
//hit undefined teerritory, where the compiler can do whatever it likes
//To avoid this, some fields have to be split at the boundaries

 
#define SINGLE_COUNT_DATA_SIZE 12
typedef union
{
	uint8_t payload[SINGLE_COUNT_DATA_SIZE];
	struct
	{
		uint64_t //8 Bytes available
			Count1      :27,           // 27 bits | Total 27
			leak        :1,            // 01 bits | Total 28
			reserved    :3,            // 03 bits | Total 31
			Delta6      :11,           // 11 bits | Total 42
			Delta5      :11,           // 11 bits | Total 53
			Delta4      :11;           // 11 bits | Total 64
		uint32_t
			Delta3      :11,           // 11 bits | Total 11
			Delta2      :11,            // 05 bits | Total 16
			pkt_tod     :2,            // 02 bits | Total 08
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}single_count_data_t;
STATIC_ASSERT((sizeof(MEMBER(single_count_data_t,members)) == SINGLE_COUNT_DATA_SIZE));

#define SINGLE_COUNT_ALARM_SIZE 3
typedef union
{
	uint8_t payload[SINGLE_COUNT_ALARM_SIZE];
	struct
	{
		uint8_t burst;          //16 - 2 bytes
		uint8_t
			tamper        :1,   //08 - 1 byte
			reserved      :7;   //07
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_single_count_alarm_t;
STATIC_ASSERT((sizeof(MEMBER(lora_single_count_alarm_t,members)) == SINGLE_COUNT_ALARM_SIZE));

/****************************************************************************/

#define TWO_COUNT_ALARM_SIZE 2
typedef union
{
	uint8_t payload[TWO_COUNT_ALARM_SIZE];
	struct
	{
		uint8_t
			reserved      :5,   //8 - 1 Byte
			tamper        :1,   //3
			burst1        :1,   //2
			burst2        :1;   //1
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_two_count_alarm_t;
STATIC_ASSERT((sizeof(MEMBER(lora_two_count_alarm_t,members)) == TWO_COUNT_ALARM_SIZE));

/****************************************************************************/

#define SHT20_SIZE 5
typedef union
{
	uint8_t payload[SHT20_SIZE];
	struct
	{
		uint16_t Humidity;
		 int16_t Temperature;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}sht20_payload_t;
STATIC_ASSERT((sizeof(MEMBER(sht20_payload_t,members)) == SHT20_SIZE));

/****************************************************************************/

#define DS18B20_SIZE 4 //bytes
typedef union
{
	uint8_t payload[DS18B20_SIZE];
	struct
	{
		uint8_t
			reserved :6,
			status   :2;
		uint16_t temperature;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}ds18b20_payload_t;
STATIC_ASSERT((sizeof(MEMBER(ds18b20_payload_t,members)) == DS18B20_SIZE));

/****************************************************************************/

#define THREE_DS18B20_SIZE 11 //bytes
typedef union
{
	uint8_t payload[THREE_DS18B20_SIZE];
	struct
	{

		uint64_t
			reserved2    :2 ,
			owpstat      :2 ,
			Temperature6 :12,
			Temperature5 :12,
			Temperature4 :12,
			Temperature3 :12,
			Temperature2 :12;
		uint16_t
			Temperature1 :12,
			input3       :1 ,
			input2       :1 ,
			input1       :1 ,
			latch        :1 ;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}six_ds18b20_payload_t;
STATIC_ASSERT((sizeof(MEMBER(six_ds18b20_payload_t,members)) == THREE_DS18B20_SIZE));

/****************************************************************************/

#define SHT30_SIZE 9
typedef union
{
	uint8_t payload[SHT30_SIZE];
	struct
	{
		uint16_t Humidity2;
		 int16_t Temperature2;
		uint16_t Humidity1;
		 int16_t Temperature1;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}sht30_payload_t;
STATIC_ASSERT((sizeof(MEMBER(sht30_payload_t,members)) == SHT30_SIZE));

/****************************************************************************/

#define TWO_IO_SIZE 6
typedef union
{
	uint8_t payload[TWO_IO_SIZE];
	struct
	{
		uint16_t input2_reading;
		uint16_t input1_reading;
		uint8_t
			reserved      :2,
			output1_state :2,
			output2_state :2,
			reserved1     :2;
		uint8_t
		    sys_voltage	:4,
			pkt_type	:4;
	}PACKED members;
}lora_twoIO_t;
STATIC_ASSERT((sizeof(MEMBER(lora_twoIO_t,members)) == TWO_IO_SIZE));

/****************************************************************************/

#define BME680_SIZE 12
typedef union
{
	uint8_t payload[BME680_SIZE];
	struct
	{
		int32_t
			Temperature2: 11,
			Temperature1: 11,
			Reserved		: 2;
		uint32_t
			Humidity2	: 7,
			Humidity1	: 7,
			Light			: 8,
			IAQ			: 9,
			Reserved1	: 1;
		uint16_t	Pressure;
		uint16_t 
			CO2			:14,
			IAQ_Accuracy: 2;
		uint8_t
		   sys_voltage	:4,
			pkt_type		:4;
	}PACKED members;
}bme680_payload_t;
STATIC_ASSERT((sizeof(MEMBER(bme680_payload_t,members)) == BME680_SIZE));
 
/********************************************************************
 *DOWNLINKS                                                         *
 ********************************************************************/

//test packets:
	//0x00300100597BE000
	//Two Counter with tamper
	//1 min wakeup
	//1 hour rejoin
	//time is 12:47:31
	//do not update time
#define GENERIC_CONFIG_DOWNLINK_SIZE 8
typedef union
{
	struct
	{
		uint64_t
			ant_gain        :4 ,
			transmit_period :8 ,
			update_time     :1 ,
			second          :6 ,
			minute          :6 ,
			hour            :5 ,
			rejoin_period   :10,
			wakeup_period   :12,
			device_mode     :8 ,
			downlink_type   :4 ;
	}PACKED members;
	uint8_t payload[GENERIC_CONFIG_DOWNLINK_SIZE];
}generic_downlink_packet_t;
STATIC_ASSERT((sizeof(MEMBER(generic_downlink_packet_t,members)) == GENERIC_CONFIG_DOWNLINK_SIZE));
STATIC_ASSERT((GENERIC_CONFIG_DOWNLINK_SIZE <= MAX_RX_DATA));

/****************************************************************************/
/* 
#define BME680_DOWNLINK_SIZE 8
typedef union
{
	struct
	{
		uint64_t
			humidity_upper_enabled   	:1,
			humidity_lower_enabled   	:1,
			temperature_upper_enabled	:1,
			temperature_lower_enabled	:1,
			light_upper_enabled			:1,
			light_lower_enabled			:1,
			pressure_upper_enabled		:1,
			pressure_lower_enabled		:1,
			co2_upper_enabled				:1,
			co2_lower_enabled				:1,
			iaq_upper_enabled				:1,
			iaq_lower_enabled				:1,
			transmit_period :8 ,
			update_time     :1 ,
			second          :6 ,
			minute          :6 ,
			hour            :5 ,
			rejoin_period   :10,
			wakeup_period   :12,
			downlink_type   :4 ;
	}PACKED members;
	uint8_t payload[BME680_DOWNLINK_SIZE];
}bme680_downlink_packet_t;
STATIC_ASSERT((sizeof(MEMBER(bme680_downlink_packet_t,members)) == BME680_DOWNLINK_SIZE));
STATIC_ASSERT((BME680_DOWNLINK_SIZE <= MAX_RX_DATA));
 */
/****************************************************************************/

#define COUNTER_DOWNLINK_SIZE 8
typedef union
{
	struct
	{
		uint64_t
			reserved      :32,
			update_count3 :1,
			count3        :27,
			downlink_type :4;
	}PACKED count3;
	struct
	{
		uint64_t
			reserved      :2,
			update_dir    :1,
			invert_dir    :1,
			update_count2 :1,
			update_count1 :1,
			count2        :27,
			count1        :27,
			downlink_type :4;
	}PACKED counts_1_2;
	struct
	{
		uint64_t
			debounce      :7, //This is 100ms*value, to max of 12 second debounce
			burst         :9, //The burst is limited to 9 bits, max of 511. This is the reporting restriction on
							  // the deltas, so it kind of makes sense
			burst_hours   :12,
			leak          :32, // Should be enough for a leak threshold?
			downlink_type :4;
	}PACKED params;
	uint8_t payload[COUNTER_DOWNLINK_SIZE];
}counter_downlinks_t;
STATIC_ASSERT((sizeof(MEMBER(counter_downlinks_t,params)) == COUNTER_DOWNLINK_SIZE));
STATIC_ASSERT((sizeof(MEMBER(counter_downlinks_t,counts_1_2)) == COUNTER_DOWNLINK_SIZE));
STATIC_ASSERT((sizeof(MEMBER(counter_downlinks_t,count3)) == COUNTER_DOWNLINK_SIZE));
STATIC_ASSERT((COUNTER_DOWNLINK_SIZE <= MAX_RX_DATA));

/****************************************************************************/

#define THRESHOLD_DOWNLINK_SIZE 6
typedef union
{
	uint8_t payload[THRESHOLD_DOWNLINK_SIZE];
	struct
	{
		uint8_t  period;
		uint16_t lower;
		uint16_t upper;
		uint8_t
			enabled_l     :1,
			enabled_h     :1,
			update_l      :1,
			update_h      :1,
			downlink_type :4;
	}PACKED threshold;
}threshold_downlinks_t;
STATIC_ASSERT((sizeof(MEMBER(threshold_downlinks_t,threshold)) == THRESHOLD_DOWNLINK_SIZE));
STATIC_ASSERT(THRESHOLD_DOWNLINK_SIZE <= MAX_RX_DATA);

/****************************************************************************/

#define CO2_ABC_DOWNLINK_SIZE 3
typedef union
{
	uint8_t payload[CO2_ABC_DOWNLINK_SIZE];
	struct
	{
		uint16_t abc_target;
		uint8_t
			reserved      :3,
			abc_enabled   :1,
			downlink_type :4;
	}PACKED abc;
}co2_abc_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(co2_abc_downlink_t,abc)) == CO2_ABC_DOWNLINK_SIZE));
STATIC_ASSERT(CO2_ABC_DOWNLINK_SIZE <= MAX_RX_DATA);

#define CO2_CAL_DOWNLINK_SIZE 3
typedef union
{
	uint8_t payload[CO2_CAL_DOWNLINK_SIZE];
	struct
	{
		uint16_t target;
		uint8_t
			reserved      :4,
			downlink_type :4;
	}PACKED cal;
}co2_cal_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(co2_cal_downlink_t,cal)) == CO2_CAL_DOWNLINK_SIZE));
STATIC_ASSERT(CO2_CAL_DOWNLINK_SIZE <= MAX_RX_DATA);

/****************************************************************************/

#define GENERIC_MODBUS_DOWNLINK_SLOT_SIZE 7
typedef union
{
	uint8_t payload[GENERIC_MODBUS_DOWNLINK_SLOT_SIZE];
	struct
	{
		uint8_t
			reserved   :4,
			table_slot :4;
		uint16_t register_count;
		uint16_t start_addr;
		uint8_t  slaveID;
		uint8_t
			function_code :4,
			downlink_type :4;
	}PACKED register_description;
}generic_modbus_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_downlink_t,register_description)) == GENERIC_MODBUS_DOWNLINK_SLOT_SIZE));
STATIC_ASSERT(GENERIC_MODBUS_DOWNLINK_SLOT_SIZE <= MAX_RX_DATA);

#define GENERIC_MODBUS_DOWNLINK_WRITE_SIZE 3
typedef union
{
	uint8_t payload[GENERIC_MODBUS_DOWNLINK_WRITE_SIZE];
	struct
	{
		uint16_t slot_value;
		uint8_t
			write_slot :4,
			downlink_type :4;
	}PACKED members;
}generic_modbus_write_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_write_downlink_t,members)) == GENERIC_MODBUS_DOWNLINK_WRITE_SIZE));
STATIC_ASSERT(GENERIC_MODBUS_DOWNLINK_WRITE_SIZE <= MAX_RX_DATA);

#define GENERIC_MODBUS_DOWNLINK_ADC_SIZE 1
typedef union
{
	uint8_t payload[GENERIC_MODBUS_DOWNLINK_ADC_SIZE];
	struct
	{
		uint8_t
			adc_enabled   :1,
			reserved      :3,
			downlink_type :4;
	}PACKED members;
}generic_modbus_adc_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(generic_modbus_adc_downlink_t,members)) == GENERIC_MODBUS_DOWNLINK_ADC_SIZE));
STATIC_ASSERT(GENERIC_MODBUS_DOWNLINK_ADC_SIZE <= MAX_RX_DATA);

/****************************************************************************/

#define SCL61D5_DOWNLINK_SIZE 4
typedef union
{
	uint8_t payload[SCL61D5_DOWNLINK_SIZE];
	struct
	{
		uint16_t reads_per_uplink;
		uint8_t  device_address;
		uint8_t
			reserved      :4,
			downlink_type :4;
	}PACKED config;
}scl61d5_downlink_t;
STATIC_ASSERT((sizeof(MEMBER(scl61d5_downlink_t,config)) == SCL61D5_DOWNLINK_SIZE));
STATIC_ASSERT(SCL61D5_DOWNLINK_SIZE <= MAX_RX_DATA);

/****************************************************************************/

#define TWO_IO_DOWNLINK_SIZE 2
typedef union
{
	uint8_t payload[TWO_IO_DOWNLINK_SIZE];
	struct
	{
		uint16_t
			reserved      :6,
			update2       :1,
			update1       :1,
			state2        :2,
			state1        :2,
			downlink_type :4;
	}PACKED newStates;
}twoIO_downlinks_t;

STATIC_ASSERT((sizeof(MEMBER(twoIO_downlinks_t,newStates)) == TWO_IO_DOWNLINK_SIZE));
STATIC_ASSERT(TWO_IO_DOWNLINK_SIZE <= MAX_RX_DATA);

/****************************************************************************/
#endif //PACKETS_HEADER		
