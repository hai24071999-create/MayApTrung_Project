#ifndef INC_MY_WIFI_H_
#define INC_MY_WIFI_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void MyWifi_Init(const char* ssid, const char* password);
bool MyWifi_Connect();
void MyWifi_DisConnect();
void MyWifi_Retry_Connect();

#ifdef __cplusplus
}
#endif

#endif /* INC_MY_WIFI_H_ */
