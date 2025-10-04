#include "system_error_manager.h"
#include "network_manager.h"

extern void debugPrintf(const char* fmt, ...);

void SystemErrorManager::begin() {
    // Initialize the error LED pin
    pinMode(SYSTEM_ERROR_LED_PIN, OUTPUT);
    digitalWrite(SYSTEM_ERROR_LED_PIN, LOW);
    
    // Initialize state
    activeErrors = 0;
    acknowledgedErrors = 0;
    ledState = false;
    lastBlinkTime = millis();
    errorStartTime = 0;
    
    debugPrintf("SystemErrorManager: Initialized with MIL on pin 9\n");
}

void SystemErrorManager::setError(SystemErrorType errorType, const char* description) {
    bool wasFirstError = (activeErrors == 0);
    
    // Set the error bit
    activeErrors |= errorType;
    
    // Record when first error occurred
    if (wasFirstError) {
        errorStartTime = millis();
    }
    
    // Generate description if not provided
    const char* errorDesc = description ? description : getErrorDescription(errorType);
    
    // Log to serial
    debugPrintf("SystemErrorManager: ERROR 0x%02X - %s\n", errorType, errorDesc);
    
    // Publish to MQTT
    publishError(errorType, errorDesc);
    
    // Update LED immediately
    updateLED();
}

void SystemErrorManager::clearError(SystemErrorType errorType) {
    if (activeErrors & errorType) {
        activeErrors &= ~errorType;
        acknowledgedErrors &= ~errorType;  // Clear acknowledgment too
        
        debugPrintf("SystemErrorManager: Cleared error 0x%02X\n", errorType);
        
        // If no more errors, reset error start time
        if (activeErrors == 0) {
            errorStartTime = 0;
        }
        
        // Update LED immediately
        updateLED();
    }
}

void SystemErrorManager::acknowledgeError(SystemErrorType errorType) {
    if (activeErrors & errorType) {
        acknowledgedErrors |= errorType;
        
        debugPrintf("SystemErrorManager: Acknowledged error 0x%02X\n", errorType);
        
        // Update LED (may change pattern)
        updateLED();
    }
}

void SystemErrorManager::clearAllErrors() {
    if (activeErrors != 0) {
        debugPrintf("SystemErrorManager: Clearing all errors\n");
        activeErrors = 0;
        acknowledgedErrors = 0;
        errorStartTime = 0;
        updateLED();
    }
}

void SystemErrorManager::update() {
    updateLED();
}

void SystemErrorManager::updateLED() {
    ErrorLedPattern pattern = getLedPattern();
    unsigned long currentTime = millis();
    
    switch (pattern) {
        case LED_OFF:
            digitalWrite(SYSTEM_ERROR_LED_PIN, LOW);
            ledState = false;
            break;
            
        case LED_SOLID:
            digitalWrite(SYSTEM_ERROR_LED_PIN, HIGH);
            ledState = true;
            break;
            
        case LED_SLOW_BLINK:
            // 0.25Hz blink (2000ms on, 2000ms off)
            if (currentTime - lastBlinkTime >= 2000) {
                ledState = !ledState;
                digitalWrite(SYSTEM_ERROR_LED_PIN, ledState ? HIGH : LOW);
                lastBlinkTime = currentTime;
            }
            break;
            
        case LED_FAST_BLINK:
            // 1Hz blink (500ms on, 500ms off)
            if (currentTime - lastBlinkTime >= 500) {
                ledState = !ledState;
                digitalWrite(SYSTEM_ERROR_LED_PIN, ledState ? HIGH : LOW);
                lastBlinkTime = currentTime;
            }
            break;
    }
}

ErrorLedPattern SystemErrorManager::getLedPattern() const {
    if (activeErrors == 0) {
        return LED_OFF;
    }
    
    // Check for critical errors (require fast blink)
    if (activeErrors & (ERROR_EEPROM_CRC | ERROR_MEMORY_LOW | ERROR_HARDWARE_FAULT)) {
        return LED_FAST_BLINK;
    }
    
    // Check for unacknowledged errors
    if (hasUnacknowledgedErrors()) {
        // Multiple unacknowledged errors get slow blink
        uint8_t unackedCount = __builtin_popcount(activeErrors & ~acknowledgedErrors);
        if (unackedCount > 1) {
            return LED_SLOW_BLINK;
        } else {
            return LED_SOLID;
        }
    }
    
    // All errors are acknowledged - slow blink to indicate they're still active
    return LED_SLOW_BLINK;
}

void SystemErrorManager::publishError(SystemErrorType errorType, const char* description) {
    if (networkManager && networkManager->isConnected()) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), "0x%02X: %s", errorType, description);
        networkManager->publish("r4/system/error", errorMsg);
        
        // Also publish error count
        char countMsg[32];
        snprintf(countMsg, sizeof(countMsg), "%d", __builtin_popcount(activeErrors));
        networkManager->publish("r4/system/error_count", countMsg);
    }
}

const char* SystemErrorManager::getErrorDescription(SystemErrorType errorType) {
    switch (errorType) {
        case ERROR_EEPROM_CRC:         return "EEPROM CRC validation failed";
        case ERROR_EEPROM_SAVE:        return "EEPROM save operation failed";
        case ERROR_SENSOR_FAULT:       return "Pressure sensor malfunction";
        case ERROR_NETWORK_PERSISTENT: return "Network connection persistently failed";
        case ERROR_CONFIG_INVALID:     return "Configuration parameters invalid";
        case ERROR_MEMORY_LOW:         return "Memory allocation issues";
        case ERROR_HARDWARE_FAULT:     return "General hardware fault";
        case ERROR_SEQUENCE_TIMEOUT:   return "Sequence operation timeout";
        default:                       return "Unknown error";
    }
}

void SystemErrorManager::getStatusString(char* buffer, size_t bufferSize) {
    if (activeErrors == 0) {
        snprintf(buffer, bufferSize, "No errors");
        return;
    }
    
    int errorCount = __builtin_popcount(activeErrors);
    int unackedCount = __builtin_popcount(activeErrors & ~acknowledgedErrors);
    unsigned long uptime = errorStartTime > 0 ? (millis() - errorStartTime) / 1000 : 0;
    
    snprintf(buffer, bufferSize, 
        "Errors: %d active (%d unacked), uptime: %lus, LED: %s",
        errorCount, unackedCount, uptime,
        getLedPattern() == LED_OFF ? "OFF" :
        getLedPattern() == LED_SOLID ? "SOLID" :
        getLedPattern() == LED_SLOW_BLINK ? "SLOW" : "FAST"
    );
}

void SystemErrorManager::listActiveErrors(char* buffer, size_t bufferSize) {
    if (activeErrors == 0) {
        snprintf(buffer, bufferSize, "No active errors");
        return;
    }
    
    size_t pos = 0;
    bool first = true;
    
    for (int i = 0; i < 8; i++) {
        SystemErrorType errorType = (SystemErrorType)(1 << i);
        if (activeErrors & errorType) {
            if (!first && pos < bufferSize - 3) {
                buffer[pos++] = ',';
                buffer[pos++] = ' ';
            }
            
            const char* desc = getErrorDescription(errorType);
            bool isAcked = (acknowledgedErrors & errorType) != 0;
            
            int written = snprintf(buffer + pos, bufferSize - pos, 
                "0x%02X:%s%s", errorType, isAcked ? "(ACK)" : "", desc);
            
            if (written > 0 && pos + written < bufferSize) {
                pos += written;
            } else {
                break;  // Buffer full
            }
            first = false;
        }
    }
    
    buffer[pos] = '\0';
}

int SystemErrorManager::getActiveErrorCount() const {
    int count = 0;
    uint8_t errors = activeErrors;
    
    // Count set bits using Brian Kernighan's algorithm
    while (errors) {
        errors &= errors - 1;  // Clear the lowest set bit
        count++;
    }
    
    return count;
}

const char* SystemErrorManager::getCurrentLedPatternString() const {
    ErrorLedPattern pattern = getLedPattern();
    
    switch (pattern) {
        case LED_OFF: return "OFF";
        case LED_SOLID: return "SOLID";
        case LED_SLOW_BLINK: return "SLOW";
        case LED_FAST_BLINK: return "FAST";
        default: return "UNKNOWN";
    }
}