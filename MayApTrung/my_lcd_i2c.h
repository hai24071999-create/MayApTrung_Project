
#ifndef INC_MY_LCD_I2C_H_
#define INC_MY_LCD_I2C_H_

#include "stm32f1xx_hal.h"

#define CLCD_COMMAND 	0x00
#define CLCD_DATA 		0x01

#define LCD_EN 0x04  // Enable bit
#define LCD_RW 0x02  // Read/Write bit
#define LCD_RS 0x01  // Register select bit

#define CLCD_COMMAND 			0x00
#define CLCD_DATA 				0x01

// commands
#define LCD_CLEARDISPLAY 		0x01
#define LCD_RETURNHOME 			0x02

#define LCD_ENTRYMODESET 		0x04
#define LCD_DISPLAYCONTROL 		0x08
#define LCD_CURSORSHIFT 		0x10
#define LCD_FUNCTIONSET 		0x20
#define LCD_SETCGRAMADDR 		0x40
#define LCD_SETDDRAMADDR	 	0x80

// flags: display entry mode
#define LCD_ENTRYRIGHT 			0x00
#define LCD_ENTRYLEFT 			0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

// flags: display on/off control
#define LCD_DISPLAYON 			0x04
#define LCD_DISPLAYOFF 			0x00
#define LCD_CURSORON 			0x02
#define LCD_CURSOROFF 			0x00
#define LCD_BLINKON 			0x01
#define LCD_BLINKOFF 			0x00

// flags: display/cursor shift
#define LCD_DISPLAYMOVE 		0x08
#define LCD_CURSORMOVE 			0x00
#define LCD_MOVERIGHT 			0x04
#define LCD_MOVELEFT 			0x00

// flags: function set
#define LCD_8BITMODE 			0x10
#define LCD_4BITMODE 			0x00
#define LCD_2LINE 				0x08
#define LCD_1LINE 				0x00
#define LCD_5x10DOTS 			0x04
#define LCD_5x8DOTS 			0x00

#define LCD_BACKLIGHT 			0x08
#define LCD_NOBACKLIGHT 		0x00

typedef struct _CLCD_I2C_CONFIG
{
	I2C_HandleTypeDef* handleI2C;
	uint8_t address;

	uint8_t cols;
	uint8_t rows;

	uint8_t entry_mode;
	uint8_t display_ctrl;
	uint8_t cursor_shift;
	uint8_t funcSet;
	uint8_t back_light;
}CLCD_I2C_CONFIG;

void My_CLCD_I2C_Init(CLCD_I2C_CONFIG *cfg);
void My_CLCD_I2C_SetCursor(uint8_t Ypos, uint8_t Xpos);
void My_CLCD_I2C_Clear();
void My_CLCD_I2C_WriteString(char *str);
void My_CLCD_I2C_WriteChar(char c);

#endif /* INC_MY_LCD_I2C_H_ */
