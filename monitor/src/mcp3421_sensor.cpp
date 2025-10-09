#include "mcp3421_sensor.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);

MCP3421_Sensor::MCP3421_Sensor(uint8_t address) :
    i2cAddress(address),
    wire(nullptr),
    initialized(false),
    debugEnabled(false),
    sampleRate(MCP3421_16_BIT_15_SPS),
    gain(MCP3421_GAIN_1X),
    conversionMode(MCP3421_CONTINUOUS),
    configRegister(0),
    referenceVoltage(2.048) {  // Default 2.048V reference
    
    lastReading.rawValue = 0;
    lastReading.voltage = 0.0;
    lastReading.config = 0;
    lastReading.valid = false;
    lastReading.timestamp = 0;
}

bool MCP3421_Sensor::begin(TwoWire& wirePort) {
    wire = &wirePort;
    
    if (debugEnabled) {
        debugPrintf("MCP3421: Initializing sensor at address 0x%02X\n", i2cAddress);
    }
    
    // Check if device is present
    wire->beginTransmission(i2cAddress);
    uint8_t error = wire->endTransmission();
    
    if (error != 0) {
        if (debugEnabled) {
            debugPrintf("MCP3421: Device not found at address 0x%02X (error: %d)\n", i2cAddress, error);
        }
        return false;
    }
    
    // Set default configuration
    configRegister = calculateConfig();
    if (!writeConfig()) {
        if (debugEnabled) {
            debugPrintf("MCP3421: Failed to write initial configuration\n");
        }
        return false;
    }
    
    initialized = true;
    
    if (debugEnabled) {
        debugPrintf("MCP3421: Sensor initialized successfully\n");
        printConfiguration();
    }
    
    return true;
}

bool MCP3421_Sensor::reset() {
    if (!wire) return false;
    
    // Perform general call reset
    wire->beginTransmission(0x00);  // General call address
    wire->write(0x06);              // Reset command
    uint8_t error = wire->endTransmission();
    
    if (error == 0) {
        delay(10);  // Allow time for reset
        initialized = false;
        return begin(*wire);  // Re-initialize
    }
    
    return false;
}

bool MCP3421_Sensor::setConfiguration(MCP3421_SampleRate rate, MCP3421_Gain gainSetting, 
                                     MCP3421_ConversionMode mode) {
    sampleRate = rate;
    gain = gainSetting;
    conversionMode = mode;
    
    configRegister = calculateConfig();
    
    if (initialized) {
        return writeConfig();
    }
    
    return true;  // Configuration will be applied on next begin()
}

bool MCP3421_Sensor::setSampleRate(MCP3421_SampleRate rate) {
    sampleRate = rate;
    configRegister = calculateConfig();
    
    if (initialized) {
        return writeConfig();
    }
    
    return true;
}

bool MCP3421_Sensor::setGain(MCP3421_Gain gainSetting) {
    gain = gainSetting;
    configRegister = calculateConfig();
    
    if (initialized) {
        return writeConfig();
    }
    
    return true;
}

bool MCP3421_Sensor::setConversionMode(MCP3421_ConversionMode mode) {
    conversionMode = mode;
    configRegister = calculateConfig();
    
    if (initialized) {
        return writeConfig();
    }
    
    return true;
}

bool MCP3421_Sensor::setReferenceVoltage(float voltage) {
    if (voltage > 0.0) {
        referenceVoltage = voltage;
        return true;
    }
    return false;
}

bool MCP3421_Sensor::startConversion() {
    if (!initialized || !wire) return false;
    
    // For one-shot mode, write config to start conversion
    if (conversionMode == MCP3421_ONE_SHOT) {
        return writeConfig();
    }
    
    // For continuous mode, conversion is always running
    return true;
}

bool MCP3421_Sensor::isConversionReady() {
    if (!initialized || !wire) return false;
    
    // Request 1 byte (config register)
    wire->requestFrom(i2cAddress, (uint8_t)1);
    
    if (wire->available()) {
        uint8_t config = wire->read();
        return (config & MCP3421_RDY_BIT) != 0;  // Ready when bit is set
    }
    
    return false;
}

bool MCP3421_Sensor::takeReading() {
    if (!initialized || !wire) return false;
    
    return readData();
}

int32_t MCP3421_Sensor::getRawValue() {
    if (lastReading.valid) {
        return lastReading.rawValue;
    }
    return 0;
}

float MCP3421_Sensor::getVoltage() {
    if (lastReading.valid) {
        return lastReading.voltage;
    }
    return 0.0;
}

float MCP3421_Sensor::getFilteredVoltage(uint8_t samples) {
    if (!initialized || samples == 0) return getVoltage();
    
    float sum = 0.0;
    uint8_t validSamples = 0;
    
    for (uint8_t i = 0; i < samples; i++) {
        if (takeReading() && lastReading.valid) {
            sum += lastReading.voltage;
            validSamples++;
        }
        delay(10);  // Small delay between readings
    }
    
    if (validSamples > 0) {
        return sum / validSamples;
    }
    
    return getVoltage();  // Return last known good reading
}

bool MCP3421_Sensor::isConnected() {
    if (!wire) return false;
    
    wire->beginTransmission(i2cAddress);
    return wire->endTransmission() == 0;
}

void MCP3421_Sensor::getStatusString(char* buffer, size_t bufferSize) {
    if (!buffer || bufferSize == 0) return;
    
    int bits = getResolutionBits();
    const char* rateStr;
    
    switch (sampleRate) {
        case MCP3421_12_BIT_240_SPS: rateStr = "240 SPS"; break;
        case MCP3421_14_BIT_60_SPS:  rateStr = "60 SPS"; break;
        case MCP3421_16_BIT_15_SPS:  rateStr = "15 SPS"; break;
        case MCP3421_18_BIT_3_75_SPS: rateStr = "3.75 SPS"; break;
        default: rateStr = "Unknown"; break;
    }
    
    int gainValue = 1 << gain;  // Convert gain enum to actual multiplier
    
    snprintf(buffer, bufferSize, 
             "MCP3421 @ 0x%02X: %s, %d-bit, Gain=%dx, %s, Vref=%.3fV, Last=%.4fV",
             i2cAddress,
             initialized ? "READY" : "NOT_INIT",
             bits,
             gainValue,
             rateStr,
             referenceVoltage,
             lastReading.voltage);
}

void MCP3421_Sensor::printConfiguration() {
    if (!debugEnabled) return;
    
    int bits = getResolutionBits();
    int gainValue = 1 << gain;
    
    debugPrintf("MCP3421 Configuration:\n");
    debugPrintf("  Address: 0x%02X\n", i2cAddress);
    debugPrintf("  Resolution: %d bits\n", bits);
    debugPrintf("  Gain: %dx\n", gainValue);
    debugPrintf("  Mode: %s\n", (conversionMode == MCP3421_CONTINUOUS) ? "Continuous" : "One-shot");
    debugPrintf("  Reference: %.3fV\n", referenceVoltage);
    debugPrintf("  Config Register: 0x%02X\n", configRegister);
}

void MCP3421_Sensor::printReading() {
    if (!debugEnabled) return;
    
    debugPrintf("MCP3421 Reading:\n");
    debugPrintf("  Raw Value: %ld\n", lastReading.rawValue);
    debugPrintf("  Voltage: %.6fV\n", lastReading.voltage);
    debugPrintf("  Config: 0x%02X\n", lastReading.config);
    debugPrintf("  Valid: %s\n", lastReading.valid ? "Yes" : "No");
    debugPrintf("  Timestamp: %lu ms\n", lastReading.timestamp);
}

// Private methods

bool MCP3421_Sensor::writeConfig() {
    if (!wire) return false;
    
    wire->beginTransmission(i2cAddress);
    wire->write(configRegister);
    uint8_t error = wire->endTransmission();
    
    if (debugEnabled && error != 0) {
        debugPrintf("MCP3421: Failed to write config 0x%02X (error: %d)\n", configRegister, error);
    }
    
    return error == 0;
}

bool MCP3421_Sensor::readData() {
    if (!wire) return false;
    
    int bytesToRead = (getResolutionBits() <= 16) ? 3 : 4;  // 2-3 data bytes + 1 config byte
    
    wire->requestFrom(i2cAddress, (uint8_t)bytesToRead);
    
    if (wire->available() < bytesToRead) {
        if (debugEnabled) {
            debugPrintf("MCP3421: Insufficient data received (%d < %d)\n", wire->available(), bytesToRead);
        }
        return false;
    }
    
    int32_t rawValue = 0;
    
    // Read data bytes (MSB first)
    if (bytesToRead == 4) {
        // 18-bit reading
        rawValue = (int32_t)wire->read() << 16;
        rawValue |= (int32_t)wire->read() << 8;
        rawValue |= (int32_t)wire->read();
        
        // Sign extend from 18 bits to 32 bits
        if (rawValue & 0x20000) {
            rawValue |= 0xFFFC0000;
        }
    } else {
        // 12, 14, or 16-bit reading
        rawValue = (int32_t)wire->read() << 8;
        rawValue |= (int32_t)wire->read();
        
        // Sign extend based on resolution
        int bits = getResolutionBits();
        int32_t signBit = 1 << (bits - 1);
        if (rawValue & signBit) {
            rawValue |= (0xFFFFFFFF << bits);
        }
    }
    
    // Read config byte
    uint8_t config = wire->read();
    
    // Update last reading
    lastReading.rawValue = rawValue;
    lastReading.voltage = convertToVoltage(rawValue);
    lastReading.config = config;
    lastReading.valid = true;
    lastReading.timestamp = millis();
    
    if (debugEnabled) {
        printReading();
    }
    
    return true;
}

uint8_t MCP3421_Sensor::calculateConfig() {
    uint8_t config = MCP3421_RDY_BIT;  // Start with ready bit set
    
    config |= sampleRate;      // Add sample rate bits
    config |= gain;            // Add gain bits
    config |= conversionMode;  // Add conversion mode bit
    
    return config;
}

float MCP3421_Sensor::convertToVoltage(int32_t rawValue) {
    int bits = getResolutionBits();
    int32_t maxValue = (1 << (bits - 1)) - 1;  // Maximum positive value
    float gainValue = 1 << gain;                // Convert gain enum to multiplier
    
    // Calculate voltage: (raw / max) * (Vref / gain)
    float voltage = ((float)rawValue / (float)maxValue) * (referenceVoltage / gainValue);
    
    return voltage;
}

int MCP3421_Sensor::getResolutionBits() const {
    switch (sampleRate) {
        case MCP3421_12_BIT_240_SPS: return 12;
        case MCP3421_14_BIT_60_SPS:  return 14;
        case MCP3421_16_BIT_15_SPS:  return 16;
        case MCP3421_18_BIT_3_75_SPS: return 18;
        default: return 16;
    }
}

float MCP3421_Sensor::getMaxVoltage() {
    float gainValue = 1 << gain;
    return referenceVoltage / gainValue;
}