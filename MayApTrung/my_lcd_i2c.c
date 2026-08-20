#include "my_lcd_i2c.h"

CLCD_I2C_CONFIG *gLCD_I2C_Cfg;

void My_CLCD_I2C_Delay(uint16_t Time)
{
	HAL_Delay(Time);
}

void My_CLCD_I2C_WriteI2C(uint8_t Data, uint8_t Mode)
{
	char Data_H;
	char Data_L;
	uint8_t Data_Write[4];

	Data_H = Data&0xF0;
	Data_L = (Data<<4)&0xF0;

	if(gLCD_I2C_Cfg->back_light)
	{
		Data_H |= LCD_BACKLIGHT;
		Data_L |= LCD_BACKLIGHT;
	}

	if(Mode == CLCD_DATA)
	{
		Data_H |= LCD_RS;
		Data_L |= LCD_RS;
	}
	else if(Mode == CLCD_COMMAND)
	{
		Data_H &= ~LCD_RS;
		Data_L &= ~LCD_RS;
	}

	Data_Write[0] = Data_H|LCD_EN;
	My_CLCD_I2C_Delay(1);

	Data_Write[1] = Data_H;
	Data_Write[2] = Data_L|LCD_EN;
	My_CLCD_I2C_Delay(1);

	Data_Write[3] = Data_L;
	HAL_I2C_Master_Transmit(gLCD_I2C_Cfg->handleI2C, gLCD_I2C_Cfg->address, (uint8_t *)Data_Write, sizeof(Data_Write), 1000);
}

void My_CLCD_I2C_Init(CLCD_I2C_CONFIG *cfg)//CLCD_I2C_Name* LCD, I2C_HandleTypeDef* hi2c_CLCD, uint8_t Address, uint8_t Colums, uint8_t Rows)
{
	gLCD_I2C_Cfg = cfg;

	//gLCD_I2C_Cfg->handleI2C = hi2c_CLCD;
	//gLCD_I2C_Cfg->ADDRESS = Address;
	//gLCD_I2C_Cfg->COLUMS = Colums;
	//gLCD_I2C_Cfg->ROWS = Rows;

	gLCD_I2C_Cfg->funcSet = LCD_FUNCTIONSET|LCD_4BITMODE|LCD_2LINE|LCD_5x8DOTS;
	gLCD_I2C_Cfg->entry_mode = LCD_ENTRYMODESET|LCD_ENTRYLEFT|LCD_ENTRYSHIFTDECREMENT;
	gLCD_I2C_Cfg->display_ctrl = LCD_DISPLAYCONTROL|LCD_DISPLAYON|LCD_CURSOROFF|LCD_BLINKOFF;
	gLCD_I2C_Cfg->cursor_shift = LCD_CURSORSHIFT|LCD_CURSORMOVE|LCD_MOVERIGHT;
	gLCD_I2C_Cfg->back_light = LCD_BACKLIGHT;

	My_CLCD_I2C_Delay(50);
	My_CLCD_I2C_WriteI2C(0x33, CLCD_COMMAND);

	My_CLCD_I2C_WriteI2C(0x33, CLCD_COMMAND);
	My_CLCD_I2C_Delay(5);
	My_CLCD_I2C_WriteI2C(0x32, CLCD_COMMAND);
	My_CLCD_I2C_Delay(5);
	My_CLCD_I2C_WriteI2C(0x20, CLCD_COMMAND);
	My_CLCD_I2C_Delay(5);

	My_CLCD_I2C_WriteI2C(gLCD_I2C_Cfg->entry_mode,CLCD_COMMAND);
	My_CLCD_I2C_WriteI2C(gLCD_I2C_Cfg->display_ctrl,CLCD_COMMAND);
	My_CLCD_I2C_WriteI2C(gLCD_I2C_Cfg->cursor_shift,CLCD_COMMAND);
	My_CLCD_I2C_WriteI2C(gLCD_I2C_Cfg->funcSet,CLCD_COMMAND);

	My_CLCD_I2C_WriteI2C(LCD_CLEARDISPLAY, CLCD_COMMAND);
	My_CLCD_I2C_WriteI2C(LCD_RETURNHOME, CLCD_COMMAND);
}

void My_CLCD_I2C_SetCursor(uint8_t Ypos, uint8_t Xpos)
{
	uint8_t dram_address = 0x00;

	if(Xpos >= gLCD_I2C_Cfg->cols)
	{
		Xpos = gLCD_I2C_Cfg->cols - 1;
	}

	if(Ypos >= gLCD_I2C_Cfg->rows)
	{
		Ypos = gLCD_I2C_Cfg->rows -1;
	}

	if(Ypos == 0)
	{
		dram_address = 0x00 + Xpos;
	}
	else if(Ypos == 1)
	{
		dram_address = 0x40 + Xpos;
	}
	else if(Ypos == 2)
	{
		dram_address = 0x14 + Xpos;
	}
	else if(Ypos == 3)
	{
		dram_address = 0x54 + Xpos;
	}

	My_CLCD_I2C_WriteI2C(LCD_SETDDRAMADDR|dram_address, CLCD_COMMAND);
}

void My_CLCD_I2C_WriteChar(char c)
{
	My_CLCD_I2C_WriteI2C(c, CLCD_DATA);
}

void My_CLCD_I2C_WriteString(char *str)
{
	while(*str != '\0')My_CLCD_I2C_WriteChar(*str++);
}

void My_CLCD_I2C_Clear()
{
	My_CLCD_I2C_WriteI2C(LCD_CLEARDISPLAY, CLCD_COMMAND);

	My_CLCD_I2C_Delay(5);
}
