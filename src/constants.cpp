#include "constants.h"

// Command validation arrays
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "pins", "pin", "set", "relay", "debug", "reset", "test", "loglevel", "heartbeat", "error", "bypass", "timing", "mesh_test", nullptr
};

const char* const ALLOWED_SET_PARAMS[] = {
    "vref", "maxpsi", "gain", "offset", "filter", "emaalpha", 
    "a1_maxpsi", "a1_gain", "a1_offset", "a1_vref",
    "a5_maxpsi", "a5_gain", "a5_offset", "a5_vref",
    "pinmode", "seqstable", "seqstartstable", "seqtimeout", "debug", "debugpins", "loglevel", nullptr
};