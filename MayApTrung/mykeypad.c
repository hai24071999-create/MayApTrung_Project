#include "mykeypad.h"

CONFIG_KEYPAD *g_lpKeyPadConfig = NULL;

void keyPad_init(CONFIG_KEYPAD *lpKeyPadConfig, DATA_KEYPAD *lpDataKeyPad);
void keyPad_int_scan(DATA_KEYPAD *dataKeyPad, uint16_t intPin);
uint32_t keyPad_getTick();

void keyPad_init(CONFIG_KEYPAD *lpKeyPadConfig, DATA_KEYPAD *lpDataKeyPad)
{
	g_lpKeyPadConfig = lpKeyPadConfig;

	lpDataKeyPad->lastError = KEY_PAD_NO_KEY;
	lpDataKeyPad->lastIntPin = 0;
	lpDataKeyPad->lastKey = ' ';
	lpDataKeyPad->previousScanMillis = 0;

	for(int i = 0; i < KEY_PAD_COLUMN_COUNT; i++)
	{
		HAL_GPIO_WritePin(g_lpKeyPadConfig->colPins[i].port, g_lpKeyPadConfig->colPins[i].pin, GPIO_PIN_SET);
	}

	for(int i = 0; i < KEY_PAD_ROW_COUNT; i++)
	{
		HAL_GPIO_WritePin(g_lpKeyPadConfig->rowPins[i].port, g_lpKeyPadConfig->rowPins[i].pin, GPIO_PIN_RESET);
	}
}

void keyPad_int_scan(DATA_KEYPAD *dataKeyPad, uint16_t intPin)
{
	if (g_lpKeyPadConfig == NULL || dataKeyPad == NULL)
	{
		dataKeyPad->lastError = KEY_PAD_ERROR;
		return;
	}

	uint32_t currentMillis = keyPad_getTick();
	if (currentMillis - dataKeyPad->previousScanMillis < 200)
	{
		dataKeyPad->lastError = KEY_PAD_NO_KEY;
		return;
	}

	dataKeyPad->previousScanMillis = currentMillis;

	bool foundKey = false;

	dataKeyPad->lastError = KEY_PAD_OK;

	GPIO_InitTypeDef GPIO_InitStructPrivate = {0};

	// configuration all rows to low
	GPIO_InitStructPrivate.Pin = 0;
	for(int i = 0; i < KEY_PAD_ROW_COUNT; i++)
	{
		GPIO_InitStructPrivate.Pin = g_lpKeyPadConfig->rowPins[i].pin;
		GPIO_InitStructPrivate.Mode = GPIO_MODE_INPUT;
		GPIO_InitStructPrivate.Pull = GPIO_NOPULL;
		GPIO_InitStructPrivate.Speed = GPIO_SPEED_FREQ_LOW;
		HAL_GPIO_Init(g_lpKeyPadConfig->rowPins[i].port, &GPIO_InitStructPrivate);
	}
	
	for(int col = 0; col < KEY_PAD_COLUMN_COUNT && !foundKey; col++)
	{
		// set current col and reset others
		for(int i = 0; i < KEY_PAD_COLUMN_COUNT; i++)
		{
			if (i == col)
			{
				HAL_GPIO_WritePin(g_lpKeyPadConfig->colPins[i].port, g_lpKeyPadConfig->colPins[i].pin, GPIO_PIN_SET);
			}else
			{
				HAL_GPIO_WritePin(g_lpKeyPadConfig->colPins[i].port, g_lpKeyPadConfig->colPins[i].pin, GPIO_PIN_RESET);
			}
		}

		// checking row
		//
		for(int row = 0; row < KEY_PAD_ROW_COUNT; row++)
		{
			if (intPin == g_lpKeyPadConfig->rowPins[row].pin && HAL_GPIO_ReadPin(g_lpKeyPadConfig->rowPins[row].port, g_lpKeyPadConfig->rowPins[row].pin))
			{
				dataKeyPad->lastKey = g_lpKeyPadConfig->mapKeys[row][col];
				dataKeyPad->lastError = KEY_PAD_OK;
				foundKey = true;
				break;
			}
		}
	}

	if (!foundKey)
	{
		dataKeyPad->lastError = KEY_PAD_NO_KEY;
	}

	for(int i = 0; i < KEY_PAD_COLUMN_COUNT; i++)
	{
		HAL_GPIO_WritePin(g_lpKeyPadConfig->colPins[i].port, g_lpKeyPadConfig->colPins[i].pin, GPIO_PIN_SET);
	}

	//-----------------------------------------
	//Configure GPIO pins of rows back to EXTI
	GPIO_InitStructPrivate.Pin = 0;
	for(int i = 0; i < KEY_PAD_ROW_COUNT; i++)
	{
		GPIO_InitStructPrivate.Pin = g_lpKeyPadConfig->rowPins[i].pin;
		GPIO_InitStructPrivate.Mode = GPIO_MODE_IT_RISING;
		GPIO_InitStructPrivate.Pull = GPIO_PULLDOWN;
		HAL_GPIO_Init(g_lpKeyPadConfig->rowPins[i].port, &GPIO_InitStructPrivate);
	}
}


uint32_t keyPad_getTick()
{
	return HAL_GetTick();
}
