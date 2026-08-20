#include "fan_controller.h"

FAN_CONFIG *g_fanCfg = NULL;

bool g_isFanOn = false;

void Fan_Initialize(FAN_CONFIG *cfg)
{
	g_fanCfg = cfg;

	// temporarily turn off fan
	HAL_GPIO_WritePin(g_fanCfg->Temper_GPIOx, g_fanCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
}

void Fan_On()
{
	if (g_fanCfg != NULL)
	{
		if (g_fanCfg->isActiveLowLevel)
		{
			HAL_GPIO_WritePin(g_fanCfg->Temper_GPIOx, g_fanCfg->Temper_GPIO_Pin, GPIO_PIN_RESET);
		}else
		{
			HAL_GPIO_WritePin(g_fanCfg->Temper_GPIOx, g_fanCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
		}
	}

	g_isFanOn = true;
}

void Fan_Off()
{
	if (g_fanCfg != NULL)
	{
		if (g_fanCfg->isActiveLowLevel)
		{
			HAL_GPIO_WritePin(g_fanCfg->Temper_GPIOx, g_fanCfg->Temper_GPIO_Pin, GPIO_PIN_SET);
		}else
		{
			HAL_GPIO_WritePin(g_fanCfg->Temper_GPIOx, g_fanCfg->Temper_GPIO_Pin, GPIO_PIN_RESET);
		}
	}

	g_isFanOn = false;
}

bool IsFan_On()
{
	return g_isFanOn;
}

