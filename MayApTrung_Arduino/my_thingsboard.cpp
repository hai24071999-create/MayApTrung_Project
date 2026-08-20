#include "my_thingsboard.h"
//#include <Server_Side_RPC.h>

THINGSBOARD_CONFIG *gThingsBoardCfg = NULL;

// Maximum size packets will ever be sent or received by the underlying MQTT client,
// if the size is to small messages might not be sent or received messages will be discarded
constexpr uint16_t MAX_THINGSBOARD_MESSAGE_SEND_SIZE = 128U;
constexpr uint16_t MAX_THINGSBOARD_MESSAGE_RECEIVE_SIZE = 128U;

constexpr uint64_t REQUEST_TIMEOUT_MICROSECONDS = 5000U * 1000U;

// Initialize used apis
Attribute_Request<2U, MAX_THINGSBOARD_SHARED_ATTRIBUTES> gthingsboard_attr_request;
Shared_Attribute_Update<1U, MAX_THINGSBOARD_SHARED_ATTRIBUTES> gthingsboard_shared_update;
const std::array<IAPI_Implementation*, 2U> gthingsboard_apis = {
    &gthingsboard_attr_request,
    &gthingsboard_shared_update
};

WiFiClient gEspClient;
Arduino_MQTT_Client gMqttClient(gEspClient);
ThingsBoard gThingsboard(gMqttClient, MAX_THINGSBOARD_MESSAGE_RECEIVE_SIZE, MAX_THINGSBOARD_MESSAGE_SEND_SIZE, Default_Max_Stack_Size, gthingsboard_apis);

unsigned long gLastThingsBoardConnect = 0;
unsigned long gLastThingsBoardSendCloud = 0;

int g_real_temperature;
int g_real_humidity;
int g_cfg_fan;
bool g_needUpdateTelemetry = false;

// Statuses for subscribing to shared attributes
bool gThingsBoarSharedAttrSubscribed = false;
bool gThingsBoarRequestedShared = false;

//void processTemperatureSwitch(const JsonVariantConst &data, JsonDocument &response);
void MyThingsBoard_PrintDebug(const char *str);

//Server_Side_RPC<2U, 1U> rpc; 
//const std::array<IAPI_Implementation*, 1U> apis = { &rpc };

void MyThingsBoard_Init(THINGSBOARD_CONFIG *lpCfg)
{
  gThingsBoardCfg = lpCfg;
}

void MyThingsBoard_SetTelemetriesToCloud(int real_temperature, int real_humidity, int cfg_fan)
{
  //356 -> 35.6
  g_real_temperature = real_temperature;
  g_real_humidity = real_humidity;
  g_cfg_fan = cfg_fan;
  g_needUpdateTelemetry = true;
}

//void processTemperatureSwitch(const JsonVariantConst &data, JsonDocument &response) {

  //MyThingsBoard_PrintDebug("\nNhan du lieu/lenh tu ThingsBoard!");

  //ledState = data.as<bool>();
  //response["setLedValue"] = ledState;
//}

void MyThingsBoard_requestTimedOut() {
  MyThingsBoard_PrintDebug("Attribute request timed out did not receive a response (timeout). Ensure client is connected to the MQTT broker and that the keys actually exist on the target device\n");
}

bool MyThingsBoard_IsConnected()
{
  return gThingsboard.connected();
}

void MyThingsBoard_LoopProcess()
{
  if (!gThingsboard.connected()) {

    if (WiFi.status() == WL_CONNECTED && (millis() - gLastThingsBoardConnect > 5000)) {
      // Reconnect to the ThingsBoard server,
      // if a connection was disrupted or has not yet been established

      MyThingsBoard_PrintDebug("\nConnecting to thingsboard ...");

      if (!gThingsboard.connect(gThingsBoardCfg->serverAddress, gThingsBoardCfg->deviceToken, gThingsBoardCfg->serverMQTTPort)) {
        MyThingsBoard_PrintDebug("\nFailed to connect thingsboard");
        return;
      }

      MyThingsBoard_PrintDebug("\nConnect to thingsboard sucesssfully.");

      //bool subscribed = rpc.RPC_Subscribe(RPC_Callback{"setTemperatureValue", &processTemperatureSwitch});
    
      //if (!subscribed) {
      //  Serial.println("Dang ky RPC setTemperatureValue that bai!");
      //} else {
      //  Serial.println("Dang ky RPC setTemperatureValue thanh cong!");
      //}

      gLastThingsBoardConnect = millis();
    }else
    {
      return;
    }
  }

  if (!gThingsBoarSharedAttrSubscribed &&
      MAX_THINGSBOARD_SHARED_ATTRIBUTES > 0 &&
      gThingsBoardCfg->callbackSharedAttributes != NULL) {
    
    MyThingsBoard_PrintDebug("Subscribing for shared attribute updates...");
    
    // Shared attributes we want to request from the server
    //
    std::array<const char*, MAX_THINGSBOARD_SHARED_ATTRIBUTES> SUBSCRIBED_SHARED_ATTRIBUTES;
    for(int i = 0; i < MAX_THINGSBOARD_SHARED_ATTRIBUTES;i++)
    {
      SUBSCRIBED_SHARED_ATTRIBUTES[i] = gThingsBoardCfg->registerEventSharedAttributes[i];
    }

    const Shared_Attribute_Callback<MAX_THINGSBOARD_SHARED_ATTRIBUTES> callback(gThingsBoardCfg->callbackSharedAttributes, SUBSCRIBED_SHARED_ATTRIBUTES);
    if (!gthingsboard_shared_update.Shared_Attributes_Subscribe(callback)) {
      MyThingsBoard_PrintDebug("Failed to subscribe for shared attribute updates");
      return;
    }

    MyThingsBoard_PrintDebug("Subscribe shared attribute updates done");
    gThingsBoarSharedAttrSubscribed = true;
  }

  if (!gThingsBoarRequestedShared && 
    gThingsBoardCfg->callbackRequestAttributes != NULL) {

    // Shared attributes we want to request from the server
    //
    std::array<const char*, MAX_THINGSBOARD_SHARED_ATTRIBUTES> SUBSCRIBED_SHARED_ATTRIBUTES;
    for(int i = 0; i < MAX_THINGSBOARD_SHARED_ATTRIBUTES;i++)
    {
      SUBSCRIBED_SHARED_ATTRIBUTES[i] = gThingsBoardCfg->registerEventSharedAttributes[i];
    }

    const Attribute_Request_Callback<MAX_THINGSBOARD_SHARED_ATTRIBUTES> sharedCallback(gThingsBoardCfg->callbackRequestAttributes, REQUEST_TIMEOUT_MICROSECONDS, &MyThingsBoard_requestTimedOut, SUBSCRIBED_SHARED_ATTRIBUTES);
    gThingsBoarRequestedShared = gthingsboard_attr_request.Shared_Attributes_Request(sharedCallback);

    if (!gThingsBoarRequestedShared) {
      MyThingsBoard_PrintDebug("\nFailed to request shared attributes");
    }
  }

  if (millis() - gLastThingsBoardSendCloud > gThingsBoardCfg->intervalSendTelemetryDataMs) {

    if(g_needUpdateTelemetry)
    {
      //----------------------------------------------------
      char tmp[128];
      sprintf(tmp, "\ntemperature: %d; humidity: %d; fan: %d", g_real_temperature/10,// g_real_temperature%10, 
                                g_real_humidity, 
                                g_cfg_fan
                                );

      MyThingsBoard_PrintDebug("\nsend telemety to cloud.");
      MyThingsBoard_PrintDebug(tmp);
      //----------------------------------------------------

	  //char temper[64];
	  //sprintf(temper, "%d.%d", g_real_temperature/10, g_real_temperature%10 );
	  
      gThingsboard.sendTelemetryData(TELEMETRY_NAME_TEMPERATURE, (int)g_real_temperature/10);
      gThingsboard.sendTelemetryData(TELEMETRY_NAME_HUMIDITY, g_real_humidity);
      gThingsboard.sendTelemetryData(TELEMETRY_NAME_FAN, g_cfg_fan);

      g_needUpdateTelemetry = false;
    }

    gLastThingsBoardSendCloud = millis();
  }

  gThingsboard.loop();
}

void MyThingsBoard_PrintDebug(const char *str)
{
  if (gThingsBoardCfg != NULL && gThingsBoardCfg->isPrintDebug)
  {
    Serial.println(str);
  }
}
