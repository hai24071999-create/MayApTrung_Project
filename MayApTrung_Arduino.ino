#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <Shared_Attribute_Update.h>
#include <ThingsBoard.h>

#include "src/my_uartdata_stm32_esp32.h"
#include "src/my_wifi.h"
#include "src/my_thingsboard.h"
//--------------------------
#define DEFAULT_BAUD 9600

#define STM32_SERIAL Serial1

#define DEFAULT_TX_PIN  6
#define DEFAULT_RX_PIN  7

// the IO8 pin is blue led on board
#define LED_PIN 8

//--------------------------
char g_wifi_ssid[64] = "S23 cua Nguyen";
char g_wifi_password[64] = "24071999";
//--------------------------

//--------------------------
#define MAX_TIMEOUT_MS 5000
#define TIMEVAL_1MIN_MS 60000

#define MAX_TIMEOUT_UPDATE_WIFI_MS 1000
#define TIMEVAL_FORCE_UPDATE_WIFI_STATE_MS 10000
//--------------------------

//----------------------------------------------------------
// THINGSBOARD defines
constexpr char TOKEN[] = "rDSu4qzP4xtbo2VfMh20";

// Thingsboard we want to establish a connection too
const char THINGSBOARD_SERVER[] = "c";

// MQTT port used to communicate with the server, 1883 is the default unencrypted MQTT port,
// whereas 8883 would be the default encrypted SSL MQTT port
const uint16_t THINGSBOARD_PORT = 1883U;

// Statuses for subscribing to shared attributes
bool subscribed = false;
//----------------------------------------------------------

THINGSBOARD_CONFIG thingsBoardCfg;

//-------------------------
// Main functions
//-----------------------

void Blink_Led(int count)
{
	for(int i = 0; i < count; i++)
	{
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
    delay(500);
	}
}

unsigned long g_lastCheckingStatus = 0;
void CheckMainStatus()
{
  if (millis() - g_lastCheckingStatus < 5000) { 
    return;  
  }

  g_lastCheckingStatus = millis();

  static bool isWifiLastStatus = false;
  bool isWifiOK = WiFi.status() == WL_CONNECTED;

  if (isWifiOK && isWifiLastStatus != isWifiOK)
  {
    Blink_Led(2);
  }

  isWifiLastStatus = isWifiOK;

  static bool isThingsBoardLastStatus = false;
  bool isThingsBoardOK = MyThingsBoard_IsConnected();

  if (isThingsBoardOK && isThingsBoardLastStatus != isThingsBoardOK)
  {
    Blink_Led(3);
  }

  isThingsBoardLastStatus = isThingsBoardOK;
}

// STM32 data processing
uint8_t g_wifiStateLastUpdate = 0;
unsigned long g_lastCheckingWifiStateTime = 0;
unsigned long g_lastSendWifiStateTime = 0;
void CheckAndUpdate_ToSTM32_WifiState()
{ 
  if (millis() - g_lastCheckingWifiStateTime < MAX_TIMEOUT_UPDATE_WIFI_MS) { 
    return;  
  }

  g_lastCheckingWifiStateTime = millis();

  // for long time, need to send immediately 
  bool needForceSend = g_lastCheckingWifiStateTime - g_lastSendWifiStateTime > TIMEVAL_FORCE_UPDATE_WIFI_STATE_MS; 

  bool isWifiOK = WiFi.status() == WL_CONNECTED;
  const char *msg = NULL;
  WIFI_STATE* lpDataSend = MyUART_Get_WifiState();

  switch(g_wifiStateLastUpdate)
  {
    case 0:

    // not yet to update wifi status
    if(isWifiOK)
    {
      strcpy(lpDataSend->status, WIFI_STATE_READY);
    }else{
      strcpy(lpDataSend->status, WIFI_STATE_NOREADY);
    }

    msg = MyUART_Convert_WifiState();
    break;
    case 1:
    // last status is OK
    // but now is not ready
    if(!isWifiOK)
    {
      strcpy(lpDataSend->status, WIFI_STATE_NOREADY);
      msg = MyUART_Convert_WifiState();
    }else if(needForceSend)
    {
      strcpy(lpDataSend->status, WIFI_STATE_READY);
      msg = MyUART_Convert_WifiState();
    }
    break;
    case 2:
    // last status is ERROR
    // but now is OK
    if(isWifiOK)
    {
      strcpy(lpDataSend->status, WIFI_STATE_READY);
      msg = MyUART_Convert_WifiState();
    }else if(needForceSend)
    {
      strcpy(lpDataSend->status, WIFI_STATE_NOREADY);
      msg = MyUART_Convert_WifiState();
    }
    break;
  }

  // store last status
  g_wifiStateLastUpdate = isWifiOK?1:2;

  //if ((msg != NULL) && (STM32_SERIAL.available() > 0))
  if (msg != NULL)
  {
    // send to debugs
    Serial.print("Send to STM32:");
    Serial.println(msg);

    //send to STM32
    STM32_SERIAL.print(msg);

    g_lastSendWifiStateTime = millis();
  }
}

// Shared attributes 
bool gThingsBoard_Shared_Mode = false;
bool gThingsBoard_Shared_Mode_Updated = false;

bool gThingsBoard_Shared_Temperature = false;
bool gThingsBoard_Shared_Temperature_Updated = false;

bool gThingsBoard_Shared_Fan = false;
bool gThingsBoard_Shared_Fan_Updated = false;

void SetAttributeToSTM32_BOOL(const char *name, bool val)
{
    if(strcmp(name, THINGSBOARD_SHARED_ATTRIBUTE_MODE) == 0)
    {
      gThingsBoard_Shared_Mode = val;
      gThingsBoard_Shared_Mode_Updated = true;
    }else if(strcmp(name, THINGSBOARD_SHARED_ATTRIBUTE_TEMPERATURE) == 0)
    {
      gThingsBoard_Shared_Temperature = val;
      gThingsBoard_Shared_Temperature_Updated = true;
    }else if(strcmp(name, THINGSBOARD_SHARED_ATTRIBUTE_FAN) == 0)
    {
      gThingsBoard_Shared_Fan = val;
      gThingsBoard_Shared_Fan_Updated = true;
    }else
    {
      Serial.print("\nUnhandle attributed: ");
      Serial.print(name);
    }
}

unsigned long gLastThingsBoardSendAttrSTM32 = 0;

void CheckAndUpdate_ToSTM32_Attributes()
{ 
  if (millis() - gLastThingsBoardSendAttrSTM32 < MAX_TIMEOUT_UPDATE_WIFI_MS) { 
    return;  
  }  
  gLastThingsBoardSendAttrSTM32 = millis();

  RECV_CLOUD_DATA* lpRecvData = MyUART_Get_RecvCloud();

  if (gThingsBoard_Shared_Mode_Updated)
  {
    //----------------------------------------------------
    char tmp[128];
    sprintf(tmp, "\ntattr %s: %s", THINGSBOARD_SHARED_ATTRIBUTE_MODE, gThingsBoard_Shared_Mode?"1":"0");
    Serial.print(tmp);
    //----------------------------------------------------

    strcpy(lpRecvData->keyName, THINGSBOARD_SHARED_ATTRIBUTE_MODE);
    strcpy(lpRecvData->data, gThingsBoard_Shared_Mode?"1":"0");
    
    const char *lpData = MyUART_Convert_RecvCloud();

    //send to STM32
    STM32_SERIAL.print(lpData);

    gThingsBoard_Shared_Mode_Updated = false;
  }

  if (gThingsBoard_Shared_Temperature_Updated)
  {
    //----------------------------------------------------
    char tmp[128];
    sprintf(tmp, "\ntattr %s: %s", THINGSBOARD_SHARED_ATTRIBUTE_TEMPERATURE, gThingsBoard_Shared_Temperature?"1":"0");
    Serial.print(tmp);
    //----------------------------------------------------

    strcpy(lpRecvData->keyName, THINGSBOARD_SHARED_ATTRIBUTE_TEMPERATURE);
    strcpy(lpRecvData->data, gThingsBoard_Shared_Temperature?"1":"0");
    
    const char *lpData = MyUART_Convert_RecvCloud();

    //send to STM32
    STM32_SERIAL.print(lpData);

    gThingsBoard_Shared_Temperature_Updated = false;
  }

  if (gThingsBoard_Shared_Fan_Updated)
  {
    //----------------------------------------------------
    char tmp[128];
    sprintf(tmp, "\ntattr %s: %s", THINGSBOARD_SHARED_ATTRIBUTE_FAN, gThingsBoard_Shared_Fan?"1":"0");
    Serial.print(tmp);
    //----------------------------------------------------

    strcpy(lpRecvData->keyName, THINGSBOARD_SHARED_ATTRIBUTE_FAN);
    strcpy(lpRecvData->data, gThingsBoard_Shared_Fan?"1":"0");
    
    const char *lpData = MyUART_Convert_RecvCloud();

    //send to STM32
    STM32_SERIAL.print(lpData);

    gThingsBoard_Shared_Fan_Updated = false;
  }
}
//-------------------------

//-------------------------
void DoListen_DATA_FROM_STM32()
{
  // Read data from Serial1 port (from STM32)
  if (STM32_SERIAL.available() > 0) {

    char * receiveBuff = NULL;
    int receiveBuffSize = 0;
    
    receiveBuff = MyUART_GetRecvBuffer(&receiveBuffSize);

    size_t len = STM32_SERIAL.readBytesUntil('\n', receiveBuff, receiveBuffSize);
	
    if (len > 0)
    {
      if (len >= receiveBuffSize)
      {
        // the buffer is too large, ...
        receiveBuff[receiveBuffSize-1] = '\0';
        Serial.print("invalid buffer from STM32: ");
        Serial.println(receiveBuff);
      }else
      {
        // manual fill buffer receive and set buffer ready for parsing 
        receiveBuff[len] = '\0';
        
        MyUART_SetRecvBuffer_Ready();
        
        Serial.print("Request from STM32: ");
        Serial.println(receiveBuff);

        // get full a STM32 data, continue to parse
        // ...
        const char* label = MyUART_TryGet_Data();
        //
        if (strcmp(label, LABEL_SEND_CLOUD) == 0)
        {
          SEND_CLOUD_DATA* lpSendCloud = MyUART_Get_SendCloud();
          
          Serial.println("send cloud data ... ");
          
          MyThingsBoard_SetTelemetriesToCloud(lpSendCloud->real_temperature, lpSendCloud->real_humidity, lpSendCloud->cfg_fan);

          //------------------------
          char tmp[128];
          sprintf(tmp, "%d;%d;%d;%d;%d;%d;%d", lpSendCloud->real_temperature, 
                                    lpSendCloud->real_humidity, 
                                    lpSendCloud->cfg_mode, 
                                    lpSendCloud->cfg_temperature,
                                    lpSendCloud->cfg_fan,
                                    lpSendCloud->cfg_wait_turning,
                                    lpSendCloud->cfg_auto_day
                                    );

          Serial.println(tmp);
          //------------------------
        }else
        {
          Serial.println("[ERROR] Not support STM32 data from STM32: ");
          Serial.print(label);
        }
      }
    }
  }
}

void processSharedAttributeUpdate(const JsonObjectConst &data) {

  Serial.println("\nprocessSharedAttributeUpdate...");

  for (auto it = data.begin(); it != data.end(); ++it) {
    Serial.println(it->key().c_str());
    // Shared attributes have to be parsed by their type.
    Serial.println(it->value().as<const char*>());

    SetAttributeToSTM32_BOOL(it->key().c_str(), it->value().as<bool>());
  }

  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}

void processResponseAttribute(const JsonObjectConst &data) {
  
  for (auto it = data.begin(); it != data.end(); ++it) {
    
    Serial.println(it->key().c_str());
    // Shared attributes have to be parsed by their type.
    Serial.println(it->value().as<bool>());
  
    SetAttributeToSTM32_BOOL(it->key().c_str(), it->value().as<bool>());
  }

  const size_t jsonSize = Helper::Measure_Json(data);
  char buffer[jsonSize];
  serializeJson(data, buffer, jsonSize);
  Serial.println(buffer);
}
//---------------------------

void setup() {

  // initialize default serial...
  Serial.begin(DEFAULT_BAUD); 

  // waiting for USB stable
  delay(2000); 
  Serial.println("\nSystem starting...!");

  // initialize led 
  pinMode(LED_PIN, OUTPUT);

  // Boud Rate: 9600, 8N1, TX=GPIO6, RX=GPIO7
  STM32_SERIAL.begin(DEFAULT_BAUD, SERIAL_8N1, DEFAULT_RX_PIN, DEFAULT_TX_PIN);

  // blink led
  Blink_Led(1);

  // Setup wifi
  MyWifi_Init(g_wifi_ssid, g_wifi_password);

  MyWifi_Connect();

  ///-----------------------------------------------------------
  // Initialize ThingsBoard functions
  
  thingsBoardCfg.isPrintDebug = true;

  strcpy(thingsBoardCfg.serverAddress, THINGSBOARD_SERVER);
  
  thingsBoardCfg.serverMQTTPort = THINGSBOARD_PORT;
  
  strcpy(thingsBoardCfg.deviceToken, TOKEN);
  
  thingsBoardCfg.intervalSendTelemetryDataMs = 15000;
  thingsBoardCfg.intervalSendSTM32DataMs = 1000;

  strcpy(thingsBoardCfg.registerEventSharedAttributes[0], THINGSBOARD_SHARED_ATTRIBUTE_MODE);
  strcpy(thingsBoardCfg.registerEventSharedAttributes[1], THINGSBOARD_SHARED_ATTRIBUTE_TEMPERATURE);
  strcpy(thingsBoardCfg.registerEventSharedAttributes[2], THINGSBOARD_SHARED_ATTRIBUTE_FAN);

  thingsBoardCfg.callbackSharedAttributes = processSharedAttributeUpdate;

  thingsBoardCfg.callbackRequestAttributes = processResponseAttribute;

  char str[128];
  //sprintf(str,"\n->server: %s, token: %s port: %d", thingsBoardCfg.serverAddress, thingsBoardCfg.deviceToken, thingsBoardCfg.serverMQTTPort);
  sprintf(str,"\n->server: %s, port: %d", thingsBoardCfg.serverAddress, thingsBoardCfg.serverMQTTPort);
  Serial.println(str);

  MyThingsBoard_Init(&thingsBoardCfg);
}

void loop() {

  // checking and retry connect wifi when connection is lost
  //
  MyWifi_Retry_Connect();

  // Update wifi status to STM32
  CheckAndUpdate_ToSTM32_WifiState();
  CheckAndUpdate_ToSTM32_Attributes();

  DoListen_DATA_FROM_STM32();

  // loop process thingsboard
  MyThingsBoard_LoopProcess();

  // checking status and blink led
  CheckMainStatus();
}
