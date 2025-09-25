#include "safety_system.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "sequence_controller.h"

extern void debugPrintf(const char* fmt, ...);

void SafetySystem::update(float currentPressure) {
    lastPressure = currentPressure;
    checkPressure(currentPressure);
    lastSafetyCheck = millis();
}

void SafetySystem::checkPressure(float pressure) {
    if (pressure >= SAFETY_THRESHOLD_PSI) {
        if (!safetyActive) {
            activate("pressure_threshold");
        }
    } else if (pressure < (SAFETY_THRESHOLD_PSI - SAFETY_HYSTERESIS_PSI)) {
        if (safetyActive) {
            // Pressure dropped below threshold with hysteresis
            Serial.print("Safety cleared: pressure ");
            Serial.print(pressure);
            Serial.println(" PSI below threshold");
            
            if (networkManager && networkManager->isConnected()) {
                networkManager->publish(TOPIC_CONTROL_RESP, "SAFETY: cleared");
            }
            
            safetyActive = false;
            
            // Re-enable relay operations
            if (relayController) {
                relayController->disableSafety();
            }
        }
    }
}

void SafetySystem::activate(const char* reason) {
    if (safetyActive) return; // Already active
    
    safetyActive = true;
    
    debugPrintf("SAFETY ACTIVATED: %s (pressure=%.1f PSI)\n", 
        reason ? reason : "unknown", lastPressure);
    
    // Emergency stop sequence
    emergencyStop(reason);
    
    // Publish safety alert
    if (networkManager && networkManager->isConnected()) {
        char alert[64];
        snprintf(alert, sizeof(alert), "SAFETY: activated - %s", reason ? reason : "unknown");
        networkManager->publish(TOPIC_CONTROL_RESP, alert);
    }
}

void SafetySystem::emergencyStop(const char* reason) {
    // Abort any active sequence
    if (sequenceController && sequenceController->isActive()) {
        sequenceController->abort();
    }
    
    // Turn off all operational relays (keep power relay as-is)
    if (relayController) {
        relayController->enableSafety(); // This will block future relay operations
        
        // Turn off relays 1-8 immediately
        for (uint8_t i = 1; i < RELAY_POWER_PIN; i++) {
            relayController->setRelay(i, false);
        }
    }
    
    Serial.print("EMERGENCY STOP: ");
    Serial.println(reason ? reason : "unknown");
}

void SafetySystem::deactivate() {
    if (!safetyActive) return;
    
    safetyActive = false;
    
    Serial.println("Safety system deactivated manually");
    
    if (relayController) {
        relayController->disableSafety();
    }
    
    if (networkManager && networkManager->isConnected()) {
        networkManager->publish(TOPIC_CONTROL_RESP, "SAFETY: deactivated");
    }
}

void SafetySystem::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, 
        "safety=%s pressure=%.1f threshold=%.1f",
        safetyActive ? "ACTIVE" : "OK",
        lastPressure,
        SAFETY_THRESHOLD_PSI
    );
}