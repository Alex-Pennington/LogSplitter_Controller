#pragma once

#include <Arduino.h>
#include "constants.h"

// Forward declaration
class SystemErrorManager;

class RelayController {
private:
    // Relay states (index 1-9 used, 0 unused)
    bool relayStates[MAX_RELAYS + 1] = {false};
    bool boardPowered = false;
    
    // Communication
    bool echoEnabled = true;
    
    // Safety state
    bool safetyActive = false;
    
    // Response checking
    SystemErrorManager* errorManager = nullptr;
    unsigned long lastCommandTime = 0;
    bool waitingForResponse = false;
    uint8_t retryCount = 0;
    uint8_t lastRelayNumber = 0;
    bool lastRelayState = false;
    static const uint8_t MAX_RETRIES = 2;
    static const unsigned long RESPONSE_TIMEOUT_MS = 100;
    
    // Helper methods
    void sendCommand(uint8_t relayNumber, bool on);
    bool sendCommandWithRetry(uint8_t relayNumber, bool on);
    bool waitForOkResponse();
    void ensurePowerOn();
    
public:
    RelayController() = default;
    
    // Initialization
    void begin();
    
    // Main update (handles serial echo)
    void update();
    
    // Relay control
    void setRelay(uint8_t relayNumber, bool on);
    void setRelay(uint8_t relayNumber, bool on, bool isManualCommand);
    bool getRelayState(uint8_t relayNumber) const;
    void allRelaysOff();
    
    // Safety controls
    void enableSafety() { safetyActive = true; allRelaysOff(); }
    void disableSafety() { safetyActive = false; }
    bool isSafetyActive() const { return safetyActive; }
    
    // Configuration
    void setEcho(bool enabled) { echoEnabled = enabled; }
    bool getEcho() const { return echoEnabled; }
    void setErrorManager(SystemErrorManager* errorMgr) { errorManager = errorMgr; }
    
    // Power management
    void powerOn();
    void powerOff();
    bool isPowered() const { return boardPowered; }
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
    void publishIndividualValues();  // Publish individual MQTT values
    
    // Direct command processing (for MQTT/Serial commands)
    bool processCommand(const char* relayToken, const char* stateToken);
};