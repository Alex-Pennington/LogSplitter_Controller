#pragma once

#include <Arduino.h>
#include "constants.h"

enum SequenceState {
    SEQ_IDLE,
    SEQ_WAIT_START_DEBOUNCE,
    SEQ_STAGE1_ACTIVE,
    SEQ_STAGE1_WAIT_LIMIT,
    SEQ_STAGE2_ACTIVE, 
    SEQ_STAGE2_WAIT_LIMIT,
    SEQ_COMPLETE,
    SEQ_ABORT
};

class SequenceController {
private:
    // State management
    SequenceState currentState = SEQ_IDLE;
    unsigned long stateEntryTime = 0;
    unsigned long lastLimitChangeTime = 0;
    
    // Timing configuration
    unsigned long stableTimeMs = DEFAULT_SEQUENCE_STABLE_MS;
    unsigned long startStableTimeMs = DEFAULT_SEQUENCE_START_STABLE_MS;
    unsigned long timeoutMs = DEFAULT_SEQUENCE_TIMEOUT_MS;
    
    // Pin state tracking
    bool startButtonsActive = false;
    bool allowButtonRelease = false;
    bool buttonStateAtStart[WATCH_PIN_COUNT] = {false};
    
    // Pending press handling
    int pendingPressPin = 0;
    unsigned long pendingPressTime = 0;
    
    // Helper methods
    void enterState(SequenceState newState);
    bool checkTimeout();
    bool checkStableLimit(uint8_t pin, bool active);
    void abortSequence(const char* reason);
    bool areStartButtonsActive(const bool* pinStates);
    
public:
    SequenceController() = default;
    
    // Configuration
    void setStableTime(unsigned long ms) { stableTimeMs = ms; }
    void setStartStableTime(unsigned long ms) { startStableTimeMs = ms; }
    void setTimeout(unsigned long ms) { timeoutMs = ms; }
    
    unsigned long getStableTime() const { return stableTimeMs; }
    unsigned long getStartStableTime() const { return startStableTimeMs; }
    unsigned long getTimeout() const { return timeoutMs; }
    
    // State access
    bool isActive() const { return currentState != SEQ_IDLE; }
    uint8_t getStage() const;
    SequenceState getState() const { return currentState; }
    unsigned long getElapsedTime() const;
    
    // Main processing
    void update();
    bool processInputChange(uint8_t pin, bool state, const bool* allPinStates);
    
    // Manual control
    void abort() { abortSequence("manual_abort"); }
    void reset();
    
    // Status for telemetry
    void getStatusString(char* buffer, size_t bufferSize);
    void publishIndividualData();
};