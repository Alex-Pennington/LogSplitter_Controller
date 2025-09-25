#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "constants.h"
#include "pressure_sensor.h"
#include "sequence_controller.h"
#include "relay_controller.h"

struct CalibrationData {
    uint32_t magic;
    float adcVref;
    float maxPressurePsi;
    float sensorGain;
    float sensorOffset;
    uint8_t filterMode;
    float emaAlpha;
    uint8_t pinModesBitmap;
    uint32_t seqStableMs;
    uint32_t seqStartStableMs;
    uint32_t seqTimeoutMs;
    bool relayEcho;
};

class ConfigManager {
private:
    CalibrationData config;
    bool configValid = false;
    
    // Pin mode configuration (true = NC, false = NO)
    bool pinIsNC[WATCH_PIN_COUNT] = {false};
    
    // Validation helpers
    bool isValidConfig(const CalibrationData& data);
    void setDefaults();
    
public:
    ConfigManager() = default;
    
    // Initialization
    void begin();
    
    // Load/Save
    bool load();
    bool save();
    
    // Pin mode configuration
    void setPinMode(uint8_t pin, bool isNC);
    bool getPinMode(uint8_t pin) const;
    bool isPinNC(size_t index) const { return pinIsNC[index]; }
    void setPinModeByIndex(size_t index, bool isNC) { if (index < WATCH_PIN_COUNT) pinIsNC[index] = isNC; }
    
    // Apply configuration to objects
    void applyToPressureSensor(PressureSensor& sensor);
    void applyToSequenceController(SequenceController& controller);
    void applyToRelayController(RelayController& relay);
    void loadFromPressureSensor(const PressureSensor& sensor);
    void loadFromSequenceController(const SequenceController& controller);
    void loadFromRelayController(const RelayController& relay);
    
    // Direct access for specific settings
    bool isConfigValid() const { return configValid; }
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
};