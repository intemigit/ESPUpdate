#ifndef ESPUpdate_h
#define ESPUpdate_h

#if defined(ESP8266)
#include <ESP8266WiFi.h> // https://github.com/esp8266/Arduino
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#else
#include <WiFi.h>
#include <HTTPUpdate.h>
#include <HTTPClient.h>
#include <AsyncTCP.h>
#endif

#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <time.h>
#include <sys/time.h>
#include <FS.h>
#include <LittleFS.h>

#include <ESPAsyncWebServer.h>

#define ESP_UPDATE_DEBUG 1

// #define USE_EADNS               // uncomment to use ESPAsyncDNSServer
#ifdef USE_EADNS
#include <ESPAsyncDNSServer.h> // https://github.com/devyte/ESPAsyncDNSServer
                               // https://github.com/me-no-dev/ESPAsyncUDP
#else
#include <DNSServer.h>
#endif
#include <memory>

// fix crash on ESP32 (see https://github.com/alanswx/ESPAsyncWiFiManager/issues/44)
#if defined(ESP8266)
typedef int wifi_ssid_count_t;
#else
typedef int16_t wifi_ssid_count_t;
#endif

const char HTML_HEADER[] PROGMEM = "<!DOCTYPE html><html><head><meta http-equiv='Content-Type' content='text/html; charset=utf-8'/><meta name='viewport' content='width=device-width, initial-scale=1, minimum-scale=1.0, shrink-to-fit=no'><title>Intemi - Temp + Humi Display</title></head><body>";
const char HTML_FOOTER[] PROGMEM = "</body></html>";
const char HTML_UPDATE[] PROGMEM = "<h1>Only .bin file</h1><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' required><input type='submit' value='Run Update'></form>";

#define FIRMWARE_VER "1.2.5"

class ESPUpdate
{
public:
#ifdef USE_EADNS
  ESPUpdate(AsyncWebServer *server, AsyncDNSServer *dns);
#else
  ESPUpdate(AsyncWebServer *server, DNSServer *dns);
#endif

  void begin(String name, String version);
  void handleClient();
  JsonDocument httpPostRPC(String function, JsonDocument doc);
  String getFormatedTime(time_t now);
  void checkUpdate(bool rebootOnUpdate = false);

private:
  AsyncWebServer *server;
#ifdef USE_EADNS
  AsyncDNSServer *dnsServer;
#else
  DNSServer *dnsServer;
#endif
  // DNS server
  const byte DNS_PORT = 53;

  const char *supabaseUrl = "https://ulfqekugviaqrdmrxwlb.supabase.co/";
  const char *supabaseRPCUrl = "https://ulfqekugviaqrdmrxwlb.supabase.co/rest/v1/rpc/";
  const char *supabaseAnonKey = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6InVsZnFla3VndmlhcXJkbXJ4d2xiIiwicm9sZSI6ImFub24iLCJpYXQiOjE3MTQ0MDIzNTgsImV4cCI6MjAyOTk3ODM1OH0.t31rAAlf76xvZpoprGkUq-C6h7EkwHQRa9J8nw-kEdY";
  
  const char *filename = "/esp_update_config.json";

  String _deviceId = "00000000-0000-0000-0000-000000000000";
  String _deviceName = "00000000-0000-0000-0000-000000000000";
  String _deviceVersion = "00000000-0000-0000-0000-000000000000";

  // void checkForUpdate();
  JsonDocument httpPost(String action, JsonDocument doc);
  void handleNotFound(AsyncWebServerRequest *request);
  void updateStarted();
  void updateFinished();
  void updateInProgress(int cur, int total);
  void updateError(int err);
  void saveConfig();
  void loadConfig();
  void deviceInit(String name, String version);
};

#endif
