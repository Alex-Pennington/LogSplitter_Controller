#include "constants.h"

// Command validation arrays
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "status", "set", "debug", "network", "reset", "test", "syslog", "mqtt", "monitor", "weight", "temp", "temperature", "lcd", "loglevel", nullptr
};

const char* const ALLOWED_SET_PARAMS[] = {
    "debug", "syslog", "mqtt", "interval", "heartbeat", "threshold", "calibration", nullptr
};