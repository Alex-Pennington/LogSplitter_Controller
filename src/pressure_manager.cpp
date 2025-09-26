#include "pressure_manager.h"
#include "network_manager.h"

extern void debugPrintf(const char* fmt, ...);

// ============================================================================
// PressureSensorChannel Implementation
// ============================================================================

void PressureSensorChannel::begin(uint8_t pin, float maxPsi) {
    analogPin = pin;
    maxPressurePsi = maxPsi;
    
    // Initialize sampling buffer
    sampleIndex = 0;
    samplesFilled = 0;
    samplesSum = 0;
    lastSampleTime = 0;
    currentPressure = 0.0f;
    
    // Initialize filter state
    emaValue = 0.0f;
    
    debugPrintf("PressureSensorChannel initialized: pin A%d, max %.1f PSI\n", 
        pin - A0, maxPsi);
}

void PressureSensorChannel::update() {
    unsigned long now = millis();
    
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        int rawValue = analogRead(analogPin);
        int filteredValue = applyFilter(rawValue);
        updateSample(filteredValue);
        
        // Calculate current pressure
        float avgCounts = computeAverageCount();
            float voltage = (avgCounts / (float)(1 << ADC_RESOLUTION_BITS)) * adcVref;

            // Extended scaling only for main hydraulic sensor (A1)
            if (analogPin == HYDRAULIC_PRESSURE_PIN) {
                // Extended scaling path (A1 only):
                // Voltage 0..vfs (nominally 5.0V) spans -NEG_FRAC .. (1 + POS_FRAC) of nominal pressure.
                // Example with fractions 0.25 & 0.25 and nominal=5000:
                //   0V  -> -1250 PSI (clamped to 0 for reporting)
                //   5V  ->  6250 PSI (clamped to 5000 for reporting)
                // This creates over-range headroom and sub-zero dead-band while keeping published values bounded.
                const float nominal = HYDRAULIC_MAX_PRESSURE_PSI;
                const float span = (1.0f + MAIN_PRESSURE_EXT_NEG_FRAC + MAIN_PRESSURE_EXT_POS_FRAC) * nominal; // e.g. 1.5 * nominal
                float vfs = MAIN_PRESSURE_EXT_FSV;
                if (vfs <= 0.1f) vfs = adcVref; // Fallback: avoid divide-by-near-zero if constant misconfigured

                // Bound measured voltage to modeled full-scale
                if (voltage < 0.0f) voltage = 0.0f;
                if (voltage > vfs) voltage = vfs;

                // Compute raw (unclamped) extended pressure then shift negative offset
                float rawPsi = (voltage / vfs) * span - (MAIN_PRESSURE_EXT_NEG_FRAC * nominal);

                // NOTE: If raw (unclamped) value is ever needed for diagnostics, store before clamp.
                if (rawPsi < 0.0f) rawPsi = 0.0f;
                if (rawPsi > nominal) rawPsi = nominal;
                currentPressure = rawPsi; // Only clamped value used by safety & telemetry
            } else {
                currentPressure = voltageToPressure(voltage);
            }
        
        lastSampleTime = now;
    }
}

int PressureSensorChannel::applyFilter(int rawValue) {
    switch (filterMode) {
        case FILTER_NONE:
            return rawValue;
            
        case FILTER_MEDIAN3: {
            // Simple median-of-3 filter (needs previous values)
            static int prev1 = rawValue, prev2 = rawValue;
            int median = rawValue;
            if ((rawValue >= prev1 && rawValue <= prev2) || (rawValue >= prev2 && rawValue <= prev1)) {
                median = rawValue;
            } else if ((prev1 >= rawValue && prev1 <= prev2) || (prev1 >= prev2 && prev1 <= rawValue)) {
                median = prev1;
            } else {
                median = prev2;
            }
            prev2 = prev1;
            prev1 = rawValue;
            return median;
        }
            
        case FILTER_EMA:
            if (emaValue == 0.0f) emaValue = rawValue;
            emaValue = emaAlpha * rawValue + (1.0f - emaAlpha) * emaValue;
            return (int)emaValue;
            
        default:
            return rawValue;
    }
}

void PressureSensorChannel::updateSample(int filteredValue) {
    // Remove old sample from sum if buffer is full
    if (samplesFilled == SAMPLE_WINDOW_COUNT) {
        samplesSum -= samples[sampleIndex];
    } else {
        samplesFilled++;
    }
    
    // Add new sample
    samples[sampleIndex] = filteredValue;
    samplesSum += filteredValue;
    
    // Advance index with wraparound
    sampleIndex = (sampleIndex + 1) % SAMPLE_WINDOW_COUNT;
}

float PressureSensorChannel::computeAverageCount() {
    if (samplesFilled == 0) return 0.0f;
    return (float)samplesSum / (float)samplesFilled;
}

float PressureSensorChannel::voltageToPressure(float voltage) {
    // Primary channel (A1) uses extended bandwidth: 0-5V spans -25% .. +125% of nominal range
    // Other channels (e.g. A5) remain linear 0-4.5/5.0 -> 0..maxPressurePsi
    // Identify A1 by analogPin match
    bool isPrimary = (analogPin == HYDRAULIC_PRESSURE_PIN);
    if (isPrimary) {
        // Map 0V -> -0.25 * maxPressure, 5V -> 1.25 * maxPressure, then clamp to 0..maxPressure
        float spanFactor = 1.5f; // (-0.25 to +1.25) total span = 1.5 * nominal
        float raw = (voltage / 5.0f) * (spanFactor * maxPressurePsi) - (0.25f * maxPressurePsi);
        if (raw < 0.0f) raw = 0.0f;
        if (raw > maxPressurePsi) raw = maxPressurePsi;
        return raw;
    } else {
           // Generic linear 0..SENSOR_MAX_VOLTAGE => 0..maxPressurePsi
           if (voltage < SENSOR_MIN_VOLTAGE) voltage = SENSOR_MIN_VOLTAGE;
           if (voltage > SENSOR_MAX_VOLTAGE) voltage = SENSOR_MAX_VOLTAGE;
           return (voltage / SENSOR_MAX_VOLTAGE) * maxPressurePsi;
    }
}

// ============================================================================
// PressureManager Implementation  
// ============================================================================

void PressureManager::begin() {
    // Initialize each pressure sensor channel
    sensors[SENSOR_HYDRAULIC].begin(HYDRAULIC_PRESSURE_PIN, HYDRAULIC_MAX_PRESSURE_PSI);
    sensors[SENSOR_HYDRAULIC_OIL].begin(HYDRAULIC_OIL_PRESSURE_PIN, HYDRAULIC_MAX_PRESSURE_PSI);
    
    lastPublishTime = 0;
    
    Serial.println("PressureManager initialized with 2 sensors:");
    Serial.print("  - Hydraulic System Pressure (A1): 0-");
    Serial.print(HYDRAULIC_MAX_PRESSURE_PSI);
    Serial.println(" PSI");
    Serial.print("  - Hydraulic Filter Pressure (A5): 0-");  
    Serial.print(HYDRAULIC_MAX_PRESSURE_PSI);
    Serial.println(" PSI");
}

void PressureManager::update() {
    // Update all sensor channels
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensors[i].update();
    }
    
    // Publish pressures periodically
    unsigned long now = millis();
    if (now - lastPublishTime >= publishInterval) {
        publishPressures();
        lastPublishTime = now;
    }
}

void PressureManager::publishPressures() {
    if (!networkManager || !networkManager->isConnected()) {
        return;
    }
    
    char buffer[64];
    
    // Publish individual pressure readings with clear labels
    if (sensors[SENSOR_HYDRAULIC].isReady()) {
        snprintf(buffer, sizeof(buffer), "%.1f", getHydraulicPressure());
        networkManager->publish(TOPIC_HYDRAULIC_SYSTEM_PRESSURE, buffer);
        networkManager->publish(TOPIC_PRESSURE, buffer); // Backward compatibility
    }
    
    if (sensors[SENSOR_HYDRAULIC_OIL].isReady()) {
        snprintf(buffer, sizeof(buffer), "%.1f", getHydraulicOilPressure());
        networkManager->publish(TOPIC_HYDRAULIC_FILTER_PRESSURE, buffer);
    }
    
    // Publish combined status
    getStatusString(buffer, sizeof(buffer));
    networkManager->publish(TOPIC_PRESSURE_STATUS, buffer);
}

void PressureManager::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, 
        "hydraulic=%.1f hydraulic_oil=%.1f",
        getHydraulicPressure(),
        getHydraulicOilPressure()
    );
}