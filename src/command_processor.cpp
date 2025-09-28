#include "command_processor.h"
#include "config_manager.h"
#include "pressure_sensor.h"
#include "pressure_manager.h"
#include "sequence_controller.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "safety_system.h"
#include "system_error_manager.h"
#include <ctype.h>

// External debug flag
extern bool g_debugEnabled;

// External E-Stop state variables
extern bool g_emergencyStopActive;
extern bool g_emergencyStopLatched;
extern SystemState currentSystemState;

// Static data for rate limiting
static unsigned long lastCommandTime = 0;
static const unsigned long COMMAND_RATE_LIMIT_MS = 50; // 20 commands/second max

// ============================================================================
// CommandValidator Implementation
// ============================================================================

bool CommandValidator::isValidCommand(const char* cmd) {
    if (!cmd || strlen(cmd) == 0 || strlen(cmd) > MAX_CMD_LENGTH) return false;
    
    // Allow shorthand relay commands like "R1 ON" without leading 'relay'
    if ((cmd[0] == 'R' || cmd[0] == 'r') && isdigit((unsigned char)cmd[1])) {
        return true;
    }

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
    if (strcasecmp(param, "vref") == 0 ||
        strcasecmp(param, "a1_vref") == 0 ||
        strcasecmp(param, "a5_vref") == 0) {
        float val = atof(value);
        return (val > 0.0f && val <= 5.0f);
    }
    
    if (strcasecmp(param, "maxpsi") == 0 || 
        strcasecmp(param, "a1_maxpsi") == 0 || 
        strcasecmp(param, "a5_maxpsi") == 0) {
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
    const char* helpText = "Commands: help, show, debug, network, reset, error";
    if (!fromMqtt) {
        snprintf(response, responseSize, "%s, pins, set <param> <val>, relay R<n> ON|OFF", helpText);
    } else {
        snprintf(response, responseSize, "%s, set <param> <val>, relay R<n> ON|OFF", helpText);
    }
}

void CommandProcessor::handleShow(char* response, size_t responseSize, bool fromMqtt) {
    // Build compact status line in stable key=value groups.
    // Order is intentionally fixed for downstream parsers: pressures, sequence, relays, safety.
    // Example:
    //   hydraulic=1234.5 hydraulic_oil=1180.2 seq=IDLE stage=NONE relays=1:ON,2:OFF safe=OK
    char pressureStatus[64] = "";
    char sequenceStatus[64] = "";
    char relayStatus[64] = "";
    char safetyStatus[32] = "";
    
    if (pressureManager) {
        pressureManager->getStatusString(pressureStatus, sizeof(pressureStatus));
    } else if (pressureSensor) {
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
    
    char errorStatus[32] = "errors=0";
    if (systemErrorManager) {
        int errorCount = systemErrorManager->getActiveErrorCount();
        const char* ledPattern = systemErrorManager->getCurrentLedPatternString();
        snprintf(errorStatus, sizeof(errorStatus), "errors=%d led=%s", errorCount, ledPattern);
    }
    
    snprintf(response, responseSize, "%s %s %s %s %s", 
        pressureStatus, sequenceStatus, relayStatus, safetyStatus, errorStatus);
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
    // Individual sensor A1 (system pressure) configuration
    else if (strcasecmp(param, "a1_maxpsi") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC).setMaxPressure(val);
            snprintf(response, responseSize, "A1 maxpsi set %.2f", val);
        } else {
            snprintf(response, responseSize, "A1 maxpsi failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a1_vref") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC).setAdcVref(val);
            snprintf(response, responseSize, "A1 vref set %.6f", val);
        } else {
            snprintf(response, responseSize, "A1 vref failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a1_gain") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC).setSensorGain(val);
            snprintf(response, responseSize, "A1 gain set %.6f", val);
        } else {
            snprintf(response, responseSize, "A1 gain failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a1_offset") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC).setSensorOffset(val);
            snprintf(response, responseSize, "A1 offset set %.6f", val);
        } else {
            snprintf(response, responseSize, "A1 offset failed: PressureManager not available");
        }
    }
    // Individual sensor A5 (filter pressure) configuration
    else if (strcasecmp(param, "a5_maxpsi") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC_OIL).setMaxPressure(val);
            snprintf(response, responseSize, "A5 maxpsi set %.2f", val);
        } else {
            snprintf(response, responseSize, "A5 maxpsi failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a5_vref") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC_OIL).setAdcVref(val);
            snprintf(response, responseSize, "A5 vref set %.6f", val);
        } else {
            snprintf(response, responseSize, "A5 vref failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a5_gain") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC_OIL).setSensorGain(val);
            snprintf(response, responseSize, "A5 gain set %.6f", val);
        } else {
            snprintf(response, responseSize, "A5 gain failed: PressureManager not available");
        }
    }
    else if (strcasecmp(param, "a5_offset") == 0) {
        float val = atof(value);
        if (pressureManager) {
            pressureManager->getSensor(SENSOR_HYDRAULIC_OIL).setSensorOffset(val);
            snprintf(response, responseSize, "A5 offset set %.6f", val);
        } else {
            snprintf(response, responseSize, "A5 offset failed: PressureManager not available");
        }
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
        // Accept both "set pinmode 6 NC" (separate tokens) and "set pinmode \"6 NC\""
        char* pinStr = value;
        char* modeStr = strchr(value, ' ');
        if (!modeStr) {
            // Try to fetch next token from the tokenizer stream (works for serial/MQTT)
            char* nextToken = strtok(NULL, " ");
            if (nextToken) {
                modeStr = nextToken;
            }
        } else {
            *modeStr = '\0'; // Split the string
            modeStr++; // Move to mode part
        }
        
        if (!modeStr) {
            snprintf(response, responseSize, "Usage: set pinmode <pin> <NO|NC>");
            return;
        }
        
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
    else if (strcasecmp(param, "debug") == 0) {
        if (strcasecmp(value, "on") == 0 || strcasecmp(value, "1") == 0) {
            g_debugEnabled = true;
            snprintf(response, responseSize, "debug ON");
        } else if (strcasecmp(value, "off") == 0 || strcasecmp(value, "0") == 0) {
            g_debugEnabled = false;
            snprintf(response, responseSize, "debug OFF");
        } else {
            snprintf(response, responseSize, "debug value must be ON|OFF|1|0");
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

void CommandProcessor::handleDebug(char* param, char* response, size_t responseSize) {
    if (!param) {
        // Show current debug status
        snprintf(response, responseSize, "debug %s", g_debugEnabled ? "ON" : "OFF");
        return;
    }
    
    if (strcasecmp(param, "on") == 0 || strcasecmp(param, "1") == 0) {
        g_debugEnabled = true;
        snprintf(response, responseSize, "debug ON");
        Serial.println("Debug output enabled");
    } else if (strcasecmp(param, "off") == 0 || strcasecmp(param, "0") == 0) {
        g_debugEnabled = false;
        snprintf(response, responseSize, "debug OFF");
        Serial.println("Debug output disabled");
    } else {
        snprintf(response, responseSize, "usage: debug [ON|OFF]");
    }
}

void CommandProcessor::handleNetwork(char* response, size_t responseSize) {
    if (networkManager) {
        networkManager->getHealthString(response, responseSize);
    } else {
        snprintf(response, responseSize, "network manager not available");
    }
}

void CommandProcessor::handleReset(char* param, char* response, size_t responseSize) {
    if (!param) {
        snprintf(response, responseSize, "usage: reset estop");
        return;
    }
    
    if (strcasecmp(param, "estop") == 0) {
        // Only allow E-Stop reset if E-Stop button is not currently pressed
        if (g_emergencyStopActive) {
            snprintf(response, responseSize, "E-Stop reset failed: E-Stop button still pressed");
            return;
        }
        
        if (g_emergencyStopLatched) {
            g_emergencyStopLatched = false;
            currentSystemState = SYS_RUNNING;
            
            // Notify safety system of reset
            if (safetySystem) {
                safetySystem->clearEmergencyStop();
            }
            
            snprintf(response, responseSize, "E-Stop reset successful - system operational");
            Serial.println("E-Stop reset: System restored to operational state");
        } else {
            snprintf(response, responseSize, "E-Stop not latched - no reset needed");
        }
    } else {
        snprintf(response, responseSize, "unknown reset parameter: %s", param);
    }
}

void CommandProcessor::handleError(char* param, char* value, char* response, size_t responseSize) {
    if (!systemErrorManager) {
        snprintf(response, responseSize, "error manager not available");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "usage: error list|ack <code>|clear");
        return;
    }
    
    if (strcasecmp(param, "list") == 0) {
        if (systemErrorManager->hasErrors()) {
            systemErrorManager->listActiveErrors(response, responseSize);
        } else {
            snprintf(response, responseSize, "no active errors");
        }
    }
    else if (strcasecmp(param, "ack") == 0) {
        if (!value) {
            snprintf(response, responseSize, "usage: error ack <error_code>");
            return;
        }
        
        // Parse error code (hex format like 0x01)
        uint8_t errorCode = 0;
        if (strncasecmp(value, "0x", 2) == 0) {
            errorCode = (uint8_t)strtol(value, NULL, 16);
        } else {
            errorCode = (uint8_t)strtol(value, NULL, 10);
        }
        
        if (errorCode == 0 || (errorCode & 0x7F) == 0) {
            snprintf(response, responseSize, "invalid error code: %s", value);
            return;
        }
        
        SystemErrorType errorType = (SystemErrorType)errorCode;
        if (systemErrorManager->hasError(errorType)) {
            systemErrorManager->acknowledgeError(errorType);
            snprintf(response, responseSize, "error 0x%02X acknowledged", errorCode);
        } else {
            snprintf(response, responseSize, "error 0x%02X not active", errorCode);
        }
    }
    else if (strcasecmp(param, "clear") == 0) {
        systemErrorManager->clearAllErrors();
        snprintf(response, responseSize, "all errors cleared");
    }
    else {
        snprintf(response, responseSize, "unknown error command: %s", param);
    }
}

bool CommandProcessor::processCommand(char* commandBuffer, bool fromMqtt, char* response, size_t responseSize) {
    // Initialize response
    response[0] = '\0';
    
    // Rate limiting (more permissive for MQTT)
    if (!fromMqtt) {
        if (!CommandValidator::checkRateLimit()) {
            snprintf(response, responseSize, "rate limited");
            return false;
        }
    } else {
        // For MQTT, apply a looser window: only block if more than 10 msgs in rapid succession
        if (!CommandValidator::checkRateLimit()) {
            // Don't hard-fail; annotate response but continue parsing
            // This helps avoid dropping legitimate batched MQTT inputs
            // response may be overwritten by a valid command handler
            snprintf(response, responseSize, "rate_warn");
        }
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
    // Support shorthand relay control: "R1 ON" style (works for both Serial & MQTT)
    else if ((cmd[0] == 'R' || cmd[0] == 'r') && cmd[1] && (cmd[1] >= '0' && cmd[1] <= '9')) {
        char* stateToken = strtok(NULL, " ");
        if (!stateToken) {
            snprintf(response, responseSize, "usage: R<n> ON|OFF");
            return false;
        }
        if (CommandValidator::validateRelayCommand(cmd, stateToken)) {
            handleRelay(cmd, stateToken, response, responseSize);
            return true;
        } else {
            snprintf(response, responseSize, "invalid relay command");
            return false;
        }
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
    else if (strcasecmp(cmd, "debug") == 0) {
        char* param = strtok(NULL, " ");
        handleDebug(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "network") == 0) {
        handleNetwork(response, responseSize);
    }
    else if (strcasecmp(cmd, "reset") == 0) {
        char* param = strtok(NULL, " ");
        handleReset(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "error") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        handleError(param, value, response, responseSize);
    }
    else {
        snprintf(response, responseSize, "unknown command: %s", cmd);
        return false;
    }
    
    return true;
}