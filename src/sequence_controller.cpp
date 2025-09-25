#include "sequence_controller.h"
#include "relay_controller.h"
#include "network_manager.h"

// External references for relay control and network publishing
extern RelayController* g_relayController;
extern NetworkManager* g_networkManager;
extern void debugPrintf(const char* fmt, ...);

void SequenceController::enterState(SequenceState newState) {
    if (currentState != newState) {
        debugPrintf("[SEQ] State change: %d -> %d\n", (int)currentState, (int)newState);
        currentState = newState;
        stateEntryTime = millis();
        
        // Reset limit change timer on state entry
        lastLimitChangeTime = 0;
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
                
                // Publish sequence start
                if (g_networkManager && g_networkManager->isConnected()) {
                    g_networkManager->publish(TOPIC_SEQUENCE_STATE, "start");
                    g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "started_R1");
                }
            }
            break;
            
        case SEQ_STAGE1_WAIT_LIMIT:
            // Handled in processInputChange when limit pin changes
            break;
            
        case SEQ_STAGE2_WAIT_LIMIT:
            // Handled in processInputChange when limit pin changes
            break;
            
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
            
            // Check limit pin 6
            if (pin == 6 && state) {
                if (checkStableLimit(pin, state)) {
                    // Transition to stage 2
                    enterState(SEQ_STAGE2_ACTIVE);
                    if (g_relayController) {
                        g_relayController->setRelay(RELAY_EXTEND, false);
                        g_relayController->setRelay(RELAY_RETRACT, true);
                    }
                    
                    if (g_networkManager && g_networkManager->isConnected()) {
                        g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "switched_to_R2");
                    }
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
            
            // Check limit pin 7
            if (pin == 7 && state) {
                if (checkStableLimit(pin, state)) {
                    // Sequence complete
                    if (g_relayController) {
                        g_relayController->setRelay(RELAY_RETRACT, false);
                    }
                    
                    enterState(SEQ_IDLE);
                    allowButtonRelease = false;
                    
                    if (g_networkManager && g_networkManager->isConnected()) {
                        g_networkManager->publish(TOPIC_SEQUENCE_EVENT, "complete");
                        g_networkManager->publish(TOPIC_SEQUENCE_STATE, "complete");
                    }
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