#ifndef SRC_FAN_CONTROLLER_H_
#define SRC_FAN_CONTROLLER_H_

#include "stm32f1xx_hal.h"
#include <stdbool.h>

typedef struct _FAN_CONFIG
{
	// The relay port and pin to control temperature
	// Active: Low Level
	//
	bool isActiveLowLevel;

	GPIO_TypeDef *Temper_GPIOx;
	uint16_t Temper_GPIO_Pin;

}FAN_CONFIG;

void Fan_Initialize(FAN_CONFIG *cfg);

void Fan_On();
void Fan_Off();
bool IsFan_On();

#endif /* SRC_FAN_CONTROLLER_H_ */
