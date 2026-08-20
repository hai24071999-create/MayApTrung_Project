#ifndef MYDHT11_H_
#define MYDHT11_H_

#include "stm32f1xx_hal.h"

typedef enum {
	DHT11_OK             = 0U,  // successful
	DHT11_NO_DATA         = 1U,  // No key pressed
	DHT11_ERROR          = 2U,  // General error
} MyDHT11_Error_t;

typedef enum {
	DHT11_TYPE   = 0U,
} MyDHT_Type;

typedef struct _MYDHT11_CONFIG
{
	MyDHT_Type dhtType;

	GPIO_TypeDef *DHT11_GPIOx;
	uint16_t DHT11_GPIO_Pin;

	TIM_HandleTypeDef *tim;
}MYDHT11_CONFIG;

typedef struct _MYDHT11_DATA
{
	//----------
	// Last Error
	MyDHT11_Error_t lastError;
	//----------

	//----------
	float lastTemperature;
	float lastHumidity;
	//----------
	 int ti; // Temperature (phan nguyen)
	 int td; // Temperature (phan thap phan)

	 int ri; //Humidity (phan nguyen)
	 int rd; //Humidity (phan thap phan)

}MYDHT11_DATA;

void MyHDT11_Init(MYDHT11_CONFIG *cfg, MYDHT11_DATA *data);
void MyHDT11_read_scan_dht11();


#endif /* MYDHT11_H_ */
