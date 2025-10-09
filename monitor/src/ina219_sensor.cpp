#include "ina219_sensor.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);

// Global instance pointer
INA219_Sensor* g_ina219Sensor = nullptr;

// MQTT topics for INA219 data
const char TOPIC_MONITOR_POWER_VOLTAGE[] = "r4/monitor/power/voltage";
const char TOPIC_MONITOR_POWER_CURRENT[] = "r4/monitor/power/current"; 
const char TOPIC_MONITOR_POWER_POWER[] = "r4/monitor/power/power";
const char TOPIC_MONITOR_POWER_STATUS[] = "r4/monitor/power/status";

INA219_Sensor::INA219_Sensor(uint8_t address) :
    wire(nullptr),
    i2cAddress(address),
    initialized(false),
    debugEnabled(false),
    currentRange(INA219_Range::RANGE_32V_2A),
    shuntResistor(0.1f),
    currentLSB(0.1f),
    calibrationValue(0)
{
    lastReading.valid = false;
    lastReading.timestamp = 0;
}

bool INA219_Sensor::begin(TwoWire& wireInterface) {
    return begin(INA219_Range::RANGE_32V_2A, wireInterface);
}

bool INA219_Sensor::begin(INA219_Range range, TwoWire& wireInterface) {
    wire = &wireInterface;
    currentRange = range;
    
    debugPrint("INA219: Initializing sensor");
    
    // Test I2C connection first
    if (!testConnection()) {
        debugPrint("INA219: Connection test failed");
        return false;
    }
    
    // Reset device
    if (!reset()) {
        debugPrint("INA219: Reset failed");
        return false;
    }
    
    // Configure for the specified range
    if (!setRange(range)) {
        debugPrint("INA219: Range configuration failed");
        return false;
    }
    
    initialized = true;
    debugPrint("INA219: Initialization successful");
    
    return true;
}

bool INA219_Sensor::reset() {
    debugPrint("INA219: Performing reset");
    return writeRegister(INA219_REG_CONFIG, INA219_CONFIG_RESET);
}

bool INA219_Sensor::setRange(INA219_Range range) {
    currentRange = range;
    uint16_t config = getRangeConfig(range);
    
    debugPrint("INA219: Setting range configuration");
    
    if (!writeRegister(INA219_REG_CONFIG, config)) {
        debugPrint("INA219: Failed to write configuration");
        return false;
    }
    
    // Calculate and set calibration
    calculateCalibration();
    
    if (!writeRegister(INA219_REG_CALIBRATION, calibrationValue)) {
        debugPrint("INA219: Failed to write calibration");
        return false;
    }
    
    debugPrintf("INA219: Range set, calibration=0x%04X\\n", calibrationValue);
    return true;
}

bool INA219_Sensor::setCalibration(float shuntResistor_ohms, float maxCurrent_A) {
    shuntResistor = shuntResistor_ohms;
    currentLSB = calculateCurrentLSB(maxCurrent_A);
    calibrationValue = calculateCalibrationValue(currentLSB, shuntResistor);
    
    debugPrintf("INA219: Custom calibration - R=%.3f ohm, MaxI=%.2fA, LSB=%.3fmA\\n", 
                shuntResistor, maxCurrent_A, currentLSB * 1000.0f);
    
    return writeRegister(INA219_REG_CALIBRATION, calibrationValue);
}

bool INA219_Sensor::takereading() {
    if (!initialized || !wire) {
        lastReading.valid = false;
        return false;
    }
    
    uint16_t busVoltageRaw, shuntVoltageRaw, currentRaw, powerRaw;
    
    // Read all registers
    if (!readRegister(INA219_REG_BUSVOLTAGE, busVoltageRaw) ||
        !readRegister(INA219_REG_SHUNTVOLTAGE, shuntVoltageRaw) ||
        !readRegister(INA219_REG_CURRENT, currentRaw) ||
        !readRegister(INA219_REG_POWER, powerRaw)) {
        
        lastReading.valid = false;
        debugPrint("INA219: Failed to read registers");
        return false;
    }
    
    // Convert raw values
    lastReading.busVoltage_V = convertBusVoltage(busVoltageRaw);
    lastReading.shuntVoltage_mV = convertShuntVoltage(shuntVoltageRaw);
    lastReading.current_mA = convertCurrent(currentRaw);
    lastReading.power_mW = convertPower(powerRaw);
    lastReading.timestamp = millis();
    lastReading.valid = true;
    
    if (debugEnabled) {
        debugPrintf("INA219: V=%.3fV, I=%.1fmA, P=%.1fmW\\n", 
                    lastReading.busVoltage_V, lastReading.current_mA, lastReading.power_mW);
    }
    
    return true;
}

float INA219_Sensor::getBusVoltage() {
    if (!takereading()) return 0.0f;
    return lastReading.busVoltage_V;
}

float INA219_Sensor::getShuntVoltage() {
    if (!takereading()) return 0.0f;
    return lastReading.shuntVoltage_mV;
}

float INA219_Sensor::getCurrent() {
    if (!takereading()) return 0.0f;
    return lastReading.current_mA;
}

float INA219_Sensor::getPower() {
    if (!takereading()) return 0.0f;
    return lastReading.power_mW;
}

bool INA219_Sensor::isConnected() {
    return testConnection();
}

void INA219_Sensor::getStatusString(char* buffer, size_t bufferSize) {
    if (!initialized) {
        snprintf(buffer, bufferSize, "INA219: Not initialized");
        return;
    }
    
    if (!lastReading.valid) {
        snprintf(buffer, bufferSize, "INA219: No valid reading (addr=0x%02X)", i2cAddress);
        return;
    }
    
    unsigned long age = millis() - lastReading.timestamp;
    snprintf(buffer, bufferSize, 
        "INA219: %.2fV %.1fmA %.1fmW (addr=0x%02X, age=%lums)",
        lastReading.busVoltage_V, lastReading.current_mA, 
        lastReading.power_mW, i2cAddress, age);
}

bool INA219_Sensor::setBusVoltageRange(bool range32V) {
    uint16_t config;
    if (!readRegister(INA219_REG_CONFIG, config)) {
        return false;
    }
    
    config &= ~INA219_CONFIG_BVOLTAGERANGE_MASK;
    if (range32V) {
        config |= INA219_CONFIG_BVOLTAGERANGE_32V;
    }
    
    return writeRegister(INA219_REG_CONFIG, config);
}

bool INA219_Sensor::setShuntGain(uint8_t gain) {
    uint16_t config;
    if (!readRegister(INA219_REG_CONFIG, config)) {
        return false;
    }
    
    config &= ~INA219_CONFIG_GAIN_MASK;
    config |= (gain << 11) & INA219_CONFIG_GAIN_MASK;
    
    return writeRegister(INA219_REG_CONFIG, config);
}

bool INA219_Sensor::setOperatingMode(uint8_t mode) {
    uint16_t config;
    if (!readRegister(INA219_REG_CONFIG, config)) {
        return false;
    }
    
    config &= ~INA219_CONFIG_MODE_MASK;
    config |= mode & INA219_CONFIG_MODE_MASK;
    
    return writeRegister(INA219_REG_CONFIG, config);
}

float INA219_Sensor::calculateCurrentLSB(float maxCurrent_A) {
    // Current LSB = Maximum Expected Current / 32767
    return maxCurrent_A / 32767.0f;
}

uint16_t INA219_Sensor::calculateCalibrationValue(float currentLSB, float shuntResistor_ohms) {
    // Calibration = 0.04096 / (currentLSB * shuntResistor)
    float calibration = 0.04096f / (currentLSB * shuntResistor_ohms);
    return (uint16_t)calibration;
}

bool INA219_Sensor::testConnection() {
    if (!wire) return false;
    
    wire->beginTransmission(i2cAddress);
    uint8_t error = wire->endTransmission();
    
    if (error == 0) {
        debugPrintf("INA219: Connected at address 0x%02X\\n", i2cAddress);
        return true;
    } else {
        debugPrintf("INA219: Connection failed at 0x%02X, error=%d\\n", i2cAddress, error);
        return false;
    }
}

void INA219_Sensor::dumpRegisters() {
    if (!wire) return;
    
    debugPrint("INA219: Register dump:");
    
    for (uint8_t reg = 0; reg <= 5; reg++) {
        uint16_t value = readRegister(reg);
        debugPrintf("  Reg 0x%02X: 0x%04X\\n", reg, value);
    }
}

bool INA219_Sensor::writeRegister(uint8_t reg, uint16_t value) {
    if (!wire) return false;
    
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    wire->write((value >> 8) & 0xFF);  // MSB first
    wire->write(value & 0xFF);         // LSB second
    uint8_t error = wire->endTransmission();
    
    if (error != 0) {
        debugPrintf("INA219: Write error %d to reg 0x%02X\\n", error, reg);
        return false;
    }
    
    return true;
}

uint16_t INA219_Sensor::readRegister(uint8_t reg) {
    uint16_t value;
    if (readRegister(reg, value)) {
        return value;
    }
    return 0;
}

bool INA219_Sensor::readRegister(uint8_t reg, uint16_t& value) {
    if (!wire) return false;
    
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    uint8_t error = wire->endTransmission();
    
    if (error != 0) {
        debugPrintf("INA219: Read setup error %d for reg 0x%02X\\n", error, reg);
        return false;
    }
    
    uint8_t bytesReceived = wire->requestFrom(i2cAddress, (uint8_t)2);
    if (bytesReceived != 2) {
        debugPrintf("INA219: Read error, got %d bytes for reg 0x%02X\\n", bytesReceived, reg);
        return false;
    }
    
    uint8_t msb = wire->read();
    uint8_t lsb = wire->read();
    value = (msb << 8) | lsb;
    
    return true;
}

uint16_t INA219_Sensor::getRangeConfig(INA219_Range range) {
    uint16_t config = INA219_CONFIG_BADCRES_12BIT |    // 12-bit bus ADC
                      INA219_CONFIG_SADCRES_12BIT_1S |  // 12-bit shunt ADC
                      INA219_CONFIG_MODE_SANDBVOLT_CONTINUOUS; // Continuous mode
    
    switch (range) {
        case INA219_Range::RANGE_32V_2A:
        default:
            config |= INA219_CONFIG_BVOLTAGERANGE_32V |
                      INA219_CONFIG_GAIN_8_320MV;
            shuntResistor = 0.1f;
            currentLSB = 0.061f; // 61µA per bit for ±2A range
            break;
            
        case INA219_Range::RANGE_32V_1A:
            config |= INA219_CONFIG_BVOLTAGERANGE_32V |
                      INA219_CONFIG_GAIN_4_160MV;
            shuntResistor = 0.1f;
            currentLSB = 0.0305f; // 30.5µA per bit for ±1A range
            break;
            
        case INA219_Range::RANGE_16V_2A:
            config |= INA219_CONFIG_BVOLTAGERANGE_16V |
                      INA219_CONFIG_GAIN_8_320MV;
            shuntResistor = 0.1f;
            currentLSB = 0.061f;
            break;
            
        case INA219_Range::RANGE_16V_1A:
            config |= INA219_CONFIG_BVOLTAGERANGE_16V |
                      INA219_CONFIG_GAIN_4_160MV;
            shuntResistor = 0.1f;
            currentLSB = 0.0305f;
            break;
            
        case INA219_Range::RANGE_16V_400MA:
            config |= INA219_CONFIG_BVOLTAGERANGE_16V |
                      INA219_CONFIG_GAIN_1_40MV;
            shuntResistor = 0.1f;
            currentLSB = 0.012f; // 12µA per bit for ±400mA range
            break;
    }
    
    return config;
}

void INA219_Sensor::calculateCalibration() {
    calibrationValue = calculateCalibrationValue(currentLSB, shuntResistor);
}

void INA219_Sensor::debugPrint(const char* message) {
    if (debugEnabled) {
        debugPrintf("%s\\n", message);
    }
}

float INA219_Sensor::convertBusVoltage(uint16_t raw) {
    // Bus voltage = (raw >> 3) * 4mV
    return ((raw >> 3) * 4) / 1000.0f; // Convert to volts
}

float INA219_Sensor::convertShuntVoltage(uint16_t raw) {
    // Shunt voltage = raw * 10µV (signed 16-bit)
    int16_t signed_raw = (int16_t)raw;
    return (signed_raw * 10) / 1000.0f; // Convert to millivolts
}

float INA219_Sensor::convertCurrent(uint16_t raw) {
    // Current = raw * currentLSB (signed 16-bit)
    int16_t signed_raw = (int16_t)raw;
    return signed_raw * currentLSB * 1000.0f; // Convert to milliamps
}

float INA219_Sensor::convertPower(uint16_t raw) {
    // Power = raw * currentLSB * 20 (unsigned 16-bit)
    return raw * currentLSB * 20.0f * 1000.0f; // Convert to milliwatts
}