#pragma once

#include <Arduino.h>
#include "constants.h"

class InputManager {
private:
    // Pin state tracking
    bool pinStates[WATCH_PIN_COUNT] = {false};
    bool lastReadings[WATCH_PIN_COUNT] = {false};
    unsigned long lastDebounceTime[WATCH_PIN_COUNT] = {0};
    
    // Configuration (managed by ConfigManager)
    class ConfigManager* configManager = nullptr;
    
    // Change callback
    void (*inputChangeCallback)(uint8_t pin, bool state, const bool* allStates) = nullptr;
    
public:
    InputManager() = default;
    
    // Initialization
    void begin(class ConfigManager* config);
    
    // Main update (call in loop)
    void update();
    
    // Configuration
    void setConfigManager(class ConfigManager* config) { configManager = config; }
    void setChangeCallback(void (*callback)(uint8_t, bool, const bool*)) { 
        inputChangeCallback = callback; 
    }
    
    // State access
    bool getPinState(uint8_t pin) const;
    bool getPinStateByIndex(size_t index) const { 
        return (index < WATCH_PIN_COUNT) ? pinStates[index] : false; 
    }
    const bool* getAllPinStates() const { return pinStates; }
    
    // Force pin state refresh (after config changes)
    void refreshPinStates();
};