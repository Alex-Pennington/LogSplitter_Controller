#include "safety_system.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "sequence_controller.h"

extern void debugPrintf(const char* fmt, ...);

void SafetySystem::update(float currentPressure) {
    lastPressure = currentPressure;
    
    // Check if cylinder is at end-of-travel (limit switches)
    extern bool g_limitExtendActive;
    extern bool g_limitRetractActive;
    bool atLimitSwitch = g_limitExtendActive || g_limitRetractActive;
    
    checkPressure(currentPressure, atLimitSwitch);
    lastSafetyCheck = millis();
}

void SafetySystem::checkPressure(float pressure) {
    checkPressure(pressure, false); // Default: not at limit switch
}

void SafetySystem::checkPressure(float pressure, bool atLimitSwitch) {
    // If cylinder is at end-of-travel, high pressure is normal and expected
    if (atLimitSwitch) {
        // Allow higher pressure threshold when at limit switches
        if (pressure >= (SAFETY_THRESHOLD_PSI + 200.0f)) { // +200 PSI tolerance at limits
            if (!safetyActive) {
                activate("extreme_pressure_at_limit");
            }
        } else {
            // Clear safety if pressure comes down, even at limits
            if (safetyActive && pressure < (SAFETY_THRESHOLD_PSI + 150.0f)) {
                Serial.print("Safety cleared: pressure ");
                Serial.print(pressure);
                Serial.println(" PSI acceptable at limit switch");
                
                if (networkManager && networkManager->isConnected()) {
                    networkManager->publish(TOPIC_CONTROL_RESP, "SAFETY: cleared at limit");
                }
                
                safetyActive = false;
                
                if (relayController) {
                    relayController->disableSafety();
                }
            }
        }
        return;
    }
    
    // Normal mid-stroke operation - use standard threshold
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
        relayController->enableSafety(); // This will block future AUTOMATIC relay operations
        
        // Turn off hydraulic relays immediately
        relayController->setRelay(RELAY_EXTEND, false, false);   // false = automatic operation
        relayController->setRelay(RELAY_RETRACT, false, false);  // false = automatic operation
        
        // Turn off other operational relays (3-7, skip engine stop for now)
        for (uint8_t i = 3; i <= 7; i++) {
            relayController->setRelay(i, false, false); // false = automatic operation
        }
        
        // Engine stop relay (R8) will be integrated with engine control later
        // For now, just log that it's available for engine safety integration
        debugPrintf("Engine stop relay (R%d) available for future engine control integration\n", RELAY_ENGINE_STOP);
    }
    
    Serial.print("EMERGENCY STOP: ");
    Serial.println(reason ? reason : "unknown");
    Serial.println("Manual relay control still available for pressure relief");
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