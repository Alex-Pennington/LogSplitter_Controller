/**
 * LogSplitter Controller - Refactored Arduino UNO R4 WiFi Implementation
 * 
 * A modular, robust industrial control system featuring:
 * - WiFi/MQTT connectivity with auto-reconnection
 * - Pressure monitoring with safety systems
 * - Complex sequence control with state machine
 * - Input debouncing and relay control
 * - EEPROM configuration management
 * - Command validation and security
 * 
 * Author: Refactored from original monolithic design
 * Date: 2025
 */

#include <Arduino.h>
#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <EEPROM.h>

// Module includes
#include "constants.h"
#include "network_manager.h"
#include "pressure_manager.h"
#include "sequence_controller.h"
#include "relay_controller.h"
#include "config_manager.h"
#include "input_manager.h"
#include "safety_system.h"
#include "command_processor.h"
#include "arduino_secrets.h"

// ============================================================================
// Global Shared Buffers (Memory Optimization)
// ============================================================================

char g_message_buffer[SHARED_BUFFER_SIZE];
char g_topic_buffer[TOPIC_BUFFER_SIZE];
char g_command_buffer[COMMAND_BUFFER_SIZE];
char g_response_buffer[SHARED_BUFFER_SIZE];

// ============================================================================
// System Components
// ============================================================================

NetworkManager networkManager;
PressureManager pressureManager;
SequenceController sequenceController;
RelayController relayController;
ConfigManager configManager;
InputManager inputManager;
SafetySystem safetySystem;
CommandProcessor commandProcessor;

// Global pointers for cross-module access
NetworkManager* g_networkManager = &networkManager;
RelayController* g_relayController = &relayController;

// Global limit switch states for safety system
bool g_limitExtendActive = false;   // Pin 6 - Cylinder fully extended
bool g_limitRetractActive = false;  // Pin 7 - Cylinder fully retracted
bool g_emergencyStopActive = false; // Pin 12 - Emergency stop button
bool g_emergencyStopLatched = false; // E-Stop latch state (requires manual reset)

// ============================================================================
// System State
// ============================================================================

SystemState currentSystemState = SYS_INITIALIZING;
unsigned long lastPublishTime = 0;
unsigned long lastWatchdogReset = 0;
unsigned long systemStartTime = 0;
const unsigned long publishInterval = 5000; // 5 seconds

// Serial command line buffer
static uint8_t serialLinePos = 0;

// ============================================================================
// Watchdog and Safety
// ============================================================================

void resetWatchdog() {
    // Simple watchdog implementation (you may need to adapt for your specific board)
    lastWatchdogReset = millis();
}

void checkSystemHealth() {
    unsigned long now = millis();
    
    // Check if main loop is running (simple watchdog)
    if (now - lastWatchdogReset > MAIN_LOOP_TIMEOUT_MS) {
        Serial.println("SYSTEM ERROR: Main loop timeout detected");
        safetySystem.emergencyStop("main_loop_timeout");
        
        // Force system restart after emergency stop
        delay(1000);
        // You might want to add a proper system restart mechanism here
    }
}

// ============================================================================
// Debug Utilities
// ============================================================================

// Global debug flag - disabled by default
bool g_debugEnabled = false;

void debugPrintf(const char* fmt, ...) {
    if (!g_debugEnabled) return; // Skip if debug disabled
    
    unsigned long ts = millis();
    Serial.print("[TS ");
    Serial.print(ts);
    Serial.print("] ");
    
    char dbgBuf[SHARED_BUFFER_SIZE];
    va_list args;
    va_start(args, fmt);
    vsnprintf(dbgBuf, sizeof(dbgBuf), fmt, args);
    va_end(args);
    
    Serial.print(dbgBuf);
}

// ============================================================================
// Callbacks
// ============================================================================

void onInputChange(uint8_t pin, bool state, const bool* allStates) {
    debugPrintf("Input change: pin %d -> %s\n", pin, state ? "ACTIVE" : "INACTIVE");
    
    // EMERGENCY STOP - Highest Priority (Pin 12)
    if (pin == EMERGENCY_STOP_PIN) {
        g_emergencyStopActive = state;
        debugPrintf("EMERGENCY STOP: %s\n", state ? "PRESSED" : "RELEASED");
        
        if (state) { // E-Stop pressed (active LOW, so state=true means button pressed)
            debugPrintf("*** EMERGENCY STOP ACTIVATED ***\n");
            g_emergencyStopLatched = true;
            currentSystemState = SYS_EMERGENCY_STOP;
            
            // Immediate safety response
            relayController.enableSafety();  // Turn off all relays immediately and enable safety mode
            sequenceController.abort(); // Abort current sequence
            safetySystem.emergencyStop("e_stop_button");
            
            Serial.println("EMERGENCY STOP: All operations halted");
            return; // Don't process any other inputs during E-Stop
        }
    }
    
    // Skip all other input processing if E-Stop is latched
    if (g_emergencyStopLatched) {
        debugPrintf("Input ignored - E-Stop latched\n");
        return;
    }
    
    // Update limit switch states for safety system
    if (pin == LIMIT_EXTEND_PIN) {
        g_limitExtendActive = state;
        debugPrintf("Limit EXTEND: %s\n", state ? "ACTIVE" : "INACTIVE");
        
        // Safety: Stop extend relay if limit switch activated during manual operation
        if (state && relayController.getRelayState(RELAY_EXTEND)) {
            debugPrintf("LIMIT SAFETY: Stopping extend relay - limit reached\n");
            relayController.setRelay(RELAY_EXTEND, false, true); // Manual override to turn off
            Serial.println("SAFETY: Extend operation stopped - limit switch reached");
        }
    } else if (pin == LIMIT_RETRACT_PIN) {
        g_limitRetractActive = state;
        debugPrintf("Limit RETRACT: %s\n", state ? "ACTIVE" : "INACTIVE");
        
        // Safety: Stop retract relay if limit switch activated during manual operation
        if (state && relayController.getRelayState(RELAY_RETRACT)) {
            debugPrintf("LIMIT SAFETY: Stopping retract relay - limit reached\n");
            relayController.setRelay(RELAY_RETRACT, false, true); // Manual override to turn off
            Serial.println("SAFETY: Retract operation stopped - limit switch reached");
        }
    }
    
    // Let sequence controller handle input first
    bool handledBySequence = sequenceController.processInputChange(pin, state, allStates);
    //debugPrintf("handledBySequence: %s\n", handledBySequence ? "ACTIVE" : "INACTIVE");
    //debugPrintf("sequenceController: %s\n", sequenceController.isActive() ? "ACTIVE" : "INACTIVE");
    if (!handledBySequence && !sequenceController.isActive()) {
        // Handle simple pin->relay mapping when no sequence active
        if (pin == 2) {
            relayController.setRelay(RELAY_RETRACT, state);  // Pin 2 -> Retract
        } else if (pin == 3) {
            relayController.setRelay(RELAY_EXTEND, state);   // Pin 3 -> Extend
        } 
    }

    // On any ACTIVE button press (state true) publish a pressure snapshot immediately
    // (Includes limit switches, but harmless; they give context to transitions.)
    if (state && networkManager.isConnected()) {
        pressureManager.publishPressures();
    }
}

void onMqttMessage(int messageSize) {
    // Capture topic for diagnostics
    String topic = networkManager.getMqttClient().messageTopic();
    
    // Read exactly the announced payload size into a local buffer (cap to SHARED_BUFFER_SIZE - 1)
    char payload[SHARED_BUFFER_SIZE];
    int toRead = min(messageSize, (int)sizeof(payload) - 1);
    int idx = 0;
    while (idx < toRead && networkManager.getMqttClient().available()) {
        payload[idx++] = (char)networkManager.getMqttClient().read();
    }
    payload[idx] = '\0';
    
    // Trim leading/trailing whitespace in-place
    int start = 0;
    while (payload[start] == ' ' || payload[start] == '\t' || payload[start] == '\r' || payload[start] == '\n') start++;
    int end = idx - 1;
    while (end >= start && (payload[end] == ' ' || payload[end] == '\t' || payload[end] == '\r' || payload[end] == '\n')) end--;
    int newLen = (end >= start) ? (end - start + 1) : 0;
    if (start > 0 && newLen > 0) {
        memmove(payload, payload + start, newLen);
    }
    payload[newLen] = '\0';
    
    debugPrintf("MQTT msg on '%s' (%dB): '%s'\n", topic.c_str(), messageSize, payload);
    
    // Process command (fromMqtt = true)
    // Copy payload into the shared command buffer for processing (safe copy)
    strncpy(g_command_buffer, payload, sizeof(g_command_buffer) - 1);
    g_command_buffer[sizeof(g_command_buffer) - 1] = '\0';
    bool success = commandProcessor.processCommand(g_command_buffer, true, g_response_buffer, SHARED_BUFFER_SIZE);
    
    // Send response if we have one
    if (strlen(g_response_buffer) > 0) {
        networkManager.publish(TOPIC_CONTROL_RESP, g_response_buffer);
    }
    
    if (!success) {
        debugPrintf("Command processing failed: %s\n", g_response_buffer);
    }
}

// ============================================================================
// System Initialization
// ============================================================================

bool initializeSystem() {
    Serial.begin(115200);
    while (!Serial && millis() < 3000) { 
        delay(10); 
    }
    
    Serial.println();
    Serial.println("=== LogSplitter Controller v2.0 ===");
    Serial.println("Initializing system...");
    
    systemStartTime = millis();
    currentSystemState = SYS_INITIALIZING;
    
    // Initialize configuration first
    configManager.begin();
    if (!configManager.isConfigValid()) {
        Serial.println("WARNING: Using default configuration");
    }
    
    // Initialize pressure sensor
    pressureManager.begin();
    pressureManager.setNetworkManager(&networkManager);
    // Note: Individual sensor configuration can be added via pressureManager.getSensor() if needed
    
    // Initialize relay controller
    relayController.begin();
    configManager.applyToRelayController(relayController);
    
    // Initialize sequence controller
    configManager.applyToSequenceController(sequenceController);
    
    // Initialize input manager
    inputManager.begin(&configManager);
    inputManager.setChangeCallback(onInputChange);
    
    // Initialize safety system
    safetySystem.setRelayController(&relayController);
    safetySystem.setNetworkManager(&networkManager);
    safetySystem.setSequenceController(&sequenceController);
    
    // Initialize command processor
    commandProcessor.setConfigManager(&configManager);
    // Legacy single-sensor interface is optional; use PressureManager when available
    commandProcessor.setPressureManager(&pressureManager);
    commandProcessor.setSequenceController(&sequenceController);
    commandProcessor.setRelayController(&relayController);
    commandProcessor.setNetworkManager(&networkManager);
    commandProcessor.setSafetySystem(&safetySystem);
    
    Serial.println("Core systems initialized");
    
    // Initialize networking
    currentSystemState = SYS_CONNECTING;
    if (networkManager.begin()) {
        networkManager.setMessageCallback(onMqttMessage);
        Serial.println("Network initialization started");
    } else {
        Serial.println("Network initialization failed");
        return false;
    }
    
    currentSystemState = SYS_RUNNING;
    lastPublishTime = millis();
    
    Serial.println("=== System Ready ===");
    return true;
}

// ============================================================================
// Main System Loop
// ============================================================================

void updateSystem() {
    resetWatchdog();
    
    // Update all subsystems
    networkManager.update();
    pressureManager.update();
    sequenceController.update();
    relayController.update();
    inputManager.update();
    
    // Update safety system with current pressure
    if (pressureManager.isReady()) {
        safetySystem.update(pressureManager.getPressure());
    }
    
    checkSystemHealth();
}

void publishTelemetry() {
    unsigned long now = millis();
    
    if (now - lastPublishTime >= publishInterval && networkManager.isConnected()) {
        lastPublishTime = now;
        
        // Publish sequence status
        sequenceController.getStatusString(g_message_buffer, SHARED_BUFFER_SIZE);
        networkManager.publish(TOPIC_SEQUENCE_STATUS, g_message_buffer);
        
        // Publish heartbeat
        snprintf(g_message_buffer, SHARED_BUFFER_SIZE, "T: %lu", now);
        networkManager.publish(TOPIC_PUBLISH, g_message_buffer);
    }
}

void processSerialCommands() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\r') continue; // Ignore CR
        
        if (c == '\n') {
            g_command_buffer[serialLinePos] = '\0';
            serialLinePos = 0;
            
            if (strlen(g_command_buffer) > 0) {
                // Process command
                bool success = commandProcessor.processCommand(g_command_buffer, false, g_response_buffer, SHARED_BUFFER_SIZE);
                
                if (strlen(g_response_buffer) > 0) {
                    Serial.print("Response: ");
                    Serial.println(g_response_buffer);
                }
                
                if (!success) {
                    Serial.println("Command failed. Type 'help' for available commands.");
                }
                
                // Also send response via MQTT if connected
                if (strlen(g_response_buffer) > 0 && networkManager.isConnected()) {
                    snprintf(g_message_buffer, SHARED_BUFFER_SIZE, "cli: %s", g_response_buffer);
                    networkManager.publish(TOPIC_CONTROL_RESP, g_message_buffer);
                }
            }
        } else {
            if (serialLinePos < COMMAND_BUFFER_SIZE - 1) {
                g_command_buffer[serialLinePos++] = c;
            }
        }
    }
}

// ============================================================================
// Arduino Main Functions
// ============================================================================

void setup() {
    if (!initializeSystem()) {
        Serial.println("CRITICAL ERROR: System initialization failed");
        currentSystemState = SYS_ERROR;
        while (true) {
            delay(1000);
            Serial.println("System halted - check connections and restart");
        }
    }
}

void loop() {
    // Handle system states
    switch (currentSystemState) {
        case SYS_RUNNING:
            updateSystem();
            publishTelemetry();
            processSerialCommands();
            break;
            
        case SYS_ERROR:
            // In error state, only do basic safety monitoring
            safetySystem.emergencyStop("system_error");
            delay(1000);
            break;
            
        case SYS_SAFE_MODE:
            // Safe mode: minimal operations, safety monitoring only
            relayController.update();
            safetySystem.update(pressureManager.getPressure());
            processSerialCommands();
            delay(100);
            break;
            
        default:
            currentSystemState = SYS_ERROR;
            break;
    }
    
    // Small delay to prevent overwhelming the system
    delay(1);
}