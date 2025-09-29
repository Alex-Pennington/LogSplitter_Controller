#include "sequence_controller.h"
#include "relay_controller.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "constants.h"

// External references for relay control and network publishing
extern RelayController* g_relayController;
extern NetworkManager* g_networkManager;
extern void debugPrintf(const char* fmt, ...);
// Limit switch states maintained in main
extern bool g_limitExtendActive;   // Pin 6 - fully extended
extern bool g_limitRetractActive;  // Pin 7 - fully retracted
// Pressure manager (defined in main.cpp)
extern PressureManager pressureManager;

void SequenceController::enterState(SequenceState newState) {
    if (currentState != newState) {
        debugPrintf("[SEQ] State change: %d -> %d\n", (int)currentState, (int)newState);
        currentState = newState;
        stateEntryTime = millis();
        
        // Reset limit change timer on state entry
        lastLimitChangeTime = 0;

        // Publish immediate pressure snapshot on every sequence state change
        if (g_networkManager && g_networkManager->isConnected()) {
            pressureManager.publishPressures();
        }
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
    debugPrintf("[SEQ] Aborting sequence: %s\n", reason ? reason : "unknown");
    
    // Turn off all hydraulic relays involved in sequence
    if (g_relayController) {
        g_relayController->setRelay(RELAY_EXTEND, false);
        g_relayController->setRelay(RELAY_RETRACT, false);
    }
    
    // Reset state
    enterState(SEQ_IDLE);
    allowButtonRelease = false;
    pendingPressPin = 0;
    pendingPressTime = 0;
    
    // Publish abort event
    if (g_networkManager && g_networkManager->isConnected()) {
        char event[64];
        snprintf(event, sizeof(event), "aborted_%s", reason ? reason : "unknown");
        g_networkManager->publish(TOPIC_SEQUENCE_EVENT, event);
        g_networkManager->publish(TOPIC_SEQUENCE_STATE, "abort");
    }
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
                
                // Publish sequence start
                if (g_networkManager && g_networkManager->isConnected()) {
                    g_networkManager->publish(TOPIC_SEQUENCE_STATE, "start");
                    g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "started_R1");
                }
            }
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
                    if (g_networkManager && g_networkManager->isConnected()) {
                        g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "switched_to_R2_pressure_or_limit");
                    }
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
                    if (g_networkManager && g_networkManager->isConnected()) {
                        g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "complete_pressure_or_limit");
                        g_networkManager->publish(TOPIC_SEQUENCE_STATE, "complete");
                    }
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
        "stage=%u active=%d elapsed=%lu stableMs=%lu startStableMs=%lu timeoutMs=%lu",
        getStage(), 
        (currentState != SEQ_IDLE) ? 1 : 0,
        elapsed,
        stableTimeMs,
        startStableTimeMs,
        timeoutMs
    );
}

void SequenceController::publishIndividualData() {
    if (!g_networkManager || !g_networkManager->isConnected()) {
        return;
    }
    
    char buffer[16];
    
    // Publish individual sequence values for database integration
    snprintf(buffer, sizeof(buffer), "%u", getStage());
    g_networkManager->publish(TOPIC_SEQUENCE_STAGE, buffer);
    
    snprintf(buffer, sizeof(buffer), "%d", (currentState != SEQ_IDLE) ? 1 : 0);
    g_networkManager->publish(TOPIC_SEQUENCE_ACTIVE, buffer);
    
    snprintf(buffer, sizeof(buffer), "%lu", getElapsedTime());
    g_networkManager->publish(TOPIC_SEQUENCE_ELAPSED, buffer);
}