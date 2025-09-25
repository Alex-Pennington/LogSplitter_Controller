#pragma once

#include <Arduino.h>
#include "constants.h"

class PressureSensor {
private:
    // Sampling buffer
    uint16_t samples[SAMPLE_WINDOW_COUNT];
    size_t sampleIndex = 0;
    size_t samplesFilled = 0;
    unsigned long samplesSum = 0;
    unsigned long lastSampleTime = 0;
    
    // Configuration
    float adcVref = DEFAULT_ADC_VREF;
    float maxPressurePsi = DEFAULT_MAX_PRESSURE_PSI;
    float sensorGain = DEFAULT_SENSOR_GAIN;
    float sensorOffset = DEFAULT_SENSOR_OFFSET;
    
    // Filtering
    FilterMode filterMode = FILTER_MEDIAN3;
    float emaValue = 0.0f;
    float emaAlpha = 0.2f;
    
    // Current values
    float currentPressure = 0.0f;
    
    // Helper methods
    int applyFilter(int rawValue);
    void updateSample(int filteredValue);
    float computeAverageCountsS();
    float countsToPressure(float avgCounts);
    
public:
    PressureSensor() = default;
    
    // Initialization
    void begin();
    
    // Main update (call regularly)
    void update();
    
    // Configuration
    void setAdcVref(float vref) { adcVref = vref; }
    void setMaxPressure(float maxPsi) { maxPressurePsi = maxPsi; }
    void setSensorGain(float gain) { sensorGain = gain; }
    void setSensorOffset(float offset) { sensorOffset = offset; }
    void setFilterMode(FilterMode mode) { filterMode = mode; if (mode == FILTER_EMA) emaValue = 0.0f; }
    void setEmaAlpha(float alpha) { if (alpha > 0.0f && alpha <= 1.0f) emaAlpha = alpha; }
    
    float getAdcVref() const { return adcVref; }
    float getMaxPressure() const { return maxPressurePsi; }
    float getSensorGain() const { return sensorGain; }
    float getSensorOffset() const { return sensorOffset; }
    FilterMode getFilterMode() const { return filterMode; }
    float getEmaAlpha() const { return emaAlpha; }
    
    // Current readings
    float getPressure() const { return currentPressure; }
    bool isReady() const { return samplesFilled > 0; }
    
    // Status
    void getStatusString(char* buffer, size_t bufferSize);
};