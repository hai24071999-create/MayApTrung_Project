#ifndef INC_MY_UARTDATA_STM32_ESP32_H_
#define INC_MY_UARTDATA_STM32_ESP32_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#define LABEL_UNKNOWN			"unknown"
#define LABEL_WIFI_STATE  		"wifi_state"
#define LABEL_SEND_CLOUD  		"send_cloud"
#define LABEL_RECEIVE_CLOUD		"recv_cloud"

#define MAX_PACKAGE_LEN 128
#define MAX_DATA_LEN 64

#define WIFI_STATE_READY 	"ready"
#define WIFI_STATE_NOREADY 	"noready"

//---------------------------
typedef struct _WIFI_STATE
{
	// status:
	//	READY or NOREADY
	char status[MAX_DATA_LEN];
}WIFI_STATE;

// Example: wifi state data
//
// 1.wifi ready
//	-> "wifi_state;ready"\n
//
// 2.wifi not ready
//	-> "wifi_state;noready"\n
//---------------------------

//---------------------------
typedef struct _SEND_CLOUD_DATA
{
	//------------------------------
	// real time data

	// example:
	//	temperature: 35.6 C	-> 356
	uint16_t real_temperature;

	// example:
	//	humidity: 80.5 %	-> 805
	uint16_t real_humidity;

	//------------------------------
	// configuration data
	//

	// mode:
	// 0: manual control
	// 1: automatic
	uint8_t cfg_mode;

	//---------------
	// values for manual control mode
	//

	// temperature
	// example: 356 (35.6 C)
	uint16_t cfg_temperature;

	//	fan
	//	0: off
	//	1: on
	uint8_t cfg_fan;

	// waiting time for servo activity (in minutes)
	// example:
	//	cfg_wait_turning: 120	(2 hours)
	uint16_t cfg_wait_turning;
	//---------------

	//---------------
	// values for automatic mode
	//

	// cfg_auto_day: [1-21]
	uint8_t cfg_auto_day;
	//---------------

	// current status
	uint8_t current_temperature_on;
	uint8_t current_fan_on;
	//------------------------------
}SEND_CLOUD_DATA;

// Example: send to cloud data
//
//	"send_cloud;<real_temperature>;<real_humidity>;<cfg_mode>;<cfg_temperature>;<cfg_fan>;<cfg_wait_turning>;<cfg_auto_day>\n"
//
//	-> "send_cloud;350;800;0;37;1;120;1\n"
//
//---------------------------

typedef struct _RECV_CLOUD_DATA
{
	// keyName: "real_temperature" or "real_humidity" or "cfg_mode" ... "time"
	char keyName[16];

	char data[32];
}RECV_CLOUD_DATA;

// Example: receive data from cloud
//
// "recv_cloud;<keyName>;<data>\n"
//
// 1) receiving mode exchange
//->"recv_cloud;cfg_mode;1\n"
//
// 2) receiving configure temperature exchange
//->"recv_cloud;cfg_temperature;367\n"
//---------------------------

//--------------------------------------------
const char* MyUART_TryGet_Data();

void MyUART_Receive_Byte(uint8_t  rec_data);

char *MyUART_GetRecvBuffer(int *lpSize);

void MyUART_SetRecvBuffer_Ready();
//--------------------------------------------

// Function to generate sending strings and parse
// ...
WIFI_STATE* MyUART_Get_WifiState();
const char* MyUART_Convert_WifiState();

SEND_CLOUD_DATA* MyUART_Get_SendCloud();
const char* MyUART_Convert_SendCloud();

RECV_CLOUD_DATA* MyUART_Get_RecvCloud();
const char* MyUART_Convert_RecvCloud();

#ifdef __cplusplus
}
#endif

#endif /* INC_MY_UARTDATA_STM32_ESP32_H_ */
