#ifndef INC_USER_INPUT_H_
#define INC_USER_INPUT_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef enum {
	USER_INPUT_OK             = 0U,  // successful
	USER_INPUT_ERROR_INVALID  = 1U,  // invalid data
} UserInput_Error;

typedef enum {
    MODE_MANUAL = 0,
    MODE_AUTO   = 1,
    MODE_DEMO   = 2
} UserInput_Mode;

typedef enum {
    STATE_IDLE = 0,
    STATE_SELECT_MODE,
    STATE_INPUT_TEMP,
    STATE_INPUT_FAN,
    STATE_INPUT_WAIT,
    STATE_INPUT_DAY,
    STATE_DONE,
    STATE_ERROR
} InputState;

#define AUTO_MIN_DAY (1)
#define AUTO_MAX_DAY (21)

#define AUTO_MIN_HUMIDITY (50) //(%)

// for default
#define DEFAULT_TEMPERATURE 	(375)
#define DEFAULT_FAN_SPEED		(1)
#define DEFAULT_WAIT_TURNING 	(120)

// for demo mode
#define DEMO_TEMPERATURE 	(370)
#define DEMO_FAN 			(1)
#define DEMO_WAIT_TURNING 	(1)

//--------------------------------------------

typedef struct _USER_INPUT_DATA
{
	// Mode
	uint8_t mode; // 0: Manual,1: Auto, 2: Demo

	// Manual Mode Data
	uint16_t manual_temperature;
	uint8_t manual_fan_speed;
	uint16_t manual_wait_turning;

	// Auto Mode Data
	uint8_t nday;
}USER_INPUT_DATA;

typedef struct _USER_AUTO_MODE_DAY_RANGE_DATA
{
	uint8_t dayBegin;
	uint8_t dayEnd;

	// temperature (example: 35.6C -> 356)
	uint16_t temperature;

	// fan speed (0-1)
	uint8_t  fan_speed;

	// waiting turning (minutes)
	uint16_t wait_turning;

}USER_AUTO_MODE_DAY_RANGE_DATA;

#define AUTO_MAX_RANGE_DAY (3)
typedef struct _USER_AUTO_MODE_DATA
{
	// store the last time starting apply data
	uint32_t lastTimeInSec;
	uint32_t totalTimeInSec;

	uint8_t  rangeCount;
	USER_AUTO_MODE_DAY_RANGE_DATA rangeData[AUTO_MAX_RANGE_DAY];

	// temperature (30C-40C)
	//uint16_t auto_temperature_days[AUTO_MAX_DAY];
	// fan speed (0-1)
	//uint8_t auto_fan_speed_days[AUTO_MAX_DAY];
	// waiting turning (minutes)
	//uint8_t auto_wait_turning_days[AUTO_MAX_DAY];
} USER_AUTO_MODE_DATA;
//----------------------------------------

// mode(1)	+ manual_temperature(2) + manual_fan_speed(1) + manual_wait_turning(2) + nday(1) +
//
// lastTimeInSec(4) + rangeCount(1) + 3*(dayBegin(1) + dayEnd(1) + temperature(2) + fan_speed(1) + wait_turning(2))
//
// = 12 + 3*(7) = 12 + 21 = 33
//
#define MAX_BUFFER_STORING_FLASH_DATA (64)
//--------------------------------------------

typedef enum {
    DEVICE_OFF = 0,
    DEVICE_ON  = 1,
    DEVICE_RELEASE = 2
} DeviceState;

typedef struct _USER_CTRL_INFO
{
	DeviceState fan_state;

	DeviceState lamp_state;

}USER_CTRL_INFO;
//----------------------------------------

// function handle keypad input
//
void Handle_KeyPad_Input(char c);

// function handle lcd
//
void My_CLCD_I2C_Clear();
void My_CLCD_I2C_SetCursor(uint8_t Ypos, uint8_t Xpos);
void My_CLCD_I2C_WriteString(char *str);
void My_CLCD_I2C_WriteChar(char c);

// function to handle user input
//...
void UserInnput_Init(GPIO_TypeDef *fanBtnPort, uint16_t fanBtnPin,
						GPIO_TypeDef *tempBtnPort, uint16_t tempBtnPin);

void Handle_KeyPad_Input(char c);
InputState UserInnput_GetCurrentState();
void UserInnput_SetCurrentState(InputState state);

USER_INPUT_DATA *UserInnput_GetData();
void UserInnput_SetData(USER_INPUT_DATA *data);
USER_CTRL_INFO *UserInnput_GetCtrlInfo();

bool IsBtnFanClick();
bool IsBtnTemperatureClick();
void Handle_Btn_Fan_Click();
void Handle_Btn_Temperature_Click();

void UserInnput_GetCurrentAction( DeviceState *lptemperature,
									DeviceState *lpfan_speed,
									int currentTemperature,
									int currentHumidity,
									DeviceState *lpLastTempState,
									DeviceState *lpLastFanState);

//helper
uint16_t HourToMinute(uint8_t hour);
uint8_t MinuteToHour(uint16_t minute);
void MinuteToHourMinute(uint16_t minute, uint8_t *hour, uint8_t *min);

#endif /* INC_USER_INPUT_H_ */
