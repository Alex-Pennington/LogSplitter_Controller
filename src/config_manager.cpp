#include "config_manager.h"
#include "pressure_manager.h"
// NetworkManager include removed - non-networking version
#include "system_error_manager.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);

// CRC32 lookup table (IEEE 802.3 polynomial: 0xEDB88320)
static const uint32_t crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3,
    0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91,
    0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5,
    0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B,
    0x35B5A8FA, 0x42B2986C, 0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D,
    0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D, 0x91646C97, 0xE6635C01,
    0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E, 0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457,
    0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB,
    0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81, 0xB7BD5C3B, 0xC0BA6CAD,
    0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683,
    0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7,
    0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5,
    0xD6D6A3E8, 0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF, 0x4669BE79,
    0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F,
    0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713,
    0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21,
    0x86D3D2D4, 0xF1D4E242, 0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB,
    0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693, 0x54DE5729, 0x23D967BF,
    0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94, 0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t ConfigManager::calculateCRC32(const CalibrationData& data) {
    uint32_t crc = 0xFFFFFFFF;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&data);
    
    // Calculate CRC for all data except the crc32 field itself
    size_t dataSize = sizeof(CalibrationData) - sizeof(uint32_t);
    for (size_t i = 0; i < dataSize; i++) {
        crc = crc32_table[(crc ^ bytes[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

bool ConfigManager::validateCRC(const CalibrationData& data) {
    uint32_t calculatedCRC = calculateCRC32(data);
    return calculatedCRC == data.crc32;
}

void ConfigManager::updateCRC(CalibrationData& data) {
    data.crc32 = calculateCRC32(data);
}

bool ConfigManager::isValidConfig(const CalibrationData& data) {
    // Check magic number first (fast check)
    if (data.magic != CALIB_MAGIC) {
        return false;
    }
    
    // Validate CRC32 checksum (critical integrity check)
    if (!validateCRC(data)) {
        Serial.println("ConfigManager: CRC32 validation failed");
        return false;
    }
    
    // Validate ranges for legacy parameters
    if (data.adcVref <= 0.0f || data.adcVref > 5.0f) return false;
    if (data.maxPressurePsi <= 0.0f || data.maxPressurePsi > 10000.0f) return false;
    if (data.sensorGain <= 0.0f || data.sensorGain > 100.0f) return false;
    if (data.filterMode > 2) return false;
    if (data.emaAlpha <= 0.0f || data.emaAlpha > 1.0f) return false;
    
    // Validate individual sensor parameters - A1 (System Pressure)
    if (data.a1_adcVref <= 0.0f || data.a1_adcVref > 5.0f) return false;
    if (data.a1_maxPressurePsi <= 0.0f || data.a1_maxPressurePsi > 10000.0f) return false;
    if (data.a1_sensorGain <= 0.0f || data.a1_sensorGain > 100.0f) return false;
    if (data.a1_sensorOffset < -1000.0f || data.a1_sensorOffset > 1000.0f) return false;
    
    // Validate individual sensor parameters - A5 (Filter Pressure)
    if (data.a5_adcVref <= 0.0f || data.a5_adcVref > 5.0f) return false;
    if (data.a5_maxPressurePsi <= 0.0f || data.a5_maxPressurePsi > 1000.0f) return false;
    if (data.a5_sensorGain <= 0.0f || data.a5_sensorGain > 100.0f) return false;
    if (data.a5_sensorOffset < -1000.0f || data.a5_sensorOffset > 1000.0f) return false;
    
    // Validate timing parameters
    if (data.seqStableMs > 10000 || data.seqStartStableMs > 10000) return false;
    if (data.seqTimeoutMs < 1000 || data.seqTimeoutMs > 600000) return false;
    
    return true;
}

void ConfigManager::setDefaults() {
    config.magic = CALIB_MAGIC;
    // Legacy single sensor parameters (kept for compatibility)
    config.adcVref = DEFAULT_ADC_VREF;
    config.maxPressurePsi = DEFAULT_MAX_PRESSURE_PSI;
    config.sensorGain = DEFAULT_SENSOR_GAIN;
    config.sensorOffset = DEFAULT_SENSOR_OFFSET;
    config.filterMode = (uint8_t)FILTER_MEDIAN3;
    config.emaAlpha = 0.2f;
    config.pinModesBitmap = 0; // All pins default to NO (normally open)
    config.seqStableMs = DEFAULT_SEQUENCE_STABLE_MS;
    config.seqStartStableMs = DEFAULT_SEQUENCE_START_STABLE_MS;
    config.seqTimeoutMs = DEFAULT_SEQUENCE_TIMEOUT_MS;
    config.relayEcho = true;
    config.debugEnabled = false;  // Debug disabled by default
    config.logLevel = 6;  // Default to INFO level (LOG_INFO)
    
    // Individual sensor defaults - A1 (System Pressure, 4-20mA)
    config.a1_maxPressurePsi = DEFAULT_A1_MAX_PRESSURE_PSI;
    config.a1_adcVref = DEFAULT_A1_ADC_VREF;
    config.a1_sensorGain = DEFAULT_A1_SENSOR_GAIN;
    config.a1_sensorOffset = DEFAULT_A1_SENSOR_OFFSET;
    
    // Individual sensor defaults - A5 (Filter Pressure, 0-4.5V)
    config.a5_maxPressurePsi = DEFAULT_A5_MAX_PRESSURE_PSI;
    config.a5_adcVref = DEFAULT_A5_ADC_VREF;
    config.a5_sensorGain = DEFAULT_A5_SENSOR_GAIN;
    config.a5_sensorOffset = DEFAULT_A5_SENSOR_OFFSET;
    
    // Set default pin modes 
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        uint8_t pin = WATCH_PINS[i];
        // E-Stop (pin 12) is NC, all others are NO
        pinIsNC[i] = (pin == 12);
    }
    
    // Calculate and set CRC32 for the default configuration
    updateCRC(config);
    
    configValid = true;
    Serial.println("ConfigManager: Using default settings with individual sensor parameters");
}

void ConfigManager::begin() {
    Serial.println("ConfigManager: Initializing with CRC32 validation...");
    
    // Try to load from EEPROM, use defaults if invalid
    if (!load()) {
        Serial.println("ConfigManager: EEPROM validation failed - loading defaults and saving");
        setDefaults();
        if (save()) {
            Serial.println("ConfigManager: Default configuration saved to EEPROM with CRC32");
        } else {
            Serial.println("ConfigManager: WARNING - Failed to save defaults to EEPROM");
            publishError("EEPROM: Failed to save default configuration");
        }
    }
}

bool ConfigManager::load() {
    CalibrationData tempConfig;
    EEPROM.get(CALIB_EEPROM_ADDR, tempConfig);
    
    // Check magic number first (fast check)
    if (tempConfig.magic != CALIB_MAGIC) {
        Serial.println("ConfigManager: Invalid magic number in EEPROM - loading defaults");
        publishError("EEPROM: Invalid magic number - using defaults");
        return false;
    }
    
    // Validate CRC32 checksum
    if (!validateCRC(tempConfig)) {
        Serial.println("ConfigManager: EEPROM CRC32 validation failed - data corrupted");
        publishError("EEPROM: CRC32 validation failed - data corrupted, using defaults");
        return false;
    }
    
    // Additional parameter validation
    if (isValidConfig(tempConfig)) {
        config = tempConfig;
        configValid = true;
        
        // Restore pin modes from bitmap
        for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
            pinIsNC[i] = (config.pinModesBitmap & (1 << i)) != 0;
        }
        
        Serial.println("ConfigManager: Loaded valid configuration from EEPROM with CRC32 verification");
        return true;
    } else {
        Serial.println("ConfigManager: EEPROM configuration parameters out of range");
        publishError("EEPROM: Configuration parameters invalid - using defaults");
        configValid = false;
        return false;
    }
}

bool ConfigManager::save() {
    if (!configValid) {
        Serial.println("ConfigManager: Cannot save invalid configuration");
        return false;
    }
    
    // Pack pin modes into bitmap
    config.pinModesBitmap = 0;
    for (size_t i = 0; i < WATCH_PIN_COUNT && i < 8; i++) {
        if (pinIsNC[i]) {
            config.pinModesBitmap |= (1 << i);
        }
    }
    
    // Calculate and update CRC32 before saving
    updateCRC(config);
    
    EEPROM.put(CALIB_EEPROM_ADDR, config);
    
    Serial.println("ConfigManager: Configuration saved to EEPROM with CRC32 verification");
    
    // Network publishing removed - non-networking version
    
    return true;
}

void ConfigManager::setPinMode(uint8_t pin, bool isNC) {
    // Find pin in watch list
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        if (WATCH_PINS[i] == pin) {
            pinIsNC[i] = isNC;
            Serial.print("Pin ");
            Serial.print(pin);
            Serial.print(" mode set to ");
            Serial.println(isNC ? "NC" : "NO");
            return;
        }
    }
    
    Serial.print("Pin ");
    Serial.print(pin);
    Serial.println(" not found in watch list");
}

bool ConfigManager::getPinMode(uint8_t pin) const {
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        if (WATCH_PINS[i] == pin) {
            return pinIsNC[i];
        }
    }
    return false; // Default to NO if not found
}

void ConfigManager::applyToPressureSensor(PressureSensor& sensor) {
    if (!configValid) return;
    
    sensor.setAdcVref(config.adcVref);
    sensor.setMaxPressure(config.maxPressurePsi);
    sensor.setSensorGain(config.sensorGain);
    sensor.setSensorOffset(config.sensorOffset);
    sensor.setFilterMode((FilterMode)config.filterMode);
    sensor.setEmaAlpha(config.emaAlpha);
}

void ConfigManager::applyToPressureManager(class PressureManager& manager) {
    if (!configValid) return;
    
    // Apply individual sensor configurations
    auto& sensorA1 = manager.getSensor(SENSOR_HYDRAULIC);
    sensorA1.setMaxPressure(config.a1_maxPressurePsi);
    sensorA1.setAdcVref(config.a1_adcVref);
    sensorA1.setSensorGain(config.a1_sensorGain);
    sensorA1.setSensorOffset(config.a1_sensorOffset);
    
    auto& sensorA5 = manager.getSensor(SENSOR_HYDRAULIC_OIL);
    sensorA5.setMaxPressure(config.a5_maxPressurePsi);
    sensorA5.setAdcVref(config.a5_adcVref);
    sensorA5.setSensorGain(config.a5_sensorGain);
    sensorA5.setSensorOffset(config.a5_sensorOffset);
    
    Serial.println("ConfigManager: Applied individual sensor configurations");
}

void ConfigManager::publishError(const char* errorMessage) {
    // Print to serial console
    Serial.print("ConfigManager ERROR: ");
    Serial.println(errorMessage);
    
    // Set system error LED if error manager is available
    if (systemErrorManager) {
        if (strstr(errorMessage, "CRC32") != nullptr) {
            systemErrorManager->setError(ERROR_EEPROM_CRC, errorMessage);
        } else if (strstr(errorMessage, "save") != nullptr) {
            systemErrorManager->setError(ERROR_EEPROM_SAVE, errorMessage);
        } else {
            systemErrorManager->setError(ERROR_CONFIG_INVALID, errorMessage);
        }
    }
    
    // Network publishing removed - non-networking version
}

void ConfigManager::applyToSequenceController(SequenceController& controller) {
    if (!configValid) return;
    
    controller.setStableTime(config.seqStableMs);
    controller.setStartStableTime(config.seqStartStableMs);
    controller.setTimeout(config.seqTimeoutMs);
}

void ConfigManager::applyToRelayController(RelayController& relay) {
    if (!configValid) return;
    
    relay.setEcho(config.relayEcho);
}

void ConfigManager::applyToLogger() {
    if (!configValid) return;
    
    Logger::setLogLevel(static_cast<LogLevel>(config.logLevel));
}

void ConfigManager::loadFromPressureSensor(const PressureSensor& sensor) {
    config.adcVref = sensor.getAdcVref();
    config.maxPressurePsi = sensor.getMaxPressure();
    config.sensorGain = sensor.getSensorGain();
    config.sensorOffset = sensor.getSensorOffset();
    config.filterMode = (uint8_t)sensor.getFilterMode();
    config.emaAlpha = sensor.getEmaAlpha();
}

void ConfigManager::loadFromSequenceController(const SequenceController& controller) {
    config.seqStableMs = controller.getStableTime();
    config.seqStartStableMs = controller.getStartStableTime();
    config.seqTimeoutMs = controller.getTimeout();
}

void ConfigManager::loadFromRelayController(const RelayController& relay) {
    config.relayEcho = relay.getEcho();
}

void ConfigManager::getStatusString(char* buffer, size_t bufferSize) {
    const char* filterName = "unknown";
    switch (config.filterMode) {
        case 0: filterName = "none"; break;
        case 1: filterName = "median3"; break;
        case 2: filterName = "ema"; break;
    }
    
    snprintf(buffer, bufferSize,
        "valid=%s adcVref=%.3f maxPsi=%.1f gain=%.3f offset=%.3f filter=%s emaAlpha=%.3f seqStable=%lu seqStart=%lu seqTimeout=%lu",
        configValid ? "yes" : "no",
        config.adcVref,
        config.maxPressurePsi,
        config.sensorGain,
        config.sensorOffset,
        filterName,
        config.emaAlpha,
        (unsigned long)config.seqStableMs,
        (unsigned long)config.seqStartStableMs,
        (unsigned long)config.seqTimeoutMs
    );
}

void ConfigManager::publishAllConfigParameters() {
    // Network publishing removed - non-networking version
    // Configuration is available via getStatusString() for serial output
    debugPrintf("Configuration publishing disabled - non-networking version\n");
}

bool ConfigManager::queryMqttForDefaults(unsigned long timeoutMs) {
    // Network querying removed - non-networking version
    debugPrintf("ConfigManager: MQTT query disabled - using firmware defaults\n");
    setDefaults();
    
    return save(); // Save the defaults to EEPROM
}