#include "pressure_sensor.h"

void PressureSensor::begin() {
    // Initialize sampling buffer
    for (size_t i = 0; i < SAMPLE_WINDOW_COUNT; ++i) {
        samples[i] = 0;
    }
    
    sampleIndex = 0;
    samplesFilled = 0;
    samplesSum = 0;
    lastSampleTime = millis();
    
    // Configure ADC resolution
    Serial.print("Setting ADC resolution to ");
    Serial.print(ADC_RESOLUTION_BITS);
    Serial.println(" bits");
    analogReadResolution(ADC_RESOLUTION_BITS);
    
    // Reset EMA state
    emaValue = 0.0f;
    
    Serial.println("PressureSensor initialized");
}

int PressureSensor::applyFilter(int rawValue) {
    switch (filterMode) {
        case FILTER_MEDIAN3: {
            // Median of 3: read two additional quick samples
            int r1 = analogRead(A1);
            int r2 = analogRead(A1);
            int vals[3] = { rawValue, r1, r2 };
            
            // Simple 3-element sort
            for (int i = 0; i < 2; ++i) {
                for (int j = i + 1; j < 3; ++j) {
                    if (vals[j] < vals[i]) {
                        int temp = vals[i];
                        vals[i] = vals[j];
                        vals[j] = temp;
                    }
                }
            }
            return vals[1]; // Return median
        }
        
        case FILTER_EMA: {
            if (samplesFilled == 0 && sampleIndex == 0 && emaValue == 0.0f) {
                emaValue = (float)rawValue;
            } else {
                emaValue = emaAlpha * (float)rawValue + (1.0f - emaAlpha) * emaValue;
            }
            return (int)emaValue;
        }
        
        case FILTER_NONE:
        default:
            return rawValue;
    }
}

void PressureSensor::updateSample(int filteredValue) {
    // Remove oldest value if buffer is full
    if (samplesFilled == SAMPLE_WINDOW_COUNT) {
        samplesSum -= samples[sampleIndex];
    } else {
        samplesFilled++;
    }
    
    // Store new sample
    samples[sampleIndex] = (uint16_t)filteredValue;
    samplesSum += filteredValue;
    
    // Advance index
    sampleIndex = (sampleIndex + 1) % SAMPLE_WINDOW_COUNT;
}

float PressureSensor::computeAverageCountsS() {
    if (samplesFilled == 0) return 0.0f;
    return (float)samplesSum / (float)samplesFilled;
}

float PressureSensor::countsToPressure(float avgCounts) {
    // Convert ADC counts to volts
    float maxCount = (float)((1UL << ADC_RESOLUTION_BITS) - 1UL);
    float volts = 0.0f;
    
    if (maxCount > 0.0f) {
        volts = (avgCounts * adcVref) / maxCount;
    }
    
    // Map volts (0..adcVref) to PSI (0..maxPressurePsi)
    float psi = 0.0f;
    if (adcVref > 0.0f) {
        psi = (volts / adcVref) * maxPressurePsi;
    }
    
    // Apply calibration (gain + offset)
    return psi * sensorGain + sensorOffset;
}

void PressureSensor::update() {
    unsigned long now = millis();
    
    // Check if it's time for next sample
    if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = now;
        
        // Read raw ADC value
        int rawValue = analogRead(A1);
        
        // Apply filtering
        int filteredValue = applyFilter(rawValue);
        
        // Update circular buffer
        updateSample(filteredValue);
        
        // Compute current pressure
        float avgCounts = computeAverageCountsS();
        currentPressure = countsToPressure(avgCounts);
        
        // Optional debug output (uncomment for debugging)
        /*
        Serial.print("ADC: raw=");
        Serial.print(rawValue);
        Serial.print(" filtered=");
        Serial.print(filteredValue);
        Serial.print(" avg=");
        Serial.print(avgCounts, 2);
        Serial.print(" PSI=");
        Serial.println(currentPressure, 2);
        */
    }
}

void PressureSensor::getStatusString(char* buffer, size_t bufferSize) {
    const char* filterName = "unknown";
    switch (filterMode) {
        case FILTER_NONE: filterName = "none"; break;
        case FILTER_MEDIAN3: filterName = "median3"; break;
        case FILTER_EMA: filterName = "ema"; break;
    }
    
    snprintf(buffer, bufferSize,
        "adcVref=%.6f maxPressurePsi=%.2f sensorGain=%.6f sensorOffset=%.6f filter=%s emaAlpha=%.3f currentPSI=%.2f",
        adcVref, maxPressurePsi, sensorGain, sensorOffset, filterName, emaAlpha, currentPressure
    );
}