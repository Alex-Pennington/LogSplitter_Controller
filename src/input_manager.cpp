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
        
        Serial.print("Pin ");
        Serial.print(pin);
        Serial.print(" configured as ");
        Serial.print((configManager && configManager->isPinNC(i)) ? "NC" : "NO");
        Serial.print(", initial state: ");
        Serial.println(pinStates[i] ? "ACTIVE" : "INACTIVE");
    }
    
    Serial.println("InputManager initialized");
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
        }
        
        // Check if debounced state should change
        if ((now - lastDebounceTime[i]) > DEBOUNCE_DELAY_MS) {
            if (lastReadings[i] != pinStates[i]) {
                // State change after debounce
                bool oldState = pinStates[i];
                pinStates[i] = lastReadings[i];
                
                debugPrintf("[INPUT] Pin %d: %s -> %s\n", 
                    pin, 
                    oldState ? "ACTIVE" : "INACTIVE",
                    pinStates[i] ? "ACTIVE" : "INACTIVE"
                );
                
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