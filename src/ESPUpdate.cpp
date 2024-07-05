#include "ESPUpdate.h"

#ifdef USE_EADNS
ESPUpdate::ESPUpdate(AsyncWebServer *server,
                     AsyncDNSServer *dns) : server(server), dnsServer(dns)
{
#else
ESPUpdate::ESPUpdate(AsyncWebServer *server,
                     DNSServer *dns) : server(server), dnsServer(dns)
{
#endif
  // Init variable
}

bool espShouldReboot = false;

void ESPUpdate::begin(String name, String version)
{
  // For read config -----------------------------------------------------------
  if (!LittleFS.begin())
  {
    Serial.println("Failed to mount file system");
    return;
  }

  // saveConfig();
  loadConfig();
  //-----------------------------------------------------------------------------

  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request)
             { request->send(200, "text/plain", "Xin chào"); });

  server->on("/update", HTTP_GET, [](AsyncWebServerRequest *request)
             {
    String view_html = "<!DOCTYPE html><html><head><title>Update</title></head><body><h1>Only .bin file</h1><form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update' required><input type='submit' value='Run Update'></form></body></html>";
    request->send(200, "text/html", view_html); });

  server->on("/update", HTTP_POST, [](AsyncWebServerRequest *request)
             {
    espShouldReboot = !Update.hasError();
    AsyncWebServerResponse *response = request->beginResponse(200, "text/html", espShouldReboot ? "<h1>Update DONE</h1><a href='/'>Return Home</a>" : "<h1>Update FAILED</h1><a href='/updt'>Retry?</a>");
    response->addHeader("Connection", "close");
    request->send(response); }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
             {
    if (!index) {
#ifdef ESP_UPDATE_DEBUG
          Serial.printf("Update Start: %s\n", filename.c_str());
#endif
      if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) {
#ifdef ESP_UPDATE_DEBUG
        Update.printError(Serial);
#endif
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
#ifdef ESP_UPDATE_DEBUG
        Update.printError(Serial);
#endif
      }
    }
    if (final) {
      if (Update.end(true)) {
#ifdef ESP_UPDATE_DEBUG
        Serial.printf("Update Success: %uB\n", index + len);
#endif
      } else {
#ifdef ESP_UPDATE_DEBUG
        Update.printError(Serial);
#endif
      }
    } });

  server->onNotFound(std::bind(&ESPUpdate::handleNotFound, this, std::placeholders::_1));
  server->begin();

  configTime(7 * 3600, 0, "pool.ntp.org", "time.nist.gov");
#ifdef ESP_UPDATE_DEBUG
  Serial.println("Configuring Time");
#endif
  while (time(nullptr) < 8 * 3600 * 2)
  {
    delay(500);
#ifdef ESP_UPDATE_DEBUG
    Serial.print(".");
#endif
  }

#ifdef ESP_UPDATE_DEBUG
  Serial.println();
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  Serial.print("Current time: ");
  Serial.print(asctime(&timeinfo));
#endif

#if ESP_UPDATE_USE_LED
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
#endif

  // Call back For HTTP update
  ESPhttpUpdate.onStart(std::bind(&ESPUpdate::updateStarted, this));
  ESPhttpUpdate.onEnd(std::bind(&ESPUpdate::updateFinished, this));
  ESPhttpUpdate.onProgress([this](int cur, int total)
                           { this->updateInProgress(cur, total); });
  ESPhttpUpdate.onError([this](int err)
                        { this->updateError(err); });
  ESPhttpUpdate.rebootOnUpdate(false);
  //------------------------------------------------------------------------

  // Call back For OTA
#ifdef ESP_UPDATE_DEBUG
  ArduinoOTA.onStart([]()
                     { Serial.println("Start"); });
  ArduinoOTA.onEnd([]()
                   { Serial.println("\nEnd"); });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total)
                        { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
  ArduinoOTA.onError([](ota_error_t error)
                     {
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed"); });
#endif
  ArduinoOTA.begin();
  // Init device to supabase
  _deviceName = name;
  _deviceVersion = version;
  deviceInit(name, version);
}

void ESPUpdate::handleClient()
{
  ArduinoOTA.handle();
}

JsonDocument ESPUpdate::httpPostRPC(String function, JsonDocument doc)
{
  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  JsonDocument responseDoc;

  HTTPClient http;
  http.begin(client, String(supabaseRPCUrl) + function);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("apikey", supabaseAnonKey);

  String requestBody;
  serializeJson(doc, requestBody);
#ifdef ESP_UPDATE_DEBUG
  Serial.println(requestBody);
#endif

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    deserializeJson(responseDoc, response);
#ifdef ESP_UPDATE_DEBUG
    Serial.println(httpResponseCode);
    Serial.println(response);
#endif
  }
  else
  {
#ifdef ESP_UPDATE_DEBUG
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
#endif
  }

  http.end();
  return responseDoc;
}

String ESPUpdate::getFormatedTime(time_t now)
{
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);

  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &timeinfo);

  struct timeval tv;
  gettimeofday(&tv, nullptr);
  long microseconds = tv.tv_usec;

  char formattedTimeBuffer[40];
  snprintf(formattedTimeBuffer, sizeof(formattedTimeBuffer), "%s.%06ld+00:00", buffer, microseconds);

  return String(formattedTimeBuffer);
}

void ESPUpdate::checkUpdate(bool rebootOnUpdate)
{
  JsonDocument doc;
  doc["device_id"] = _deviceId;
  doc["current_version"] = _deviceVersion;
  JsonDocument resDoc = httpPost("rest/v1/rpc/check_update", doc);
  bool updateAvailable = resDoc["updateAvailable"];
  const char *fileUrl = resDoc["fileUrl"];
#ifdef ESP_UPDATE_DEBUG
  Serial.print("Update Available: ");
  Serial.println(updateAvailable);
  Serial.print("File URL: ");
  Serial.println(fileUrl);
#endif

  if (updateAvailable && fileUrl != nullptr)
  {
    doc["expiresIn"] = 3600;
    JsonDocument resDocStorage = httpPost("storage/v1/object/sign/firmwares/" + String(fileUrl), doc);

    const char *signedUrl = resDocStorage["signedURL"];
#ifdef ESP_UPDATE_DEBUG
    Serial.print("Signed Url: ");
    Serial.println(signedUrl);
#endif

    String firmwareUrl = "https://ulfqekugviaqrdmrxwlb.supabase.co/storage/v1" + String(signedUrl);

    ESPhttpUpdate.rebootOnUpdate(rebootOnUpdate);
    BearSSL::WiFiClientSecure client;
    client.setInsecure();

    t_httpUpdate_return ret = ESPhttpUpdate.update(client, firmwareUrl, FIRMWARE_VER);
    // if successful, ESP will restart
    switch (ret)
    {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
    }
  }
}

// Private -----------------------------------------------------------------------
JsonDocument ESPUpdate::httpPost(String action, JsonDocument doc)
{
  BearSSL::WiFiClientSecure client;
  client.setInsecure();

  JsonDocument responseDoc;

  HTTPClient http;
  http.begin(client, String(supabaseUrl) + action);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(supabaseAnonKey));
  http.addHeader("apikey", supabaseAnonKey);

  String requestBody;
  serializeJson(doc, requestBody);
#ifdef ESP_UPDATE_DEBUG
  Serial.println(requestBody);
#endif

  int httpResponseCode = http.POST(requestBody);

  if (httpResponseCode > 0)
  {
    String response = http.getString();
    deserializeJson(responseDoc, response);
#ifdef ESP_UPDATE_DEBUG
    Serial.println(httpResponseCode);
    Serial.println(response);
#endif
  }
  else
  {
#ifdef ESP_UPDATE_DEBUG
    Serial.print("Error on sending POST: ");
    Serial.println(httpResponseCode);
#endif
  }

  http.end();
  return responseDoc;
}

void ESPUpdate::handleNotFound(AsyncWebServerRequest *request)
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += request->url();
  message += "\nMethod: ";
  message += (request->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += request->args();
  message += "\n";

  for (unsigned int i = 0; i < request->args(); i++)
  {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }
  AsyncWebServerResponse *response = request->beginResponse(404, "text/plain", message);
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "-1");
  request->send(response);
}

void ESPUpdate::updateStarted()
{
#ifdef ESP_UPDATE_DEBUG
  Serial.println("HTTP update process started");
#endif
}

void ESPUpdate::updateFinished()
{
#ifdef ESP_UPDATE_DEBUG
  Serial.println("HTTP update process finished");
#endif
}

void ESPUpdate::updateInProgress(int cur, int total)
{
#ifdef ESP_UPDATE_DEBUG
  Serial.printf("HTTP update progress: %u%%\r", (cur / (total / 100)));
#endif
}

void ESPUpdate::updateError(int err)
{
#ifdef ESP_UPDATE_DEBUG
  Serial.printf("HTTP update fatal error code %d\n", err);
#endif
}

void ESPUpdate::saveConfig()
{
  // Tạo JSON object
  JsonDocument doc;
  doc["deviceId"] = _deviceId.c_str();

  // Mở tệp để ghi
  File file = LittleFS.open(filename, "w");
  if (!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Ghi dữ liệu JSON vào tệp
  if (serializeJson(doc, file) == 0)
  {
    Serial.println("Failed to write to file");
  }

  file.close();
}

void ESPUpdate::loadConfig()
{
  // Mở tệp để đọc
  File file = LittleFS.open(filename, "r");
  if (!file)
  {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Đọc dữ liệu JSON từ tệp
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print("Failed to read file, using default configuration: ");
    Serial.println(error.c_str());
    _deviceId = "00000000-0000-0000-0000-000000000000";
    saveConfig();
    return;
  }

  // Đóng tệp sau khi đọc
  file.close();

  // Truy xuất giá trị từ JSON object
  _deviceId = doc["deviceId"] | "00000000-0000-0000-0000-000000000000";
  Serial.print("deviceId: ");
  Serial.println(_deviceId);
}

void ESPUpdate::deviceInit(String name, String version)
{
  JsonDocument doc;
  doc["device_id"] = _deviceId;
  doc["device_name"] = name;
  doc["device_version"] = version;
  JsonDocument resDoc = httpPost("rest/v1/rpc/connect", doc);
  bool resResult = resDoc["result"];
  const char *resId = resDoc["id"];
#ifdef ESP_UPDATE_DEBUG
  Serial.print("Response Result: ");
  Serial.println(resResult);
  Serial.print("Response Id: ");
  Serial.println(resId);
#endif

  if (resResult && resId != "00000000-0000-0000-0000-000000000000")
  {
    _deviceId = String(resId);
    saveConfig();
  }
  // rest/v1/rpc/
}