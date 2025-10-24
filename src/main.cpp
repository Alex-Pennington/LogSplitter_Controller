/**
 * LogSplitter Controller - Non-Networking Version
 * 
 * A modular, robust industrial control system featuring:
 * - Serial communication for control and monitoring
 * - Pressure monitoring with safety systems
 * - Complex sequence control with state machine
 * - Input debouncing and relay control
 * - EEPROM configuration management
 * - Command validation and security
 * 
 * Author: Refactored from original networked design
 * Date: 2025
 */

#include <Arduino.h>
#include <EEPROM.h>

// Module includes
#include "constants.h"
#include "logger.h"
#include "pressure_manager.h"
#include "sequence_controller.h"
#include "relay_controller.h"
#include "config_manager.h"
#include "input_manager.h"
#include "safety_system.h"
#include "system_error_manager.h"
#include "command_processor.h"
#include "subsystem_timing_monitor.h"
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

PressureManager pressureManager;
SequenceController sequenceController;
RelayController relayController;
ConfigManager configManager;
InputManager inputManager;
SafetySystem safetySystem;
SystemErrorManager systemErrorManager;
CommandProcessor commandProcessor;
SubsystemTimingMonitor timingMonitor;

// Global pointers for cross-module access
RelayController* g_relayController = &relayController;

// Global limit switch states for safety system
bool g_limitExtendActive = false;   // Pin 6 - Cylinder fully extended
bool g_limitRetractActive = false;  // Pin 7 - Cylinder fully retracted

// Global debug and safety state variables
bool g_debugEnabled = false;        // Debug output control
bool g_emergencyStopActive = false; // Emergency stop current state
bool g_emergencyStopLatched = false; // Emergency stop latched state

// ============================================================================
// System State
// ============================================================================

SystemState currentSystemState = SYS_INITIALIZING;
unsigned long lastPublishTime = 0;
unsigned long lastWatchdogReset = 0;
unsigned long systemStartTime = 0;
const unsigned long publishInterval = 5000; // 5 seconds

// Telnet tracking removed - non-networking version

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
        LOG_CRITICAL("Main loop timeout detected - system unresponsive");
        
        // Generate detailed timing analysis for the timeout
        char timingAnalysis[512];
        timingMonitor.analyzeTimeout(timingAnalysis, sizeof(timingAnalysis));
        LOG_CRITICAL("TIMEOUT ANALYSIS: %s", timingAnalysis);
        
        // Log detailed timing report to identify bottleneck
        LOG_CRITICAL("=== TIMEOUT TIMING REPORT ===");
        timingMonitor.logTimingReport();
        
        // Emergency timeout detected - no network bypass needed in non-networking version
        
        safetySystem.emergencyStop("main_loop_timeout");
        
        // Force system restart after emergency stop
        delay(1000);
        // You might want to add a proper system restart mechanism here
    }
}

// ============================================================================
// Debug Utilities
// ============================================================================

// Legacy debugPrintf function - now uses new Logger system
// This maintains compatibility with existing code
void debugPrintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_message_buffer, SHARED_BUFFER_SIZE, fmt, args);
    va_end(args);
    
    // Route to new logging system as DEBUG level
    Logger::log(LOG_DEBUG, "%s", g_message_buffer);
}

// ============================================================================
// Callbacks
// ============================================================================

void onInputChange(uint8_t pin, bool state, const bool* allStates) {
    debugPrintf("Input change: pin %d -> %s\n", pin, state ? "ACTIVE" : "INACTIVE");
    
    // Update limit switch states for safety system
    if (pin == LIMIT_EXTEND_PIN) {
        g_limitExtendActive = state;
        debugPrintf("Limit EXTEND: %s\n", state ? "ACTIVE" : "INACTIVE");
        
        // SAFETY: Turn off extend relay when limit switch activates
        if (state) {  // Limit switch activated (cylinder fully extended)
            relayController.setRelay(RELAY_EXTEND, false);
            debugPrintf("SAFETY: Extend relay R%d turned OFF - cylinder at full extension\n", RELAY_EXTEND);
        }
        
    } else if (pin == LIMIT_RETRACT_PIN) {
        g_limitRetractActive = state;
        debugPrintf("Limit RETRACT: %s\n", state ? "ACTIVE" : "INACTIVE");
        
        // SAFETY: Turn off retract relay when limit switch activates  
        if (state) {  // Limit switch activated (cylinder fully retracted)
            relayController.setRelay(RELAY_RETRACT, false);
            debugPrintf("SAFETY: Retract relay R%d turned OFF - cylinder at full retraction\n", RELAY_RETRACT);
        }
    }
    
    // PRIORITY: Handle E-Stop before any other processing
    if (pin == E_STOP_PIN) {
        if (!state) {  // E-stop pressed (NC switch goes LOW)
            LOG_CRITICAL("E-STOP ACTIVATED - Emergency shutdown initiated");
            safetySystem.activateEStop();
            sequenceController.abort(); // Immediate sequence abort
            sequenceController.disableSequence(); // Disable until E-stop cleared
            return; // Skip all other processing
        } else {
            // E-stop released - but system should remain in safe state
            LOG_INFO("E-STOP: Physical button released - system remains in safe state");
            return; // Don't process as normal input
        }
    }
    
    // Let sequence controller handle input first
    bool handledBySequence = sequenceController.processInputChange(pin, state, allStates);
    //debugPrintf("handledBySequence: %s\n", handledBySequence ? "ACTIVE" : "INACTIVE");
    //debugPrintf("sequenceController: %s\n", sequenceController.isActive() ? "ACTIVE" : "INACTIVE");
    
    // Handle safety clear button (Pin 4) - allows operational recovery without clearing error history
    if (pin == SAFETY_CLEAR_PIN && state) {  // Safety clear button pressed
        bool systemCleared = false;
        
        // Clear E-stop state if active
        if (safetySystem.isEStopActive()) {
            LOG_INFO("SAFETY: Manager override - clearing E-stop state, preserving error history");
            safetySystem.clearEStop();
            systemCleared = true;
        }
        
        // Clear general safety system if active
        if (safetySystem.isActive()) {
            LOG_INFO("SAFETY: Manager override - clearing safety system, preserving error history");
            safetySystem.clearEmergencyStop();  // Clear safety state to allow operation
            systemCleared = true;
            // Note: Error list is NOT cleared - manager must clear errors separately via commands
        }
        
        // Re-enable sequence controller if it was disabled due to timeout or E-stop
        if (!sequenceController.isSequenceEnabled()) {
            LOG_INFO("SEQ: Re-enabling sequence controller after safety clear");
            sequenceController.enableSequence();
            systemCleared = true;
        }
        
        if (systemCleared) {
            LOG_INFO("SAFETY: System fully restored to operational state");
        } else {
            LOG_INFO("SAFETY: Clear button pressed - system already operational");
        }
        
        return;  // Safety clear handled, don't process as normal input
    }
    
    if (!handledBySequence && !sequenceController.isActive()) {
        // Handle simple pin->relay mapping when no sequence active
        if (pin == 2) {
            // SAFETY: Don't allow retract if already at retract limit
            if (state && g_limitRetractActive) {
                debugPrintf("SAFETY: Retract blocked - cylinder already at retract limit\n");
            } else {
                relayController.setRelay(RELAY_RETRACT, state);  // Pin 2 -> Retract
            }
        } else if (pin == 3) {
            // SAFETY: Don't allow extend if already at extend limit
            if (state && g_limitExtendActive) {
                debugPrintf("SAFETY: Extend blocked - cylinder already at extend limit\n");
            } else {
                relayController.setRelay(RELAY_EXTEND, state);   // Pin 3 -> Extend
            }
        } 
    }
}

// MQTT callback removed - non-networking version

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
    // Network manager removed - non-networking version
    // Note: Individual sensor configuration can be added via pressureManager.getSensor() if needed
    
    // Initialize relay controller
    relayController.begin();
    configManager.applyToRelayController(relayController);
    
    // Initialize sequence controller
    configManager.applyToSequenceController(sequenceController);
    
    // Apply saved log level configuration
    configManager.applyToLogger();
    
    // Initialize input manager
    inputManager.begin(&configManager);
    inputManager.setChangeCallback(onInputChange);
    
    // Initialize safety system
    safetySystem.setRelayController(&relayController);
    safetySystem.setSequenceController(&sequenceController);
    safetySystem.begin();  // Initialize engine relay to running state
    
    // Initialize system error manager
    systemErrorManager.begin();
    
    // Initialize command processor
    commandProcessor.setConfigManager(&configManager);
    commandProcessor.setPressureManager(&pressureManager);  // FIXED: Connect pressure manager
    commandProcessor.setSequenceController(&sequenceController);
    commandProcessor.setRelayController(&relayController);
    commandProcessor.setSafetySystem(&safetySystem);
    commandProcessor.setInputManager(&inputManager);
    commandProcessor.setSystemErrorManager(&systemErrorManager);
    commandProcessor.setTimingMonitor(&timingMonitor);
    
    // Connect error manager to other components
    configManager.setSystemErrorManager(&systemErrorManager);
    sequenceController.setErrorManager(&systemErrorManager);
    sequenceController.setInputManager(&inputManager);
    
    Serial.println("Core systems initialized");
    
    // Initialize logger (serial only in non-networking version)
    currentSystemState = SYS_RUNNING;
    Logger::begin(nullptr);  // No network manager - serial only
    Logger::setLogLevel(LOG_DEBUG);  // Default to debug level, can be configured later
    LOG_INFO("Logger initialized (serial only mode)");
    
    // Initialize timing monitor
    timingMonitor.begin();
    LOG_INFO("Subsystem timing monitor initialized");
    
    currentSystemState = SYS_RUNNING;
    lastPublishTime = millis();
    
    Serial.println("=== System Ready ===");
    Serial.println("Network connection will start in 3 seconds to avoid boot delays");
    Serial.println("Network Failsafe: Use 'bypass on' command if network causes unresponsiveness");
    return true;
}

// ============================================================================
// Main System Loop
// ============================================================================

void updateSystem() {
    TIME_SUBSYSTEM(&timingMonitor, SubsystemID::MAIN_LOOP_TOTAL);
    resetWatchdog();
    
    // Update all subsystems with timing monitoring
    // Network manager and telnet removed in non-networking version
    
    {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::PRESSURE_MANAGER);
        pressureManager.update();
    }
    
    {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::SEQUENCE_CONTROLLER);
        sequenceController.update();
    }
    
    {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::RELAY_CONTROLLER);
        relayController.update();
    }
    
    {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::INPUT_MANAGER);
        inputManager.update();
    }
    
    {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::SYSTEM_ERROR_MANAGER);
        systemErrorManager.update();
    }
    
    // Update safety system with current pressure
    if (pressureManager.isReady()) {
        TIME_SUBSYSTEM(&timingMonitor, SubsystemID::SAFETY_SYSTEM);
        safetySystem.update(pressureManager.getPressure());
    }
    
    // Check timing monitor health
    timingMonitor.checkHealthStatus();
    
    checkSystemHealth();
}

void publishTelemetry() {
    // Telemetry publishing removed - non-networking version
    // All telemetry now available via serial commands only
}

void processSerialCommands() {
    TIME_SUBSYSTEM(&timingMonitor, SubsystemID::COMMAND_PROCESSING_SERIAL);
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
                
                // MQTT response removed - non-networking version
            }
        } else {
            if (serialLinePos < COMMAND_BUFFER_SIZE - 1) {
                g_command_buffer[serialLinePos++] = c;
            }
        }
    }
}

// Telnet command processing removed - non-networking version

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