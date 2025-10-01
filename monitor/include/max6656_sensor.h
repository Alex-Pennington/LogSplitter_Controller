#ifndef MAX6656_SENSOR_H
#define MAX6656_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

// MAX6656 I2C addresses (7-bit)
#define MAX6656_ADDRESS_DEFAULT     0x4C  // Default address
#define MAX6656_ADDRESS_ALT1        0x4D  // Alternative address 1
#define MAX6656_ADDRESS_ALT2        0x4E  // Alternative address 2
#define MAX6656_ADDRESS_ALT3        0x4F  // Alternative address 3

// MAX6656 Register Map
#define MAX6656_REG_LOCAL_TEMP      0x00  // Local temperature reading
#define MAX6656_REG_REMOTE_TEMP     0x01  // Remote temperature reading
#define MAX6656_REG_STATUS          0x02  // Status register
#define MAX6656_REG_CONFIG          0x03  // Configuration register
#define MAX6656_REG_CONV_RATE       0x04  // Conversion rate register
#define MAX6656_REG_LOCAL_HIGH      0x05  // Local high limit
#define MAX6656_REG_LOCAL_LOW       0x06  // Local low limit
#define MAX6656_REG_REMOTE_HIGH     0x07  // Remote high limit
#define MAX6656_REG_REMOTE_LOW      0x08  // Remote low limit
#define MAX6656_REG_REMOTE_OFFSET   0x11  // Remote offset register
#define MAX6656_REG_REMOTE_TEMP_EXT 0x10  // Remote extended temperature
#define MAX6656_REG_LOCAL_TEMP_EXT  0x15  // Local extended temperature
#define MAX6656_REG_MANUFACTURER_ID 0xFE  // Manufacturer ID (should be 0x4D)
#define MAX6656_REG_REVISION        0xFF  // Revision register

// Configuration register bits
#define MAX6656_CONFIG_STANDBY      0x40  // Standby mode
#define MAX6656_CONFIG_ALERT_MASK   0x80  // Alert/THERM2 mask
#define MAX6656_CONFIG_RANGE        0x04  // Extended range (0-150°C vs 0-127°C)

// Status register bits
#define MAX6656_STATUS_BUSY         0x80  // ADC busy
#define MAX6656_STATUS_LOCAL_HIGH   0x40  // Local high temperature fault
#define MAX6656_STATUS_LOCAL_LOW    0x20  // Local low temperature fault
#define MAX6656_STATUS_REMOTE_HIGH  0x10  // Remote high temperature fault
#define MAX6656_STATUS_REMOTE_LOW   0x08  // Remote low temperature fault
#define MAX6656_STATUS_REMOTE_OPEN  0x04  // Remote diode open/short fault
#define MAX6656_STATUS_REMOTE_SHORT 0x02  // Remote diode short fault

// Conversion rates
#define MAX6656_CONV_RATE_0_0625HZ  0x00  // 16 seconds
#define MAX6656_CONV_RATE_0_125HZ   0x01  // 8 seconds
#define MAX6656_CONV_RATE_0_25HZ    0x02  // 4 seconds
#define MAX6656_CONV_RATE_0_5HZ     0x03  // 2 seconds
#define MAX6656_CONV_RATE_1HZ       0x04  // 1 second
#define MAX6656_CONV_RATE_2HZ       0x05  // 0.5 seconds
#define MAX6656_CONV_RATE_4HZ       0x06  // 0.25 seconds
#define MAX6656_CONV_RATE_8HZ       0x07  // 0.125 seconds

class MAX6656Sensor {
public:
    MAX6656Sensor(uint8_t address = MAX6656_ADDRESS_DEFAULT);
    
    // Initialization and setup
    bool begin();
    bool isAvailable();
    bool checkConnection();
    
    // Temperature readings
    float getLocalTemperature();
    float getRemoteTemperature();
    float getLocalTemperatureExtended();
    float getRemoteTemperatureExtended();
    
    // Configuration
    bool setConversionRate(uint8_t rate);
    bool setExtendedRange(bool enabled);
    bool setStandbyMode(bool enabled);
    
    // Status and diagnostics
    uint8_t getStatus();
    bool isLocalTempFault();
    bool isRemoteTempFault();
    bool isRemoteDiodeFault();
    bool isBusy();
    
    // Limit settings
    bool setLocalHighLimit(int8_t temperature);
    bool setLocalLowLimit(int8_t temperature);
    bool setRemoteHighLimit(int8_t temperature);
    bool setRemoteLowLimit(int8_t temperature);
    bool setRemoteOffset(int8_t offset);
    
    // Device information
    uint8_t getManufacturerID();
    uint8_t getRevision();
    
    // Utility functions
    void getStatusString(char* buffer, size_t bufferSize);
    void getDetailedStatus(char* buffer, size_t bufferSize);
    
    // Calibration and filtering
    void setTemperatureOffset(float localOffset, float remoteOffset);
    void enableFiltering(bool enabled, uint8_t filterSize = 5);
    
private:
    uint8_t i2cAddress;
    bool initialized;
    bool extendedRange;
    float localTempOffset;
    float remoteTempOffset;
    bool filteringEnabled;
    uint8_t filterSize;
    float localTempBuffer[10];
    float remoteTempBuffer[10];
    uint8_t bufferIndex;
    uint8_t validSamples;
    
    // Low-level I2C functions
    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool checkI2CDevice(uint8_t address);
    
    // Helper functions
    float convertTemperature(uint8_t tempReg, uint8_t extReg = 0);
    float applyFiltering(float* buffer, float newValue);
    void updateBuffer(float* buffer, float newValue);
    float calculateAverage(float* buffer, uint8_t samples);
};

// Global external declaration for access from other modules
extern MAX6656Sensor* g_max6656Sensor;

#endif // MAX6656_SENSOR_H