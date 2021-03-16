/*
   _____             _____                 
  / ____|           / ____|                
 | (___   ___ _ __ | (___  _   _ _ __ ___  
  \___ \ / _ \ '_ \ \___ \| | | | '_ ` _ \ 
  ____) |  __/ | | |____) | |_| | | | | | |
 |_____/ \___|_| |_|_____/ \__,_|_| |_| |_|
                                           
                                           
	Description: USART Register Level driver for LoRa only implementation project

	Maintainer: Shea Gosnell

	IF THIS IS BEING COMPILED FOR A DIFFERENT PLATFORM, SEVERAL THINGS CAN GO WRONG
	1) THE REGISTERS COULD BE DEFINED DIFFERENTLT - CHECK THE PROGRAMMING MANUAL
	2) THE ORDER OF THE BITFOElDS (MSB FIRST / LSB FIRST) COULD BE DIFfERENT - BE CAREFUL
	3) THE ADDRESSES OF THE REGIStERS COULD BE DIFFERENT, CHECK THE MANUAL

*/

#ifndef UART_SIGFOX_HEADER_PRIVATE
#define UART_SIGFOX_HEADER_PRIVATE
#include <stdint.h>
#include <stdarg.h>

/********************************************************************
 *Definitions                                                       *
 ********************************************************************/
//Clock Selection
#define USART5_CLKEN_ADDR	(RCC_APB1ENR_t*)0x40021038

//UART1
#define USART5_BASE_ADDR	0x40005000
#define USART5_CR1_ADDR (USART_CR1_t*)0x40005000
#define USART5_CR3_ADDR (USART_CR3_t*)0x40005008
#define USART5_BRR_ADDR (USART_BRR_t*)0x4000500C
#define USART5_RDR_ADDR (USART_RDR_t*)0x40005024
#define USART5_TDR_ADDR (USART_TDR_t*)0x40005028
#define USART5_ISR_ADDR (USART_ISR_t*)0x4000501C
#define USART5_ICR_ADDR (USART_ICR_t*)0x40005020

//CLI
#define RX_BUFFER_LENGTH 100
#define TX_BUFFER_LENGTH 50

/********************************************************************
 *Register Bitfields                                                *
 ********************************************************************/

//Control Register 1
// LSB FIRST
typedef struct
{
	uint32_t
		UE			:1,	//USART enable
		UESM		:1,	//USART enable in stop mode
		RE			:1,	//Receiver Enable
		TE			:1,	//Transmitter enable
		IDLEIE	:1,	//IDLE interrupt enable
		RXNEIE	:1,	//RXNE interrupt enable
		TCIE		:1,	//Transmission Complete interrupt enable
		TXEIE		:1,	//TX interrupt enable
		PEIE		:1,	//PE interrupt enable
		PS			:1,	//Parity Selection
		PCE			:1,	//Parity control enable
		WAKE		:1,	//Receiver wakeup method
		M0			:1,	//Word length, along with M1
		MME			:1,	//Mute mode enable
		CMIE		:1,	//Character match interrupt enable
		OVER8		:1,	//Oversampling mode
		DEDT		:5,	//Driver enable de-assertion time
		DEAT		:5,	//Driver enable assertion time
		RTOIE		:1,	//Receiver Timeout interrupt enable
		EOBIE		:1,	//End of block interrupt enable
		M1			:1,	//Word length, along with M0.
		Res1		:3;	//Reserved, must be kept at reset value
}USART_CR1_t; //at offset 0x00

//Control Register 2
//LSB FIRST
typedef struct
{
	uint32_t
		RES2			:4,	//reserved, must be kept at reset value
		ADDM7			:1,	//7-bit/4-bit addtess detection
		LBDL			:1,	//LIN break detection length
		LBDIE			:1,	//LIN break detection interrupt enable
		RES1			:1,	//reserved, must be kept at reset value
		LBCL			:1,	//Last bit clock pulse
		CPHA			:1,	//Clock phase
		CPOL			:1,	//Clock polarity
		CLKEN			:1,	//Clock Enable, used for smartcard or synchronous modes
		STOP			:2,	//STOP bits
		LINEN			:1,	// LIN mode enable
		SWAP			:1,	//Swap TX/RX pins
		RXINV			:1,	//RX pin active level inversion
		TXINV			:1,	//TX pin active level inversion
		DATAINV		:1,	//Binary data inversion
		MSBFI_RST	:1,	//Most signisicant bit first?
		ABREN			:1,	//Auto Baud Rate enable
		ABRMOD		:2,	//Auto Baud Rate Mode
		RTOEN			:1,	//Receiver Timeout Enable
		ADD				:8;	//USART node addresss
}USART_CR2_t; //at offset 0x04


//control Register 3
//LSB FIRST
typedef struct
{
	uint32_t
		EIE				:1,	//Error interrupt enable
		IREN			:1,	//IrDA mode enable
		IRLP			:1,	//IrDA low-power
		HDSEL			:1,	//Half-duplex selection
		NACK			:1,	//Smartcard NACK enable
		SCEN			:1,	//Smartcard mode enable
		DMAR			:1,	//DMA enable receiver
		DMAT			:1,	//DMA enable transmitter
		RTSE			:1,	//RTS enable
		CTSE			:1,	//CTS enable
		CTSIE			:1,	//CTS interrupt enable
		ONEBIT		:1,	//One sample bit method enable
		OVRDIS		:1,	//Overrun disable
		DDRE			:1,	//DMA disable on reception error
		DEM				:1,	//Driver enable mode
		DEP				:1,	//Driver enable polarity selection
		RES2			:1,	//reserved, must be kept at default value
		SCARCNT		:3,	//Smartcard auto-retry count
		WUS				:2,	//Wakeup from stop mode interrupt flag selection
		WUFIE			:1,	//Wakeup from stop mode interrupt enable
		UCESM			:1,	//USART_CR1_t clock enable in stop mode
		RES1			:8;	//Reserved, must be kept at default value
}USART_CR3_t;

//Baud Rate Register
//LSB FIRST
typedef struct
{
	uint16_t BRR;
	uint16_t RES1;
}USART_BRR_t;

//Guard Time Register
//LSB FIRST
typedef struct
{
	uint32_t
		PSC				:8,		//Prescaler value, only for smartcard and IrDA modes
		GT				:8,		//Guard Time value, for smartcard mode
		RES1			:16;	//Reserved, must be kept at reset value
}USART_GTPR_t;	//at offset 0x10

//Block Length Register
//LSB FIRST
typedef struct
{
	uint32_t
		RTO				:24,	//Receiver timeout value
		BLEN			:8;	//Block length
}USART_RTOR_t; //at offset 0x14

//Request Register
//LSB FIRST
typedef struct
{
	uint32_t
		ABRRQ			:1,		//Auto baud rate request
		SBKRQ			:1,		//Send break request
		MMRQ			:1,		//Mute mode request
		RXFRQ			:1,		//Receive data flush request
		TXFRQ,		:1,		//Transmit data flush request
		RES1			:27;	//Reserved, must be kept at reset value
}USART_RQR_t;	//at offset 0x18

//ISR Register (flags)
//LSB FIRST
typedef struct
{
	uint32_t
		PE				:1,	//Parity error
		FE				:1,	//framing error
		NF				:1,	//start bit noise detection flag
		ORE				:1,	//Overrun error
		IDLE			:1,	//idle line detected
		RXNE			:1,	//Read data register not empty
		TC				:1,	//Transmission Complete
		TXE				:1,	//Transmit data register empty
		LBDF			:1,	//LIN break detection flag
		CTSIF			:1,	//Clear to send interrupt flag
		CTS				:1,	//Clear to send flag
		RTOF			:1,	//Receiver timeout
		EOBF			:1,	//End of block flag
		RES2			:1,	//Reserved, must be kept at reset value
		ABRE			:1,	//Auto baud rate error
		ABRF			:1,	//Auto baud rate flag
		BUSY			:1,	//Busy flag
		CMF				:1,	//Character Match flag
		SBKF			:1,	//Send break flag
		RWU				:1,	//Receiver wakeup from mute mode
		WUF				:1,	//Wakeup from stop flag
		TEACK			:1,	//Transmit enable acknowledge flag
		REACK			:1,	//Receive enable acknowledge flag
		RES1			:9;	//Reserved, must be kept at reset value
}USART_ISR_t; //offset 0x1C
		
//Interrupt Clear register
//LSB FIRST
typedef struct
{
	uint32_t
		PECF			:1,		//Parity error clear flag
		FECF			:1,		//Framing error clear flag
		NCF				:1,		//Noise detected clear flag
		ORECF			:1,		//Overrun error clear flag
		IDLECF		:1,		//Idel line detected clear flag
		RES6			:1,		//Reserved, must be kept at reset value
		TCCF			:1,		//Transmission complete clear flag
		RES5			:1,		//Reserved, must be kept at reset value
		LBDCF			:1,		//LIN break detection clear flag
		CTSCF			:1,		//CTS clear flag
		RES4			:1,		//Reserved, must be kept at reset value
		RTOCF			:1,		//Receiver timeout clear flag
		EOBCF			:1,		//End of block clear flag
		RES3			:4,		//Reserved, must be kept at reset value
		CMCF			:1,		//Character match clear flag
		RES2			:2,		//Reserved, must be kept at reset value
		WUCF			:1,		//Wakeup from stop clear flag
		RES1			:11;	//Reserved, must be kept at reset value
}USART_ICR_t;	//offset 0x20

//Data receive register
//LSB first
typedef struct
{
	uint32_t
		RDR					:8,		//Receive data value
		RES1				:24;	//Reserved, must be kept at reset value
}USART_RDR_t;	//at offset 0x24


//Data transmit register
//LSB First
typedef struct
{
	uint32_t
		TDR					:8,		//Transmit data value
		RES1				:24;	//Reserved, must be kept at reset value
}USART_TDR_t;


//USART Clock select register
//LSB FIRST
typedef struct
{
	uint32_t
		USART5_SEL		:2,
		USART2_SEL		:2,
		RES2					:6,		//Reserved, must not be changed
		LPUSART5_SEL	:2,
		RES1					:20;	//Reserved, including clocks that are not USART
}USART_CLOCK_t;

//USART 1 peripheral clock enable register
//LSB FIRST
typedef struct
{
	uint32_t
		RES					:14,
		USART1_EN		:1,
		RES1				:17;
}RCC_APB2ENR_t;

typedef struct
{
	uint32_t
	RES1			:17,
	USART2_EN	:1,
	LPUART1_EN	:1,
	USART4_EN	:1,
	USART5_EN	:1,
	RES2			:11;
}RCC_APB1ENR_t;

/********************************************************************
 *Function Prototypes                                               *
 ********************************************************************/


#endif //UART_SIGFOX_HEADER_PRIVATE
