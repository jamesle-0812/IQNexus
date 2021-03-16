Use the Program in :

STM32Cube Expansion\Projects\Multi\Applications\LoRa\AT_Slave\MDK-ARM\B-L072Z-LRWAN1|Lora.uvprojx

1.	Right Click mlm32107x01 --> Options to Build --> C/C++ --> Change the Preprocessor Symbols under define to REGION_AS923
2.	Open Jumper CN8 to program external boards
3. 	Build --> Download 


Changes done : 

Under RegionAS923.h

Line 268 - changed SF10 max payload from 11 to 51
static const uint8_t MaxPayloadOfDatarateDwell1UpAS923[] = { 0, 0, 51, 53, 125, 242, 242, 242 }; //org 11

Line 274 - changed SF10 max payload from 11 to 51
static const uint8_t MaxPayloadOfDatarateDwell1DownAS923[] = { 0, 0, 51, 53, 126, 242, 242, 242 }; //org 11

 
