#ifndef INC_TEMPERATURE_CONTROLLER_H_
#define INC_TEMPERATURE_CONTROLLER_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef struct _TEMPERATURE_CONFIG
{
	// The relay port and pin to control temperature
	// Active: Low Level
	//
	bool isActiveLowLevel;

	GPIO_TypeDef *Temper_GPIOx;
	uint16_t Temper_GPIO_Pin;

}TEMPERATURE_CONFIG;

void Temperature_Init(TEMPERATURE_CONFIG *cfg);

void Temperature_On();
void Temperature_Off();
bool IsTemperature_On();

#endif /* INC_TEMPERATURE_CONTROLLER_H_ */
