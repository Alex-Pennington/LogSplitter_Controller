#include "input_manager.h"
// NetworkManager include removed - non-networking version
#include "config_manager.h"
#include "logger.h"
#include "telemetry_manager.h"

// NetworkManager extern removed - non-networking version
extern void debugPrintf(const char* fmt, ...);
extern TelemetryManager telemetryManager;

// Forward declaration for helper function
Telemetry::InputType getInputTypeFromPin(uint8_t pin);

void InputManager::begin(ConfigManager* config) {
    configManager = config;
    
    // Initialize pin modes and states
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        uint8_t pin = WATCH_PINS[i];
        
        // Configure pin mode based on type
        if (configManager && configManager->isPinNC(i)) {
            // Normally closed: use INPUT (no pullup needed)
            pinMode(pin, INPUT);
        } else {
            // Normally open: use INPUT_PULLUP
            pinMode(pin, INPUT_PULLUP);
        }
        
        // Read initial state
        refreshPinStates();
        
        lastDebounceTime[i] = millis();
        
        LOG_INFO("DI%d configured as %s, initial state: %s", 
                   pin, 
                   (configManager && configManager->isPinNC(i)) ? "NC" : "NO",
                   pinStates[i] ? "ACTIVE" : "INACTIVE");
        debugPrintf("DI%d configured as %s, initial state: %s\n", 
                   pin, 
                   (configManager && configManager->isPinNC(i)) ? "NC" : "NO",
                   pinStates[i] ? "ACTIVE" : "INACTIVE");
    }
    
    LOG_INFO("INPUT: InputManager initialized");
    debugPrintf("InputManager initialized\n");
}

void InputManager::refreshPinStates() {
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        uint8_t pin = WATCH_PINS[i];
        bool reading;
        
        if (configManager && configManager->isPinNC(i)) {
            // Normally closed: active when pin reads HIGH (switch opened)
            reading = digitalRead(pin) == HIGH;
        } else {
            // Normally open: active when pin reads LOW (with pullup)
            reading = digitalRead(pin) == LOW;
        }
        
        pinStates[i] = reading;
        lastReadings[i] = reading;
    }
}

void InputManager::update() {
    unsigned long now = millis();
    
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        uint8_t pin = WATCH_PINS[i];
        bool reading;
        
        // Read pin according to its configuration
        if (configManager && configManager->isPinNC(i)) {
            // Normally closed: active when HIGH (open)
            reading = digitalRead(pin) == HIGH;
        } else {
            // Normally open: active when LOW (pressed, with pullup)
            reading = digitalRead(pin) == LOW;
        }
        
        // Check for raw reading change
        if (reading != lastReadings[i]) {
            // Raw reading changed, reset debounce timer
            lastDebounceTime[i] = now;
            lastReadings[i] = reading;
            
            // Debug raw changes for limit switches (if enabled)
            if (debugPinChanges && (pin == LIMIT_EXTEND_PIN || pin == LIMIT_RETRACT_PIN)) {
                // Send telemetry for raw input change (before debouncing)
                Telemetry::InputType inputType = getInputTypeFromPin(pin);
                telemetryManager.sendDigitalInput(pin, reading, false, 0, inputType);
                
                debugPrintf("[DI%d] RAW: digitalRead()=%s -> %s (starting debounce)\n", 
                    pin, 
                    digitalRead(pin) == HIGH ? "HIGH" : "LOW",
                    reading ? "ACTIVE" : "INACTIVE"
                );
            }
        }
        
        // Check if debounced state should change
        unsigned long debounceDelay = DEBOUNCE_DELAY_MS;  // Default
        
        // Use configurable debounce for limit switches (pins 6,7)
        if (pin == LIMIT_EXTEND_PIN) {
            debounceDelay = pin6DebounceMs;
        } else if (pin == LIMIT_RETRACT_PIN) {
            debounceDelay = pin7DebounceMs;
        } else {
            debounceDelay = BUTTON_DEBOUNCE_MS;
        }
        
        if ((now - lastDebounceTime[i]) > debounceDelay) {
            if (lastReadings[i] != pinStates[i]) {
                // State change after debounce
                bool oldState = pinStates[i];
                pinStates[i] = lastReadings[i];
                
                // Enhanced debug for limit switches
                // Send telemetry for debounced input change
                Telemetry::InputType inputType = getInputTypeFromPin(pin);
                telemetryManager.sendDigitalInput(pin, pinStates[i], true, debounceDelay, inputType);
                
                if (pin == LIMIT_EXTEND_PIN || pin == LIMIT_RETRACT_PIN) {
                    debugPrintf("[DI%d] %s -> %s (debounced in %lums)\n", 
                        pin, 
                        oldState ? "ACTIVE" : "INACTIVE",
                        pinStates[i] ? "ACTIVE" : "INACTIVE",
                        debounceDelay
                    );
                } else {
                    debugPrintf("[DI%d] %s -> %s\n", 
                        pin, 
                        oldState ? "ACTIVE" : "INACTIVE",
                        pinStates[i] ? "ACTIVE" : "INACTIVE"
                    );
                }
                
                // Network publishing removed - non-networking version
                
                // Notify callback if set
                if (inputChangeCallback) {
                    inputChangeCallback(pin, pinStates[i], pinStates);
                }
            }
        }
    }
}

bool InputManager::getPinState(uint8_t pin) const {
    for (size_t i = 0; i < WATCH_PIN_COUNT; i++) {
        if (WATCH_PINS[i] == pin) {
            return pinStates[i];
        }
    }
    return false; // Pin not found
}

void InputManager::setPinDebounce(uint8_t pin, const char* level) {
    unsigned long newDebounceMs;
    
    // Convert level string to timing value
    if (strcasecmp(level, "low") == 0) {
        newDebounceMs = 2;  // Fastest response
    } else if (strcasecmp(level, "med") == 0 || strcasecmp(level, "medium") == 0) {
        newDebounceMs = 5;  // Medium filtering
    } else if (strcasecmp(level, "high") == 0) {
        newDebounceMs = 10; // Most filtering (current default)
    } else {
        return; // Invalid level
    }
    
    // Apply to the specified pin
    if (pin == LIMIT_EXTEND_PIN) {
        pin6DebounceMs = newDebounceMs;
        debugPrintf("Pin 6 (EXTEND) debounce set to %s (%lums)\n", level, newDebounceMs);
    } else if (pin == LIMIT_RETRACT_PIN) {
        pin7DebounceMs = newDebounceMs;
        debugPrintf("Pin 7 (RETRACT) debounce set to %s (%lums)\n", level, newDebounceMs);
    }
}

unsigned long InputManager::getPinDebounceMs(uint8_t pin) const {
    if (pin == LIMIT_EXTEND_PIN) {
        return pin6DebounceMs;
    } else if (pin == LIMIT_RETRACT_PIN) {
        return pin7DebounceMs;
    } else {
        return BUTTON_DEBOUNCE_MS; // Default for other pins
    }
}

const char* InputManager::getPinDebounceLevel(uint8_t pin) const {
    unsigned long ms = getPinDebounceMs(pin);
    if (ms <= 2) return "LOW";
    else if (ms <= 5) return "MED"; 
    else return "HIGH";
}

// Helper function to map pin numbers to telemetry input types
Telemetry::InputType getInputTypeFromPin(uint8_t pin) {
    switch (pin) {
        case 2: return Telemetry::INPUT_MANUAL_EXTEND;
        case 3: return Telemetry::INPUT_MANUAL_RETRACT;
        case 4: return Telemetry::INPUT_SAFETY_CLEAR;
        case 5: return Telemetry::INPUT_SEQUENCE_START;
        case 6: return Telemetry::INPUT_LIMIT_EXTEND;
        case 7: return Telemetry::INPUT_LIMIT_RETRACT;
        case 8: return Telemetry::INPUT_SPLITTER_OPERATOR;
        case 12: return Telemetry::INPUT_EMERGENCY_STOP;
        default: return Telemetry::INPUT_UNKNOWN;
    }
}