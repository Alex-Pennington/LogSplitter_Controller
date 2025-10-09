#include "constants.h"

// Command validation arrays
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "status", "set", "debug", "network", "reset", "test", "syslog", "mqtt", "monitor", "weight", "temp", "temperature", "power", "adc", "lcd", "loglevel", "ota", nullptr
};

const char* const ALLOWED_SET_PARAMS[] = {
    "debug", "syslog", "mqtt", "mqtt_broker", "wifi_ssid", "loglevel", "interval", "heartbeat", "threshold", "calibration", nullptr
};