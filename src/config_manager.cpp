#include "config_manager.h"

bool ConfigManager::isValidConfig(const CalibrationData& data) {
    // Check magic number
    if (data.magic != CALIB_MAGIC) {
        return false;
    }
    
    // Validate ranges for critical parameters
    if (data.adcVref <= 0.0f || data.adcVref > 5.0f) return false;
    if (data.maxPressurePsi <= 0.0f || data.maxPressurePsi > 10000.0f) return false;
    if (data.sensorGain <= 0.0f || data.sensorGain > 100.0f) return false;
    if (data.filterMode > 2) return false;
    if (data.emaAlpha <= 0.0f || data.emaAlpha > 1.0f) return false;
    
    // Validate timing parameters
    if (data.seqStableMs > 10000 || data.seqStartStableMs > 10000) return false;
    if (data.seqTimeoutMs < 1000 || data.seqTimeoutMs > 600000) return false;
    
    return true;
}

void ConfigManager::setDefaults() {
    config.magic = CALIB_MAGIC;
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
    
    // Set default pin modes (pins 6,7 as NC by default for limit switches)
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        pinIsNC[i] = (WATCH_PINS[i] == 6 || WATCH_PINS[i] == 7);
    }
    
    configValid = true;
    Serial.println("ConfigManager: Using default settings");
}

void ConfigManager::begin() {
    Serial.println("ConfigManager: Initializing...");
    
    // Try to load from EEPROM, use defaults if invalid
    if (!load()) {
        setDefaults();
        save(); // Save defaults to EEPROM
    }
}

bool ConfigManager::load() {
    CalibrationData tempConfig;
    EEPROM.get(CALIB_EEPROM_ADDR, tempConfig);
    
    if (isValidConfig(tempConfig)) {
        config = tempConfig;
        configValid = true;
        
        // Restore pin modes from bitmap
        for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
            pinIsNC[i] = (config.pinModesBitmap & (1 << i)) != 0;
        }
        
        Serial.println("ConfigManager: Loaded valid configuration from EEPROM");
        return true;
    } else {
        Serial.println("ConfigManager: No valid configuration found in EEPROM");
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
    
    EEPROM.put(CALIB_EEPROM_ADDR, config);
    
    Serial.println("ConfigManager: Configuration saved to EEPROM");
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