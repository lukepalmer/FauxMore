#include <Arduino.h>
#include "settings.h"
#include <ESPmDNS.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <fauxmoESP.h>
#include <ESPAsync_WiFiManager.h>
#include <ESPAsync_WiFiManager-Impl.h>
#include <RemoteDebug.h>
#include <ArduinoJson.h>
#include "config.h"

// LittleFS has been incorporated into current arduino core, but at time of writing,
// platformio current = 5.2.4 is on an old arduino core version that needs an external LittleFS.
// https://github.com/platformio/platform-espressif32/issues/619
#if (defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 2))
#include <LittleFS.h>
#define FileFS LittleFS
#else
#include <LITTLEFS.h>
#define FileFS LITTLEFS
#endif

FS *filesystem = &FileFS;

// This header is badly behaved and must be placed after LittleFS initialization
#include <ESP_DoubleResetDetector.h>

RemoteDebug Debug;
DoubleResetDetector *drd;
AsyncWebServer webServer(80);
fauxmoESP fauxmo;
Config config;
uint8_t deviceIdToGPIO[MAX_NUM_FAUXMO_DEVICES];

void webServerSetup()
{
  // onRequestBody and onNotFound are required for fauxomo gen1 and gen3 compatibility
  webServer.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
                          {
                            if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), String((char *)data)))
                              return;
                            /* Non-fauxmo requests can be handled here */ });

  webServer.onNotFound([](AsyncWebServerRequest *request)
                       {
                         String body = (request->hasParam("body", true)) ? request->getParam("body", true)->value() : String();
                         if (fauxmo.process(request->client(), request->method() == HTTP_GET, request->url(), body))
                         {
                           return;
                         }
                         /* Non-fauxmo requests can be handled here */ });

  AsyncElegantOTA.begin(&webServer, config.otaUsername.c_str(), config.otaPassword.c_str());
  webServer.begin();
}

void wifiConfigPortal(ESPAsync_WiFiManager &wifiManager, Config &config)
{
  debugI("Starting Config Portal");

  ESPAsync_WMParameter deviceName("DeviceName", "DeviceName", config.deviceName.c_str(), 32);
  wifiManager.addParameter(&deviceName);

  char checkboxHtml[24] = "type=\"checkbox\"";
  if (config.invertPwmOutput)
  {
    strcat(checkboxHtml, " checked");
  }

  ESPAsync_WMParameter otaUsername("OTA Update Username", "OTA Update Username", config.otaUsername.c_str(), 32);
  wifiManager.addParameter(&otaUsername);
  ESPAsync_WMParameter otaPassword("OTA Update Password", "OTA Update Password", config.otaPassword.c_str(), 32);
  wifiManager.addParameter(&otaPassword);

  ESPAsync_WMParameter invertPwmOutput("Invert PWM Output", "Invert PWM Output", "T", 2, checkboxHtml);
  wifiManager.addParameter(&invertPwmOutput);
  std::unique_ptr<ESPAsync_WMParameter> fauxmoNames[MAX_NUM_FAUXMO_DEVICES];
  std::unique_ptr<ESPAsync_WMParameter> fauxmoGPIO[MAX_NUM_FAUXMO_DEVICES];

  String names[MAX_NUM_FAUXMO_DEVICES];
  String gpios[MAX_NUM_FAUXMO_DEVICES];
  for (uint8_t i = 0; i < MAX_NUM_FAUXMO_DEVICES; ++i)
  {
    names[i] = String("Light_name_") + i;
    fauxmoNames[i] = std::unique_ptr<ESPAsync_WMParameter>(new ESPAsync_WMParameter(names[i].c_str(), names[i].c_str(), config.devices[i].name.c_str(), 32));
    wifiManager.addParameter(fauxmoNames[i].get());
    gpios[i] = String("GPIO_pin_") + i;
    fauxmoGPIO[i] = std::unique_ptr<ESPAsync_WMParameter>(new ESPAsync_WMParameter(gpios[i].c_str(), gpios[i].c_str(), String(config.devices[i].pin).c_str(), 3));
    wifiManager.addParameter(fauxmoGPIO[i].get());
  }

  digitalWrite(LED_BUILTIN, HIGH);
  if (wifiManager.startConfigPortal(config.deviceName.c_str(), NULL))
  {
    debugI("Connected to WiFi");
  }
  else
  {
    debugE("Not connected to WiFi");
  }
  config.deviceName = deviceName.getValue();
  config.invertPwmOutput = (strncmp(invertPwmOutput.getValue(), "T", 1) == 0);
  config.otaUsername = otaUsername.getValue();
  config.otaPassword = otaPassword.getValue();
  for (uint8_t i = 0; i < MAX_NUM_FAUXMO_DEVICES; ++i)
  {
    config.devices[i].name = fauxmoNames[i]->getValue();
    String gpiostr(fauxmoGPIO[i]->getValue());
    config.devices[i].pin = gpiostr.toInt();
  }

  Config::save(config, filesystem, Debug);
}

void setPwmLevel(uint8_t channel, uint8_t level, bool invert)
{
  if (invert)
    level = 255 - level;
  debugI("Setting PWM for channel %d to %d", channel, level);
  ledcWrite(channel, level);
}

void setup(void)
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(SERIAL_BAUD_RATE);

  Debug.setResetCmdEnabled(false);
  Debug.showColors(true);
  Debug.setSerialEnabled(true);

  debugI("Beginning setup");
  if (!FileFS.begin(FORMAT_LITTLEFS_ON_FAIL))
    debugE("LittleFS mount failed");

  Config::loadInto(config, filesystem, Debug);

  if (!MDNS.begin(config.deviceName.c_str()))
    debugE("Error setting up MDNS");

  drd = new DoubleResetDetector(DOUBLE_RESET_DETECTION_TIMEOUT, DOUBLE_RESET_DETECTION_ADDRESS);

#if (USING_ESP32_S2 || USING_ESP32_C3)
  ESPAsync_WiFiManager wifiManager(&webServer, NULL, config.deviceName.c_str()));
#else
  DNSServer dnsServer;
  ESPAsync_WiFiManager wifiManager(&webServer, &dnsServer, config.deviceName.c_str());
#endif

  if (wifiManager.WiFi_SSID() == "")
  {
    debugI("No AP credentials, starting config portal");
    wifiConfigPortal(wifiManager, config);
  }
  else if (drd->detectDoubleReset())
  {
    debugI("Double reset detected, starting config portal");
    wifiConfigPortal(wifiManager, config);
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin();
  digitalWrite(LED_BUILTIN, LOW);
  Debug.begin(config.deviceName.c_str());

  int connRes = WiFi.waitForConnectResult();
  if (connRes == WL_CONNECTED)
  {
    debugI("WiFi Connected at %s", WiFi.localIP().toString().c_str());
  }
  else
    debugE("Failed to connect to WiFi: %d", connRes);

  fauxmo.createServer(false);
  fauxmo.setPort(80);
  fauxmo.enable(true);

  for (uint8_t i = 0; i < MAX_NUM_FAUXMO_DEVICES; ++i)
  {
    const String deviceName = config.devices[i].name.c_str();
    uint8_t gpio = config.devices[i].pin;
    if (!deviceName.isEmpty())
    {
      unsigned char deviceId = fauxmo.addDevice(deviceName.c_str());

      ledcSetup(deviceId, PWM_FREQUENCY, 8);
      ledcAttachPin(gpio, deviceId);
      setPwmLevel(deviceId, 0, config.invertPwmOutput); // off at startup
      debugI("Set up fauxmo device \"%s\" on GPIO pin %d", deviceName.c_str(), gpio);
    }
  }
  fauxmo.onSetState([](unsigned char deviceId, const char *deviceName, bool state, unsigned char value)
                    {
                      debugI("Device #%d (%s) state: %s value: %d", deviceId, deviceName, state ? "ON" : "OFF", value);
                    

                      if (state) {
                          // The value received from FauxmoESP for 100% brightness is sometimes 255
                          // and sometimes 254. Correct it. This makes a value of 0 impossible 
                          // but that doesn't matter because 'state' is separate.
                          value = min(value, (unsigned char)254) + 1;
                      }
                      else {
                        value = 0;
                      }

                      setPwmLevel(deviceId, value, config.invertPwmOutput); });

  webServerSetup();
  debugI("Setup is complete");
}

void loop(void)
{
  Debug.handle();
  drd->loop();
  fauxmo.handle();
}