#include "max6656_sensor.h"

extern void debugPrintf(const char* fmt, ...);

MAX6656Sensor::MAX6656Sensor(uint8_t address) :
    i2cAddress(address),
    initialized(false),
    extendedRange(false),
    localTempOffset(0.0),
    remoteTempOffset(0.0),
    filteringEnabled(false),
    filterSize(5),
    bufferIndex(0),
    validSamples(0)
{
    // Initialize filter buffers
    for (int i = 0; i < 10; i++) {
        localTempBuffer[i] = 0.0;
        remoteTempBuffer[i] = 0.0;
    }
}

bool MAX6656Sensor::begin() {
    debugPrintf("MAX6656: Initializing temperature sensor at address 0x%02X using Wire1", i2cAddress);
    
    // Initialize Wire1 for all I2C devices on Arduino R4 WiFi
    Wire1.begin();
    delay(50); // Give I2C time to initialize
    
    // Check if device is present
    if (!checkI2CDevice(i2cAddress)) {
        debugPrintf("MAX6656: Device not found at address 0x%02X", i2cAddress);
        return false;
    }
    
    // Verify manufacturer ID
    uint8_t manufacturerID = getManufacturerID();
    if (manufacturerID != 0x4D) {
        debugPrintf("MAX6656: Invalid manufacturer ID: 0x%02X (expected 0x4D)", manufacturerID);
        return false;
    }
    
    // Read revision
    uint8_t revision = getRevision();
    debugPrintf("MAX6656: Found device, revision: 0x%02X", revision);
    
    // Configure sensor for optimal operation
    // Set conversion rate to 2Hz (0.5 second conversion)
    if (!setConversionRate(MAX6656_CONV_RATE_2HZ)) {
        debugPrintf("MAX6656: Failed to set conversion rate");
        return false;
    }
    
    // Enable extended range (0-150°C instead of 0-127°C)
    if (!setExtendedRange(true)) {
        debugPrintf("MAX6656: Failed to set extended range");
        return false;
    }
    
    // Ensure device is not in standby mode
    if (!setStandbyMode(false)) {
        debugPrintf("MAX6656: Failed to exit standby mode");
        return false;
    }
    
    // Set reasonable temperature limits
    setLocalHighLimit(85);   // 85°C high limit
    setLocalLowLimit(-10);   // -10°C low limit
    setRemoteHighLimit(85);  // 85°C high limit
    setRemoteLowLimit(-10);  // -10°C low limit
    
    // Enable filtering by default
    enableFiltering(true, 5);
    
    initialized = true;
    debugPrintf("MAX6656: Temperature sensor initialized successfully on Wire1");
    
    return true;
}

bool MAX6656Sensor::isAvailable() {
    return initialized && checkI2CDevice(i2cAddress);
}

bool MAX6656Sensor::checkConnection() {
    if (!initialized) return false;
    
    // Try to read manufacturer ID to verify connection
    uint8_t manufacturerID = getManufacturerID();
    return (manufacturerID == 0x4D);
}

float MAX6656Sensor::getLocalTemperature() {
    if (!initialized) return -999.0;
    
    uint8_t tempReg = readRegister(MAX6656_REG_LOCAL_TEMP);
    uint8_t extReg = readRegister(MAX6656_REG_LOCAL_TEMP_EXT);
    
    float temperature = convertTemperature(tempReg, extReg);
    temperature += localTempOffset;
    
    if (filteringEnabled) {
        temperature = applyFiltering(localTempBuffer, temperature);
    }
    
    return temperature;
}

float MAX6656Sensor::getRemoteTemperature() {
    if (!initialized) return -999.0;
    
    uint8_t tempReg = readRegister(MAX6656_REG_REMOTE_TEMP);
    uint8_t extReg = readRegister(MAX6656_REG_REMOTE_TEMP_EXT);
    
    float temperature = convertTemperature(tempReg, extReg);
    temperature += remoteTempOffset;
    
    if (filteringEnabled) {
        temperature = applyFiltering(remoteTempBuffer, temperature);
    }
    
    return temperature;
}

float MAX6656Sensor::getLocalTemperatureExtended() {
    if (!initialized) return -999.0;
    
    uint8_t tempReg = readRegister(MAX6656_REG_LOCAL_TEMP);
    uint8_t extReg = readRegister(MAX6656_REG_LOCAL_TEMP_EXT);
    
    return convertTemperature(tempReg, extReg) + localTempOffset;
}

float MAX6656Sensor::getRemoteTemperatureExtended() {
    if (!initialized) return -999.0;
    
    uint8_t tempReg = readRegister(MAX6656_REG_REMOTE_TEMP);
    uint8_t extReg = readRegister(MAX6656_REG_REMOTE_TEMP_EXT);
    
    return convertTemperature(tempReg, extReg) + remoteTempOffset;
}

bool MAX6656Sensor::setConversionRate(uint8_t rate) {
    if (!initialized || rate > 7) return false;
    
    return writeRegister(MAX6656_REG_CONV_RATE, rate);
}

bool MAX6656Sensor::setExtendedRange(bool enabled) {
    if (!initialized) return false;
    
    uint8_t config = readRegister(MAX6656_REG_CONFIG);
    if (enabled) {
        config |= MAX6656_CONFIG_RANGE;
    } else {
        config &= ~MAX6656_CONFIG_RANGE;
    }
    
    extendedRange = enabled;
    return writeRegister(MAX6656_REG_CONFIG, config);
}

bool MAX6656Sensor::setStandbyMode(bool enabled) {
    if (!initialized) return false;
    
    uint8_t config = readRegister(MAX6656_REG_CONFIG);
    if (enabled) {
        config |= MAX6656_CONFIG_STANDBY;
    } else {
        config &= ~MAX6656_CONFIG_STANDBY;
    }
    
    return writeRegister(MAX6656_REG_CONFIG, config);
}

uint8_t MAX6656Sensor::getStatus() {
    if (!initialized) return 0xFF;
    
    return readRegister(MAX6656_REG_STATUS);
}

bool MAX6656Sensor::isLocalTempFault() {
    uint8_t status = getStatus();
    return (status & (MAX6656_STATUS_LOCAL_HIGH | MAX6656_STATUS_LOCAL_LOW)) != 0;
}

bool MAX6656Sensor::isRemoteTempFault() {
    uint8_t status = getStatus();
    return (status & (MAX6656_STATUS_REMOTE_HIGH | MAX6656_STATUS_REMOTE_LOW)) != 0;
}

bool MAX6656Sensor::isRemoteDiodeFault() {
    uint8_t status = getStatus();
    return (status & (MAX6656_STATUS_REMOTE_OPEN | MAX6656_STATUS_REMOTE_SHORT)) != 0;
}

bool MAX6656Sensor::isBusy() {
    uint8_t status = getStatus();
    return (status & MAX6656_STATUS_BUSY) != 0;
}

bool MAX6656Sensor::setLocalHighLimit(int8_t temperature) {
    if (!initialized) return false;
    return writeRegister(MAX6656_REG_LOCAL_HIGH, (uint8_t)temperature);
}

bool MAX6656Sensor::setLocalLowLimit(int8_t temperature) {
    if (!initialized) return false;
    return writeRegister(MAX6656_REG_LOCAL_LOW, (uint8_t)temperature);
}

bool MAX6656Sensor::setRemoteHighLimit(int8_t temperature) {
    if (!initialized) return false;
    return writeRegister(MAX6656_REG_REMOTE_HIGH, (uint8_t)temperature);
}

bool MAX6656Sensor::setRemoteLowLimit(int8_t temperature) {
    if (!initialized) return false;
    return writeRegister(MAX6656_REG_REMOTE_LOW, (uint8_t)temperature);
}

bool MAX6656Sensor::setRemoteOffset(int8_t offset) {
    if (!initialized) return false;
    return writeRegister(MAX6656_REG_REMOTE_OFFSET, (uint8_t)offset);
}

uint8_t MAX6656Sensor::getManufacturerID() {
    return readRegister(MAX6656_REG_MANUFACTURER_ID);
}

uint8_t MAX6656Sensor::getRevision() {
    return readRegister(MAX6656_REG_REVISION);
}

void MAX6656Sensor::getStatusString(char* buffer, size_t bufferSize) {
    if (!initialized) {
        snprintf(buffer, bufferSize, "MAX6656: Not initialized");
        return;
    }
    
    uint8_t status = getStatus();
    float localTemp = getLocalTemperature();
    float remoteTemp = getRemoteTemperature();
    
    snprintf(buffer, bufferSize, 
        "MAX6656: Local=%.1f°C Remote=%.1f°C Status=0x%02X %s", 
        localTemp, remoteTemp, status,
        (status == 0) ? "OK" : "FAULT");
}

void MAX6656Sensor::getDetailedStatus(char* buffer, size_t bufferSize) {
    if (!initialized) {
        snprintf(buffer, bufferSize, "MAX6656: Not initialized");
        return;
    }
    
    uint8_t status = getStatus();
    uint8_t config = readRegister(MAX6656_REG_CONFIG);
    uint8_t convRate = readRegister(MAX6656_REG_CONV_RATE);
    float localTemp = getLocalTemperature();
    float remoteTemp = getRemoteTemperature();
    
    snprintf(buffer, bufferSize, 
        "MAX6656 Detail: Local=%.2f°C Remote=%.2f°C Status=0x%02X Config=0x%02X Rate=%d %s%s%s%s",
        localTemp, remoteTemp, status, config, convRate,
        (status & MAX6656_STATUS_BUSY) ? "BUSY " : "",
        (status & (MAX6656_STATUS_LOCAL_HIGH | MAX6656_STATUS_LOCAL_LOW)) ? "LOCAL_FAULT " : "",
        (status & (MAX6656_STATUS_REMOTE_HIGH | MAX6656_STATUS_REMOTE_LOW)) ? "REMOTE_FAULT " : "",
        (status & (MAX6656_STATUS_REMOTE_OPEN | MAX6656_STATUS_REMOTE_SHORT)) ? "DIODE_FAULT " : "");
}

void MAX6656Sensor::setTemperatureOffset(float localOffset, float remoteOffset) {
    localTempOffset = localOffset;
    remoteTempOffset = remoteOffset;
    debugPrintf("MAX6656: Set temperature offsets - Local: %.2f°C, Remote: %.2f°C", 
               localOffset, remoteOffset);
}

void MAX6656Sensor::enableFiltering(bool enabled, uint8_t size) {
    filteringEnabled = enabled;
    if (size > 0 && size <= 10) {
        filterSize = size;
    }
    
    if (enabled) {
        // Reset filter buffers
        bufferIndex = 0;
        validSamples = 0;
        for (int i = 0; i < 10; i++) {
            localTempBuffer[i] = 0.0;
            remoteTempBuffer[i] = 0.0;
        }
    }
    
    debugPrintf("MAX6656: Temperature filtering %s (size: %d)", 
               enabled ? "enabled" : "disabled", filterSize);
}

bool MAX6656Sensor::checkI2CDevice(uint8_t address) {
    Wire1.beginTransmission(address);
    uint8_t error = Wire1.endTransmission();
    return (error == 0);
}

bool MAX6656Sensor::writeRegister(uint8_t reg, uint8_t value) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(reg);
    Wire1.write(value);
    uint8_t error = Wire1.endTransmission();
    return (error == 0);
}

uint8_t MAX6656Sensor::readRegister(uint8_t reg) {
    Wire1.beginTransmission(i2cAddress);
    Wire1.write(reg);
    if (Wire1.endTransmission() != 0) {
        return 0xFF; // Error reading
    }
    
    Wire1.requestFrom(i2cAddress, (uint8_t)1);
    if (Wire1.available()) {
        return Wire1.read();
    }
    
    return 0xFF; // Error reading
}

float MAX6656Sensor::convertTemperature(uint8_t tempReg, uint8_t extReg) {
    // Convert temperature register to Celsius
    // Main register is in whole degrees
    // Extended register provides 0.125°C resolution in upper 3 bits
    
    float temperature = (float)(int8_t)tempReg;
    
    if (extReg != 0) {
        // Add fractional part from extended register
        float fraction = (extReg >> 5) * 0.125;
        temperature += fraction;
    }
    
    return temperature;
}

float MAX6656Sensor::applyFiltering(float* buffer, float newValue) {
    updateBuffer(buffer, newValue);
    return calculateAverage(buffer, min(validSamples, filterSize));
}

void MAX6656Sensor::updateBuffer(float* buffer, float newValue) {
    buffer[bufferIndex] = newValue;
    bufferIndex = (bufferIndex + 1) % filterSize;
    if (validSamples < filterSize) {
        validSamples++;
    }
}

float MAX6656Sensor::calculateAverage(float* buffer, uint8_t samples) {
    if (samples == 0) return 0.0;
    
    float sum = 0.0;
    for (uint8_t i = 0; i < samples; i++) {
        sum += buffer[i];
    }
    
    return sum / samples;
}