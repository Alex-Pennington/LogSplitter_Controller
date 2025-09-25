#pragma once

#include <Arduino.h>
#include "constants.h"

class RelayController {
private:
    // Relay states (index 1-9 used, 0 unused)
    bool relayStates[MAX_RELAYS + 1] = {false};
    bool boardPowered = false;
    
    // Communication
    bool echoEnabled = true;
    
    // Safety state
    bool safetyActive = false;
    
    // Helper methods
    void sendCommand(uint8_t relayNumber, bool on);
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
    
    // Power management
    void powerOn();
    void powerOff();
    bool isPowered() const { return boardPowered; }
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
    
    // Direct command processing (for MQTT/Serial commands)
    bool processCommand(const char* relayToken, const char* stateToken);
};