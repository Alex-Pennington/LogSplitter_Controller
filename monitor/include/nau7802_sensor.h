#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h"
#include "constants.h"

enum NAU7802Status {
    NAU7802_OK,
    NAU7802_NOT_FOUND,
    NAU7802_CALIBRATION_FAILED,
    NAU7802_READ_ERROR,
    NAU7802_NOT_READY
};

class NAU7802Sensor {
public:
    NAU7802Sensor();
    
    // Initialization and setup
    NAU7802Status begin();
    bool isConnected();
    void reset();
    
    // Calibration functions
    NAU7802Status calibrateAFE();
    NAU7802Status calibrateZero();
    NAU7802Status calibrateScale(float knownWeight);
    void setCalibrationFactor(float factor);
    float getCalibrationFactor() const;
    
    // Reading functions
    bool isReady();
    long getRawReading();
    float getWeight();
    float getFilteredWeight();
    bool dataAvailable();
    
    // Update function (non-const)
    void updateReading();
    
    // Configuration
    void setGain(uint8_t gain);
    uint8_t getGain() const;
    void setSampleRate(uint8_t rate);
    uint8_t getSampleRate() const;
    
    // Zero and tare functions
    void tareScale();
    void setZeroOffset(long offset);
    long getZeroOffset() const;
    
    // Statistics and filtering
    void enableFiltering(bool enable, uint8_t samples = 10);
    float getAverageReading(uint8_t samples = 10);
    void resetStatistics();
    
    // Status and diagnostics
    NAU7802Status getLastError() const;
    const char* getStatusString() const;
    uint8_t getRevisionCode();
    
    // Power management
    void powerUp();
    void powerDown();
    bool isPoweredUp();
    
    // EEPROM calibration storage
    bool saveCalibration();
    bool loadCalibration();
    void clearCalibration();

private:
    mutable NAU7802 scale;  // SparkFun library uses NAU7802 class name, mutable for const functions
    NAU7802Status lastError;
    
    // Calibration data
    float calibrationFactor;
    long zeroOffset;
    bool isCalibrated;
    
    // Cached readings
    mutable long lastRawReading;
    mutable float lastWeight;
    mutable float lastFilteredWeight;
    
    // Filtering variables
    bool filteringEnabled;
    uint8_t filterSamples;
    float* filterBuffer;
    uint8_t filterIndex;
    bool filterBufferFull;
    
    // Configuration
    uint8_t currentGain;
    uint8_t currentSampleRate;
    
    // Statistics
    unsigned long totalReadings;
    float minReading;
    float maxReading;
    
    // Helper functions
    void initializeDefaults();
    float applyCalibration(long rawValue);
    void updateFilter(float value);
    void logError(NAU7802Status error, const char* function);
};

// Constants for calibration storage in EEPROM
const int NAU7802_CALIBRATION_ADDR = 100;  // Start after monitor config
const uint32_t NAU7802_CALIBRATION_MAGIC = 0x4E415538; // 'NAU8'

struct NAU7802CalibrationData {
    uint32_t magic;
    float calibrationFactor;
    long zeroOffset;
    uint8_t gain;
    uint8_t sampleRate;
    uint32_t checksum;
};