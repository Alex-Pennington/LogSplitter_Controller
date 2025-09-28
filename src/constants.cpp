#include "constants.h"

// Command validation arrays
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "pins", "set", "relay", "debug", nullptr
};

const char* const ALLOWED_SET_PARAMS[] = {
    "vref", "maxpsi", "gain", "offset", "filter", "emaalpha", 
    "pinmode", "seqstable", "seqstartstable", "seqtimeout", "debug", nullptr
};