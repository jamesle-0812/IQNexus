/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description:	Private I2C1 header for register descriptions, locations, etc
								Implements I2C flash external periphheral.

	Maintainer: Shea Gosnell


*/



#ifndef I2C_HEADER_PRIVATE
#define I2C_HEADER_PRIVATE
#include <stdint.h>

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
 
//Register addresses
#define I2C1_BASE_ADDR																	0x40005400

#define I2C1_CR1_ADDR						(I2C_CR1_t*)						0x40005400
#define I2C1_CR2_ADDR						(I2C_CR2_t*)						0x40005404
#define I2C1_OAR1_ADDR					(I2C_OAR1_t*)						0x40005408
#define I2C1_OAR2_ADDR					(I2C_OAR2_t*)						0x4000540C
#define I2C1_TIMINGR_ADDR				(I2C_TIMINGR_t*)				0x40005410
#define I2C1_TIMEOUTR_ADDR			(I2C_TIMEOUTR_t*)				0x40005414
#define I2C1_ISR_ADDR						(I2C_ISR_t*)						0x40005418
#define I2C1_ICR_ADDR						(I2C_ICR_t*)						0x4000541C

#define I2C1_PECR_ADDR					(I2C_PECR_t*)						0x40005420
#define I2C1_RXDR_ADDR					(I2C_RXDR_t*)						0x40005424
#define I2C1_TXDR_ADDR					(I2C_TXDR_t*)						0x40005428

#define I2C_RCC_APB1RSTR_ADDR		(I2C_RCC_APB1RSTR_t*)		0x40021028
#define I2C_RCC_APB1ENR_ADDR		(I2C_RCC_APB1ENR_t*)		0x40021038
#define I2C_RCC_APB1SMENR_ADDR	(I2C_RCC_APB1SMENR_t*)	0x40021048
#define I2C_RCC_CCIPR_ADDR			(I2C_RCC_CCIPR_t*)			0x4002104C

//peripheral clock register

/********************************************************************
*Register Bitfields                                                *
********************************************************************/

//LSB FIRST
typedef struct
{
	uint32_t
		PE					:1,
		TXIE				:1,
		RXIE				:1,
		ADDRIE			:1,
		NACKIE			:1,
		STOPIE			:1,
		TCIE				:1,
		ERRIE				:1,
		DNF					:4,
		ANFOFF			:1,
		RES1				:1,
		TXDMAEN			:1,
		RXDMAEN			:1,
		SBC					:1,
		NOSTRETCH		:1,
		WUPEN				:1,
		GCEN				:1,
		SMBHEN			:1,
		SMBDEN			:1,
		ALERTEN			:1,
		PECEN				:1,
		RES2				:8;
}I2C_CR1_t;	//offset 0x00

//LSB FIRST
typedef struct
{
	uint32_t
		SADD				:10,
		RD_WRN			:1,
		ADD10				:1,
		HEAD10R			:1,
		START				:1,
		STOP				:1,
		NACK				:1,
		NBYTES			:8,
		RELOAD			:1,
		AUTOEND			:1,
		PECBYTE			:1,
		RES1				:5;
}I2C_CR2_t;	//offset 0x04


//LSB FIRST
typedef struct
{
	uint32_t
		OA1					:10,
		OA1MODE			:1,
		RES1				:4,
		OA1EN				:1,
		RES2				:16;
}I2C_OAR1_t;	//offset 0x04

//LSB First
typedef struct
{
	uint32_t
		OA2					:8,
		OA2MS				:3,
		RES1				:4,
		OA2EN				:1,
		RES2				:16;
}I2C_OAR2_t;	//offset 0x0C

 //LSB FIRST
typedef struct
{
	uint32_t
		SCLL				:8,
		SCLH				:8,
		SDADEL			:4,
		SCLDEL			:4,
		RES1				:4,
		PRESC				:4;
}I2C_TIMINGR_t;	//offset 0x10

 //LSB FIRST
typedef struct
{
	uint32_t
		TIMEOUTA		:12,
		TIDLE				:1,
		RES1				:2,
		TIMEOUTEN		:1,
		TIMEOUTB		:12,
		RES2				:3,
		TEXTEN			:1;
}I2C_TIMEOUTR_t;	//offset 0x14

 //LSB FIRST
typedef struct
{
	uint32_t
		TXE					:1,
		TXIS				:1,
		RXNE				:1,
		ADDR				:1,
		NACKF				:1,
		STOPF				:1,
		TC					:1,
		TCR					:1,
		BERR				:1,
		ARL0				:1,
		OVR					:1,
		PECERR			:1,
		TIMEOUT			:1,
		ALERT				:1,
		RES1				:1,
		BUSY				:1,
		DIR					:1,
		ADDCODE			:7,
		RES2				:8;
}I2C_ISR_t;	//offset 0x18
 
 //LSB FIRST
typedef struct
{
	uint32_t
		RES1				:3,
		ADDRCF			:1,
		NACKCF			:1,
		STOPCF			:1,
		RES2				:2,
		BERRCF			:1,
		ARLOCF			:1,
		OVRCF				:1,
		PECCF				:1,
		TIMOUTCF		:1,
		ALERTCF			:1,
		RES3				:18;
}I2C_ICR_t;	//offset 0x1C

 //LSB FIRST
typedef struct
{
	uint32_t
		PEC					:8,
		RES1				:24;
}I2C_PECR_t;	//offset 0x20

//LSB FIRST
typedef struct
{
	uint32_t
		RXDATA			:8,
		RES1				:24;
}I2C_RXDR_t;	//offset 0x24

//LSB FIRST
typedef struct
{
	uint32_t
		TXDATA			:8,
		RES1				:24;
}I2C_TXDR_t;	//offset 0x28


//Peripheral Clock registers
typedef struct
{
	uint32_t
		RES1				:21,
		I2C1RST			:1,
		I2C2RST			:1,
		RES2				:7,
		I2C3RST			:1,
		RES3				:1;	
}I2C_RCC_APB1RSTR_t; //offset 0x28

typedef struct
{
	uint32_t
		RES1				:21,
		I2C1EN			:1,
		I2C2EN			:1,
		RES2				:7,
		I2C3EN			:1,
		RES3				:1;
}I2C_RCC_APB1ENR_t;	//offset 0x38

typedef struct
{
	uint32_t
		RES1				:21,
		I2C1SMEN		:1,
		I2C2SMEN		:1,
		RES2				:7,
		I2C3SMEN		:1,
		RES3				:1;
}I2C_RCC_APB1SMENR_t;	//offset 0x48

typedef struct
{
	uint32_t
		RES1				:12,
		I2C1SEL			:2,
		RES2				:2,
		I2C3SEL			:2,
		RES3				:14;
}I2C_RCC_CCIPR_t;	//offset 0x4c

#endif //I2C_HEADER_PRIVATE


