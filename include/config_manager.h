#pragma once

#include <Arduino.h>
#include <EEPROM.h>
#include "constants.h"
#include "pressure_sensor.h"
#include "sequence_controller.h"
#include "relay_controller.h"

// Forward declaration
class PressureManager;

struct CalibrationData {
    uint32_t magic;
    // Legacy single sensor parameters (kept for compatibility)
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
    bool debugEnabled;
    uint8_t logLevel;  // Log level (0-7, default 6=INFO)
    
    // Individual sensor parameters - A1 (System Pressure)
    float a1_maxPressurePsi;
    float a1_adcVref;
    float a1_sensorGain;
    float a1_sensorOffset;
    
    // Individual sensor parameters - A5 (Filter Pressure)
    float a5_maxPressurePsi;
    float a5_adcVref;
    float a5_sensorGain;
    float a5_sensorOffset;
    
    // CRC32 checksum (must be last field)
    uint32_t crc32;
};

class ConfigManager {
private:
    CalibrationData config;
    bool configValid = false;
    
    // Pin mode configuration (true = NC, false = NO)
    bool pinIsNC[WATCH_PIN_COUNT] = {false};
    
    // Network manager pointer for error publishing
    class NetworkManager* networkManager = nullptr;
    
    // System error manager pointer for error LED indication
    class SystemErrorManager* systemErrorManager = nullptr;
    
    // Validation helpers
    bool isValidConfig(const CalibrationData& data);
    void setDefaults();
    
    // CRC32 calculation helpers
    uint32_t calculateCRC32(const CalibrationData& data);
    bool validateCRC(const CalibrationData& data);
    void updateCRC(CalibrationData& data);
    void publishError(const char* errorMessage);
    
public:
    ConfigManager() = default;
    
    // Initialization
    void begin();
    void setNetworkManager(class NetworkManager* netMgr) { networkManager = netMgr; }
    void setSystemErrorManager(class SystemErrorManager* errorMgr) { systemErrorManager = errorMgr; }
    
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
    void applyToPressureManager(class PressureManager& manager);
    void applyToSequenceController(SequenceController& controller);
    void applyToRelayController(RelayController& relay);
    void applyToLogger();
    void loadFromPressureSensor(const PressureSensor& sensor);
    void loadFromSequenceController(const SequenceController& controller);
    void loadFromRelayController(const RelayController& relay);
    
    // Individual sensor configuration access
    float getA1MaxPressure() const { return config.a1_maxPressurePsi; }
    float getA1AdcVref() const { return config.a1_adcVref; }
    float getA1SensorGain() const { return config.a1_sensorGain; }
    float getA1SensorOffset() const { return config.a1_sensorOffset; }
    float getA5MaxPressure() const { return config.a5_maxPressurePsi; }
    float getA5AdcVref() const { return config.a5_adcVref; }
    float getA5SensorGain() const { return config.a5_sensorGain; }
    float getA5SensorOffset() const { return config.a5_sensorOffset; }
    
    void setA1MaxPressure(float val) { config.a1_maxPressurePsi = val; }
    void setA1AdcVref(float val) { config.a1_adcVref = val; }
    void setA1SensorGain(float val) { config.a1_sensorGain = val; }
    void setA1SensorOffset(float val) { config.a1_sensorOffset = val; }
    void setA5MaxPressure(float val) { config.a5_maxPressurePsi = val; }
    void setA5AdcVref(float val) { config.a5_adcVref = val; }
    void setA5SensorGain(float val) { config.a5_sensorGain = val; }
    void setA5SensorOffset(float val) { config.a5_sensorOffset = val; }
    
    // EMA Alpha configuration
    float getEmaAlpha() const { return config.emaAlpha; }
    void setEmaAlpha(float val) { config.emaAlpha = val; }
    
    // Debug configuration
    bool getDebugEnabled() const { return config.debugEnabled; }
    void setDebugEnabled(bool enabled) { config.debugEnabled = enabled; }
    
    // Log level configuration
    uint8_t getLogLevel() const { return config.logLevel; }
    void setLogLevel(uint8_t level) { config.logLevel = level; }
    
    // Direct access for specific settings
    bool isConfigValid() const { return configValid; }
    
    // MQTT Configuration Publishing
    void publishAllConfigParameters();
    bool queryMqttForDefaults(unsigned long timeoutMs = 10000);
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
};