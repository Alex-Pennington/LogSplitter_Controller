#include <Arduino.h>
#include "constants.h"
#include "network_manager.h"
#include "telnet_server.h"
#include "monitor_system.h"
#include "command_processor.h"
#include "lcd_display.h"
#include "mcp9600_sensor.h"
#include "shared/logger/logger.h"

// Global instances
NetworkManager networkManager;
TelnetServer telnetServer;
MonitorSystem monitorSystem;
CommandProcessor commandProcessor;
LCDDisplay lcdDisplay;

// Global pointer for external access
NetworkManager* g_networkManager = &networkManager;
LCDDisplay* g_lcdDisplay = &lcdDisplay;
MCP9600Sensor* g_mcp9600Sensor = nullptr; // Will be set by monitor system

// Global debug flag
bool g_debugEnabled = true; // Enable debug by default for troubleshooting

// System state
SystemState currentSystemState = SYS_INITIALIZING;
unsigned long lastWatchdog = 0;

// Debug printf function that sends to syslog
void debugPrintf(const char* fmt, ...) {
    if (!g_debugEnabled) return;
    
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    // Send to Serial for local debugging
    Serial.print("DEBUG: ");
    Serial.print(buffer);
    
    // Send to syslog server if network is available
    if (networkManager.isWiFiConnected()) {
        // Remove newlines for syslog
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        networkManager.sendSyslog(buffer);
    }
}

void setup() {
    // Initialize Serial FIRST
    Serial.begin(115200);
    delay(1000); // Give time for serial monitor to connect
    
    Serial.println("MONITOR STARTING...");
    Serial.println("===============================================");
    Serial.println("   LogSplitter Monitor - Starting Up");
    Serial.println("   Version: 1.0.0");
    Serial.println("===============================================");
    
    // Initialize system components one by one with debug output
    Serial.println("DEBUG: About to initialize components");
    
    Serial.println("DEBUG: Initializing monitor system...");
    monitorSystem.begin();
    Serial.println("DEBUG: Monitor system initialized");
    
    Serial.println("DEBUG: Initializing LCD display...");
    if (lcdDisplay.begin()) {
        Serial.println("DEBUG: LCD display initialized successfully");
    } else {
        Serial.println("DEBUG: LCD display initialization failed or not present");
    }
    
    monitorSystem.setSystemState(SYS_INITIALIZING);
    Serial.println("DEBUG: System state set to initializing");
    
    Serial.println("DEBUG: Initializing command processor...");
    commandProcessor.begin(&networkManager, &monitorSystem);
    Serial.println("DEBUG: Command processor initialized");
    
    Serial.println("DEBUG: About to initialize network manager...");
    networkManager.begin();
    Serial.println("DEBUG: Network manager initialized");
    
    // Initialize Logger system with NetworkManager
    Serial.println("DEBUG: Initializing Logger system...");
    Logger::begin(&networkManager);
    Logger::setLogLevel(LOG_INFO);  // Default to INFO level
    Serial.println("DEBUG: Logger initialized");
    
    // Show connecting message on LCD
    lcdDisplay.showConnectingMessage();
    
    Serial.println("DEBUG: Setting hostname...");
    networkManager.setHostname("LogMonitor");
    Serial.println("DEBUG: Hostname set");
    
    Serial.println("DEBUG: Setting telnet server connection info...");
    telnetServer.setConnectionInfo("LogMonitor", "1.0.0");
    Serial.println("DEBUG: Telnet connection info set");
    
    debugPrintf("System: All components initialized\n");
    currentSystemState = SYS_CONNECTING;
    monitorSystem.setSystemState(SYS_CONNECTING);
    
    Serial.println("System initialization complete");
    Serial.println("Waiting for network connection...");
}

void loop() {
    unsigned long now = millis();
    
    // Update watchdog
    lastWatchdog = now;
    
    // Update all system components
    networkManager.update();
    monitorSystem.update();
    
    // Start telnet server once network is connected
    if (networkManager.isWiFiConnected() && currentSystemState == SYS_CONNECTING) {
        telnetServer.begin(23);
        currentSystemState = SYS_MONITORING;
        monitorSystem.setSystemState(SYS_MONITORING);
        
        debugPrintf("System: Network connected, telnet server started\n");
        Serial.println("Network connected! Telnet server running on port 23");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    }
    
    // Update telnet server
    if (networkManager.isWiFiConnected()) {
        telnetServer.update();
        
        // Process telnet commands
        if (telnetServer.isConnected()) {
            String command = telnetServer.readLine();
            if (command.length() > 0) {
                char commandBuffer[COMMAND_BUFFER_SIZE];
                command.toCharArray(commandBuffer, sizeof(commandBuffer));
                
                char response[SHARED_BUFFER_SIZE];
                bool success = commandProcessor.processCommand(commandBuffer, false, response, sizeof(response));
                
                if (strlen(response) > 0) {
                    telnetServer.print(response);
                    telnetServer.print("\r\n");
                }
                
                // Show prompt
                telnetServer.print("\r\n> ");
                
                debugPrintf("Telnet command: %s, response: %s\n", 
                    commandBuffer, strlen(response) > 0 ? response : "none");
            }
        }
    }
    
    // Handle serial commands for local debugging
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.length() > 0) {
            char commandBuffer[COMMAND_BUFFER_SIZE];
            command.toCharArray(commandBuffer, sizeof(commandBuffer));
            
            char response[SHARED_BUFFER_SIZE];
            bool success = commandProcessor.processCommand(commandBuffer, false, response, sizeof(response));
            
            if (strlen(response) > 0) {
                Serial.print("Response: ");
                Serial.println(response);
            }
            
            Serial.print("> ");
        }
    }
    
    // Check for system errors
    if (!networkManager.isWiFiConnected() && currentSystemState == SYS_MONITORING) {
        currentSystemState = SYS_CONNECTING;
        monitorSystem.setSystemState(SYS_CONNECTING);
        debugPrintf("System: Network disconnected, entering connecting state\n");
    }
    
    // System health check
    static unsigned long lastHealthCheck = 0;
    if (now - lastHealthCheck >= 60000) { // Every minute
        lastHealthCheck = now;
        
        char healthStatus[256];
        monitorSystem.getStatusString(healthStatus, sizeof(healthStatus));
        debugPrintf("Health check: %s\n", healthStatus);
        
        // Check memory and system health
        unsigned long freeMemory = monitorSystem.getFreeMemory();
        if (freeMemory < 5000) { // Less than 5KB free
            debugPrintf("WARNING: Low memory detected: %lu bytes free\n", freeMemory);
        }
    }
    
    // Prevent watchdog timeout with a small delay
    delay(10);
}

// Handle system interrupts and errors
void systemErrorHandler(const char* error) {
    debugPrintf("SYSTEM ERROR: %s\n", error);
    Serial.print("SYSTEM ERROR: ");
    Serial.println(error);
    
    // Set system to error state
    currentSystemState = SYS_ERROR;
    monitorSystem.setSystemState(SYS_ERROR);
    
    // Try to send error to MQTT if possible
    if (networkManager.isMQTTConnected()) {
        char errorTopic[64];
        snprintf(errorTopic, sizeof(errorTopic), "%s", TOPIC_MONITOR_ERROR);
        networkManager.publish(errorTopic, error);
    }
}

// System status helper
const char* getSystemStateString(SystemState state) {
    switch (state) {
        case SYS_INITIALIZING: return "INITIALIZING";
        case SYS_CONNECTING: return "CONNECTING";
        case SYS_MONITORING: return "MONITORING";
        case SYS_ERROR: return "ERROR";
        case SYS_MAINTENANCE: return "MAINTENANCE";
        default: return "UNKNOWN";
    }
}