#include "pressure_manager.h"
// NetworkManager include removed - non-networking version

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
            lastVoltage = voltage; // store raw computed voltage before clamping / mapping for diagnostics

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
    // Differentiate between sensor types based on analog pin
    bool isSystemPressure = (analogPin == HYDRAULIC_PRESSURE_PIN);  // A1 = system pressure
    
    if (isSystemPressure) {
        // A1: System pressure sensor - 4-20mA current loop
        // 4mA × 250Ω = 1V = 0 PSI, 20mA × 250Ω = 5V = 5000 PSI
        float psi = 0.0f;
        if (voltage >= CURRENT_LOOP_MIN_VOLTAGE) {
            float voltageRange = CURRENT_LOOP_MAX_VOLTAGE - CURRENT_LOOP_MIN_VOLTAGE; // 4V
            float normalizedVoltage = voltage - CURRENT_LOOP_MIN_VOLTAGE; // Remove 1V offset
            if (normalizedVoltage < 0.0f) normalizedVoltage = 0.0f;
            if (normalizedVoltage > voltageRange) normalizedVoltage = voltageRange;
            psi = (normalizedVoltage / voltageRange) * maxPressurePsi;
        }
        // Apply calibration (gain + offset)
        return psi * sensorGain + sensorOffset;
    } else {
        // A5: Filter pressure sensor - 0-5V voltage output (configurable)
        // Generic linear 0..SENSOR_MAX_VOLTAGE => 0..maxPressurePsi with calibration
        if (voltage < SENSOR_MIN_VOLTAGE) voltage = SENSOR_MIN_VOLTAGE;
        if (voltage > SENSOR_MAX_VOLTAGE) voltage = SENSOR_MAX_VOLTAGE;
        float psi = (voltage / SENSOR_MAX_VOLTAGE) * maxPressurePsi;
        // Apply calibration (gain + offset)
        return psi * sensorGain + sensorOffset;
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
    
    debugPrintf("PressureManager initialized with 2 sensors:\n");
    debugPrintf("  - Hydraulic System Pressure (A1): 0-%d PSI\n", HYDRAULIC_MAX_PRESSURE_PSI);
    debugPrintf("  - Hydraulic Filter Pressure (A5): 0-%d PSI\n", HYDRAULIC_MAX_PRESSURE_PSI);
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
    // MQTT publishing removed - non-networking version
    // Pressure data available via serial commands
}

void PressureManager::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, 
        "hydraulic=%.1f hydraulicV=%.2f hydraulic_oil=%.1f hydraulic_oilV=%.2f",
        getHydraulicPressure(),
        sensors[SENSOR_HYDRAULIC].getVoltage(),
        getHydraulicOilPressure(),
        sensors[SENSOR_HYDRAULIC_OIL].getVoltage()
    );
}