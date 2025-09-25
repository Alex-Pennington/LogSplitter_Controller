#include "command_processor.h"
#include "config_manager.h"
#include "pressure_sensor.h"
#include "sequence_controller.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "safety_system.h"

// Static data for rate limiting
static unsigned long lastCommandTime = 0;
static const unsigned long COMMAND_RATE_LIMIT_MS = 100; // 10 commands/second max

// ============================================================================
// CommandValidator Implementation
// ============================================================================

bool CommandValidator::isValidCommand(const char* cmd) {
    if (!cmd || strlen(cmd) == 0 || strlen(cmd) > MAX_CMD_LENGTH) return false;
    
    // Check against whitelist
    for (int i = 0; ALLOWED_COMMANDS[i] != nullptr; i++) {
        if (strcasecmp(cmd, ALLOWED_COMMANDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool CommandValidator::isValidSetParam(const char* param) {
    if (!param || strlen(param) == 0) return false;
    
    for (int i = 0; ALLOWED_SET_PARAMS[i] != nullptr; i++) {
        if (strcasecmp(param, ALLOWED_SET_PARAMS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool CommandValidator::isValidRelayNumber(int num) {
    return (num >= 1 && num <= MAX_RELAYS);
}

bool CommandValidator::isValidRelayState(const char* state) {
    return (strcasecmp(state, "ON") == 0 || strcasecmp(state, "OFF") == 0);
}

bool CommandValidator::validateCommand(const char* command) {
    return isValidCommand(command);
}

bool CommandValidator::validateSetCommand(const char* param, const char* value) {
    if (!isValidSetParam(param) || !value) return false;
    
    // Additional parameter-specific validation
    if (strcasecmp(param, "vref") == 0) {
        float val = atof(value);
        return (val > 0.0f && val <= 5.0f);
    }
    
    if (strcasecmp(param, "maxpsi") == 0) {
        float val = atof(value);
        return (val > 0.0f && val <= 10000.0f);
    }
    
    if (strcasecmp(param, "gain") == 0) {
        float val = atof(value);
        return (val > 0.0f && val <= 100.0f);
    }
    
    if (strcasecmp(param, "emaalpha") == 0) {
        float val = atof(value);
        return (val > 0.0f && val <= 1.0f);
    }
    
    if (strcasecmp(param, "filter") == 0) {
        return (strcasecmp(value, "none") == 0 || 
                strcasecmp(value, "median3") == 0 || 
                strcasecmp(value, "ema") == 0);
    }
    
    return true; // Allow other parameters with basic validation
}

bool CommandValidator::validateRelayCommand(const char* relayToken, const char* stateToken) {
    if (!relayToken || !stateToken) return false;
    
    // Check relay token format
    if ((relayToken[0] != 'R' && relayToken[0] != 'r') || strlen(relayToken) < 2) {
        return false;
    }
    
    int num = atoi(relayToken + 1);
    return isValidRelayNumber(num) && isValidRelayState(stateToken);
}

void CommandValidator::sanitizeInput(char* input, size_t maxLength) {
    if (!input) return;
    
    size_t len = strlen(input);
    if (len > maxLength) {
        input[maxLength] = '\0';
        len = maxLength;
    }
    
    // Remove any non-printable characters
    for (size_t i = 0; i < len; i++) {
        if (input[i] < 32 || input[i] > 126) {
            input[i] = ' ';
        }
    }
}

bool CommandValidator::isAlphaNumeric(char c) {
    return (c >= 'a' && c <= 'z') || 
           (c >= 'A' && c <= 'Z') || 
           (c >= '0' && c <= '9');
}

bool CommandValidator::checkRateLimit() {
    unsigned long now = millis();
    if (now - lastCommandTime < COMMAND_RATE_LIMIT_MS) {
        return false; // Rate limited
    }
    lastCommandTime = now;
    return true;
}

// ============================================================================
// CommandProcessor Implementation
// ============================================================================

void CommandProcessor::handleHelp(char* response, size_t responseSize, bool fromMqtt) {
    const char* helpText = "Commands: help, show, pins";
    if (fromMqtt) {
        snprintf(response, responseSize, "%s, set <param> <val>, relay R<n> ON|OFF", helpText);
    } else {
        snprintf(response, responseSize, "%s (+ pins command available in serial)", helpText);
    }
}

void CommandProcessor::handleShow(char* response, size_t responseSize, bool fromMqtt) {
    // Build comprehensive status
    char pressureStatus[64] = "";
    char sequenceStatus[64] = "";
    char relayStatus[64] = "";
    char safetyStatus[32] = "";
    
    if (pressureSensor) {
        pressureSensor->getStatusString(pressureStatus, sizeof(pressureStatus));
    }
    
    if (sequenceController) {
        sequenceController->getStatusString(sequenceStatus, sizeof(sequenceStatus));
    }
    
    if (relayController) {
        relayController->getStatusString(relayStatus, sizeof(relayStatus));
    }
    
    if (safetySystem) {
        safetySystem->getStatusString(safetyStatus, sizeof(safetyStatus));
    }
    
    snprintf(response, responseSize, "%s %s %s %s", 
        pressureStatus, sequenceStatus, relayStatus, safetyStatus);
}

void CommandProcessor::handlePins(char* response, size_t responseSize, bool fromMqtt) {
    if (fromMqtt) {
        snprintf(response, responseSize, "pins command only available via serial");
        return;
    }
    
    // This will be printed directly to Serial, not stored in response
    Serial.println("=== Pin Configuration ===");
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        bool isNC = configManager ? configManager->isPinNC(i) : false;
        uint8_t pin = WATCH_PINS[i];
        
        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(": ");
        
        // Add function labels for known pins
        if (pin == LIMIT_EXTEND_PIN) {
            Serial.print("EXTEND_LIMIT ");
        } else if (pin == LIMIT_RETRACT_PIN) {
            Serial.print("RETRACT_LIMIT ");
        } else if (pin == 2 || pin == 3) {
            Serial.print("MANUAL_CTRL ");
        } else if (pin == 4 || pin == 5) {
            Serial.print("SEQUENCE_CTRL ");
        }
        
        Serial.print("mode=");
        Serial.print(isNC ? "NC" : "NO");
        Serial.println();
    }
    
    Serial.println("\nUsage: set pinmode <pin> <NO|NC>");
    Serial.println("Example: set pinmode 6 NC  (set extend limit to normally closed)");
    
    response[0] = '\0'; // No MQTT response
}

void CommandProcessor::handleSet(char* param, char* value, char* response, size_t responseSize) {
    if (!param || !value) {
        snprintf(response, responseSize, "Usage: set <param> <value>");
        return;
    }
    
    if (strcasecmp(param, "vref") == 0) {
        float val = atof(value);
        if (pressureSensor) pressureSensor->setAdcVref(val);
        if (configManager) {
            configManager->loadFromPressureSensor(*pressureSensor);
            configManager->save();
        }
        snprintf(response, responseSize, "vref set %.6f", val);
    }
    else if (strcasecmp(param, "maxpsi") == 0) {
        float val = atof(value);
        if (pressureSensor) pressureSensor->setMaxPressure(val);
        if (configManager) {
            configManager->loadFromPressureSensor(*pressureSensor);
            configManager->save();
        }
        snprintf(response, responseSize, "maxpsi set %.2f", val);
    }
    else if (strcasecmp(param, "gain") == 0) {
        float val = atof(value);
        if (pressureSensor) pressureSensor->setSensorGain(val);
        if (configManager) {
            configManager->loadFromPressureSensor(*pressureSensor);
            configManager->save();
        }
        snprintf(response, responseSize, "gain set %.6f", val);
    }
    else if (strcasecmp(param, "offset") == 0) {
        float val = atof(value);
        if (pressureSensor) pressureSensor->setSensorOffset(val);
        if (configManager) {
            configManager->loadFromPressureSensor(*pressureSensor);
            configManager->save();
        }
        snprintf(response, responseSize, "offset set %.6f", val);
    }
    else if (strcasecmp(param, "filter") == 0) {
        FilterMode mode = FILTER_NONE;
        if (strcasecmp(value, "median3") == 0) mode = FILTER_MEDIAN3;
        else if (strcasecmp(value, "ema") == 0) mode = FILTER_EMA;
        
        if (pressureSensor) pressureSensor->setFilterMode(mode);
        if (configManager) {
            configManager->loadFromPressureSensor(*pressureSensor);
            configManager->save();
        }
        snprintf(response, responseSize, "filter set %s", value);
    }
    else if (strcasecmp(param, "seqstable") == 0) {
        unsigned long val = strtoul(value, NULL, 10);
        if (sequenceController) sequenceController->setStableTime(val);
        if (configManager) {
            configManager->loadFromSequenceController(*sequenceController);
            configManager->save();
        }
        snprintf(response, responseSize, "seqStableMs set %lu", val);
    }
    else if (strcasecmp(param, "seqstartstable") == 0) {
        unsigned long val = strtoul(value, NULL, 10);
        if (sequenceController) sequenceController->setStartStableTime(val);
        if (configManager) {
            configManager->loadFromSequenceController(*sequenceController);
            configManager->save();
        }
        snprintf(response, responseSize, "seqStartStableMs set %lu", val);
    }
    else if (strcasecmp(param, "seqtimeout") == 0) {
        unsigned long val = strtoul(value, NULL, 10);
        if (sequenceController) sequenceController->setTimeout(val);
        if (configManager) {
            configManager->loadFromSequenceController(*sequenceController);
            configManager->save();
        }
        snprintf(response, responseSize, "seqTimeoutMs set %lu", val);
    }
    else if (strcasecmp(param, "emaalpha") == 0) {
        float val = atof(value);
        if (val > 0.0f && val <= 1.0f) {
            // Note: This would need PressureManager integration to work properly
            snprintf(response, responseSize, "emaAlpha set %.3f (Note: requires PressureManager integration)", val);
        } else {
            snprintf(response, responseSize, "emaAlpha must be between 0.0 and 1.0");
        }
    }
    else if (strcasecmp(param, "pinmode") == 0) {
        // Parse format: "pinmode 6 NC" or "pinmode 7 NO"
        char* pinStr = value;
        char* modeStr = strchr(value, ' ');
        
        if (!modeStr) {
            snprintf(response, responseSize, "Usage: set pinmode <pin> <NO|NC>");
            return;
        }
        
        *modeStr = '\0'; // Split the string
        modeStr++; // Move to mode part
        
        uint8_t pin = atoi(pinStr);
        bool isNC = (strcasecmp(modeStr, "NC") == 0);
        bool isNO = (strcasecmp(modeStr, "NO") == 0);
        
        if (!isNC && !isNO) {
            snprintf(response, responseSize, "Mode must be NO or NC");
            return;
        }
        
        if (configManager) {
            configManager->setPinMode(pin, isNC);
            configManager->save();
            snprintf(response, responseSize, "Pin %d set to %s", pin, isNC ? "NC" : "NO");
        } else {
            snprintf(response, responseSize, "Config manager not available");
        }
    }
    // Add more parameter handlers as needed
    else {
        snprintf(response, responseSize, "unknown parameter %s", param);
    }
}

void CommandProcessor::handleRelay(char* relayToken, char* stateToken, char* response, size_t responseSize) {
    if (relayController && relayController->processCommand(relayToken, stateToken)) {
        snprintf(response, responseSize, "relay %s %s", relayToken, stateToken);
    } else {
        snprintf(response, responseSize, "relay command failed");
    }
}

bool CommandProcessor::processCommand(char* commandBuffer, bool fromMqtt, char* response, size_t responseSize) {
    // Initialize response
    response[0] = '\0';
    
    // Rate limiting
    if (!CommandValidator::checkRateLimit()) {
        snprintf(response, responseSize, "rate limited");
        return false;
    }
    
    // Sanitize input
    CommandValidator::sanitizeInput(commandBuffer, COMMAND_BUFFER_SIZE - 1);
    
    // Tokenize command
    char* cmd = strtok(commandBuffer, " ");
    if (!cmd) {
        snprintf(response, responseSize, "empty command");
        return false;
    }
    
    // Validate command
    if (!CommandValidator::validateCommand(cmd)) {
        snprintf(response, responseSize, "invalid command: %s", cmd);
        return false;
    }
    
    // Handle specific commands
    if (strcasecmp(cmd, "help") == 0) {
        handleHelp(response, responseSize, fromMqtt);
    }
    else if (strcasecmp(cmd, "show") == 0) {
        handleShow(response, responseSize, fromMqtt);
    }
    else if (strcasecmp(cmd, "pins") == 0) {
        handlePins(response, responseSize, fromMqtt);
    }
    else if (strcasecmp(cmd, "set") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        
        if (CommandValidator::validateSetCommand(param, value)) {
            handleSet(param, value, response, responseSize);
        } else {
            snprintf(response, responseSize, "invalid set command");
        }
    }
    else if (strcasecmp(cmd, "relay") == 0) {
        char* relayToken = strtok(NULL, " ");
        char* stateToken = strtok(NULL, " ");
        
        if (CommandValidator::validateRelayCommand(relayToken, stateToken)) {
            handleRelay(relayToken, stateToken, response, responseSize);
        } else {
            snprintf(response, responseSize, "invalid relay command");
        }
    }
    else {
        snprintf(response, responseSize, "unknown command: %s", cmd);
        return false;
    }
    
    return true;
}