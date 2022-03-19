#ifndef _SETTINGS_H
#define _SETTINGS_H

#define ESP_DRD_USE_LITTLEFS true
#define FORMAT_LITTLEFS_ON_FAIL true

// double reset detection
#define DOUBLE_RESET_DETECTION_TIMEOUT 5
#define DOUBLE_RESET_DETECTION_ADDRESS 0

#define MAX_NUM_FAUXMO_DEVICES 8
#define SERIAL_BAUD_RATE 115200

#define PWM_FREQUENCY 10000

// if desired, disable remote debugging for release
// #define DEBUG_DISABLED true

#endif // _SETTINGS_H