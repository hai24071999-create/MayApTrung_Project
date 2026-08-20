#include "user_input.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "my_flash.h"

//-------------------------------------------------------------------------
typedef struct _TEMP_CTRL_INFO {
    uint16_t target_temp;   // standard temperature *10 (example: 375 = 37.5°C)
    uint16_t lower_bound;   // lower bound *10
    uint16_t upper_bound;   // higher bound *10
    uint16_t fan_margin;    // hysteresis for fan *10 (example: 5 = 0.5°C)
    DeviceState lamp_state; // lamp status
    DeviceState fan_state;  // fan status
} TEMP_CTRL_INFO;

// Apply input user data
static InputState currentState = STATE_IDLE;
static USER_INPUT_DATA userDataApply;

// Temporarily data for input user data
static USER_INPUT_DATA userData;
static char buffer[16];
static uint8_t bufIndex = 0;

static USER_CTRL_INFO userCtrlInfo = {DEVICE_RELEASE, DEVICE_RELEASE};

static USER_AUTO_MODE_DATA userAutoModeData;

GPIO_TypeDef *gFanBtnPort = GPIOA;
uint16_t gFanBtnPin = GPIO_PIN_2;

GPIO_TypeDef *gTempBtnPort = GPIOA;
uint16_t gTempBtnPin = GPIO_PIN_5;

static void RefreshAutoMode();
static void SetDefault_Temperature(USER_INPUT_DATA *lpData);

// flash functions
static bool UserInnput_ReadFromFlash();
static bool UserInnput_WriteToFlash();

void LoadDefault_AllValues()
{
	// set default value to userDataApply
	userDataApply.mode = MODE_MANUAL;

	SetDefault_Temperature(&userDataApply);

	userDataApply.nday = 1;
	//----------------------------------

	userAutoModeData.rangeCount = 0;

	if (userAutoModeData.rangeCount < AUTO_MAX_RANGE_DAY)
	{
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayBegin = 1;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayEnd = 7;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].temperature = 378;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].fan_speed = 1;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].wait_turning = 2*60;
		userAutoModeData.rangeCount++;
	}

	if (userAutoModeData.rangeCount < AUTO_MAX_RANGE_DAY)
	{
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayBegin = 8;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayEnd = 17;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].temperature = 375;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].fan_speed = 1;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].wait_turning = 2*60;
		userAutoModeData.rangeCount++;
	}

	if (userAutoModeData.rangeCount < AUTO_MAX_RANGE_DAY)
	{
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayBegin = 18;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].dayEnd = 21;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].temperature = 372;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].fan_speed = 1;
		userAutoModeData.rangeData[userAutoModeData.rangeCount].wait_turning = 0;
		userAutoModeData.rangeCount++;
	}

	userAutoModeData.lastTimeInSec = HAL_GetTick()/1000;
	userAutoModeData.totalTimeInSec = 0;
	//----------------------------------

	//----------------------------------
	userData.mode = MODE_DEMO;
	userData.manual_temperature = DEMO_TEMPERATURE;
	userData.manual_fan_speed = DEMO_FAN;
	userData.manual_wait_turning = DEMO_WAIT_TURNING;
	//----------------------------------

	userData.nday = 1;
}

void UserInnput_Init(GPIO_TypeDef *fanBtnPort, uint16_t fanBtnPin,
					GPIO_TypeDef *tempBtnPort, uint16_t tempBtnPin)
{
	// Load default values
	LoadDefault_AllValues();

	// Try loading data from flash
	UserInnput_ReadFromFlash();

	userCtrlInfo.fan_state = DEVICE_RELEASE;
	userCtrlInfo.lamp_state = DEVICE_RELEASE;

	gFanBtnPort = fanBtnPort;
	gFanBtnPin = fanBtnPin;

	gTempBtnPort = tempBtnPort;
	gTempBtnPin = tempBtnPin;
}

//-------------------------------------------
// FLash functions
//

bool UserInnput_ConvertDataToBuffer(uint8_t *store_buffer, uint32_t  buffSize, uint32_t *lpWordCount)
{
	size_t sizeUserDataApply = sizeof(userDataApply);
	size_t sizeUserAutoModeData = sizeof(userAutoModeData);

	int totalSize = sizeUserDataApply + sizeUserAutoModeData;

	// round to word size
	//
	int totalWords = (totalSize + 3) / 4;

	if (totalWords*4 > buffSize)
	{
		return false;
	}

	uint8_t *lpBuffer = store_buffer;

	// set value of user input data to buffer
	uint8_t *lpData = (uint8_t *)&userDataApply;

	memcpy(lpBuffer, lpData, sizeUserDataApply);

	lpBuffer += sizeUserDataApply;

	// set value of user auto data to buffer
	lpData = (uint8_t *)&userAutoModeData;

	memcpy(lpBuffer, lpData, sizeUserAutoModeData);

	*lpWordCount = totalWords;

	return true;
}

bool UserInnput_ConvertBufferToData(uint8_t *store_buffer, uint32_t  buffLen)
{
	size_t sizeUserDataApply = sizeof(userDataApply);
	size_t sizeUserAutoModeData = sizeof(userAutoModeData);

	int totalSize = sizeUserDataApply + sizeUserAutoModeData;

	if (totalSize > buffLen)
	{
		return false;
	}

	uint8_t *lpBuffer = store_buffer;

	uint8_t *lpData = (uint8_t *)&userDataApply;

	memcpy(lpData, lpBuffer, sizeUserDataApply);

	lpBuffer += sizeUserDataApply;

	lpData = (uint8_t *)&userAutoModeData;

	memcpy(lpData, lpBuffer, sizeUserAutoModeData);

	return true;
}

bool UserInnput_ReadFromFlash()
{
	// read data from flash
	//

	// find good page for reading data
	// (find reverse from page 63 to page 54)

	PageFoundStatus status;
	uint32_t pageAddr = My_Flash_FindLatestGoodOrEmptyPage(FIND_REVERSE_PAGE_FROM, FIND_REVERSE_PAGE_TO, &status, true);

	if(status == PAGE_FOUND_NONE){
		// not found a valid page
		return false;
	}else if(status == PAGE_FOUND_EMPTY){
		// find an empty page
		//

		// get default value instead of
		//
		LoadDefault_AllValues();

		return true;
	}

	//else if(status == PAGE_FOUND_GOOD){

	uint8_t buffer[MAX_BUFFER_STORING_FLASH_DATA] = {0};
	uint32_t word_count = (MAX_BUFFER_STORING_FLASH_DATA + 3)/4;

	if (!My_Flash_ReadPage(pageAddr, (uint32_t *)buffer, word_count, true))
	{
		LoadDefault_AllValues();
		return false;
	}

	return UserInnput_ConvertBufferToData(buffer, sizeof(buffer));
}

bool UserInnput_WriteToFlash()
{
	PageFoundStatus status;
	uint32_t pageAddr = My_Flash_FindLatestGoodOrEmptyPage(FIND_REVERSE_PAGE_FROM, FIND_REVERSE_PAGE_TO, &status, true);

	if(status == PAGE_FOUND_NONE){

		// not found a valid page
		return false;
	} else if(status == PAGE_FOUND_EMPTY || status == PAGE_FOUND_GOOD){

		// write data to flash
		//

		uint8_t buffer[MAX_BUFFER_STORING_FLASH_DATA] = {0};
		uint32_t wordCount = 0;

		if (!UserInnput_ConvertDataToBuffer(buffer, sizeof(buffer), &wordCount))
		{
			return false;
		}

		return My_Flash_WritePage(pageAddr, (uint32_t *)buffer, wordCount, true);
	}

	return false;
}
//-------------------------------------------

static void SetDefault_Temperature(USER_INPUT_DATA *lpData)
{
	lpData->manual_temperature = DEFAULT_TEMPERATURE;
	lpData->manual_fan_speed = DEFAULT_FAN_SPEED;
	lpData->manual_wait_turning = DEFAULT_WAIT_TURNING;
}

USER_INPUT_DATA *UserInnput_GetData()
{
	return &userDataApply;
}

void UserInnput_SetData(USER_INPUT_DATA *data)
{
	USER_INPUT_DATA *lpUserData = &userDataApply;

	*lpUserData = *data;
}

USER_CTRL_INFO *UserInnput_GetCtrlInfo()
{
	return &userCtrlInfo;
}

InputState UserInnput_GetCurrentState()
{
	return currentState;
}

void UserInnput_SetCurrentState(InputState state)
{
	currentState = state;
}

uint16_t HourToMinute(uint8_t hour) {
    return (uint16_t)hour * 60;
}

uint8_t MinuteToHour(uint16_t minute) {
    return (uint8_t)(minute / 60);
}

void MinuteToHourMinute(uint16_t minute, uint8_t *hour, uint8_t *min) {
    *hour = minute / 60;
    *min  = minute % 60;
}

static void LCD_ShowMessage(const char *line1, const char *line2) {
    My_CLCD_I2C_Clear();
    My_CLCD_I2C_SetCursor(0,0);
    My_CLCD_I2C_WriteString((char*)line1);
    My_CLCD_I2C_SetCursor(1,0);
    My_CLCD_I2C_WriteString((char*)line2);
}

// Reset buffer
static void ResetBuffer() {
    memset(buffer, 0, sizeof(buffer));
    bufIndex = 0;
}

// ==================== VALIDATION ====================
static UserInput_Error Validate_TemperatureDecimal(uint16_t temp10x) {
    // temp10x là giá trị nhiệt độ nhân 10, ví dụ 32.5°C = 325
    if(temp10x <= 999) {
        return USER_INPUT_OK;
    } else {
        return USER_INPUT_ERROR_INVALID;
    }
}

static UserInput_Error Validate_FanSpeed(uint8_t fan) {
    return (fan <= 3) ? USER_INPUT_OK : USER_INPUT_ERROR_INVALID;
}

/*
static UserInput_Error Validate_WaitHour(uint8_t hour) {
    return (hour <= 24) ? USER_INPUT_OK : USER_INPUT_ERROR_INVALID;
}*/

static UserInput_Error Validate_Day(uint8_t day) {
    return (day >= 1 && day <= 21) ? USER_INPUT_OK : USER_INPUT_ERROR_INVALID;
}
// ====================================================

void My_Soft_Delay(uint32_t Delay)
{
	for(int i = 0; i < Delay; i++)
	{
		for(int  j = 0; j < 1608; j++)
		{

		}
	}
}

// processing keypad
void Handle_KeyPad_Input(char c) {
    switch(currentState) {
    case STATE_IDLE:
        if(c == '*') {
            currentState = STATE_SELECT_MODE;
            LCD_ShowMessage("(A)uto/(C)ontrol", "");
            ResetBuffer();
        }
        break;

    case STATE_SELECT_MODE:
        if(c == 'A') {
            userData.mode = MODE_AUTO;
            LCD_ShowMessage("Day(1-21):", "");
            currentState = STATE_INPUT_DAY;
            ResetBuffer();
        } else if(c == 'C') {
            userData.mode = MODE_MANUAL;
            LCD_ShowMessage("Temperature(C):", "");
            currentState = STATE_INPUT_TEMP;
            ResetBuffer();
        } else if(c == 'D') {
            userData.mode = MODE_DEMO;
            userData.manual_temperature = DEMO_TEMPERATURE;
            userData.manual_fan_speed = DEMO_FAN;
            userData.manual_wait_turning = DEMO_WAIT_TURNING; // demo: 1 minute
            currentState = STATE_DONE;
        }
        break;

    case STATE_INPUT_TEMP:
    	if(c == '#') {
			int whole = 0, decimal = 0;
			char *dot = strchr(buffer, '.');
			if(dot) {
				*dot = '\0';
				whole = atoi(buffer);
				decimal = atoi(dot+1);
				if(decimal > 9) decimal = 9; // chỉ lấy 1 chữ số thập phân
			} else {
				whole = atoi(buffer);
				decimal = 0;
			}
			uint16_t val = whole * 10 + decimal; // lưu dạng nhân 10

			if(Validate_TemperatureDecimal(val) == USER_INPUT_OK) {
				userData.manual_temperature = val;
				LCD_ShowMessage("Fan Speed(0-1):", "");
				currentState = STATE_INPUT_FAN;
			} else {
				currentState = STATE_ERROR;
			}
			ResetBuffer();
		}else if(c == 'D') {
        	ResetBuffer();
        	LCD_ShowMessage("Temperature(C):", buffer);
        }else {
			if(c >= '0' && c <= '9')
				buffer[bufIndex++] = c;

			LCD_ShowMessage("Temperature(C):", buffer);
		}
        break;

    case STATE_INPUT_FAN:
        if(c == '#') {
            uint8_t val = atoi(buffer);
            if(Validate_FanSpeed(val) == USER_INPUT_OK) {
                userData.manual_fan_speed = val;
                LCD_ShowMessage("Wait turn(hour):", "");
                currentState = STATE_INPUT_WAIT;
            } else {
                currentState = STATE_ERROR;
            }
            ResetBuffer();
        }else if(c == 'D') {
        	ResetBuffer();
        	LCD_ShowMessage("Fan Speed(0-1):", buffer);
        }else {
        	if(c >= '0' && c <= '9')
        		buffer[bufIndex++] = c;
            LCD_ShowMessage("Fan Speed(0-1):", buffer);
        }
        break;

    case STATE_INPUT_WAIT:
    	if(c == '#') {
			uint8_t valHour = atoi(buffer); // input hour
			uint16_t valMinute = HourToMinute(valHour); // change to minutes
			if(valHour <= 24) {
				userData.manual_wait_turning = valMinute;
				currentState = STATE_DONE;
			} else {
				currentState = STATE_ERROR;
			}
			ResetBuffer();
		} else if(c == 'D') {
        	ResetBuffer();
        	LCD_ShowMessage("Wait turn(hour):", buffer);
        }else {
			if(c >= '0' && c <= '9')
				buffer[bufIndex++] = c;
			LCD_ShowMessage("Wait turn(hour):", buffer);
		}
        break;

    case STATE_INPUT_DAY:
        if(c == '#') {
            uint8_t val = atoi(buffer);
            if(Validate_Day(val) == USER_INPUT_OK) {
                userData.nday = val;
                currentState = STATE_DONE;
            } else {
                currentState = STATE_ERROR;
            }
            ResetBuffer();
        }else if(c == 'D') {
        	ResetBuffer();
        	LCD_ShowMessage("Day(1-21):", buffer);
        }else {
        	if(c >= '0' && c <= '9')
        		buffer[bufIndex++] = c;
            LCD_ShowMessage("Day(1-21):", buffer);
        }
        break;

    case STATE_DONE:
        LCD_ShowMessage("Input Done!", "");
        UserInnput_SetData(&userData);
        RefreshAutoMode();
        currentState = STATE_IDLE;

        // try write data to flash
        //...
        if(userDataApply.mode != MODE_DEMO)
        {
        	UserInnput_WriteToFlash();
        }

        HAL_Delay(2000);
        break;

    case STATE_ERROR:
        LCD_ShowMessage("Error Input!", "");
        HAL_Delay(2000);
        currentState = STATE_IDLE;
        break;
    }
}

bool IsBtnFanClick()
{
	bool isClick = false;

	if (HAL_GPIO_ReadPin(gFanBtnPort, gFanBtnPin) == GPIO_PIN_RESET)
	{
		My_Soft_Delay(20);
		if (HAL_GPIO_ReadPin(gFanBtnPort, gFanBtnPin) == GPIO_PIN_RESET)
		{
			uint32_t timeout = 0;
			while((HAL_GPIO_ReadPin(gFanBtnPort, gFanBtnPin) == GPIO_PIN_RESET) && (++timeout < 10000)) {
				// checking time out...
				//
			}

			isClick = true;
		}
	}

	return isClick;
}

bool IsBtnTemperatureClick()
{
	bool isClick = false;

	if (HAL_GPIO_ReadPin(gTempBtnPort, gTempBtnPin) == GPIO_PIN_RESET)
	{
		My_Soft_Delay(20);
		if (HAL_GPIO_ReadPin(gTempBtnPort, gTempBtnPin) == GPIO_PIN_RESET)
		{
			uint32_t timeout = 0;
			while((HAL_GPIO_ReadPin(gTempBtnPort, gTempBtnPin) == GPIO_PIN_RESET) && (++timeout < 10000)) {
				// checking time out...
				//
			}

			isClick = true;
		}
	}

	return isClick;
}

void Handle_Btn_Fan_Click()
{
	switch(userCtrlInfo.fan_state) {
	case DEVICE_ON:      userCtrlInfo.fan_state = DEVICE_OFF; break;
	case DEVICE_OFF:     userCtrlInfo.fan_state = DEVICE_RELEASE; break;
	case DEVICE_RELEASE: userCtrlInfo.fan_state = DEVICE_ON; break;
	}
	//LCD_ShowNormalStatus(&userData, &ctrlInfo, currentTempSensor, currentHumiditySensor);
}

void Handle_Btn_Temperature_Click()
{
    switch(userCtrlInfo.lamp_state) {
    case DEVICE_ON:      userCtrlInfo.lamp_state = DEVICE_OFF; break;
    case DEVICE_OFF:     userCtrlInfo.lamp_state = DEVICE_RELEASE; break;
    case DEVICE_RELEASE: userCtrlInfo.lamp_state = DEVICE_ON; break;
    }
    //LCD_ShowNormalStatus(&userData, &ctrlInfo, currentTempSensor, currentHumiditySensor);
}

//-------------------------------------------------------------------------
void Control_Temperature_Manual(TEMP_CTRL_INFO *ctrl, uint16_t currentTemp) {

	// control lamp
    if(currentTemp < ctrl->lower_bound) {
        ctrl->lamp_state = DEVICE_ON;
    } else if(currentTemp >= ctrl->upper_bound) {
        ctrl->lamp_state = DEVICE_OFF;
    }

    // nochange status fan
}

void Control_Temperature_Auto(TEMP_CTRL_INFO *ctrl, uint16_t currentTemp, int currentHumidity) {

	// control lamp
    if(currentTemp < ctrl->lower_bound) {
        ctrl->lamp_state = DEVICE_ON;
    } else if(currentTemp >= ctrl->upper_bound) {
        ctrl->lamp_state = DEVICE_OFF;
    }

    // control fan
    /*
    if(currentTemp > (ctrl->upper_bound + ctrl->fan_margin)) {
        ctrl->fan_state = DEVICE_ON;
    } else if(currentTemp < ctrl->target_temp) {
        ctrl->fan_state = DEVICE_OFF;
    }

    if(ctrl->fan_state == DEVICE_OFF && currentHumidity < AUTO_MIN_HUMIDITY)
    {
    	ctrl->fan_state = DEVICE_ON;
    }
    */
    // fan is always on
    ctrl->fan_state = DEVICE_ON;

    if (currentHumidity >= 70)
    {
    	ctrl->fan_state = DEVICE_OFF;
    }

    // If in range target_temp … upper_bound+margin → nochange status fan
}
//-------------------------------------------------------------------------
static void RefreshAutoMode()
{
	if (userDataApply.mode == MODE_AUTO)
	{
		if (userDataApply.nday < AUTO_MIN_DAY || userDataApply.nday > AUTO_MAX_DAY)
			userDataApply.nday = AUTO_MIN_DAY;

		for(int i = 0; i < userAutoModeData.rangeCount; i++)
		{
			if (userDataApply.nday >= userAutoModeData.rangeData[i].dayBegin &&
					userDataApply.nday <= userAutoModeData.rangeData[i].dayEnd)
			{
				userDataApply.manual_temperature = userAutoModeData.rangeData[i].temperature;
				userDataApply.manual_fan_speed = userAutoModeData.rangeData[i].fan_speed;
				userDataApply.manual_wait_turning = userAutoModeData.rangeData[i].wait_turning;
				break;
			}
		}
	}
}

void CheckNewDayAndRefreshAutoMode()
{
	static int32_t lastGetTicksInSec = 0;

	if (userDataApply.mode == MODE_AUTO)
	{
		int32_t newTick = HAL_GetTick()/1000;
		int32_t incTicks = 0;

		if (newTick > lastGetTicksInSec)
		{
			lastGetTicksInSec = newTick;

			incTicks = newTick - lastGetTicksInSec;
		}

		lastGetTicksInSec = newTick;

		userAutoModeData.lastTimeInSec = newTick;

		if (incTicks > 0)
		{
			userAutoModeData.totalTimeInSec += incTicks;
		}

		if (userAutoModeData.totalTimeInSec > 86400)
		{
			userAutoModeData.totalTimeInSec = 0;
			userDataApply.nday++;

	        // try write data to flash
	        //...
			if(userDataApply.mode != MODE_DEMO)
			{
				UserInnput_WriteToFlash();
			}

			RefreshAutoMode();
		}
	}
}

void UserInnput_GetCurrentAction( DeviceState *lptemperature,
						DeviceState *lpfan_speed,
						int currentTemperature,
						int currentHumidity,
						DeviceState *lpLastTempState,
						DeviceState *lpLastFanState)
{
	CheckNewDayAndRefreshAutoMode();

	USER_INPUT_DATA *inputData = UserInnput_GetData();
	USER_CTRL_INFO *ctrlInfo = UserInnput_GetCtrlInfo();

	DeviceState fan_speed;
	DeviceState temperature;

	if (ctrlInfo->fan_state == DEVICE_RELEASE || ctrlInfo->lamp_state == DEVICE_RELEASE )
	{
		TEMP_CTRL_INFO tempCtrl;

		// Initialize values
		tempCtrl.target_temp = inputData->manual_temperature; // 37.5°C
		tempCtrl.lower_bound = inputData->manual_temperature>5?(inputData->manual_temperature - 5):(inputData->manual_temperature); // 37.0°C
		tempCtrl.upper_bound = inputData->manual_temperature + 5; // 38.0°C
		tempCtrl.fan_margin = 5;    // 0.5°C hysteresis
		tempCtrl.lamp_state = DEVICE_OFF;
		tempCtrl.fan_state = DEVICE_OFF;

		if (lpLastTempState != NULL && (*lpLastTempState == DEVICE_OFF || *lpLastTempState == DEVICE_ON))
		{
			tempCtrl.lamp_state = *lpLastTempState;
		}

		if (lpLastFanState != NULL && (*lpLastFanState == DEVICE_OFF || *lpLastFanState == DEVICE_ON))
		{
			tempCtrl.fan_state = *lpLastFanState;
		}

		if(inputData->mode == MODE_MANUAL ||
				inputData->mode == MODE_DEMO )
		{
			Control_Temperature_Manual(&tempCtrl, currentTemperature);

			temperature = tempCtrl.lamp_state;
			fan_speed = inputData->manual_fan_speed>0?DEVICE_ON:DEVICE_OFF;
		}else
		{
			// auto mode
			Control_Temperature_Auto(&tempCtrl, currentTemperature, currentHumidity);

			temperature = tempCtrl.lamp_state;
			fan_speed = tempCtrl.fan_state;
		}
	}

	if (ctrlInfo->fan_state != DEVICE_RELEASE)
	{
		if (ctrlInfo->fan_state == DEVICE_ON)
		{
			fan_speed = DEVICE_ON;
		}else
		{
			fan_speed = DEVICE_OFF;
		}
	}

	if (ctrlInfo->lamp_state != DEVICE_RELEASE)
	{
		if (ctrlInfo->lamp_state == DEVICE_ON)
		{
			temperature = DEVICE_ON;
		}else
		{
			temperature = DEVICE_OFF;
		}
	}

	if (lptemperature != NULL)
	{
		*lptemperature = temperature;
	}

	if(lpfan_speed != NULL)
	{
		*lpfan_speed = fan_speed;
	}
}
