#pragma once

#include <Arduino.h>
#include "constants.h"

class CommandValidator {
private:
    // Command validation helpers
    static bool isValidCommand(const char* cmd);
    static bool isValidSetParam(const char* param);
    static bool isValidRelayNumber(int num);
    static bool isValidRelayState(const char* state);
    
public:
    // Main validation functions
    static bool validateCommand(const char* command);
    static bool validateSetCommand(const char* param, const char* value);
    static bool validateRelayCommand(const char* relayToken, const char* stateToken);
    
    // Input sanitization
    static void sanitizeInput(char* input, size_t maxLength);
    static bool isAlphaNumeric(char c);
    
    // Rate limiting (simple implementation)
    static bool checkRateLimit();
};

// Command processor - handles validated commands
class CommandProcessor {
private:
    // System components (injected)
    class ConfigManager* configManager = nullptr;
    class PressureSensor* pressureSensor = nullptr;
    class PressureManager* pressureManager = nullptr;
    class SequenceController* sequenceController = nullptr;
    class RelayController* relayController = nullptr;
    class SafetySystem* safetySystem = nullptr;
    class SystemErrorManager* systemErrorManager = nullptr;
    class InputManager* inputManager = nullptr;
    class SubsystemTimingMonitor* timingMonitor = nullptr;
    
    // Command handlers
    void handleHelp(char* response, size_t responseSize, bool fromMqtt);
    void handleShow(char* response, size_t responseSize, bool fromMqtt);
    void handlePins(char* response, size_t responseSize, bool fromMqtt);
    void handlePin(char* param1, char* param2, char* param3, char* response, size_t responseSize);
    void handleSet(char* param, char* value, char* response, size_t responseSize);
    void handleRelay(char* relayToken, char* stateToken, char* response, size_t responseSize);
    void handleDebug(char* param, char* response, size_t responseSize);
    void handleNetwork(char* param, char* response, size_t responseSize);
    void handleBypass(char* param, char* response, size_t responseSize);
    void handleReset(char* param, char* response, size_t responseSize);
    void handleError(char* param, char* value, char* response, size_t responseSize);
    void handleSyslog(char* param, char* response, size_t responseSize);
    void handleMqtt(char* param, char* response, size_t responseSize);
    void handleLogLevel(const char* param, char* response, size_t responseSize);
    void handleTiming(char* param, char* response, size_t responseSize);
    
public:
    CommandProcessor() = default;
    
    // Dependency injection
    void setConfigManager(class ConfigManager* config) { configManager = config; }
    void setPressureSensor(class PressureSensor* pressure) { pressureSensor = pressure; }
    void setPressureManager(class PressureManager* pm) { pressureManager = pm; }
    void setSequenceController(class SequenceController* seq) { sequenceController = seq; }
    void setRelayController(class RelayController* relay) { relayController = relay; }
    void setSafetySystem(class SafetySystem* safety) { safetySystem = safety; }
    void setSystemErrorManager(class SystemErrorManager* errorMgr) { systemErrorManager = errorMgr; }
    void setInputManager(class InputManager* input) { inputManager = input; }
    void setTimingMonitor(class SubsystemTimingMonitor* timing) { timingMonitor = timing; }
    
    // Main processing function
    bool processCommand(char* commandBuffer, bool fromMqtt, char* response, size_t responseSize);
};