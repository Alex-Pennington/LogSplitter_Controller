#pragma once

#include <Arduino.h>
#include <Wire.h>

// MCP3421 I2C address (7-bit address)
#define MCP3421_DEFAULT_ADDRESS 0x68

// MCP3421 Configuration register bits
#define MCP3421_RDY_BIT        0x80  // Ready bit (0 = conversion in progress, 1 = conversion complete)
#define MCP3421_C1_BIT         0x40  // Configuration bit C1
#define MCP3421_C0_BIT         0x20  // Configuration bit C0
#define MCP3421_OC_BIT         0x10  // One-shot conversion mode bit
#define MCP3421_S1_BIT         0x08  // Sample rate bit S1
#define MCP3421_S0_BIT         0x04  // Sample rate bit S0
#define MCP3421_G1_BIT         0x02  // Gain bit G1
#define MCP3421_G0_BIT         0x01  // Gain bit G0

// Sample rate and resolution settings
enum MCP3421_SampleRate {
    MCP3421_12_BIT_240_SPS = 0x00,  // 12-bit, 240 SPS
    MCP3421_14_BIT_60_SPS  = 0x04,  // 14-bit, 60 SPS
    MCP3421_16_BIT_15_SPS  = 0x08,  // 16-bit, 15 SPS
    MCP3421_18_BIT_3_75_SPS = 0x0C  // 18-bit, 3.75 SPS
};

// Gain settings
enum MCP3421_Gain {
    MCP3421_GAIN_1X = 0x00,  // Gain = 1
    MCP3421_GAIN_2X = 0x01,  // Gain = 2
    MCP3421_GAIN_4X = 0x02,  // Gain = 4
    MCP3421_GAIN_8X = 0x03   // Gain = 8
};

// Conversion mode
enum MCP3421_ConversionMode {
    MCP3421_CONTINUOUS = 0x00,  // Continuous conversion
    MCP3421_ONE_SHOT   = 0x10   // One-shot conversion
};

// Reading structure
struct MCP3421_Reading {
    int32_t rawValue;      // Raw ADC value (18-bit signed)
    float voltage;         // Converted voltage
    uint8_t config;        // Configuration byte
    bool valid;            // Reading validity
    unsigned long timestamp; // Reading timestamp
};

class MCP3421_Sensor {
private:
    uint8_t i2cAddress;
    TwoWire* wire;
    bool initialized;
    bool debugEnabled;
    MCP3421_Reading lastReading;
    
    // Configuration settings
    MCP3421_SampleRate sampleRate;
    MCP3421_Gain gain;
    MCP3421_ConversionMode conversionMode;
    uint8_t configRegister;
    
    // Reference voltage for conversion calculations
    float referenceVoltage;
    
    // Internal methods
    bool writeConfig();
    bool readData();
    uint8_t calculateConfig();
    float convertToVoltage(int32_t rawValue);
    int getResolutionBits() const;
    float getMaxVoltage();
    
public:
    MCP3421_Sensor(uint8_t address = MCP3421_DEFAULT_ADDRESS);
    
    // Initialization and configuration
    bool begin(TwoWire& wirePort = Wire1);
    bool reset();
    bool setConfiguration(MCP3421_SampleRate rate, MCP3421_Gain gainSetting, 
                         MCP3421_ConversionMode mode = MCP3421_CONTINUOUS);
    bool setSampleRate(MCP3421_SampleRate rate);
    bool setGain(MCP3421_Gain gainSetting);
    bool setConversionMode(MCP3421_ConversionMode mode);
    bool setReferenceVoltage(float voltage);
    
    // Reading functions
    bool startConversion();
    bool isConversionReady();
    bool takeReading();
    MCP3421_Reading getLastReading() const { return lastReading; }
    
    // Individual value getters
    int32_t getRawValue();
    float getVoltage();
    float getFilteredVoltage(uint8_t samples = 5);
    
    // Status and diagnostics
    bool isReady() const { return initialized && lastReading.valid; }
    bool isConnected();
    void getStatusString(char* buffer, size_t bufferSize);
    uint8_t getAddress() const { return i2cAddress; }
    uint8_t getConfigRegister() const { return configRegister; }
    
    // Configuration getters
    MCP3421_SampleRate getSampleRate() const { return sampleRate; }
    MCP3421_Gain getGain() const { return gain; }
    MCP3421_ConversionMode getConversionMode() const { return conversionMode; }
    float getReferenceVoltage() const { return referenceVoltage; }
    int getResolution() const { return getResolutionBits(); }
    
    // Debug and utilities
    void enableDebugOutput(bool enable) { debugEnabled = enable; }
    bool isDebugEnabled() const { return debugEnabled; }
    void printConfiguration();
    void printReading();
};