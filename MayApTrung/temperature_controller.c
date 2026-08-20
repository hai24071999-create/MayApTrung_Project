#include "temperature_controller.h"

TEMPERATURE_CONFIG *g_temperatureCfg = NULL;
bool g_isTemperatureOn = false;

void Temperature_Init(TEMPERATURE_CONFIG *cfg)
{
	g_temperatureCfg = cfg;

	// temporarily turn off temperature
	HAL_GPIO_WritePin(g_temperatureCfg->Temper_GPIOx, g_temperatureCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
}

void Temperature_On()
{
	if (g_temperatureCfg != NULL)
	{
		if (g_temperatureCfg->isActiveLowLevel)
		{
			HAL_GPIO_WritePin(g_temperatureCfg->Temper_GPIOx, g_temperatureCfg->Temper_GPIO_Pin, GPIO_PIN_RESET);
		}else
		{
			HAL_GPIO_WritePin(g_temperatureCfg->Temper_GPIOx, g_temperatureCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
		}
	}

	g_isTemperatureOn = true;
}

void Temperature_Off()
{
	if (g_temperatureCfg != NULL)
	{
		if (g_temperatureCfg->isActiveLowLevel)
		{
			HAL_GPIO_WritePin(g_temperatureCfg->Temper_GPIOx, g_temperatureCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
		}else
		{
			HAL_GPIO_WritePin(g_temperatureCfg->Temper_GPIOx, g_temperatureCfg->Temper_GPIO_Pin, GPIO_PIN_RESET);
		}
	}

	g_isTemperatureOn = false;
}

bool IsTemperature_On()
{
	return g_isTemperatureOn;
}


