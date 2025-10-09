#include "nau7802_sensor.h"
#include <EEPROM.h>

extern void debugPrintf(const char* fmt, ...);

NAU7802Sensor::NAU7802Sensor() :
    lastError(NAU7802_OK),
    calibrationFactor(1.0),
    zeroOffset(0),
    isCalibrated(false),
    lastRawReading(0),
    lastWeight(0.0),
    lastFilteredWeight(0.0),
    filteringEnabled(false),
    filterSamples(10),
    filterBuffer(nullptr),
    filterIndex(0),
    filterBufferFull(false),
    currentGain(NAU7802_GAIN_128),
    currentSampleRate(NAU7802_SPS_10),
    totalReadings(0),
    minReading(0),
    maxReading(0) {
    
    initializeDefaults();
}

NAU7802Status NAU7802Sensor::begin() {
    debugPrintf("NAU7802: Initializing sensor...\n");
    
    // Initialize I2C on Wire1 for Arduino R4 WiFi Qwiic connector
    Wire1.begin();
    
    // Initialize the NAU7802
    if (!scale.begin(Wire1)) {
        lastError = NAU7802_NOT_FOUND;
        logError(lastError, "begin");
        return lastError;
    }
    
    // Turn off the internal LDO. Save 150uA
    scale.setLDO(NAU7802_LDO_2V4);
    
    // Set gain to 128 (default)
    scale.setGain(currentGain);
    
    // Set sample rate
    scale.setSampleRate(currentSampleRate);
    
    // Turn on digital and analog power
    scale.powerUp();
    
    // Calibrate analog front end
    NAU7802Status calibStatus = calibrateAFE();
    if (calibStatus != NAU7802_OK) {
        return calibStatus;
    }
    
    // Wait for sensor to stabilize after initialization
    debugPrintf("NAU7802: Waiting for sensor to stabilize...\n");
    delay(500);  // Give sensor time to stabilize
    
    // Take a few dummy readings to ensure sensor is ready
    for (int i = 0; i < 5; i++) {
        if (scale.available()) {
            scale.getReading();  // Dummy reading
        }
        delay(100);
    }
    
    // Try to load calibration from EEPROM
    if (!loadCalibration()) {
        debugPrintf("NAU7802: No valid calibration found in EEPROM\n");
    }
    
    debugPrintf("NAU7802: Sensor initialized successfully\n");
    debugPrintf("NAU7802: Gain=%d, SampleRate=%d, RevisionCode=0x%02X\n", 
        currentGain, currentSampleRate, getRevisionCode());
    
    lastError = NAU7802_OK;
    return NAU7802_OK;
}

bool NAU7802Sensor::isConnected() {
    return scale.isConnected();
}

void NAU7802Sensor::reset() {
    debugPrintf("NAU7802: Performing sensor reset\n");
    scale.reset();
    
    // Re-apply configuration after reset
    scale.setGain(currentGain);
    scale.setSampleRate(currentSampleRate);
    scale.powerUp();
    
    // Reset statistics
    resetStatistics();
    
    lastError = NAU7802_OK;
}

NAU7802Status NAU7802Sensor::calibrateAFE() {
    debugPrintf("NAU7802: Starting AFE calibration...\n");
    
    // Calibrate analog front end
    if (!scale.calibrateAFE()) {
        lastError = NAU7802_CALIBRATION_FAILED;
        logError(lastError, "calibrateAFE");
        return lastError;
    }
    
    debugPrintf("NAU7802: AFE calibration completed\n");
    return NAU7802_OK;
}

NAU7802Status NAU7802Sensor::calibrateZero() {
    debugPrintf("NAU7802: Starting simple zero calibration...\n");
    
    // Simple approach: just take current reading as zero offset
    // Wait a moment for any settling
    delay(500);
    
    // Try to get a reading directly
    long reading = 0;
    bool readingObtained = false;
    
    // Try a few times to get a reading
    for (int i = 0; i < 5; i++) {
        if (scale.available()) {
            reading = scale.getReading();
            readingObtained = true;
            debugPrintf("NAU7802: Got zero reading: %ld\n", reading);
            break;
        }
        delay(100);
        yield(); // Allow other tasks
    }
    
    if (!readingObtained) {
        // Fallback: try to get reading through scale object
        if (scale.available()) {
            reading = scale.getReading();
            readingObtained = true;
            debugPrintf("NAU7802: Used fallback scale reading for zero: %ld\n", reading);
        }
    }
    
    if (!readingObtained) {
        lastError = NAU7802_NOT_READY;
        logError(lastError, "calibrateZero - could not get reading");
        debugPrintf("NAU7802: Zero calibration failed - no reading available\n");
        return lastError;
    }
    
    // Set the zero offset
    zeroOffset = reading;
    debugPrintf("NAU7802: Zero calibration completed, offset=%ld\n", zeroOffset);
    
    // Save calibration to EEPROM
    debugPrintf("NAU7802: Saving zero calibration...\n");
    saveCalibration();
    debugPrintf("NAU7802: Zero calibration saved\n");
    
    return NAU7802_OK;
}

NAU7802Status NAU7802Sensor::calibrateScale(float knownWeight) {
    debugPrintf("NAU7802: MINIMAL scale calibration with weight %.2f\n", knownWeight);
    
    if (knownWeight <= 0) {
        debugPrintf("NAU7802: ERROR - Invalid weight %.2f\n", knownWeight);
        return NAU7802_CALIBRATION_FAILED;
    }
    
    // Wait for scale to be ready and get reading
    long currentReading = 0;
    bool readingObtained = false;
    
    for (int i = 0; i < 10; i++) {
        if (scale.available()) {
            currentReading = scale.getReading();
            readingObtained = true;
            debugPrintf("NAU7802: Current reading: %ld\n", currentReading);
            break;
        }
        delay(100);
        yield();
    }
    
    if (!readingObtained) {
        debugPrintf("NAU7802: ERROR - Could not get reading for scale calibration\n");
        return NAU7802_CALIBRATION_FAILED;
    }
    
    debugPrintf("NAU7802: Zero offset: %ld\n", zeroOffset);
    
    // Calculate adjusted reading
    long adjustedReading = currentReading - zeroOffset;
    debugPrintf("NAU7802: Adjusted reading: %ld\n", adjustedReading);
    
    if (adjustedReading == 0) {
        debugPrintf("NAU7802: ERROR - No weight detected\n");
        return NAU7802_CALIBRATION_FAILED;
    }
    
    // Calculate and set calibration factor
    calibrationFactor = knownWeight / (float)adjustedReading;
    isCalibrated = true;
    
    debugPrintf("NAU7802: SUCCESS - Calibration factor: %.8f\n", calibrationFactor);
    debugPrintf("NAU7802: Test: %.2fg should read %.2fg\n", knownWeight, adjustedReading * calibrationFactor);
    
    // Save to EEPROM
    saveCalibration();
    debugPrintf("NAU7802: Calibration saved\n");
    
    return NAU7802_OK;
}

void NAU7802Sensor::setCalibrationFactor(float factor) {
    calibrationFactor = factor;
    isCalibrated = (factor != 1.0);
    debugPrintf("NAU7802: Calibration factor set to %.6f\n", factor);
}

float NAU7802Sensor::getCalibrationFactor() const {
    return calibrationFactor;
}

bool NAU7802Sensor::isReady() {
    return scale.available();
}

bool NAU7802Sensor::dataAvailable() {
    return scale.available();
}

long NAU7802Sensor::getRawReading() {
    return lastRawReading;
}

float NAU7802Sensor::getWeight() {
    return lastWeight;
}

float NAU7802Sensor::getFilteredWeight() {
    return lastFilteredWeight;
}

void NAU7802Sensor::updateReading() {
    if (!scale.available()) {
        return;
    }
    
    lastRawReading = scale.getReading();
    totalReadings++;
    
    // Update min/max tracking
    if (totalReadings == 1) {
        minReading = lastRawReading;
        maxReading = lastRawReading;
    } else {
        if (lastRawReading < minReading) minReading = lastRawReading;
        if (lastRawReading > maxReading) maxReading = lastRawReading;
    }
    
    lastWeight = applyCalibration(lastRawReading);
    
    if (filteringEnabled) {
        updateFilter(lastWeight);
        
        // Calculate filtered weight
        if (!filterBufferFull && filterIndex == 0) {
            lastFilteredWeight = lastWeight;
        } else {
            float sum = 0;
            uint8_t count = filterBufferFull ? filterSamples : filterIndex;
            
            for (uint8_t i = 0; i < count; i++) {
                sum += filterBuffer[i];
            }
            
            lastFilteredWeight = sum / count;
        }
    } else {
        lastFilteredWeight = lastWeight;
    }
}

void NAU7802Sensor::setGain(uint8_t gain) {
    currentGain = gain;
    scale.setGain(gain);
    debugPrintf("NAU7802: Gain set to %d\n", gain);
}

uint8_t NAU7802Sensor::getGain() const {
    return currentGain;
}

void NAU7802Sensor::setSampleRate(uint8_t rate) {
    currentSampleRate = rate;
    scale.setSampleRate(rate);
    debugPrintf("NAU7802: Sample rate set to %d\n", rate);
}

uint8_t NAU7802Sensor::getSampleRate() const {
    return currentSampleRate;
}

void NAU7802Sensor::tareScale() {
    debugPrintf("NAU7802: Taring scale...\n");
    
    // Take current reading as new zero
    long currentReading = getRawReading();
    if (currentReading != 0) {
        zeroOffset = currentReading;
        debugPrintf("NAU7802: Scale tared, new zero offset=%ld\n", zeroOffset);
        saveCalibration();
    }
}

void NAU7802Sensor::setZeroOffset(long offset) {
    zeroOffset = offset;
    debugPrintf("NAU7802: Zero offset set to %ld\n", offset);
}

long NAU7802Sensor::getZeroOffset() const {
    return zeroOffset;
}

void NAU7802Sensor::enableFiltering(bool enable, uint8_t samples) {
    if (enable && samples > 0 && samples <= 50) {
        if (filterBuffer) {
            delete[] filterBuffer;
        }
        
        filterBuffer = new float[samples];
        filterSamples = samples;
        filterIndex = 0;
        filterBufferFull = false;
        filteringEnabled = true;
        
        debugPrintf("NAU7802: Filtering enabled with %d samples\n", samples);
    } else {
        if (filterBuffer) {
            delete[] filterBuffer;
            filterBuffer = nullptr;
        }
        filteringEnabled = false;
        debugPrintf("NAU7802: Filtering disabled\n");
    }
}

float NAU7802Sensor::getAverageReading(uint8_t samples) {
    float sum = 0;
    uint8_t validSamples = 0;
    
    for (uint8_t i = 0; i < samples; i++) {
        if (scale.available()) {
            float weight = getWeight();
            sum += weight;
            validSamples++;
        }
        delay(50); // Wait between readings
    }
    
    return validSamples > 0 ? sum / validSamples : 0.0;
}

void NAU7802Sensor::resetStatistics() {
    totalReadings = 0;
    minReading = 0;
    maxReading = 0;
}

NAU7802Status NAU7802Sensor::getLastError() const {
    return lastError;
}

const char* NAU7802Sensor::getStatusString() const {
    switch (lastError) {
        case NAU7802_OK: return "OK";
        case NAU7802_NOT_FOUND: return "NOT_FOUND";
        case NAU7802_CALIBRATION_FAILED: return "CALIBRATION_FAILED";
        case NAU7802_READ_ERROR: return "READ_ERROR";
        case NAU7802_NOT_READY: return "NOT_READY";
        default: return "UNKNOWN";
    }
}

uint8_t NAU7802Sensor::getRevisionCode() {
    return scale.getRevisionCode();
}

void NAU7802Sensor::powerUp() {
    scale.powerUp();
    debugPrintf("NAU7802: Powered up\n");
}

void NAU7802Sensor::powerDown() {
    scale.powerDown();
    debugPrintf("NAU7802: Powered down\n");
}

bool NAU7802Sensor::isPoweredUp() {
    // The NAU7802 library doesn't provide a direct way to check power status
    // We'll assume it's powered up if we can communicate with it
    return isConnected();
}

bool NAU7802Sensor::saveCalibration() {
    NAU7802CalibrationData data;
    data.magic = NAU7802_CALIBRATION_MAGIC;
    data.calibrationFactor = calibrationFactor;
    data.zeroOffset = zeroOffset;
    data.gain = currentGain;
    data.sampleRate = currentSampleRate;
    
    // Calculate simple checksum
    data.checksum = (uint32_t)data.magic + 
                   (uint32_t)(data.calibrationFactor * 1000) + 
                   (uint32_t)data.zeroOffset + 
                   data.gain + data.sampleRate;
    
    // Write to EEPROM
    uint8_t* dataPtr = (uint8_t*)&data;
    for (size_t i = 0; i < sizeof(data); i++) {
        EEPROM.write(NAU7802_CALIBRATION_ADDR + i, dataPtr[i]);
    }
    
    debugPrintf("NAU7802: Calibration saved to EEPROM\n");
    return true;
}

bool NAU7802Sensor::loadCalibration() {
    NAU7802CalibrationData data;
    
    // Read from EEPROM
    uint8_t* dataPtr = (uint8_t*)&data;
    for (size_t i = 0; i < sizeof(data); i++) {
        dataPtr[i] = EEPROM.read(NAU7802_CALIBRATION_ADDR + i);
    }
    
    // Verify magic number
    if (data.magic != NAU7802_CALIBRATION_MAGIC) {
        debugPrintf("NAU7802: Invalid calibration magic in EEPROM\n");
        return false;
    }
    
    // Verify checksum
    uint32_t calculatedChecksum = (uint32_t)data.magic + 
                                 (uint32_t)(data.calibrationFactor * 1000) + 
                                 (uint32_t)data.zeroOffset + 
                                 data.gain + data.sampleRate;
    
    if (data.checksum != calculatedChecksum) {
        debugPrintf("NAU7802: Calibration checksum mismatch in EEPROM\n");
        return false;
    }
    
    // Apply loaded calibration
    calibrationFactor = data.calibrationFactor;
    zeroOffset = data.zeroOffset;
    setGain(data.gain);
    setSampleRate(data.sampleRate);
    isCalibrated = true;
    
    debugPrintf("NAU7802: Calibration loaded from EEPROM (factor=%.6f, offset=%ld)\n", 
        calibrationFactor, zeroOffset);
    return true;
}

void NAU7802Sensor::clearCalibration() {
    // Clear EEPROM by writing zeros
    for (size_t i = 0; i < sizeof(NAU7802CalibrationData); i++) {
        EEPROM.write(NAU7802_CALIBRATION_ADDR + i, 0);
    }
    
    // Reset to defaults
    calibrationFactor = 1.0;
    zeroOffset = 0;
    isCalibrated = false;
    
    debugPrintf("NAU7802: Calibration cleared from EEPROM\n");
}

// Private helper functions
void NAU7802Sensor::initializeDefaults() {
    resetStatistics();
}

float NAU7802Sensor::applyCalibration(long rawValue) {
    float adjustedValue = rawValue - zeroOffset;
    return adjustedValue * calibrationFactor;
}

void NAU7802Sensor::updateFilter(float value) {
    if (!filteringEnabled || !filterBuffer) {
        return;
    }
    
    filterBuffer[filterIndex] = value;
    filterIndex++;
    
    if (filterIndex >= filterSamples) {
        filterIndex = 0;
        filterBufferFull = true;
    }
}

void NAU7802Sensor::logError(NAU7802Status error, const char* function) {
    debugPrintf("NAU7802 ERROR in %s: %s\n", function, getStatusString());
}