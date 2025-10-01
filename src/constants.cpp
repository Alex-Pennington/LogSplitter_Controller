#include "constants.h"

// Command validation arrays
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "pins", "set", "relay", "debug", "network", "reset", "test", "syslog", "loglevel", "heartbeat", nullptr
};

const char* const ALLOWED_SET_PARAMS[] = {
    "vref", "maxpsi", "gain", "offset", "filter", "emaalpha", 
    "a1_maxpsi", "a1_gain", "a1_offset", "a1_vref",
    "a5_maxpsi", "a5_gain", "a5_offset", "a5_vref",
    "pinmode", "seqstable", "seqstartstable", "seqtimeout", "debug", "loglevel", "syslog", nullptr
};