#include "my_wifi.h"

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>

//---------------------------------------------
char gwifi_ssid[64] = {0};
char gwifi_password[64] = {0};
//---------------------------------------------

void MyWifi_Init(const char* ssid, const char* password)
{
	strcpy(gwifi_ssid, ssid);
	strcpy(gwifi_password, password);
}

bool MyWifi_Connect()
{
  int timeout = 0;
  bool status = true;

  // Connecting to WiFi...
  WiFi.mode(WIFI_STA);
  WiFi.begin(gwifi_ssid, gwifi_password);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  Serial.print("\nWiFi connecting...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    timeout += 500;

    if(timeout > 15000)
    {
      status = false;
      break;
    }

    Serial.print(".");
  }

  if(status) {
    Serial.print("\nWiFi connection success!!");
    //Serial.print("\nLocal IP: ");
    //Serial.print(WiFi.localIP());
  }else
    Serial.print("\nWiFi connect failed!");

  return status;
}

void MyWifi_DisConnect()
{
  WiFi.disconnect(); 
  delay(1000);
}

void MyWifi_Retry_Connect()
{
  if (strlen(gwifi_ssid) > 0 && WiFi.status() != WL_CONNECTED)
  {
    MyWifi_DisConnect();

    MyWifi_Connect();
  }
}
