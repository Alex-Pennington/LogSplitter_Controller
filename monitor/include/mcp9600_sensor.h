#ifndef MCP9600_SENSOR_H
#define MCP9600_SENSOR_H

#include <Arduino.h>
#include <Wire.h>

// MCP9600/MCP9601 I2C addresses (7-bit)
#define MCP9600_ADDRESS_DEFAULT     0x67  // Default address
#define MCP9600_ADDRESS_ALT1        0x66  // Alternative address 1
#define MCP9600_ADDRESS_ALT2        0x65  // Alternative address 2
#define MCP9600_ADDRESS_ALT3        0x64  // Alternative address 3

// MCP9600 Register Map
#define MCP9600_REG_HOT_JUNCTION     0x00  // Thermocouple temperature (2 bytes)
#define MCP9600_REG_JUNCTION_DELTA   0x01  // Junction temperature delta (2 bytes)
#define MCP9600_REG_COLD_JUNCTION    0x02  // Cold junction temperature (2 bytes)
#define MCP9600_REG_RAW_ADC          0x03  // Raw ADC data (3 bytes)
#define MCP9600_REG_STATUS           0x04  // Status register
#define MCP9600_REG_THERMOCOUPLE_CFG 0x05  // Thermocouple configuration
#define MCP9600_REG_DEVICE_CFG       0x06  // Device configuration
#define MCP9600_REG_ALERT1_CFG       0x08  // Alert 1 configuration
#define MCP9600_REG_ALERT2_CFG       0x09  // Alert 2 configuration
#define MCP9600_REG_ALERT3_CFG       0x0A  // Alert 3 configuration
#define MCP9600_REG_ALERT4_CFG       0x0B  // Alert 4 configuration
#define MCP9600_REG_ALERT1_HYSTERESIS 0x0C // Alert 1 hysteresis
#define MCP9600_REG_ALERT2_HYSTERESIS 0x0D // Alert 2 hysteresis
#define MCP9600_REG_ALERT3_HYSTERESIS 0x0E // Alert 3 hysteresis
#define MCP9600_REG_ALERT4_HYSTERESIS 0x0F // Alert 4 hysteresis
#define MCP9600_REG_ALERT1_LIMIT     0x10  // Alert 1 temperature limit
#define MCP9600_REG_ALERT2_LIMIT     0x11  // Alert 2 temperature limit
#define MCP9600_REG_ALERT3_LIMIT     0x12  // Alert 3 temperature limit
#define MCP9600_REG_ALERT4_LIMIT     0x13  // Alert 4 temperature limit
#define MCP9600_REG_DEVICE_ID        0x20  // Device ID/revision

// Thermocouple types
#define MCP9600_TYPE_K               0x00  // Type K thermocouple
#define MCP9600_TYPE_J               0x01  // Type J thermocouple
#define MCP9600_TYPE_T               0x02  // Type T thermocouple
#define MCP9600_TYPE_N               0x03  // Type N thermocouple
#define MCP9600_TYPE_S               0x04  // Type S thermocouple
#define MCP9600_TYPE_E               0x05  // Type E thermocouple
#define MCP9600_TYPE_B               0x06  // Type B thermocouple
#define MCP9600_TYPE_R               0x07  // Type R thermocouple

// Filter coefficients
#define MCP9600_FILTER_OFF           0x00  // No filtering
#define MCP9600_FILTER_MIN           0x01  // Minimum filtering
#define MCP9600_FILTER_MID           0x04  // Medium filtering
#define MCP9600_FILTER_MAX           0x07  // Maximum filtering

// Status register bits
#define MCP9600_STATUS_ALERT1        0x01  // Alert 1 status
#define MCP9600_STATUS_ALERT2        0x02  // Alert 2 status
#define MCP9600_STATUS_ALERT3        0x04  // Alert 3 status
#define MCP9600_STATUS_ALERT4        0x08  // Alert 4 status
#define MCP9600_STATUS_INPUT_RANGE   0x10  // Input range exceeded
#define MCP9600_STATUS_TH_UPDATE     0x40  // Thermocouple update flag
#define MCP9600_STATUS_BURST_DONE    0x80  // Burst mode complete

/**
 * MCP9600/MCP9601 Thermocouple-to-Digital Converter
 * 
 * High-precision thermocouple sensor with integrated cold-junction compensation.
 * Supports multiple thermocouple types and provides both thermocouple and
 * ambient temperature readings via I2C interface using Wire1 bus.
 */
class MCP9600Sensor {
public:
    /**
     * Constructor
     * @param address I2C address (default 0x67)
     */
    MCP9600Sensor(uint8_t address = MCP9600_ADDRESS_DEFAULT);
    
    /**
     * Initialize the sensor
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * Check if sensor is available and responding
     * @return true if sensor is responding
     */
    bool isAvailable();
    
    /**
     * Check sensor connection
     * @return true if sensor is connected and responding
     */
    bool checkConnection();
    
    /**
     * Get thermocouple (hot junction) temperature
     * @return temperature in degrees Celsius
     */
    float getThermocoupleTemperature();
    
    /**
     * Get thermocouple (hot junction) temperature in Fahrenheit
     * @return temperature in degrees Fahrenheit
     */
    float getThermocoupleTemperatureF();
    
    /**
     * Get ambient (cold junction) temperature
     * @return temperature in degrees Celsius
     */
    float getAmbientTemperature();
    
    /**
     * Get ambient (cold junction) temperature in Fahrenheit
     * @return temperature in degrees Fahrenheit
     */
    float getAmbientTemperatureF();
    
    /**
     * Get local temperature (alias for ambient)
     * @return temperature in degrees Celsius
     */
    float getLocalTemperature();
    
    /**
     * Get local temperature in Fahrenheit (alias for ambient)
     * @return temperature in degrees Fahrenheit
     */
    float getLocalTemperatureF();
    
    /**
     * Get remote temperature (alias for thermocouple)
     * @return temperature in degrees Celsius
     */
    float getRemoteTemperature();
    
    /**
     * Get remote temperature in Fahrenheit (alias for thermocouple)
     * @return temperature in degrees Fahrenheit
     */
    float getRemoteTemperatureF();
    
    /**
     * Set thermocouple type
     * @param type Thermocouple type (MCP9600_TYPE_K, etc.)
     * @return true if successful
     */
    bool setThermocoupleType(uint8_t type);
    
    /**
     * Set filter coefficient
     * @param filter Filter setting (0-7, 0=off, 7=maximum)
     * @return true if successful
     */
    bool setFilterCoefficient(uint8_t filter);
    
    /**
     * Get sensor status
     * @return status register value
     */
    uint8_t getStatus();
    
    /**
     * Get device ID
     * @return device ID
     */
    uint16_t getDeviceID();
    
    /**
     * Set temperature offsets for calibration
     * @param ambientOffset Offset for ambient temperature
     * @param thermocoupleOffset Offset for thermocouple temperature
     */
    void setTemperatureOffset(float ambientOffset, float thermocoupleOffset);
    
    /**
     * Get comprehensive status string
     * @param buffer Buffer to store status string
     * @param bufferSize Size of buffer
     */
    void getStatusString(char* buffer, size_t bufferSize);
    
    /**
     * Enable/disable temperature filtering
     * @param enabled true to enable filtering
     * @param filterLevel Filter level (1-7, higher = more filtering)
     */
    void enableFiltering(bool enabled, uint8_t filterLevel = 4);

private:
    uint8_t i2cAddress;                // I2C address
    bool initialized;                  // Initialization status
    float ambientTempOffset;           // Ambient temperature offset for calibration
    float thermocoupleTempOffset;      // Thermocouple temperature offset for calibration
    bool filteringEnabled;             // Temperature filtering enabled
    uint8_t filterLevel;               // Filter level (1-7)
    
    // Temperature filter buffers
    static const int MAX_FILTER_SIZE = 10;
    float ambientTempBuffer[MAX_FILTER_SIZE];
    float thermocoupleTempBuffer[MAX_FILTER_SIZE];
    int filterSize;
    int bufferIndex;
    int validSamples;
    
    /**
     * Check if I2C device is present at address
     * @param address I2C address to check
     * @return true if device responds
     */
    bool checkI2CDevice(uint8_t address);
    
    /**
     * Read 8-bit register
     * @param reg Register address
     * @return register value
     */
    uint8_t readRegister8(uint8_t reg);
    
    /**
     * Read 16-bit register (big-endian)
     * @param reg Register address
     * @return register value
     */
    uint16_t readRegister16(uint8_t reg);
    
    /**
     * Write 8-bit register
     * @param reg Register address
     * @param value Value to write
     * @return true if successful
     */
    bool writeRegister8(uint8_t reg, uint8_t value);
    
    /**
     * Convert raw temperature data to Celsius
     * @param raw Raw temperature data
     * @return temperature in Celsius
     */
    float convertRawToTemperature(uint16_t raw);
    
    /**
     * Convert Celsius to Fahrenheit
     * @param celsius Temperature in Celsius
     * @return temperature in Fahrenheit
     */
    float celsiusToFahrenheit(float celsius);
    
    /**
     * Apply filtering to temperature reading
     * @param newValue New temperature value
     * @param buffer Filter buffer to use
     * @return filtered temperature value
     */
    float applyFilter(float newValue, float* buffer);
};

#endif // MCP9600_SENSOR_H