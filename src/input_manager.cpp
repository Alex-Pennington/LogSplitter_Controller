#include "input_manager.h"
#include "network_manager.h"
#include "config_manager.h"

extern NetworkManager* g_networkManager;
extern void debugPrintf(const char* fmt, ...);

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
        
        debugPrintf("Pin %d configured as %s, initial state: %s\n", 
                   pin, 
                   (configManager && configManager->isPinNC(i)) ? "NC" : "NO",
                   pinStates[i] ? "ACTIVE" : "INACTIVE");
    }
    
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
                debugPrintf("[INPUT] Pin %d RAW: digitalRead()=%s -> %s (starting debounce)\n", 
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
                if (pin == LIMIT_EXTEND_PIN || pin == LIMIT_RETRACT_PIN) {
                    debugPrintf("[INPUT] Pin %d: %s -> %s (debounced in %lums)\n", 
                        pin, 
                        oldState ? "ACTIVE" : "INACTIVE",
                        pinStates[i] ? "ACTIVE" : "INACTIVE",
                        debounceDelay
                    );
                } else {
                    debugPrintf("[INPUT] Pin %d: %s -> %s\n", 
                        pin, 
                        oldState ? "ACTIVE" : "INACTIVE",
                        pinStates[i] ? "ACTIVE" : "INACTIVE"
                    );
                }
                
                // Publish to MQTT
                if (g_networkManager && g_networkManager->isConnected()) {
                    char topic[32];
                    char payload[16];
                    
                    snprintf(topic, sizeof(topic), "r4/inputs/%d", pin);
                    snprintf(payload, sizeof(payload), "%s %d", 
                        pinStates[i] ? "ON" : "OFF",
                        pinStates[i] ? 1 : 0
                    );
                    
                    g_networkManager->publish(topic, payload);
                }
                
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