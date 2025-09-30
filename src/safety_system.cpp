#include "safety_system.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "sequence_controller.h"
#include "logger.h"

extern void debugPrintf(const char* fmt, ...);

void SafetySystem::begin() {
    // Initialize engine stop pin as output
    initEngineStopPin();
    LOG_INFO("SafetySystem: Engine stop pin initialized");
}

void SafetySystem::initEngineStopPin() {
    pinMode(ENGINE_STOP_PIN, OUTPUT);
    // Engine runs when pin is LOW, stops when pin is HIGH
    digitalWrite(ENGINE_STOP_PIN, LOW);  // Allow engine to run initially
    engineStopped = false;
    debugPrintf("Engine stop pin %d initialized (LOW = run, HIGH = stop)\n", ENGINE_STOP_PIN);
}

void SafetySystem::setEngineStop(bool stop) {
    if (engineStopped != stop) {
        engineStopped = stop;
        digitalWrite(ENGINE_STOP_PIN, stop ? HIGH : LOW);
        debugPrintf("Engine %s via pin %d\n", stop ? "STOPPED" : "STARTED", ENGINE_STOP_PIN);
        
        // Publish engine state change
        if (networkManager && networkManager->isConnected()) {
            networkManager->publish("r4/engine/stopped", stop ? "1" : "0");
        }
    }
}

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
    // Simplified pressure checking - no special allowances at limit switches
    // Sequence controller now handles pressure-based limits, so we don't need
    // to allow higher pressures at physical limits
    
    if (pressure >= SAFETY_THRESHOLD_PSI) {
        if (!safetyActive) {
            const char* reason = atLimitSwitch ? "pressure_at_limit" : "pressure_threshold";
            activate(reason);
        }
    } else if (pressure < (SAFETY_THRESHOLD_PSI - SAFETY_HYSTERESIS_PSI)) {
        if (safetyActive) {
            // Pressure dropped below threshold with hysteresis
            debugPrintf("Safety cleared: pressure %.1f PSI below threshold\n", pressure);
            
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
    
    LOG_CRITICAL("SAFETY ACTIVATED: %s (pressure=%.1f PSI)", 
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
        
        // Turn off other operational relays (3-8 available for future use)
        for (uint8_t i = 3; i <= 8; i++) {
            relayController->setRelay(i, false, false); // false = automatic operation
        }
        
    }
    
    // SAFETY CRITICAL: Stop engine immediately via direct GPIO
    setEngineStop(true);
    LOG_CRITICAL("SAFETY: Engine stopped via direct GPIO pin %d", ENGINE_STOP_PIN);
    
    Serial.print("EMERGENCY STOP: ");
    Serial.println(reason ? reason : "unknown");
    Serial.println("Manual relay control still available for pressure relief");
}

void SafetySystem::deactivate() {
    if (!safetyActive) return;
    
    safetyActive = false;
    
    LOG_WARN("Safety system deactivated manually");
    
    if (relayController) {
        relayController->disableSafety();
    }
    
    // Restart engine when safety is cleared
    setEngineStop(false);
    Serial.println("Engine restarted - safety system deactivated");
    
    if (networkManager && networkManager->isConnected()) {
        networkManager->publish(TOPIC_CONTROL_RESP, "SAFETY: deactivated");
    }
}

void SafetySystem::clearEmergencyStop() {
    if (safetyActive) {
        Serial.println("Safety system cleared via E-Stop reset");
        
        if (networkManager && networkManager->isConnected()) {
            networkManager->publish(TOPIC_CONTROL_RESP, "SAFETY: E-Stop cleared");
        }
        
        deactivate();
    }
}

void SafetySystem::getStatusString(char* buffer, size_t bufferSize) {
    snprintf(buffer, bufferSize, 
        "safety=%s engine=%s pressure=%.1f threshold=%.1f",
        safetyActive ? "ACTIVE" : "OK",
        engineStopped ? "STOPPED" : "RUNNING",
        lastPressure,
        SAFETY_THRESHOLD_PSI
    );
}