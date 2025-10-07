#pragma once

#include <Arduino.h>
#include <Wire.h>
#include "constants.h"

// INA219 I2C addresses (7-bit addresses)
#define INA219_ADDRESS_DEFAULT    0x40  // 1000000 (A1=GND, A0=GND)
#define INA219_ADDRESS_A0_VDD     0x41  // 1000001 (A1=GND, A0=VDD)
#define INA219_ADDRESS_A1_VDD     0x44  // 1000100 (A1=VDD, A0=GND)
#define INA219_ADDRESS_BOTH_VDD   0x45  // 1000101 (A1=VDD, A0=VDD)

// INA219 Register Map
#define INA219_REG_CONFIG         0x00
#define INA219_REG_SHUNTVOLTAGE   0x01
#define INA219_REG_BUSVOLTAGE     0x02
#define INA219_REG_POWER          0x03
#define INA219_REG_CURRENT        0x04
#define INA219_REG_CALIBRATION    0x05

// Configuration register bit definitions
#define INA219_CONFIG_RESET       0x8000  // Reset bit
#define INA219_CONFIG_BVOLTAGERANGE_MASK 0x2000  // Bus voltage range mask
#define INA219_CONFIG_BVOLTAGERANGE_16V   0x0000  // 0-16V range
#define INA219_CONFIG_BVOLTAGERANGE_32V   0x2000  // 0-32V range

#define INA219_CONFIG_GAIN_MASK   0x1800  // Gain mask
#define INA219_CONFIG_GAIN_1_40MV 0x0000  // +/- 40mV range
#define INA219_CONFIG_GAIN_2_80MV 0x0800  // +/- 80mV range
#define INA219_CONFIG_GAIN_4_160MV 0x1000 // +/- 160mV range
#define INA219_CONFIG_GAIN_8_320MV 0x1800 // +/- 320mV range

#define INA219_CONFIG_BADCRES_MASK 0x0780  // Bus ADC resolution mask
#define INA219_CONFIG_BADCRES_9BIT 0x0000   // 9-bit bus resolution
#define INA219_CONFIG_BADCRES_10BIT 0x0080  // 10-bit bus resolution
#define INA219_CONFIG_BADCRES_11BIT 0x0100  // 11-bit bus resolution
#define INA219_CONFIG_BADCRES_12BIT 0x0180  // 12-bit bus resolution

#define INA219_CONFIG_SADCRES_MASK 0x0078  // Shunt ADC resolution mask
#define INA219_CONFIG_SADCRES_9BIT_1S 0x0000   // 9-bit shunt resolution
#define INA219_CONFIG_SADCRES_10BIT_1S 0x0008  // 10-bit shunt resolution
#define INA219_CONFIG_SADCRES_11BIT_1S 0x0010  // 11-bit shunt resolution
#define INA219_CONFIG_SADCRES_12BIT_1S 0x0018  // 12-bit shunt resolution

#define INA219_CONFIG_MODE_MASK   0x0007  // Operating mode mask
#define INA219_CONFIG_MODE_POWERDOWN 0x0000
#define INA219_CONFIG_MODE_SVOLT_TRIGGERED 0x0001
#define INA219_CONFIG_MODE_BVOLT_TRIGGERED 0x0002
#define INA219_CONFIG_MODE_SANDBVOLT_TRIGGERED 0x0003
#define INA219_CONFIG_MODE_ADCOFF 0x0004
#define INA219_CONFIG_MODE_SVOLT_CONTINUOUS 0x0005
#define INA219_CONFIG_MODE_BVOLT_CONTINUOUS 0x0006
#define INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS 0x0007

// Calibration constants
#define INA219_CURRENT_LSB_DEFAULT 0.1f  // 0.1mA per bit default

enum class INA219_Range {
    RANGE_16V_400MA,   // 16V, 400mA (0.1 ohm shunt)
    RANGE_16V_1A,      // 16V, 1A (0.1 ohm shunt)  
    RANGE_16V_2A,      // 16V, 2A (0.1 ohm shunt)
    RANGE_32V_1A,      // 32V, 1A (0.1 ohm shunt)
    RANGE_32V_2A       // 32V, 2A (0.1 ohm shunt)
};

struct INA219_Reading {
    float busVoltage_V;     // Bus voltage in volts
    float shuntVoltage_mV;  // Shunt voltage in millivolts
    float current_mA;       // Current in milliamps
    float power_mW;         // Power in milliwatts
    bool valid;             // Reading validity
    unsigned long timestamp;
};

class INA219_Sensor {
public:
    INA219_Sensor(uint8_t address = INA219_ADDRESS_DEFAULT);
    
    // Initialization and configuration
    bool begin(TwoWire& wire = Wire1);
    bool begin(INA219_Range range, TwoWire& wire = Wire1);
    bool reset();
    bool setRange(INA219_Range range);
    bool setCalibration(float shuntResistor_ohms, float maxCurrent_A);
    
    // Reading functions
    bool takereading();
    INA219_Reading getLastReading() const { return lastReading; }
    
    // Individual value getters
    float getBusVoltage();
    float getShuntVoltage();
    float getCurrent();
    float getPower();
    
    // Status and diagnostics
    bool isReady() const { return initialized && lastReading.valid; }
    bool isConnected();
    void getStatusString(char* buffer, size_t bufferSize);
    uint8_t getAddress() const { return i2cAddress; }
    
    // Advanced configuration
    bool setBusVoltageRange(bool range32V);
    bool setShuntGain(uint8_t gain);
    bool setADCResolution(uint8_t busRes, uint8_t shuntRes);
    bool setOperatingMode(uint8_t mode);
    
    // Calibration helpers
    static float calculateCurrentLSB(float maxCurrent_A);
    static uint16_t calculateCalibrationValue(float currentLSB, float shuntResistor_ohms);
    
    // Debug and testing
    void enableDebug(bool enable) { debugEnabled = enable; }
    bool testConnection();
    void dumpRegisters();

private:
    // I2C communication
    TwoWire* wire;
    uint8_t i2cAddress;
    
    // Configuration state
    bool initialized;
    bool debugEnabled;
    INA219_Range currentRange;
    float shuntResistor;
    float currentLSB;
    uint16_t calibrationValue;
    
    // Last reading
    INA219_Reading lastReading;
    
    // Register I/O
    bool writeRegister(uint8_t reg, uint16_t value);
    uint16_t readRegister(uint8_t reg);
    bool readRegister(uint8_t reg, uint16_t& value);
    
    // Configuration helpers
    uint16_t getRangeConfig(INA219_Range range);
    void calculateCalibration();
    void debugPrint(const char* message);
    
    // Conversion functions
    float convertBusVoltage(uint16_t raw);
    float convertShuntVoltage(uint16_t raw);
    float convertCurrent(uint16_t raw);
    float convertPower(uint16_t raw);
};

// Global convenience functions for monitor system
extern INA219_Sensor* g_ina219Sensor;

// MQTT topic definitions for INA219 data
extern const char TOPIC_MONITOR_POWER_VOLTAGE[];
extern const char TOPIC_MONITOR_POWER_CURRENT[];
extern const char TOPIC_MONITOR_POWER_POWER[];
extern const char TOPIC_MONITOR_POWER_STATUS[];