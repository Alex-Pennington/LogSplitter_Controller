#pragma once

#include <Arduino.h>
#include "constants.h"

class SafetySystem {
private:
    bool safetyActive = false;
    float lastPressure = 0.0f;
    unsigned long lastSafetyCheck = 0;
    
    // External dependencies (set by main)
    class RelayController* relayController = nullptr;
    class NetworkManager* networkManager = nullptr;
    class SequenceController* sequenceController = nullptr;
    
public:
    SafetySystem() = default;
    
    // Dependency injection
    void setRelayController(class RelayController* relay) { relayController = relay; }
    void setNetworkManager(class NetworkManager* network) { networkManager = network; }
    void setSequenceController(class SequenceController* seq) { sequenceController = seq; }
    
    // Safety checks
    void update(float currentPressure);
    void checkPressure(float pressure);
    void emergencyStop(const char* reason);
    
    // Manual control
    void activate(const char* reason = "manual");
    void deactivate();
    
    // Status
    bool isActive() const { return safetyActive; }
    void getStatusString(char* buffer, size_t bufferSize);
};