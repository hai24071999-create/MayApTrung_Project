#include <stdio.h>
#include <string.h>
#include "mydht11.h"

#include "timer.h"
#include "mylcd16x2.h"

MYDHT11_CONFIG *g_myDHT11Cfg = NULL;
MYDHT11_DATA *g_myDHT11Data = NULL;

void MyHDT11_Init(MYDHT11_CONFIG *cfg, MYDHT11_DATA *data)
{
	g_myDHT11Cfg = cfg;

	g_myDHT11Data = data;

	g_myDHT11Data->lastError = DHT11_NO_DATA;

	g_myDHT11Data->lastTemperature = 0.0;
	g_myDHT11Data->lastHumidity = 0.0;

	g_myDHT11Data->ti = 0;
	g_myDHT11Data->td = 0;

	g_myDHT11Data->ri = 0;
	g_myDHT11Data->rd = 0;
}

void MyHDT11_Delay_ms(uint32_t ms)
{
	timer_delay_ms(ms);
}

void MyHDT11_Delay_us(uint32_t us)
{
	timer_delay_us(us);
}

void MyDHT11_Set_Pin_Output (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

void MyDHT11_Set_Pin_Input (GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};
	GPIO_InitStruct.Pin = GPIO_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOx, &GPIO_InitStruct);
}

/* Cho chan DHT11 dat muc 'state' (0/1), toi da 'timeout_us' micro-giay.
 * Dung TIM1 (1us/tick) lam dong ho. Tra ve 1 neu dat duoc trong thoi gian
 * cho, 0 neu het gio -> tranh vong while treo cung khi sensor mat/dau sai. */
static uint8_t MyDHT11_DHT11_Wait_Level(uint8_t state, uint16_t timeout_us)
{
	__HAL_TIM_SET_COUNTER(g_myDHT11Cfg->tim, 0);

	while (((HAL_GPIO_ReadPin(g_myDHT11Cfg->DHT11_GPIOx, g_myDHT11Cfg->DHT11_GPIO_Pin)) ? 1U : 0U) != state)
	{
		if (__HAL_TIM_GET_COUNTER(g_myDHT11Cfg->tim) > timeout_us){

			return 0;
		}
	}

	return 1;
}

void MyDHT11_DHT11_Start (void)
{
	MyDHT11_Set_Pin_Output (g_myDHT11Cfg->DHT11_GPIOx, g_myDHT11Cfg->DHT11_GPIO_Pin);  // set the pin as output
	HAL_GPIO_WritePin (g_myDHT11Cfg->DHT11_GPIOx, g_myDHT11Cfg->DHT11_GPIO_Pin, 0);   // pull the pin low

	// tin hieu START: keo THAP >=18ms (datasheet 18ms)
	MyHDT11_Delay_ms(20);

	// pull the pin high
	// keo CAO 20-40us roi tha bus
	MyDHT11_Set_Pin_Input(g_myDHT11Cfg->DHT11_GPIOx, g_myDHT11Cfg->DHT11_GPIO_Pin);    // set as input
	MyHDT11_Delay_us(30);
}

uint8_t MyDHT11_DHT11_Check_Response (void)
{
	/* Bat theo SUON (edge) co timeout thay vi lay mau o moc co dinh -> ben vung
	 * voi sai lech timing cua model DHT11 trong Proteus.
	 * Pha dap ung: DHT keo THAP 80us -> CAO 80us -> THAP (bat dau bit dau tien). */
	if (!MyDHT11_DHT11_Wait_Level(0, 1000)) return 1;   // cho suon xuong (80us THAP); khong thay -> No response
	if (!MyDHT11_DHT11_Wait_Level(1, 1000)) return 2;   // cho suon len   (80us CAO)
	if (!MyDHT11_DHT11_Wait_Level(0, 1000)) return 3;   // cho suon xuong (vao bit dau tien)
	return 0;                                  // da bat tay xong
}

/* Do do rong xung (so tick TIM1, 1us/tick) khi chan o muc 'level',
 * toi da 2000us. Tra ve do dai do duoc, hoac 0xFFFF neu timeout. */
static uint16_t MyDHT11_DHT11_Pulse_Len(uint8_t level)
{
	__HAL_TIM_SET_COUNTER(g_myDHT11Cfg->tim, 0);
	while (((HAL_GPIO_ReadPin(g_myDHT11Cfg->DHT11_GPIOx, g_myDHT11Cfg->DHT11_GPIO_Pin)) ? 1U : 0U) == level)
	{
		if (__HAL_TIM_GET_COUNTER(g_myDHT11Cfg->tim) > 2000) return 0xFFFF;   // timeout
	}
	return (uint16_t)__HAL_TIM_GET_COUNTER(g_myDHT11Cfg->tim);
}

uint8_t MyDHT11_DHT11_Read (void)
{
	uint8_t i = 0, j;
	for (j=0;j<8;j++)
	{
		/* Moi bit: ~50us THAP roi xung CAO (26us=0 / 70us=1).
		 * Do CA pha THAP va pha CAO roi SO SANH tuong doi -> khong phu thuoc
		 * vao gia tri tuyet doi cua clock/model (mien nhiem sai lech ty le). */
		uint16_t tLow  = MyDHT11_DHT11_Pulse_Len(0);   // do dai pha THAP dau bit
		uint16_t tHigh = MyDHT11_DHT11_Pulse_Len(1);   // do dai xung CAO

		if (tLow == 0xFFFF || tHigh == 0xFFFF) { g_myDHT11Data->lastError = DHT11_ERROR; return i; }

		if (tHigh > tLow) i |= (uint8_t)(1<<(7-j));   // xung CAO dai hon THAP (~50us) -> bit 1
		else              i &= (uint8_t)~(1<<(7-j));  // ngan hon -> bit 0
	}
	return i;
}

void MyHDT11_read_scan_dht11()
{
	g_myDHT11Data->lastError = DHT11_NO_DATA;

	uint8_t Presence   = 0;

	for(int retry = 0; retry < 2; retry++)
	{
		switch(g_myDHT11Cfg->dhtType)
		{
		case DHT11_TYPE:
			MyDHT11_DHT11_Start();
			break;
		default:
			MyDHT11_DHT11_Start();
			break;
		};

		Presence  = MyDHT11_DHT11_Check_Response();
		if (Presence == 0)
		{
			break;
		}

		MyHDT11_Delay_ms(2000);
	}

	if (Presence > 0)
	{
		g_myDHT11Data->lastError = DHT11_ERROR;
		return;
	}

	uint8_t Rh_byte1   = MyDHT11_DHT11_Read();   // do am - phan nguyen
	uint8_t Rh_byte2   = MyDHT11_DHT11_Read();   // do am - phan thap phan
	uint8_t Temp_byte1 = MyDHT11_DHT11_Read();   // nhiet do - phan nguyen
	uint8_t Temp_byte2 = MyDHT11_DHT11_Read();   // nhiet do - phan thap phan
	uint16_t SUM        = MyDHT11_DHT11_Read();   // byte checksum (truoc day bi bo)

	/* trang thai loi truoc do (1 = loi) -> xoa man 1 lan khi chuyen trang thai */
	static uint8_t fail_cnt = 0;        // dem so lan doc loi LIEN TIEP (debounce)

	/* Chi cap nhat khi checksum dung: (tong 4 byte) & 0xFF == SUM */
	if ((g_myDHT11Data->lastError != DHT11_ERROR) && (((Rh_byte1 + Rh_byte2 + Temp_byte1 + Temp_byte2) & 0xFF) == SUM))
	{
	  fail_cnt = 0;

	  float Temperature = (float) Temp_byte1 + (float) Temp_byte2 / 10.0f;
	  float Humidity    = (float) Rh_byte1   + (float) Rh_byte2   / 10.0f;

	  switch(g_myDHT11Cfg->dhtType)
	  	{
	  	case DHT11_TYPE:
	  		Temperature = (float) Temp_byte1 + (float) Temp_byte2 / 10.0f;
	  		Humidity    = (float) Rh_byte1    + (float) Rh_byte2   / 10.0f;
	  		break;
	  	default:
	  		break;
	  	};

	  g_myDHT11Data->lastTemperature = Temperature;
	  g_myDHT11Data->lastHumidity = Humidity;

	  g_myDHT11Data->ti = (int)Temperature;
	  g_myDHT11Data->td = (int)((Temperature - g_myDHT11Data->ti) * 10.0f + 0.5f);

	  g_myDHT11Data->ri = (int)Humidity;
	  g_myDHT11Data->rd = (int)((Humidity - g_myDHT11Data->ri) * 10.0f + 0.5f);

	  g_myDHT11Data->lastError = DHT11_OK;
	}
	else if (++fail_cnt >= 3)
	{
	  fail_cnt = 3;   // giu o nguong, chong tran bien

	  //if (!Presence)
	  //	"No response  "
	  //else if (g_myDHT11Data->lastError == DHT11_ERROR)
	  //	"Read timeout "
	  //else
	  //	"Checksum fail"
	}else
	{
	}
}
