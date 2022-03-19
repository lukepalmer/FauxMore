#include <Arduino.h>
#include "config.h"
#include <ArduinoJson.h>
#define CONFIG_FILE_NAME "/config.json"
#define JSON_SIZE 1024

void Config::loadInto(Config &config, FS *fs, RemoteDebug &Debug)
{
    File configFile = fs->open(CONFIG_FILE_NAME, "r");

    if (!configFile)
    {
        debugE("Could not open config file");
    }
    else
    {
        debugD("Opened config file");
        size_t configFileSize = configFile.size();
        std::unique_ptr<char[]> buf(new char[configFileSize + 1]);
        configFile.readBytes(buf.get(), configFileSize);
        DynamicJsonDocument json(JSON_SIZE);
        auto deserializeError = deserializeJson(json, buf.get(), configFileSize);

        if (deserializeError)
        {
            debugE("Could not deserialize JSON: %s", deserializeError.c_str());
        }
        else
        {
            serializeJsonPretty(json, Debug);
            config.deviceName = json["deviceName"] | "";
            config.invertPwmOutput = json["invertPwmOutput"] | false;
            config.otaUsername = json["otaUsername"] | "";
            config.otaPassword = json["otaPassword"] | "";
            for (uint8_t i = 0; i < MAX_NUM_FAUXMO_DEVICES; ++i)
            {
                const JsonObject device = json["devices"][i];
                config.devices[i].name = device["name"] | "";
                config.devices[i].pin = device["pin"] | 0;
            }
        }
    }
    if (config.deviceName.isEmpty())
        config.deviceName = String("ESP32_") + String((uint32_t)ESP.getEfuseMac(), HEX);
}

void Config::save(Config &config, FS *fs, RemoteDebug &Debug)
{
    DynamicJsonDocument json(JSON_SIZE);
    json["deviceName"] = config.deviceName;
    json["invertPwmOutput"] = config.invertPwmOutput;
    json["otaUsername"] = config.otaUsername;
    json["otaPassword"] = config.otaPassword;

    for (uint8_t i = 0; i < MAX_NUM_FAUXMO_DEVICES; ++i)
    {
        json["devices"][i]["name"] = config.devices[i].name;
        json["devices"][i]["pin"] = config.devices[i].pin;
    }
    debugI("Saving config");
    serializeJsonPretty(json, Debug);

    File configFile = fs->open(CONFIG_FILE_NAME, "w");
    if (!configFile)
    {
        debugE("Failed to open config file for writing");
    }
    else
    {
        serializeJson(json, configFile);
    }
    configFile.close();
}