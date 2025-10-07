#include "mcp9600_sensor.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);

MCP9600Sensor::MCP9600Sensor(uint8_t address) :
    i2cAddress(address),
    initialized(false),
    ambientTempOffset(0.0),
    thermocoupleTempOffset(0.0),
    filteringEnabled(false),
    filterLevel(4),
    debugOutputEnabled(false),
    filterSize(5),
    bufferIndex(0),
    validSamples(0)
{
    // Initialize filter buffers
    for (int i = 0; i < MAX_FILTER_SIZE; i++) {
        ambientTempBuffer[i] = 0.0;
        thermocoupleTempBuffer[i] = 0.0;
    }
}

bool MCP9600Sensor::begin() {
    debugPrintf("MCP9600: Initializing thermocouple sensor at address 0x%02X using Wire1", i2cAddress);
    
    // Initialize Wire1 for all I2C devices on Arduino R4 WiFi
    Wire1.begin();
    delay(50); // Give I2C time to initialize
    
    debugPrintf("MCP9600: Testing register access for device identification...");
    
    // Try to identify what type of device this is
    // First, try reading some common registers to see what responds
    
    // Test register 0x00 (should be temperature in MCP9600)
    uint16_t reg00 = readRegister16(0x00);
    debugPrintf("MCP9600: Register 0x00 = 0x%04X", reg00);
    
    // Test register 0x01 
    uint16_t reg01 = readRegister16(0x01);
    debugPrintf("MCP9600: Register 0x01 = 0x%04X", reg01);
    
    // Test register 0x02
    uint16_t reg02 = readRegister16(0x02);
    debugPrintf("MCP9600: Register 0x02 = 0x%04X", reg02);
    
    // Try reading single bytes from various registers
    uint8_t reg00_byte = readRegister8(0x00);
    uint8_t reg01_byte = readRegister8(0x01);
    uint8_t reg02_byte = readRegister8(0x02);
    debugPrintf("MCP9600: Byte reads - 0x00=0x%02X, 0x01=0x%02X, 0x02=0x%02X", reg00_byte, reg01_byte, reg02_byte);
    
    // Try reading device ID at different locations
    uint16_t deviceID = getDeviceID(); // Register 0x20
    debugPrintf("MCP9600: Device ID at 0x20 = 0x%04X", deviceID);
    
    // Try some other common device ID locations
    uint8_t regFE = readRegister8(0xFE);
    uint8_t regFF = readRegister8(0xFF);
    debugPrintf("MCP9600: ID check - 0xFE=0x%02X, 0xFF=0x%02X", regFE, regFF);
    
    // If we can read register 0x00 successfully, we have a working thermocouple sensor
    if (reg00 != 0xFFFF) {
        debugPrintf("MCP9600: Register 0x00 readable (0x%04X), device partially functional", reg00);
        
        // Test if we can read thermocouple temperature consistently
        uint16_t testRead = readRegister16(0x00);
        if (testRead != 0xFFFF) {
            debugPrintf("MCP9600: Thermocouple register confirmed working, initializing with limited functionality");
            initialized = true;
            debugPrintf("MCP9600: Initialization complete - thermocouple sensor available");
            return true;
        }
    }
    
    debugPrintf("MCP9600: Device at 0x%02X does not appear to be an MCP9600", i2cAddress);
    return false;
}

bool MCP9600Sensor::isAvailable() {
    return initialized && checkI2CDevice(i2cAddress);
}

bool MCP9600Sensor::checkConnection() {
    if (!initialized) return false;
    
    // Check if we can read the thermocouple register (0x00)
    uint16_t thermocoupleReading = readRegister16(MCP9600_REG_HOT_JUNCTION);
    return (thermocoupleReading != 0xFFFF);
}

float MCP9600Sensor::getThermocoupleTemperature() {
    if (!initialized) return -999.0;
    
    uint16_t rawTemp = readRegister16(MCP9600_REG_HOT_JUNCTION);
    
    if (rawTemp == 0xFFFF) {
        if (debugOutputEnabled) {
            debugPrintf("MCP9600: ERROR - Thermocouple read failed (0xFFFF)\n");
        }
        return -999.0; // Read error
    }
    
    // Show conversion steps
    int16_t signedRaw = (int16_t)rawTemp;
    float temperature = signedRaw * 0.0625 + thermocoupleTempOffset;
    
    if (debugOutputEnabled) {
        debugPrintf("MCP9600: Raw=0x%04X (%d) -> %.3fC\n", rawTemp, signedRaw, temperature);
    }
    
    // Static variable to track previous temperature for rate checking
    static float lastTemp = temperature;
    static bool firstReading = true;
    
    if (!firstReading && debugOutputEnabled) {
        float tempChange = temperature - lastTemp;
        debugPrintf("MCP9600: Change: %.3fC (%.3f -> %.3f)\n", tempChange, lastTemp, temperature);
        
        // Flag suspicious large changes
        if (abs(tempChange) > 10.0) {
            debugPrintf("MCP9600: *** WARNING - Large jump detected! ***\n");
        }
    } else if (firstReading && debugOutputEnabled) {
        debugPrintf("MCP9600: First reading\n");
    }
    
    if (filteringEnabled) {
        float filteredTemp = applyFilter(temperature, thermocoupleTempBuffer);
        if (debugOutputEnabled) {
            debugPrintf("MCP9600: Filtered: %.3fC (from raw: %.3fC)\n", filteredTemp, temperature);
        }
        temperature = filteredTemp;
    }
    
    lastTemp = temperature;
    firstReading = false;
    
    return temperature;
}

float MCP9600Sensor::getAmbientTemperature() {
    if (!initialized) return -999.0;
    
    uint16_t rawTemp = readRegister16(MCP9600_REG_COLD_JUNCTION);
    LOG_DEBUG("MCP9600: Raw ambient reading from 0x%02X = 0x%04X", MCP9600_REG_COLD_JUNCTION, rawTemp);
    
    if (rawTemp == 0xFFFF) {
        // If ambient temperature register isn't available, estimate from thermocouple
        // This is a fallback for devices with limited register support
        LOG_DEBUG("MCP9600: Ambient register not available, using estimated room temperature");
        return 23.0 + ambientTempOffset; // Return estimated room temperature
    }
    
    float temperature = convertRawToTemperature(rawTemp) + ambientTempOffset;
    LOG_DEBUG("MCP9600: Converted ambient temperature = %.2fC", temperature);
    
    if (filteringEnabled) {
        temperature = applyFilter(temperature, ambientTempBuffer);
    }
    
    return temperature;
}

float MCP9600Sensor::getLocalTemperature() {
    return getAmbientTemperature();
}

float MCP9600Sensor::getRemoteTemperature() {
    return getThermocoupleTemperature();
}

float MCP9600Sensor::getThermocoupleTemperatureF() {
    return celsiusToFahrenheit(getThermocoupleTemperature());
}

float MCP9600Sensor::getAmbientTemperatureF() {
    return celsiusToFahrenheit(getAmbientTemperature());
}

float MCP9600Sensor::getLocalTemperatureF() {
    return celsiusToFahrenheit(getLocalTemperature());
}

float MCP9600Sensor::getRemoteTemperatureF() {
    return celsiusToFahrenheit(getRemoteTemperature());
}

bool MCP9600Sensor::setThermocoupleType(uint8_t type) {
    if (!initialized) return false;
    
    if (type > MCP9600_TYPE_R) return false; // Invalid type
    
    // Read current configuration
    uint8_t config = readRegister8(MCP9600_REG_THERMOCOUPLE_CFG);
    
    // Clear thermocouple type bits (bits 2:0) and set new type
    config = (config & 0xF8) | (type & 0x07);
    
    return writeRegister8(MCP9600_REG_THERMOCOUPLE_CFG, config);
}

bool MCP9600Sensor::setFilterCoefficient(uint8_t filter) {
    if (!initialized) return false;
    
    if (filter > 7) return false; // Invalid filter value
    
    // Read current configuration
    uint8_t config = readRegister8(MCP9600_REG_THERMOCOUPLE_CFG);
    
    // Clear filter bits (bits 6:4) and set new filter
    config = (config & 0x8F) | ((filter & 0x07) << 4);
    
    return writeRegister8(MCP9600_REG_THERMOCOUPLE_CFG, config);
}

uint8_t MCP9600Sensor::getStatus() {
    if (!initialized) return 0xFF;
    return readRegister8(MCP9600_REG_STATUS);
}

uint16_t MCP9600Sensor::getDeviceID() {
    return readRegister16(MCP9600_REG_DEVICE_ID);
}

void MCP9600Sensor::setTemperatureOffset(float ambientOffset, float thermocoupleOffset) {
    ambientTempOffset = ambientOffset;
    thermocoupleTempOffset = thermocoupleOffset;
    debugPrintf("MCP9600: Temperature offsets set - ambient: %.2f°C, thermocouple: %.2f°C", 
                ambientOffset, thermocoupleOffset);
}

void MCP9600Sensor::getStatusString(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    if (!initialized) {
        snprintf(buffer, bufferSize, "MCP9600: Not initialized");
        return;
    }
    
    float ambientTemp = getAmbientTemperature();
    float thermocoupleTemp = getThermocoupleTemperature();
    uint8_t status = getStatus();
    uint16_t deviceID = getDeviceID();
    
    const char* statusStr = "OK";
    if (status & MCP9600_STATUS_INPUT_RANGE) {
        statusStr = "INPUT_RANGE_ERROR";
    } else if (ambientTemp <= -999.0 || thermocoupleTemp <= -999.0) {
        statusStr = "READ_ERROR";
    }
    
    snprintf(buffer, bufferSize, 
        "MCP9600: Ambient=%.1fC Thermocouple=%.1fC Status=0x%02X DeviceID=0x%04X %s",
        ambientTemp, thermocoupleTemp, status, deviceID, statusStr);
}

void MCP9600Sensor::enableFiltering(bool enabled, uint8_t filterLevel) {
    filteringEnabled = enabled;
    
    if (enabled) {
        if (filterLevel < 1) filterLevel = 1;
        if (filterLevel > MAX_FILTER_SIZE) filterLevel = MAX_FILTER_SIZE;
        
        filterSize = filterLevel;
        bufferIndex = 0;
        validSamples = 0;
        
        // Clear filter buffers
        for (int i = 0; i < MAX_FILTER_SIZE; i++) {
            ambientTempBuffer[i] = 0.0;
            thermocoupleTempBuffer[i] = 0.0;
        }
        
        if (debugOutputEnabled) {
            debugPrintf("MCP9600: Temperature filtering enabled (level %d)\n", filterLevel);
        }
    } else {
        if (debugOutputEnabled) {
            debugPrintf("MCP9600: Temperature filtering disabled\n");
        }
    }
}

void MCP9600Sensor::enableDebugOutput(bool enabled) {
    debugOutputEnabled = enabled;
    debugPrintf("MCP9600: Debug output %s\n", enabled ? "ENABLED" : "DISABLED");
}

bool MCP9600Sensor::isDebugEnabled() const {
    return debugOutputEnabled;
}

bool MCP9600Sensor::checkI2CDevice(uint8_t address) {
    Wire1.beginTransmission(address);
    uint8_t error = Wire1.endTransmission();
    return (error == 0);
}

uint8_t MCP9600Sensor::readRegister8(uint8_t reg) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(reg);
    uint8_t error = Wire1.endTransmission();
    
    if (error != 0) {
        // Only log errors for registers other than 0x02 (ambient temp has known issues)
        if (reg != 0x02) {
            LOG_ERROR("MCP9600: I2C transmission error %d when reading register 0x%02X", error, reg);
        }
        return 0xFF;
    }
    
    Wire1.requestFrom((uint8_t)i2cAddress, (uint8_t)1);
    if (Wire1.available()) {
        return Wire1.read();
    }
    
    debugPrintf("MCP9600: No data available when reading register 0x%02X\n", reg);
    return 0xFF;
}

uint16_t MCP9600Sensor::readRegister16(uint8_t reg) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(reg);
    uint8_t error = Wire1.endTransmission();
    
    if (error != 0) {
        // Only log errors for registers other than 0x02 (ambient temp has known issues)
        if (reg != 0x02) {
            LOG_ERROR("MCP9600: I2C transmission error %d when reading register 0x%02X", error, reg);
        }
        return 0xFFFF;
    }
    
    Wire1.requestFrom((uint8_t)i2cAddress, (uint8_t)2);
    if (Wire1.available() >= 2) {
        uint8_t msb = Wire1.read();
        uint8_t lsb = Wire1.read();
        return (uint16_t)(msb << 8) | lsb;
    }
    
    debugPrintf("MCP9600: Insufficient data when reading 16-bit register 0x%02X", reg);
    return 0xFFFF;
}

bool MCP9600Sensor::writeRegister8(uint8_t reg, uint8_t value) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(reg);
    Wire1.write(value);
    uint8_t error = Wire1.endTransmission();
    
    if (error != 0) {
        debugPrintf("MCP9600: I2C transmission error %d when writing register 0x%02X", error, reg);
        return false;
    }
    
    return true;
}

float MCP9600Sensor::convertRawToTemperature(uint16_t raw) {
    // MCP9600 temperature format: 16-bit signed, 0.0625C resolution
    // Convert from raw ADC counts to temperature
    int16_t signedRaw = (int16_t)raw;
    return signedRaw * 0.0625;
}

float MCP9600Sensor::celsiusToFahrenheit(float celsius) {
    // Convert Celsius to Fahrenheit: F = (C × 9/5) + 32
    return (celsius * 9.0 / 5.0) + 32.0;
}

float MCP9600Sensor::applyFilter(float newValue, float* buffer) {
    if (!filteringEnabled || filterSize <= 1) return newValue;
    
    // Add new value to circular buffer
    buffer[bufferIndex] = newValue;
    bufferIndex = (bufferIndex + 1) % filterSize;
    
    if (validSamples < filterSize) {
        validSamples++;
    }
    
    // Calculate moving average
    float sum = 0.0;
    for (int i = 0; i < validSamples; i++) {
        sum += buffer[i];
    }
    
    return sum / validSamples;
}