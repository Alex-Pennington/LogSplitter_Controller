#pragma once

#include <Arduino.h>
#include "constants.h"
#include "telemetry_manager.h"

// NetworkManager removed - non-networking version

// System error types
enum SystemErrorType {
    ERROR_NONE = 0x00,
    ERROR_EEPROM_CRC = 0x01,           // EEPROM CRC validation failed
    ERROR_EEPROM_SAVE = 0x02,          // EEPROM save operation failed
    ERROR_SENSOR_FAULT = 0x04,         // Pressure sensor malfunction
    ERROR_NETWORK_PERSISTENT = 0x08,   // Network connection persistently failed
    ERROR_CONFIG_INVALID = 0x10,       // Configuration parameters invalid
    ERROR_MEMORY_LOW = 0x20,           // Memory allocation issues
    ERROR_HARDWARE_FAULT = 0x40,       // General hardware fault
    ERROR_SEQUENCE_TIMEOUT = 0x80      // Sequence operation timeout
};

// LED blink patterns
enum ErrorLedPattern {
    LED_OFF = 0,                // No error - LED off
    LED_SOLID = 1,              // Single error - solid on
    LED_SLOW_BLINK = 2,         // Multiple errors - slow blink (0.25Hz)
    LED_FAST_BLINK = 3          // Critical errors - fast blink (1Hz)
};

class SystemErrorManager {
private:
    uint8_t activeErrors = 0;           // Bitmask of active errors
    uint8_t acknowledgedErrors = 0;     // Bitmask of acknowledged errors
    unsigned long lastBlinkTime = 0;    // Last LED blink timestamp
    bool ledState = false;              // Current mill lamp state (pin 9)
    bool builtinLedState = false;       // Current LED_BUILTIN state (pin 13)
    unsigned long errorStartTime = 0;   // When first error occurred
    
    // Network manager removed - non-networking version
    
    // LED control
    void updateLED();
    ErrorLedPattern getLedPattern() const;
    
    // Error reporting
    void publishError(SystemErrorType errorType, const char* description);
    const char* getErrorDescription(SystemErrorType errorType);
    Telemetry::ErrorSeverity getSeverityFromErrorType(SystemErrorType errorType);
    
public:
    SystemErrorManager() = default;
    
    // Initialization
    void begin();
    // setNetworkManager removed - non-networking version
    
    // Error management
    void setError(SystemErrorType errorType, const char* description = nullptr);
    void clearError(SystemErrorType errorType);
    void acknowledgeError(SystemErrorType errorType);
    void clearAllErrors();
    
    // Status checking
    bool hasErrors() const { return activeErrors != 0; }
    bool hasUnacknowledgedErrors() const { return (activeErrors & ~acknowledgedErrors) != 0; }
    uint8_t getActiveErrors() const { return activeErrors; }
    bool hasError(SystemErrorType errorType) const { return (activeErrors & errorType) != 0; }
    
    // Update function (call in main loop)
    void update();
    
    // Status reporting
    void getStatusString(char* buffer, size_t bufferSize);
    void listActiveErrors(char* buffer, size_t bufferSize);
    int getActiveErrorCount() const;
    const char* getCurrentLedPatternString() const;
};