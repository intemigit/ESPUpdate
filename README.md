# ESPUpdate

A library for updating ESP firmware over the air.

## Installation

1. Download the repository.
2. Copy the `ESPUpdate` folder to your Arduino `libraries` folder.

## Usage

```cpp

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h> //https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#else
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#endif

#include <FS.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>

#include <ESPUpdate.h>

AsyncWebServer server(80);
DNSServer dns;

ESPUpdate espUpdate(&server, &dns);

#define DEVICE_NAME "Device 1"
#define FIRMWARE_VERSION "1.2.5"

const char *ssid = "********"; 
const char *password = "********"; 

unsigned long time_update = 0;

void updateRealtime(
    const char *deviceId,
    const char *lastSeen,
    const char *orderCode,
    int currentLength,
    int setLength,
    int quantity);

long current_length = 0;

void setup()
{
  Serial.begin(115200);

  AsyncWiFiManager wifiManager(&server, &dns);

  for (uint8_t t = 4; t > 0; t--)
  {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  Serial.println();
  Serial.print("connecting to ");
  Serial.println(ssid);

  bool res = wifiManager.autoConnect("AutoConnectAP", "password"); // password protected ap

  if (!res)
  {
    Serial.println("Failed to connect");
  }
  else
  {
    Serial.println("connected...yeey :)");
  }
  //====================================================================================
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Firmware Ver: ");
  Serial.println(FIRMWARE_VERSION);

  espUpdate.begin(DEVICE_NAME, FIRMWARE_VERSION);
  espUpdate.checkUpdate();
}

void loop()
{
  espUpdate.handleClient();
  if (millis() > time_update)
  {
    time_update = millis() + 5000;
    time_t now = time(nullptr);
    String formattedTime = espUpdate.getFormatedTime(now);
    Serial.println(formattedTime);
    current_length++;
  }
}
