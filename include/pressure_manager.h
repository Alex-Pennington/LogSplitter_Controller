#pragma once

#include <Arduino.h>
#include "constants.h"

enum PressureSensorType {
    SENSOR_HYDRAULIC,     // Main hydraulic system (A1)
    SENSOR_HYDRAULIC_OIL, // Hydraulic oil pressure (A4)
    SENSOR_COUNT
};

class PressureSensorChannel {
private:
    // Configuration
    uint8_t analogPin;
    float maxPressurePsi;
    float adcVref = DEFAULT_ADC_VREF;
    
    // Sampling buffer
    uint16_t samples[SAMPLE_WINDOW_COUNT];
    size_t sampleIndex = 0;
    size_t samplesFilled = 0;
    unsigned long samplesSum = 0;
    unsigned long lastSampleTime = 0;
    
    // Filtering
    FilterMode filterMode = FILTER_MEDIAN3;
    float emaValue = 0.0f;
    float emaAlpha = 0.2f;
    
    // Current values
    float currentPressure = 0.0f;
    
    // Helper methods
    int applyFilter(int rawValue);
    void updateSample(int filteredValue);
    float computeAverageCount();
    float voltageToPressure(float voltage);
    
public:
    PressureSensorChannel() = default;
    
    // Initialization
    void begin(uint8_t pin, float maxPsi);
    
    // Main update (call regularly)
    void update();
    
    // Configuration
    void setMaxPressure(float maxPsi) { maxPressurePsi = maxPsi; }
    void setFilterMode(FilterMode mode) { filterMode = mode; if (mode == FILTER_EMA) emaValue = 0.0f; }
    void setEmaAlpha(float alpha) { if (alpha > 0.0f && alpha <= 1.0f) emaAlpha = alpha; }
    
    // Current readings
    float getPressure() const { return currentPressure; }
    bool isReady() const { return samplesFilled > 0; }
    uint8_t getPin() const { return analogPin; }
    float getMaxPressure() const { return maxPressurePsi; }
};

class PressureManager {
private:
    PressureSensorChannel sensors[SENSOR_COUNT];
    unsigned long lastPublishTime = 0;
    const unsigned long publishInterval = 1000; // 1 second
    
    // External dependencies
    class NetworkManager* networkManager = nullptr;
    
public:
    PressureManager() = default;
    
    // Initialization
    void begin();
    
    // Dependency injection
    void setNetworkManager(class NetworkManager* network) { networkManager = network; }
    
    // Main update (call regularly)
    void update();
    
    // Individual sensor access
    float getHydraulicPressure() const { return sensors[SENSOR_HYDRAULIC].getPressure(); }
    float getHydraulicOilPressure() const { return sensors[SENSOR_HYDRAULIC_OIL].getPressure(); }
    
    // Sensor channel access
    PressureSensorChannel& getSensor(PressureSensorType type) { return sensors[type]; }
    const PressureSensorChannel& getSensor(PressureSensorType type) const { return sensors[type]; }
    
    // Status and publishing
    void publishPressures();
    void getStatusString(char* buffer, size_t bufferSize);
    
    // Compatibility with existing safety system
    float getPressure() const { return getHydraulicPressure(); } // Main hydraulic pressure for safety
    bool isReady() const { return sensors[SENSOR_HYDRAULIC].isReady(); }
};