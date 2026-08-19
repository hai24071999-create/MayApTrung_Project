/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "timer.h"
#include "mydht11.h"
#include "fan_control.h"

#include "temperature_controller.h"
#include "fan_controller.h"
#include "my_lcd_i2c.h"
#include "mykeypad.h"

#include "user_input.h"
#include "servo.h"

#include "my_flash.h"
#include "my_uartdata_stm32_esp32.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

#define MAX_TIME_SEND_CLOUND_MS	15000
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

//------------------------------------
// port and pin for DHT11
#define DHT11_PORT GPIOC
#define DHT11_PIN GPIO_PIN_14

MYDHT11_CONFIG cfgDHT11 = {};
MYDHT11_DATA dataDHT11 = {};
//------------------------------------

//------------------------------------
//Data Key pad

DATA_KEYPAD dataKeyPad = {};

char DetectNewUserInput();
//------------------------------------

//------------------------------------
// Button manual control fan and temperature (lamp)
#define BUTTON_FAN_GPIO_Port GPIOA
#define BUTTON_FAN_Pin GPIO_PIN_2

bool isMayBeClickOnFanBtn = false;

#define BUTTON_TEMP_GPIO_Port GPIOA
#define BUTTON_TEMP_Pin GPIO_PIN_5

bool isMayBeClickOnTempBtn = false;
//------------------------------------

// checking the servo is running
static bool isRunningServo = false;
//------------------------------------

//------------------------------------
// sending and receiving cloud variables
uint8_t  g_uart_recv_data;
bool isWifiAvailable =false;

bool g_last_mode_manual = false;
bool g_last_mode_manual_clamp_on = false;
bool g_last_mode_manual_fan_on = false;
//------------------------------------

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

//------------------------------------
// Function to get information
int GetCurrentTemperature();
int GetCurrentHumidity();
//------------------------------------

//------------------------------------
// Main processing functions
TEMPERATURE_CONFIG temperCfg = {true, GPIOC, GPIO_PIN_15};
FAN_CONFIG fanCfg = {true, GPIOA, GPIO_PIN_1};

void Process_Fan();

void Process_Temperature();

void Process_Motor();

void Process_Send_Recv_DataCloud();

void SendToCloud(const uint8_t *pSendData, uint16_t dataLen);
//------------------------------------

//------------------------------------
// Function showing status on LCD
void LCD_ShowNormalStatus(USER_INPUT_DATA *data, USER_CTRL_INFO *ctrl, int ti, int td, int ri, int rd);
void ShowNormalStatus_All();
//------------------------------------

// Blink led for debugging
//
void Blink_Led(int count)
{
	for(int i = 0; i < count; i++)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
		HAL_Delay(500);
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
		HAL_Delay(500);
	}
}

int32_t Get_Random_Range(int32_t min, int32_t max) {
	return min + (rand() % (max - min + 1));
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_Base_Start(&htim2);

  //---------------------------------------------
  // Initialize timer
	if (timer_init(TIM2) != TIMER_OK)
	{
		Error_Handler();
	}

	//---------------------------------------------
	// Initialize LCD I2C
	//

	// Checking common addresses 0x40 (Proteus) và 0x4E (Real)
	uint8_t target_addresses[] = {0x4E, 0x40};
	uint8_t LCD_Actual_Addr = 0;
	for (int i = 0; i < 2; i++) {
		if (HAL_I2C_IsDeviceReady(&hi2c1, target_addresses[i], 3, 500) == HAL_OK) {
			LCD_Actual_Addr = target_addresses[i];
			break;
		}
	}

	CLCD_I2C_CONFIG lcd_cfg = {
			&hi2c1,
			LCD_Actual_Addr,
			16,
			2
	};

	My_CLCD_I2C_Init(&lcd_cfg);

	My_CLCD_I2C_SetCursor(0, 0);

	My_CLCD_I2C_WriteString("Starting.....");

	//---------------------------------------------
	// Initialize DHT11
	//

	HAL_Delay(1000);

	cfgDHT11.dhtType = DHT11_TYPE;
	cfgDHT11.DHT11_GPIOx = DHT11_PORT;
	cfgDHT11.DHT11_GPIO_Pin = DHT11_PIN;
	cfgDHT11.tim = &htim2;

	MyHDT11_Init(&cfgDHT11, &dataDHT11);
	//-------------------------------

	//---------------------------------------------
	// Starting timer PWM
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	//-------------------------------

	//---------------------------------------------
	Temperature_Init(&temperCfg);
	//---------------------------------------------

	//---------------------------------------------
	Fan_Initialize(&fanCfg);
	//---------------------------------------------

	//---------------------------------------------
	// Keypad 4x4
	CONFIG_KEYPAD keyPadConfig = {
		  {
				  {GPIOB, GPIO_PIN_0},
				  {GPIOB, GPIO_PIN_1},
				  {GPIOB, GPIO_PIN_3},
				  {GPIOB, GPIO_PIN_4}
		  },
		  {
				  {GPIOB, GPIO_PIN_12},
				  {GPIOB, GPIO_PIN_13},
				  {GPIOB, GPIO_PIN_14},
				  {GPIOB, GPIO_PIN_15}
		  },
		 {
			   {'1', '2', '3', 'A'},
			   {'4', '5', '6', 'B'},
			   {'7', '8', '9', 'C'},
			   {'*', '0', '#', 'D'},
		  },
	};

	keyPad_init(&keyPadConfig, &dataKeyPad);

	HAL_Delay(1000);

	dataKeyPad.lastIntPin = 0;

	//---------------------------------------------
	// Servo initialize
	//
	SERVO_Init();

	HAL_Delay(2000);
	//---------------------------------------------

	//---------------------------------------------
	// initialize button
	//
	isMayBeClickOnFanBtn = false;
	HAL_GPIO_WritePin(BUTTON_FAN_GPIO_Port, BUTTON_FAN_Pin, GPIO_PIN_SET);

	isMayBeClickOnTempBtn = false;
	HAL_GPIO_WritePin(BUTTON_TEMP_GPIO_Port, BUTTON_TEMP_Pin, GPIO_PIN_SET);
	//---------------------------------------------

	// Initialize user input controller
	//
	UserInnput_Init(BUTTON_FAN_GPIO_Port, BUTTON_FAN_Pin, BUTTON_TEMP_GPIO_Port, BUTTON_TEMP_Pin);
	//---------------------------------------------

	//---------------------------------------------
	// show status first time
	ShowNormalStatus_All();
	//---------------------------------------------

	// Initialize receiving UART data
	HAL_UART_Receive_IT(&huart1, &g_uart_recv_data, 1);

	//notify setup done
	Blink_Led(2);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  char cNewInput = 0;

  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	// main processing
	Process_Fan();

	Process_Temperature();

	Process_Motor();

	// handle user input and show status
	if(isMayBeClickOnFanBtn)
	{
		if(IsBtnFanClick())
		{
			Handle_Btn_Fan_Click();
		}

		isMayBeClickOnFanBtn = false;
	}else if(isMayBeClickOnTempBtn)
	{
		if(IsBtnTemperatureClick())
		{
			Handle_Btn_Temperature_Click();
		}

		isMayBeClickOnTempBtn = false;
	}else
	{
		cNewInput = DetectNewUserInput();
		if (cNewInput > 0)
		{
			Handle_KeyPad_Input(cNewInput);
		}else
		{
			if(UserInnput_GetCurrentState() == STATE_IDLE)
			{
				// Print DHT 11: Temperature, Humidity, ...
				//
				ShowNormalStatus_All();
			}
		}
	}

	Process_Send_Recv_DataCloud();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 72-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 20000-1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 9600;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_4, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_2|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PC13 PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA3 PA4 PA6
                           PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_6
                          |GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PA2 PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB1 PB3 PB4 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : PB2 PB12 PB13 PB14
                           PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI2_IRQn);

  HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI3_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI9_5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int dht11_fGetData = 1000;
uint32_t last_dht = 0;
int lastPrintErrDHT = 0;

char lastShowLCDLine1[32] = {0};
char lastShowLCDLine2[32] = {0};

void LCD_ShowNormalStatus(USER_INPUT_DATA *data, USER_CTRL_INFO *ctrl, int ti, int td, int ri, int rd) {
    char line1[32];
    char line2[32];

	if(ctrl->fan_state != DEVICE_RELEASE || ctrl->lamp_state != DEVICE_RELEASE) {
		snprintf(line1, sizeof(line1), "Fan:%s Lamp:%s",
				 (ctrl->fan_state == DEVICE_ON) ? "On" :
				 (ctrl->fan_state == DEVICE_OFF) ? "Off" : "Rel",
				 (ctrl->lamp_state == DEVICE_ON) ? "On" :
				 (ctrl->lamp_state == DEVICE_OFF) ? "Off" : "Rel");
	 } else {
		switch(data->mode) {
		case MODE_MANUAL:
			// Example: "C T35C F1 W24"
			snprintf(line1, sizeof(line1), "C T%d.%dC F%d W%d",
					 data->manual_temperature/10,
					 data->manual_temperature % 10,
					 data->manual_fan_speed,
					 MinuteToHour(data->manual_wait_turning));
			break;

		case MODE_AUTO:
			// Example: "D20 T35C F1 W2"
			snprintf(line1, sizeof(line1), "D%02d T%d.%dC F%d W%d",
					 data->nday,
					 data->manual_temperature/10,
					 data->manual_temperature % 10,
					 data->manual_fan_speed,
					 MinuteToHour(data->manual_wait_turning));
			break;

		case MODE_DEMO:
			//snprintf(line1, sizeof(line1), "Demo T35C F1 W1");
			uint8_t h, m;
			MinuteToHourMinute(data->manual_wait_turning, &h, &m);
			snprintf(line1, sizeof(line1), "D T%d.%dC F%d W%dh%dm",
					data->manual_temperature / 10,
					data->manual_temperature % 10,
					data->manual_fan_speed,
			         h, m);
			break;

		default:
			snprintf(line1, sizeof(line1), "Unknown Mode");
			break;
		}
    }

    // line 2: real temperature and humidity
    // Example: "T:32.5C H:70%"

    snprintf(line2, sizeof(line2), "T:%d.%dC H:%d.%d%%", ti, td, ri, rd);

    if(strcmp(lastShowLCDLine1, (char*)line1) != 0 ||
    		strcmp(lastShowLCDLine2, (char*)line2) != 0 )
    {
    	strcpy(lastShowLCDLine1, (char*)line1);
    	strcpy(lastShowLCDLine2, (char*)line2);

    	// Show on the LCD
		My_CLCD_I2C_Clear();
		My_CLCD_I2C_SetCursor(0,0);
		My_CLCD_I2C_WriteString((char*)line1);
		My_CLCD_I2C_SetCursor(1,0);
		My_CLCD_I2C_WriteString((char*)line2);
    }
}

void ShowNormalStatus_All()
{
	char str[64];
	uint32_t now = HAL_GetTick();
	bool hasError = false;

	// when servo turning
	//	- temporarily stop scan DHT11
	//  - allow user input
	//
	if (!isRunningServo)
	{
		/* ---- DHT11: doc + cap nhat LCD moi 1000ms (>= chu ky lay mau toi thieu) ---- */
		if (now - last_dht >= dht11_fGetData)
		{
			MyHDT11_read_scan_dht11();

			if (dataDHT11.lastError != DHT11_NO_DATA)
			{
			  if (dataDHT11.lastError == DHT11_OK)
			  {
				  if (lastPrintErrDHT > 0)
				  {
					  lastPrintErrDHT = 0;
					  My_CLCD_I2C_Clear();
				  }
			  }else
			  {
				  if (lastPrintErrDHT == 0)
				  {
					  lastPrintErrDHT = 1;

					  My_CLCD_I2C_Clear();
				  }

				  sprintf (str, "e: %d", dataDHT11.lastError);

				  hasError = true;
			  }
			}

			last_dht = HAL_GetTick();
		}
	}

	if (!hasError)
	{
		USER_INPUT_DATA *inputData = UserInnput_GetData();
		USER_CTRL_INFO *ctrlInfo = UserInnput_GetCtrlInfo();

		LCD_ShowNormalStatus(inputData, ctrlInfo, dataDHT11.ti, dataDHT11.td, dataDHT11.ri, dataDHT11.rd);
	}else
	{
		My_CLCD_I2C_Clear();
		My_CLCD_I2C_SetCursor(0, 0);
		My_CLCD_I2C_WriteString(str);
	}
}

char DetectNewUserInput()
{
	char ret = 0;

	if (dataKeyPad.lastIntPin > 0) {

		keyPad_int_scan(&dataKeyPad, dataKeyPad.lastIntPin);
		dataKeyPad.lastIntPin = 0;

		if (dataKeyPad.lastError == KEY_PAD_OK)
		{
			ret = dataKeyPad.lastKey;
		}
	}

	return ret;
}

int GetCurrentTemperature()
{
	return dataDHT11.ti*10 + dataDHT11.td;
}

int GetCurrentHumidity()
{
	return dataDHT11.ri;
}

void Process_Fan()
{
	static int lastActionMS_Fan = 0;

	static DeviceState last_fan_speed_device = DEVICE_RELEASE;

	if (HAL_GetTick() - lastActionMS_Fan > 1000)
	{
		lastActionMS_Fan = HAL_GetTick();

		// Testing RELAY: ON/OFF FAN DC...
		//

		DeviceState fan_speed_device;

		int currentTemperature = GetCurrentTemperature();
		int currentHumidity = GetCurrentHumidity();

		UserInnput_GetCurrentAction(NULL, &fan_speed_device, currentTemperature, currentHumidity, NULL, &last_fan_speed_device);

		if(last_fan_speed_device == DEVICE_RELEASE ||
				last_fan_speed_device != fan_speed_device)
		{
			last_fan_speed_device = fan_speed_device;

			if(last_fan_speed_device == DEVICE_ON)
			{
				Fan_On();
			}else
			{
				Fan_Off();
			}
		}
	}
}

void Process_Temperature()
{
	static int lastActionMS_Temp = 0;

	static DeviceState last_temperature_device = DEVICE_RELEASE;

	// Testing RELAY: ON/OFF Temperature...
	if (HAL_GetTick() - lastActionMS_Temp > 5000)
	{
		lastActionMS_Temp = HAL_GetTick();

		DeviceState temp_device;

		int currentTemperature = GetCurrentTemperature();
		int currentHumidity = GetCurrentHumidity();

		UserInnput_GetCurrentAction(&temp_device, NULL, currentTemperature, currentHumidity, &last_temperature_device, NULL);

		if(last_temperature_device == DEVICE_RELEASE ||
				last_temperature_device != temp_device)
		{
			last_temperature_device = temp_device;

			if(last_temperature_device == DEVICE_ON)
			{
				Temperature_On();
			}else
			{
				Temperature_Off();
			}
		}
	}
}

void Process_Motor()
{
	static int lastActionMS_Motor = 0;
	static uint16_t angle = 0;
	static bool isTurningRight = false;
	static uint32_t lastTurnTick = 0;
	const uint16_t waitingAfterTurnning = 56;

	uint32_t currentTick = HAL_GetTick();
	USER_INPUT_DATA *inputData = UserInnput_GetData();

	uint32_t waitMs = inputData->manual_wait_turning*60*1000;

	if (inputData->mode == MODE_DEMO)
	{
		waitMs = 10000;
	}

	//if (!isRunningServo && (currentTick - lastActionMS_Motor > 10000))//inputData->manual_wait_turning*60*1000)
	if (!isRunningServo && (currentTick - lastActionMS_Motor > waitMs))
	{
		isRunningServo = true;
		angle = 0;
		isTurningRight = true;
		lastTurnTick = 0;
	}else if(isRunningServo)
	{
		if (currentTick - lastTurnTick > waitingAfterTurnning)
		{
			if (isTurningRight)
			{
				// turn right

				SERVO_SetAngle(angle);
				angle++;
				if(angle > 90)
				{
					isTurningRight = false;
					angle = 90;
				}
			}else // turn left
			{
				SERVO_SetAngle(angle);
				angle--;
				if (angle == 0)
				{
					isRunningServo = false;
				}
			}

			lastTurnTick = currentTick;
		}

		if (!isRunningServo)
		{
			lastActionMS_Motor = HAL_GetTick();
		}
	}
	//------------------------------------------
}

void SendToCloud(const uint8_t *pSendData, uint16_t dataLen)
{
	HAL_UART_Transmit(&huart1, pSendData, dataLen, 1000);
}

void Process_Send_DataCloud()
{
	static int lastActionMS_SendCloud = MAX_TIME_SEND_CLOUND_MS;

	if (isWifiAvailable && (HAL_GetTick() - lastActionMS_SendCloud > MAX_TIME_SEND_CLOUND_MS))
	{
		lastActionMS_SendCloud = HAL_GetTick();

		SEND_CLOUD_DATA* lpSendData = MyUART_Get_SendCloud();

		lpSendData->real_temperature = GetCurrentTemperature();
		lpSendData->real_humidity = GetCurrentHumidity();

		USER_INPUT_DATA *inputData = UserInnput_GetData();

		lpSendData->cfg_mode = inputData->mode;
		lpSendData->cfg_temperature = inputData->manual_temperature;
		lpSendData->cfg_fan = inputData->manual_fan_speed;
		lpSendData->cfg_wait_turning = inputData->manual_wait_turning;
		lpSendData->cfg_auto_day = inputData->nday;

		lpSendData->current_temperature_on = IsTemperature_On()?1:0;
		lpSendData->current_fan_on = IsFan_On()?1:0;

		//for test
		//lpSendData->real_temperature = Get_Random_Range(260, 370);
		//lpSendData->real_humidity = Get_Random_Range(40, 70);
		//lpSendData->cfg_fan = Get_Random_Range(0, 1);

		// Sending data to cloud by UART to ESP32
		const char* pData = MyUART_Convert_SendCloud();

		SendToCloud((const uint8_t*)pData, strlen((char *)pData));
	}
}

void Process_Recv_DataCloud()
{
	const char *dataLabel = MyUART_TryGet_Data();

	if (strcmp(dataLabel, LABEL_WIFI_STATE) == 0)
	{
		//test
		Blink_Led(1);

		WIFI_STATE* lpWifiState = MyUART_Get_WifiState();
		if (strcmp(lpWifiState->status, WIFI_STATE_READY) == 0)
		{
			isWifiAvailable = true;
		}else
		{
			isWifiAvailable = false;
		}
	}else if (strcmp(dataLabel, LABEL_RECEIVE_CLOUD) == 0)
	{
		USER_CTRL_INFO *ctrlInfo = UserInnput_GetCtrlInfo();

		RECV_CLOUD_DATA* lpRecvCloudData = MyUART_Get_RecvCloud();

		if (strcmp(lpRecvCloudData->keyName, "mode_manual") == 0)
		{
			bool mode_manual = atoi(lpRecvCloudData->data)>0;
			if (!mode_manual)
			{
				ctrlInfo->lamp_state = DEVICE_RELEASE;
				ctrlInfo->fan_state = DEVICE_RELEASE;
			}else
			{
				ctrlInfo->lamp_state = g_last_mode_manual_clamp_on?DEVICE_ON:DEVICE_OFF;
				ctrlInfo->fan_state = g_last_mode_manual_fan_on?DEVICE_ON:DEVICE_OFF;
			}

			g_last_mode_manual = mode_manual;
		}else if (strcmp(lpRecvCloudData->keyName, "tempc") == 0)
		{
			bool clamp_on = atoi(lpRecvCloudData->data)>0;

			ctrlInfo->lamp_state = clamp_on?DEVICE_ON:DEVICE_OFF;

			g_last_mode_manual_clamp_on = clamp_on;
		}else if (strcmp(lpRecvCloudData->keyName, "fan_speed") == 0)
		{
			bool fan_on = atoi(lpRecvCloudData->data)>0;

			ctrlInfo->fan_state = fan_on?DEVICE_ON:DEVICE_OFF;

			g_last_mode_manual_fan_on = fan_on;
		}

		/*
		RECV_CLOUD_DATA* lpRecvCloudData = MyUART_Get_RecvCloud();

		USER_INPUT_DATA *inputData = UserInnput_GetData();

		if (strcmp(lpRecvCloudData->keyName, "cfg_mode") == 0)
		{
			inputData->mode = atoi(lpRecvCloudData->data);
		}else if (strcmp(lpRecvCloudData->keyName, "cfg_temperature") == 0)
		{
			inputData->manual_temperature = atoi(lpRecvCloudData->data);
		}else if (strcmp(lpRecvCloudData->keyName, "cfg_fan") == 0)
		{
			inputData->manual_fan_speed = atoi(lpRecvCloudData->data);
		}else if (strcmp(lpRecvCloudData->keyName, "cfg_wait_turning") == 0)
		{
			inputData->manual_wait_turning = atoi(lpRecvCloudData->data);
		}else if (strcmp(lpRecvCloudData->keyName, "cfg_auto_day") == 0)
		{
			inputData->nday = atoi(lpRecvCloudData->data);
		}else if (strcmp(lpRecvCloudData->keyName, "cloud_time") == 0)
		{
			int cloudTime = atoi(lpRecvCloudData->data);
		}
*/
		//test
		// storing data ...
	}

}

void Process_Send_Recv_DataCloud()
{
	// processing data
	//...
	Process_Send_DataCloud();

	// processing receiving data
	//...
	Process_Recv_DataCloud();
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == BUTTON_FAN_Pin)
	{
		isMayBeClickOnFanBtn = true;
	}else if(GPIO_Pin == BUTTON_TEMP_Pin)
	{
		isMayBeClickOnTempBtn = true;
	}else
	{
		dataKeyPad.lastIntPin = GPIO_Pin;
	}
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART1)
	{
		MyUART_Receive_Byte(g_uart_recv_data);

		// Call function to get next byte
		HAL_UART_Receive_IT(&huart1, &g_uart_recv_data, 1);
	}
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
