#ifndef INC_MY_THINGSBOARD_H_
#define INC_MY_THINGSBOARD_H_

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Attribute_Request.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>

#define TELEMETRY_NAME_TEMPERATURE "temperature"
#define TELEMETRY_NAME_HUMIDITY "humidity"
#define TELEMETRY_NAME_FAN "fan"

// Maximum amount of attributs we can request or subscribe, has to be set both in the ThingsBoard template list and Attribute_Request_Callback template list
// and should be the same as the amount of variables in the passed array. If it is less not all variables will be requested or subscribed
#define  MAX_THINGSBOARD_SHARED_ATTRIBUTES (3)

#define THINGSBOARD_SHARED_ATTRIBUTE_MODE         "mode_manual"
#define THINGSBOARD_SHARED_ATTRIBUTE_TEMPERATURE  "tempc"
#define THINGSBOARD_SHARED_ATTRIBUTE_FAN          "fan_speed"

typedef struct _THINGSBOARD_CONFIG
{
  // the thingsboard server address/domain
  char serverAddress[64];
  uint16_t serverMQTTPort;
  char deviceToken[64];

  bool isPrintDebug;

  // Sending telemetry data

  // waiting interval for sending telemetry data
  int intervalSendTelemetryDataMs;
  int intervalSendSTM32DataMs;

  // Shared Attributes
  char registerEventSharedAttributes[MAX_THINGSBOARD_SHARED_ATTRIBUTES][32];
  
  void (*callbackRequestAttributes)(JsonObjectConst const &);
  void (*callbackSharedAttributes)(JsonObjectConst const &);
}THINGSBOARD_CONFIG;

void MyThingsBoard_Init(THINGSBOARD_CONFIG *lpCfg);
bool MyThingsBoard_IsConnected();
void MyThingsBoard_SetTelemetriesToCloud(int real_temperature, int real_humidity, int cfg_fan);
void MyThingsBoard_SetAttributeToSTM32_BOOL(const char *name, bool val);
void MyThingsBoard_LoopProcess();

#endif /* INC_MY_THINGSBOARD_H_ */