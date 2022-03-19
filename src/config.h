#ifndef _CONFIG_H
#define _CONFIG_H
#include <Arduino.h>
#include <FS.h>
#include <RemoteDebug.h>
#include "settings.h"

struct FauxmoDevice
{
    uint8_t pin;
    String name;
};

struct Config
{
    FauxmoDevice devices[MAX_NUM_FAUXMO_DEVICES];
    String deviceName;
    bool invertPwmOutput;
    String otaUsername;
    String otaPassword;

    static void loadInto(Config &, FS*, RemoteDebug&);
    static void save(Config &, FS*, RemoteDebug&);
};

#endif // _CONFIG_H