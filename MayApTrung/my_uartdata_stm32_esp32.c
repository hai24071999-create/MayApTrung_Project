#include "my_uartdata_stm32_esp32.h"
#include <string.h>
#include <stdlib.h>

//------------------------------------
// global buffer to send and get data
char g_sendBuffer[MAX_PACKAGE_LEN] = {0};

char  g_recvBuffer[MAX_PACKAGE_LEN] = {'\0'};
uint16_t g_recIdx = 0;

uint8_t  g_has_recv_cmd = 0;
//------------------------------------
WIFI_STATE g_wifiState = {""};
SEND_CLOUD_DATA g_sendCloudData;
RECV_CLOUD_DATA g_recvCloudData;
//------------------------------------

char *MyUART_GetRecvBuffer(int *lpSize)
{
	if(lpSize != NULL)
		*lpSize = MAX_PACKAGE_LEN;

	return g_recvBuffer;
}

void MyUART_SetRecvBuffer_Ready()
{
	g_has_recv_cmd = 1;
}

void MyUART_Receive_Byte(uint8_t  rec_data)
{
	if (rec_data == '\n')
	{
		g_recvBuffer[g_recIdx] = '\0';

		// reset buffer index
		g_recIdx = 0;

		// receive buffer is ready to parse
		g_has_recv_cmd = 1;
	}else if(g_recIdx < MAX_PACKAGE_LEN - 1)
	{
		g_recvBuffer[g_recIdx] = rec_data;
		g_recIdx++;
	}else // full buffer
	{
		// reset buffer index
		g_recIdx = 0;
	}
}

const char* MyUART_Convert_WifiState()
{
	// generate a command string in format: <LABEL_WIFI_STATE>;<status>\n
	//
	sprintf((char *)g_sendBuffer, "%s;%s\n", LABEL_WIFI_STATE, g_wifiState.status);

	return g_sendBuffer;
}

WIFI_STATE* MyUART_Get_WifiState()
{
	return &g_wifiState;
}

SEND_CLOUD_DATA* MyUART_Get_SendCloud()
{
	return &g_sendCloudData;
}

const char* MyUART_Convert_SendCloud()
{
	// generate a command string in format:
	//	"<LABEL_SEND_CLOUD>;<real_temperature>;<real_humidity>;<cfg_mode>;<cfg_temperature>;<cfg_fan>;<cfg_wait_turning>;<cfg_auto_day>;<current_temperature_on>;<current_fan_on>\n"
	sprintf((char *)g_sendBuffer, "%s;%d;%d;%d;%d;%d;%d;%d;%d;%d\n", LABEL_SEND_CLOUD,
			g_sendCloudData.real_temperature,
			g_sendCloudData.real_humidity,
			g_sendCloudData.cfg_mode,
			g_sendCloudData.cfg_temperature,
			g_sendCloudData.cfg_fan,
			g_sendCloudData.cfg_wait_turning,
			g_sendCloudData.cfg_auto_day,
			g_sendCloudData.current_temperature_on,
			g_sendCloudData.current_fan_on);

	return g_sendBuffer;
}

RECV_CLOUD_DATA* MyUART_Get_RecvCloud()
{
	return &g_recvCloudData;
}

const char* MyUART_Convert_RecvCloud()
{
	// generate a command string in format: "recv_cloud;<keyName>;<data>\n"\n
	//
	sprintf((char *)g_sendBuffer, "%s;%s;%s\n", LABEL_RECEIVE_CLOUD, g_recvCloudData.keyName, g_recvCloudData.data);

	return g_sendBuffer;
}


const char* MyUART_Parse(char *sbuff)
{
	const char* label = LABEL_UNKNOWN;

	if(sbuff == NULL)
		return label;

	const char *split = ";";
	const char *findLabel;
	char *token = strtok((char*)sbuff, split);

	if( token != NULL ) {

		//printf("Chuoi con: %s\n", token);
		findLabel = token;
		if (strcmp(findLabel, LABEL_WIFI_STATE) == 0)
		{
			// Example: wifi state data
			//
			// 1.wifi ready
			//	-> "wifi_state;ready"\n
			token = strtok(NULL, split);
			if (token != NULL) {
				strcpy(g_wifiState.status, token);
				label = findLabel;
			}
		}else if (strcmp(findLabel, LABEL_SEND_CLOUD) == 0)
		{
			//exmaple:
			//	-> "send_cloud;350;800;0;37;1;120;1\n"
			//

			token = strtok(NULL, split);
			if (token != NULL) {
				g_sendCloudData.real_temperature = atoi(token);

				token = strtok(NULL, split);
				if (token != NULL) {
					g_sendCloudData.real_humidity = atoi(token);

					token = strtok(NULL, split);
					if (token != NULL) {
						g_sendCloudData.cfg_mode = atoi(token);

						token = strtok(NULL, split);
						if (token != NULL) {
							g_sendCloudData.cfg_temperature = atoi(token);

							token = strtok(NULL, split);
							if (token != NULL) {
								g_sendCloudData.cfg_fan = atoi(token);

								token = strtok(NULL, split);
								if (token != NULL) {
									g_sendCloudData.cfg_wait_turning = atoi(token);

									token = strtok(NULL, split);
									if (token != NULL) {
										g_sendCloudData.cfg_auto_day = atoi(token);

										token = strtok(NULL, split);
										if (token != NULL) {
											g_sendCloudData.current_temperature_on = atoi(token);

											token = strtok(NULL, split);
											if (token != NULL) {
												g_sendCloudData.current_fan_on = atoi(token);

												label = findLabel;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}else if (strcmp(findLabel, LABEL_RECEIVE_CLOUD) == 0)
		{
			// "recv_cloud;<keyName>;<data>\n"
			//
			// 1) receiving mode exchange
			//->"recv_cloud;cfg_mode;1\n"

			token = strtok(NULL, split);
			if (token != NULL) {
				strcpy(g_recvCloudData.keyName, token);
				token = strtok(NULL, split);
				if (token != NULL) {
					strcpy(g_recvCloudData.data, token);
					label = findLabel;
				}
			}
		}else
		{
			// not found valid label
			//..
		}
	}

	return label;
}

const char* MyUART_TryGet_Data()
{
	const char* label = LABEL_UNKNOWN;

	if (g_has_recv_cmd > 0)
	{
	  g_has_recv_cmd = 0;

	  label = MyUART_Parse(g_recvBuffer);
	}

	return label;
}

