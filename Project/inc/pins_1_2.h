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

#ifndef PINS_1_2_HEADER
#define PINS_1_2_HEADER


/********************************************************************
 *Global Defines                                                    *
 ********************************************************************/
 
#define PER_SUPPLY_ENABLE_PORT	GPIOB
#define PER_SUPPLY_ENABLE_PIN	   LL_GPIO_PIN_5

#define COUNT1_PORT      		GPIOA
#define COUNT1_PIN       		LL_GPIO_PIN_8
#define COUNT1_ADC_CH         LL_ADC_CHANNEL_0

#define PROBE_BLEED_PORT 		GPIOA
#define PROBE_BLEED_PIN  		LL_GPIO_PIN_0

#define COUNT2_PORT  		   GPIOA
#define COUNT2_PIN   		   LL_GPIO_PIN_3
//Count 2 pin on HW 1.1 doesn't have any ADC associated with it
#define COUNT2_ADC_CH       LL_ADC_CHANNEL_VREFINT
		
#define COUNT1_DIR_PORT  		COUNT2_PORT
#define COUNT1_DIR_PIN   		COUNT2_PIN
		
#define STM_ADC_IN_PORT  		GPIOA
#define STM_ADC_IN_PIN   		LL_GPIO_PIN_5
#define STM_ADC_IN_CH    		LL_ADC_CHANNEL_5
		
#define COUNT3_PORT           GPIOA
#define COUNT3_PIN            LL_GPIO_PIN_2

#define ADC1_PORT               GPIOA
#define ADC1_PIN                LL_GPIO_PIN_0
#define ADC1_CH                 LL_ADC_CHANNEL_0
                                
#define ADC2_PORT               GPIOA
#define ADC2_PIN                LL_GPIO_PIN_3
#define ADC2_CH                 LL_ADC_CHANNEL_3
                                
#define ADC3_PORT               GPIOA
#define ADC3_PIN                LL_GPIO_PIN_5
#define ADC3_CH                 LL_ADC_CHANNEL_5

#define MODBUS_ADC_PORT         GPIOA
#define MODBUS_ADC_PIN          LL_GPIO_PIN_5
#define MODBUS_ADC_CH           LL_ADC_CHANNEL_5
		
#define TAMPER_PORT      		STM_ADC_IN_PORT
#define TAMPER_PIN       		STM_ADC_IN_PIN
		
#define DEBUG_TX_PORT    		GPIOA
#define DEBUG_TX_PIN     		LL_GPIO_PIN_9
		
#define DEBUG_RX_PORT    		GPIOA
#define DEBUG_RX_PIN     		LL_GPIO_PIN_10
		
#define MODBUS_TX_PORT   		GPIOA
#define MODBUS_TX_PIN    		LL_GPIO_PIN_2
	
#define MODBUS_RX_PORT   		GPIOA
#define MODBUS_RX_PIN    		LL_GPIO_PIN_3
		
#define CO2_EN_PORT      		GPIOA
#define CO2_EN_PIN       		LL_GPIO_PIN_2
		
#define CO2_nRDY_PORT    		GPIOA
#define CO2_nRDY_PIN     		LL_GPIO_PIN_3
		
#define MODBUS_M1_PORT   		GPIOB
#define MODBUS_M1_PIN    		LL_GPIO_PIN_7
		
#define MODBUS_M2_PORT   		GPIOB
#define MODBUS_M2_PIN    		LL_GPIO_PIN_2
		
#define I2C_CLK_PORT     		GPIOB
#define I2C_CLK_PIN      		LL_GPIO_PIN_8
		
#define I2C_DAT_PORT     		GPIOB
#define I2C_DAT_PIN      		LL_GPIO_PIN_9
		
#define PROBE_POWER_PORT 		GPIOB
#define PROBE_POWER_PIN  		LL_GPIO_PIN_5

#define LED_GREEN_PORT   		GPIOH
#define LED_GREEN_PIN    		LL_GPIO_PIN_1
		
#define LED_ORANGE_PORT  		GPIOA
#define LED_ORANGE_PIN   		LL_GPIO_PIN_11

#define LED_RED_PORT     		GPIOB
#define LED_RED_PIN      		LL_GPIO_PIN_12

#define SPI_MOSI_PORT           GPIOB
#define SPI_MOSI_PIN            LL_GPIO_PIN_15

#define SPI_MISO_PORT           GPIOB
#define SPI_MISO_PIN            LL_GPIO_PIN_14

#define SPI_SCLK_PORT           GPIOB
#define SPI_SCLK_PIN            LL_GPIO_PIN_13

#define SPI_CS_PORT             GPIOB
#define SPI_CS_PIN              LL_GPIO_PIN_6

#define PT1000_DRDY_PORT        GPIOH
#define PT1000_DRDY_PIN         LL_GPIO_PIN_0

#define SF_RESET_PORT           GPIOC
#define SF_RESET_PIN            LL_GPIO_PIN_0

#define SF_UART_RX_PORT         GPIOB
#define SF_UART_RX_PIN          LL_GPIO_PIN_4

#define SF_UART_TX_PORT         GPIOB
#define SF_UART_TX_PIN          LL_GPIO_PIN_3

#define DEBUG_1_PIN      		LED_RED_PIN
#define DEBUG_1_PORT     		LED_RED_PORT
		
#define DEBUG_2_PIN      		LED_ORANGE_PIN
#define DEBUG_2_PORT     		LED_ORANGE_PORT
		
#define DEBUG_3_PIN      		LED_GREEN_PIN
#define DEBUG_3_PORT     		LED_GREEN_PORT

#endif //PINS_1_2_HEADER		
