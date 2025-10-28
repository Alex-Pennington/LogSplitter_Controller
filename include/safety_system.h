#pragma once

#include <Arduino.h>
#include "constants.h"

class SafetySystem {
private:
    bool safetyActive = false;
    bool engineStopped = false;
    bool eStopActive = false;  // Tracks E-stop button state
    float lastPressure = 0.0f;
    unsigned long lastSafetyCheck = 0;
    
    // Pin 11 safety status LED control
    bool safetyStatusLedState = false;      // Current pin 11 state
    bool pressureLimitUsedPersistent = false; // Persistent flag until power reset
    unsigned long lastSafetyStatusBlink = 0;
    const unsigned long SAFETY_STATUS_BLINK_INTERVAL_MS = 500; // 1Hz flashing
    
    // Time-based pressure monitoring for E-stop activation
    bool highPressureActive = false;
    unsigned long highPressureStartTime = 0;
    const unsigned long HIGH_PRESSURE_TIMEOUT_MS = 10000; // 10 seconds
    const float HIGH_PRESSURE_ESTOP_THRESHOLD = 2300.0f;  // PSI threshold for E-stop
    
    // External dependencies (set by main) - networking removed
    class RelayController* relayController = nullptr;
    class SequenceController* sequenceController = nullptr;
    
public:
    SafetySystem() = default;
    
    // Initialization
    void begin();
    
    // Dependency injection - networking removed
    void setRelayController(class RelayController* relay) { relayController = relay; }
    void setSequenceController(class SequenceController* seq) { sequenceController = seq; }
    
    // Safety checks
    void update(float currentPressure);
    void checkPressure(float pressure);
    void checkPressure(float pressure, bool atLimitSwitch);
    void emergencyStop(const char* reason);
    void activateEStop();  // E-stop specific activation
    void clearEStop();     // E-stop specific clearing
    
    // Manual control
    void activate(const char* reason = "manual");
    void deactivate();
    void clearEmergencyStop();
    void setEngineStop(bool stop);  // For testing
    
    // Status
    bool isActive() const { return safetyActive; }
    bool isEngineStopped() const { return engineStopped; }
    bool isEStopActive() const { return eStopActive; }
    void getStatusString(char* buffer, size_t bufferSize);
    void publishIndividualValues();  // Publish individual MQTT values
    
    // Safety status LED control (pin 11)
    void markPressureLimitUsed();  // Mark that pressure limits were used (persistent until power reset)
    void updateSafetyStatusLed();  // Update pin 11 based on current state
};