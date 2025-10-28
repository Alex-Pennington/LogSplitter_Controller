#include "sequence_controller.h"
#include "relay_controller.h"
#include "pressure_manager.h"
#include "system_error_manager.h"
#include "input_manager.h"
#include "safety_system.h"
#include "telemetry_manager.h"
#include "logger.h"
#include "constants.h"

// External references for relay control (network manager removed)
extern RelayController* g_relayController;
extern TelemetryManager telemetryManager;
extern void debugPrintf(const char* fmt, ...);
// Limit switch states maintained in main
extern bool g_limitExtendActive;   // Pin 6 - fully extended
extern bool g_limitRetractActive;  // Pin 7 - fully retracted
// Pressure manager (defined in main.cpp)
extern PressureManager pressureManager;

void SequenceController::enterState(SequenceState newState) {
    if (currentState != newState) {
        debugPrintf("[SEQ] State change: %d -> %d (type: %d)\n", (int)currentState, (int)newState, (int)sequenceType);
        
        // Send telemetry for sequence state change
        telemetryManager.sendSequenceEvent((uint8_t)newState, 0, (uint16_t)(millis() - stateEntryTime));
        
        currentState = newState;
        stateEntryTime = millis();
        
        // Reset limit change timer on state entry
        lastLimitChangeTime = 0;

        // Pressure publishing removed - non-networking version
    }
}

bool SequenceController::checkTimeout() {
    unsigned long now = millis();
    
    // Check overall sequence timeout
    if (currentState != SEQ_IDLE && stateEntryTime > 0) {
        if (now - stateEntryTime > timeoutMs) {
            abortSequence("timeout");
            return true;
        }
    }
    
    return false;
}

bool SequenceController::checkStableLimit(uint8_t pin, bool active) {
    unsigned long now = millis();
    
    if (active) {
        // Limit is active, check if it's been stable long enough
        if (lastLimitChangeTime == 0) {
            lastLimitChangeTime = now;
        }
        return (now - lastLimitChangeTime >= stableTimeMs);
    } else {
        // Limit not active, reset stability timer
        lastLimitChangeTime = 0;
        return false;
    }
}

void SequenceController::abortSequence(const char* reason) {
    // Use appropriate log level based on reason
    const char* abortReason = reason ? reason : "unknown";
    if (reason && strcmp(reason, "timeout") == 0) {
        debugPrintf("SEQ: Sequence TIMEOUT - aborting operation (type: %d)\n", (int)sequenceType);
    } else {
        debugPrintf("[SEQ] Aborting sequence: %s (type: %d)\n", abortReason, (int)sequenceType);
    }
    
    // Send telemetry for sequence abort
    telemetryManager.sendSequenceEvent(255, 0, (uint16_t)(millis() - stateEntryTime)); // 255 = abort event
    
    // Turn off all hydraulic relays involved in sequence
    if (g_relayController) {
        g_relayController->setRelay(RELAY_EXTEND, false);
        g_relayController->setRelay(RELAY_RETRACT, false);
    }
    
    // Reset state
    sequenceType = SEQ_TYPE_AUTOMATIC;
    enterState(SEQ_IDLE);
    allowButtonRelease = false;
    pendingPressPin = 0;
    pendingPressTime = 0;
    
    // Trigger mill light for timeout errors and disable sequence
    if (reason && strcmp(reason, "timeout") == 0 && errorManager) {
        errorManager->setError(ERROR_SEQUENCE_TIMEOUT, "Hydraulic sequence operation timed out");
        sequenceDisabled = true;
        LOG_WARN("SEQ: Sequence controller DISABLED due to timeout - requires safety clear to re-enable");
    }
    
    // MQTT publishing removed - non-networking version
}

bool SequenceController::areStartButtonsActive(const bool* pinStates) {
    // Find pins 3 and 5 in WATCH_PINS array
    bool pin3Active = false, pin5Active = false;
    
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        if (WATCH_PINS[i] == 5) pin5Active = pinStates[i];
    }
    
    return pin5Active;
}

uint8_t SequenceController::getStage() const {
    switch (currentState) {
        case SEQ_STAGE1_ACTIVE:
        case SEQ_STAGE1_WAIT_LIMIT:
            return 1;
        case SEQ_STAGE2_ACTIVE:
        case SEQ_STAGE2_WAIT_LIMIT:
            return 2;
        default:
            return 0;
    }
}

unsigned long SequenceController::getElapsedTime() const {
    if (stateEntryTime == 0) return 0;
    return millis() - stateEntryTime;
}

void SequenceController::update() {
    unsigned long now = millis();
    
    // Check for overall timeout
    if (checkTimeout()) {
        return; // Already aborted
    }
    
    // Handle pending press timeout
    if (pendingPressPin > 0 && pendingPressTime > 0) {
        if (now - pendingPressTime >= startStableTimeMs) {
            // Apply pending press action
            if (g_relayController && pendingPressPin == 3) {
                // Find pin 3 state
                for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
                    if (WATCH_PINS[i] == 3) {
                        // This would need access to current pin states
                        // For now, we'll clear the pending press
                        break;
                    }
                }
            }
            pendingPressPin = 0;
            pendingPressTime = 0;
        }
    }
    
    // State-specific processing
    switch (currentState) {
        case SEQ_WAIT_START_DEBOUNCE:
            // Wait for start debounce to complete
            if (now - stateEntryTime >= startStableTimeMs) {
                // Start the actual sequence
                enterState(SEQ_STAGE1_ACTIVE);
                if (g_relayController) {
                    g_relayController->setRelay(RELAY_EXTEND, true);
                    g_relayController->setRelay(RELAY_RETRACT, false);
                }
                allowButtonRelease = true;
                // Log configured stability windows for quick reference
                debugPrintf("[SEQ] Start confirmed; stable windows: limit=%lums start=%lums\n", 
                    stableTimeMs, startStableTimeMs);
                
                // MQTT publishing removed - non-networking version
            }
            break;
            
        case SEQ_MANUAL_EXTEND_ACTIVE:
            handleManualExtend();
            break;
            
        case SEQ_MANUAL_RETRACT_ACTIVE:
            handleManualRetract();
            break;
            
        case SEQ_STAGE1_ACTIVE:
        case SEQ_STAGE1_WAIT_LIMIT:
        {
            // Poll extend limit (physical switch OR pressure-based limit)
            bool extendLimitReached = g_limitExtendActive;
            
            // Check pressure-based extend limit (parallel check, not conditional)
            if (pressureManager.isReady()) {
                float currentPressure = pressureManager.getHydraulicPressure();
                if (currentPressure >= EXTEND_PRESSURE_LIMIT_PSI) {
                    extendLimitReached = true;
                    // Mark that pressure limit was used (persistent until power reset)
                    if (safetySystem) {
                        safetySystem->markPressureLimitUsed();
                    }
                    debugPrintf("[SEQ] Pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                               currentPressure, EXTEND_PRESSURE_LIMIT_PSI);
                }
            }
            
            if (extendLimitReached) {
                if (lastLimitChangeTime == 0) {
                    lastLimitChangeTime = now; // start stability timer
                    debugPrintf("[SEQ] Extend limit reached (switch=%s, pressure=%s); timing for %lums\n", 
                               g_limitExtendActive ? "YES" : "NO",
                               (pressureManager.isReady() && pressureManager.getHydraulicPressure() >= EXTEND_PRESSURE_LIMIT_PSI) ? "YES" : "NO",
                               stableTimeMs);
                } else if (now - lastLimitChangeTime >= stableTimeMs) {
                    // Transition to stage 2 (retract)
                    debugPrintf("[SEQ] Extend limit stable for %lums; switching to R2\n", now - lastLimitChangeTime);
                    enterState(SEQ_STAGE2_ACTIVE);
                    if (g_relayController) {
                        g_relayController->setRelay(RELAY_EXTEND, false);
                        g_relayController->setRelay(RELAY_RETRACT, true);
                    }
                    lastLimitChangeTime = 0; // reset for next stage
                    // MQTT publishing removed - non-networking version
                }
            } else {
                if (lastLimitChangeTime != 0) {
                    debugPrintf("[SEQ] Extend limit lost before stable (%lums elapsed)\n", now - lastLimitChangeTime);
                }
                lastLimitChangeTime = 0; // lost stability, reset timer
            }
            break;
        }

        case SEQ_STAGE2_ACTIVE:
        case SEQ_STAGE2_WAIT_LIMIT:
        {
            // Poll retract limit (physical switch OR pressure-based limit)
            bool retractLimitReached = g_limitRetractActive;
            
            // Check pressure-based retract limit (parallel check, not conditional)
            if (pressureManager.isReady()) {
                float currentPressure = pressureManager.getHydraulicPressure();
                if (currentPressure >= RETRACT_PRESSURE_LIMIT_PSI) {
                    retractLimitReached = true;
                    // Mark that pressure limit was used (persistent until power reset)
                    if (safetySystem) {
                        safetySystem->markPressureLimitUsed();
                    }
                    debugPrintf("[SEQ] Retract pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                               currentPressure, RETRACT_PRESSURE_LIMIT_PSI);
                }
            }
            
            if (retractLimitReached) {
                if (lastLimitChangeTime == 0) {
                    lastLimitChangeTime = now;
                    debugPrintf("[SEQ] Retract limit reached (switch=%s, pressure=%s); timing for %lums\n", 
                               g_limitRetractActive ? "YES" : "NO",
                               (pressureManager.isReady() && pressureManager.getHydraulicPressure() >= RETRACT_PRESSURE_LIMIT_PSI) ? "YES" : "NO",
                               stableTimeMs);
                } else if (now - lastLimitChangeTime >= stableTimeMs) {
                    debugPrintf("[SEQ] Retract limit stable for %lums; complete\n", now - lastLimitChangeTime);
                    if (g_relayController) {
                        g_relayController->setRelay(RELAY_RETRACT, false);
                    }
                    enterState(SEQ_IDLE);
                    allowButtonRelease = false;
                    lastLimitChangeTime = 0;
                    // MQTT publishing removed - non-networking version
                }
            } else {
                if (lastLimitChangeTime != 0) {
                    debugPrintf("[SEQ] Retract limit lost before stable (%lums elapsed)\n", now - lastLimitChangeTime);
                }
                lastLimitChangeTime = 0;
            }
            break;
        }
            
        default:
            // No special processing for other states
            break;
    }
}

bool SequenceController::processInputChange(uint8_t pin, bool state, const bool* allPinStates) {
    bool startButtonsActive = areStartButtonsActive(allPinStates);
    unsigned long now = millis();
    
    switch (currentState) {
        case SEQ_IDLE:
            // Check if sequence is disabled (timeout lockout)
            if (sequenceDisabled) {
                if (startButtonsActive) {
                    LOG_WARN("SEQ: Sequence start blocked - controller disabled due to timeout");
                    debugPrintf("[SEQ] Sequence start blocked - controller disabled (timeout lockout)\n");
                }
                return false; // Don't handle the input, allow normal processing
            }
            
            // Check for sequence start condition
            if (startButtonsActive) {
                // Just became active, start debounce timer
                enterState(SEQ_WAIT_START_DEBOUNCE);
                return true; // Handled by sequence
            }
                      
            // Handle other pins normally (return false to allow normal processing)
            return false;
            
        case SEQ_WAIT_START_DEBOUNCE:
            // Check if start buttons released during debounce
            if (!startButtonsActive) {
                abortSequence("released_during_debounce");
                return true;
            }
            break;
            
        case SEQ_STAGE1_ACTIVE:
        case SEQ_STAGE1_WAIT_LIMIT:
            // Check for abort conditions
            if (allowButtonRelease) {
                // Check for new button presses (abort condition)
                for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
                    uint8_t watchPin = WATCH_PINS[i];
                    if (watchPin >= 2 && watchPin <= 5 && watchPin == pin && state) {
                        if (!buttonStateAtStart[i]) {
                            abortSequence("new_press");
                            return true;
                        }
                    }
                }
            } else {
                // Must keep start buttons active
                if (!startButtonsActive) {
                    abortSequence("start_released");
                    return true;
                }
            }
            
            // Rising edge on limit 6: start stability timer and wait in update()
            if (pin == 6 && state) {
                lastLimitChangeTime = now;
                debugPrintf("[SEQ] Limit 6 edge detected; starting stability timing\n");
                // hint state for clarity, update() handles the transition safely
                if (currentState == SEQ_STAGE1_ACTIVE) {
                    enterState(SEQ_STAGE1_WAIT_LIMIT);
                }
            }
            break;
            
        case SEQ_STAGE2_ACTIVE:
        case SEQ_STAGE2_WAIT_LIMIT:
            // Similar abort checking as stage 1
            if (allowButtonRelease) {
                for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
                    uint8_t watchPin = WATCH_PINS[i];
                    if (watchPin >= 2 && watchPin <= 5 && watchPin == pin && state) {
                        if (!buttonStateAtStart[i]) {
                            abortSequence("new_press");
                            return true;
                        }
                    }
                }
            }
            
            // Rising edge on limit 7: start stability timer and wait in update()
            if (pin == 7 && state) {
                lastLimitChangeTime = now;
                debugPrintf("[SEQ] Limit 7 edge detected; starting stability timing\n");
                if (currentState == SEQ_STAGE2_ACTIVE) {
                    enterState(SEQ_STAGE2_WAIT_LIMIT);
                }
            }
            break;
            
        default:
            break;
    }
    
    return true; // Sequence handled the input
}

void SequenceController::reset() {
    abortSequence("manual_reset");
}

void SequenceController::getStatusString(char* buffer, size_t bufferSize) {
    unsigned long elapsed = getElapsedTime();
    
    snprintf(buffer, bufferSize, 
        "stage=%u active=%d elapsed=%lu stableMs=%lu startStableMs=%lu timeoutMs=%lu disabled=%d",
        getStage(), 
        (currentState != SEQ_IDLE) ? 1 : 0,
        elapsed,
        stableTimeMs,
        startStableTimeMs,
        timeoutMs,
        sequenceDisabled ? 1 : 0
    );
}

void SequenceController::publishIndividualData() {
    // MQTT publishing removed - non-networking version
    // Individual sequence data available via serial commands
}

// Manual operation methods

bool SequenceController::startManualExtend() {
    if (currentState != SEQ_IDLE) {
        LOG_WARN("SEQ: Cannot start manual extend - sequence active (state: %d)", (int)currentState);
        return false;
    }
    
    // Check if sequence is disabled (timeout lockout)
    if (sequenceDisabled) {
        LOG_WARN("SEQ: Manual extend blocked - controller disabled due to timeout");
        return false;
    }
    
    // Safety check: verify we're not already at extend limit
    if (inputManager && inputManager->getPinState(LIMIT_EXTEND_PIN)) {
        LOG_WARN("SEQ: Manual extend blocked - already at extend limit");
        return false;
    }
    
    // Check pressure limit before starting
    if (pressureManager.isReady()) {
        float currentPressure = pressureManager.getHydraulicPressure();
        if (currentPressure >= EXTEND_PRESSURE_LIMIT_PSI) {
            LOG_WARN("SEQ: Manual extend blocked - pressure limit reached (%.1f >= %.1f PSI)", 
                     currentPressure, EXTEND_PRESSURE_LIMIT_PSI);
            return false;
        }
    }
    
    // Start manual extend sequence
    sequenceType = SEQ_TYPE_MANUAL_EXTEND;
    enterState(SEQ_MANUAL_EXTEND_ACTIVE);
    
    if (g_relayController) {
        g_relayController->setRelay(RELAY_EXTEND, true);
    }
    
    LOG_INFO("SEQ: Manual extend sequence started");
    
    // MQTT publishing removed - non-networking version
    
    return true;
}

bool SequenceController::startManualRetract() {
    if (currentState != SEQ_IDLE) {
        LOG_WARN("SEQ: Cannot start manual retract - sequence active (state: %d)", (int)currentState);
        return false;
    }
    
    // Check if sequence is disabled (timeout lockout)
    if (sequenceDisabled) {
        LOG_WARN("SEQ: Manual retract blocked - controller disabled due to timeout");
        return false;
    }
    
    // Safety check: verify we're not already at retract limit
    if (inputManager && inputManager->getPinState(LIMIT_RETRACT_PIN)) {
        LOG_WARN("SEQ: Manual retract blocked - already at retract limit");
        return false;
    }
    
    // Check pressure limit before starting
    if (pressureManager.isReady()) {
        float currentPressure = pressureManager.getHydraulicPressure();
        if (currentPressure >= RETRACT_PRESSURE_LIMIT_PSI) {
            LOG_WARN("SEQ: Manual retract blocked - pressure limit reached (%.1f >= %.1f PSI)", 
                     currentPressure, RETRACT_PRESSURE_LIMIT_PSI);
            return false;
        }
    }
    
    // Start manual retract sequence
    sequenceType = SEQ_TYPE_MANUAL_RETRACT;
    enterState(SEQ_MANUAL_RETRACT_ACTIVE);
    
    if (g_relayController) {
        g_relayController->setRelay(RELAY_RETRACT, true);
    }
    
    LOG_INFO("SEQ: Manual retract sequence started");
    
    // MQTT publishing removed - non-networking version
    
    return true;
}

bool SequenceController::stopManualOperation() {
    if (!isManualActive()) {
        return false; // No manual operation to stop
    }
    
    // Stop the appropriate relay
    if (currentState == SEQ_MANUAL_EXTEND_ACTIVE && g_relayController) {
        g_relayController->setRelay(RELAY_EXTEND, false);
        LOG_INFO("SEQ: Manual extend stopped by user command");
    } else if (currentState == SEQ_MANUAL_RETRACT_ACTIVE && g_relayController) {
        g_relayController->setRelay(RELAY_RETRACT, false);
        LOG_INFO("SEQ: Manual retract stopped by user command");
    }
    
    // Return to idle
    sequenceType = SEQ_TYPE_AUTOMATIC;
    enterState(SEQ_IDLE);
    
    // Publish manual stop
    // MQTT publishing removed - non-networking version
    
    return true;
}

void SequenceController::handleManualExtend() {
    // Check for extend limit (physical switch OR pressure-based limit)
    bool extendLimitReached = false;
    
    if (inputManager) {
        extendLimitReached = inputManager->getPinState(LIMIT_EXTEND_PIN);
    }
    
    // Check pressure-based extend limit (parallel check)
    if (pressureManager.isReady()) {
        float currentPressure = pressureManager.getHydraulicPressure();
        if (currentPressure >= EXTEND_PRESSURE_LIMIT_PSI) {
            extendLimitReached = true;
            debugPrintf("[SEQ] Manual extend pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                       currentPressure, EXTEND_PRESSURE_LIMIT_PSI);
        }
    }
    
    if (extendLimitReached) {
        // Stop extend relay immediately
        if (g_relayController) {
            g_relayController->setRelay(RELAY_EXTEND, false);
        }
        
        LOG_INFO("SEQ: Manual extend stopped - limit reached");
        
        // Return to idle
        sequenceType = SEQ_TYPE_AUTOMATIC;
        enterState(SEQ_IDLE);
        
        // MQTT publishing removed - non-networking version
        return;
    }
    
    // Check for timeout
    checkTimeout();
}

void SequenceController::handleManualRetract() {
    // Check for retract limit (physical switch OR pressure-based limit)
    bool retractLimitReached = false;
    
    if (inputManager) {
        retractLimitReached = inputManager->getPinState(LIMIT_RETRACT_PIN);
    }
    
    // Check pressure-based retract limit (parallel check)
    if (pressureManager.isReady()) {
        float currentPressure = pressureManager.getHydraulicPressure();
        if (currentPressure >= RETRACT_PRESSURE_LIMIT_PSI) {
            retractLimitReached = true;
            debugPrintf("[SEQ] Manual retract pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                       currentPressure, RETRACT_PRESSURE_LIMIT_PSI);
        }
    }
    
    if (retractLimitReached) {
        // Stop retract relay immediately
        if (g_relayController) {
            g_relayController->setRelay(RELAY_RETRACT, false);
        }
        
        LOG_INFO("SEQ: Manual retract stopped - limit reached");
        
        // Return to idle
        sequenceType = SEQ_TYPE_AUTOMATIC;
        enterState(SEQ_IDLE);
        
        // MQTT publishing removed - non-networking version
        return;
    }
    
    // Check for timeout
    checkTimeout();
}

void SequenceController::checkManualStopConditions() {
    // This method can be expanded later for additional stop conditions
    // For now, limits and timeouts are handled in the individual handlers
}