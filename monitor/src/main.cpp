#include <Arduino.h>
#include <Wire.h>
#include "constants.h"
#include "config_manager.h"
#include "network_manager.h"
#include "telnet_server.h"
#include "monitor_system.h"
#include "command_processor.h"
#include "lcd_display.h"
#include "mcp9600_sensor.h"
#include "logger.h"
#include "tca9548a_multiplexer.h"
#include "tftp_server.h"

// Global instances
ConfigManager configManager;
NetworkManager networkManager(&configManager);
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
    
    // Initialize I2C bus BEFORE any sensors with retry logic
    Serial.println("DEBUG: Initializing I2C bus (Wire1)...");
    bool i2cInitialized = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
        Wire1.begin();
        delay(100); // Allow I2C bus to stabilize
        
        // Test I2C bus by scanning for any device
        bool busWorking = false;
        for (byte testAddr = 8; testAddr < 120 && !busWorking; testAddr++) {
            Wire1.beginTransmission(testAddr);
            if (Wire1.endTransmission() != 2) { // 2 = NACK on address (no device), anything else means bus is working
                busWorking = true;
            }
        }
        
        if (busWorking) {
            i2cInitialized = true;
            Serial.println("DEBUG: I2C bus initialized successfully");
            break;
        } else {
            Serial.print("DEBUG: I2C initialization attempt ");
            Serial.print(attempt);
            Serial.println(" failed, retrying...");
            delay(500);
        }
    }
    
    if (!i2cInitialized) {
        Serial.println("ERROR: I2C bus initialization failed after 3 attempts!");
        Serial.println("ERROR: Check wiring: SDA to A4/SDA1, SCL to A5/SCL1");
    }
    
    // Comprehensive I2C device scan with device identification
    Serial.println("DEBUG: Scanning I2C bus for devices...");
    int deviceCount = 0;
    bool foundTCA9548A = false;
    bool foundMCP9600 = false;
    bool foundNAU7802 = false;
    bool foundINA219 = false;
    bool foundMCP3421 = false;
    bool foundLCD = false;
    
    for (byte address = 1; address < 127; address++) {
        Wire1.beginTransmission(address);
        byte error = Wire1.endTransmission();
        
        if (error == 0) {
            Serial.print("DEBUG: I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            
            // Identify known devices
            switch (address) {
                case 0x70:
                    Serial.print(" (TCA9548A I2C Multiplexer)");
                    foundTCA9548A = true;
                    break;
                case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
                    Serial.print(" (MCP9600 Temperature Sensor)");
                    foundMCP9600 = true;
                    break;
                case 0x2A:
                    Serial.print(" (NAU7802 Load Cell ADC)");
                    foundNAU7802 = true;
                    break;
                case 0x40: case 0x41: case 0x44: case 0x45:
                    Serial.print(" (INA219 Power Monitor)");
                    foundINA219 = true;
                    break;
                case 0x68:
                    Serial.print(" (MCP3421 18-bit ADC)");
                    foundMCP3421 = true;
                    break;
                case 0x3C: case 0x3D:
                    Serial.print(" (OLED Display)");
                    foundLCD = true;
                    break;
                default:
                    Serial.print(" (Unknown device)");
                    break;
            }
            Serial.println();
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("ERROR: No I2C devices found on the bus!");
        Serial.println("ERROR: Check wiring: SDA to A4/SDA1, SCL to A5/SCL1");
    } else {
        Serial.print("DEBUG: Found ");
        Serial.print(deviceCount);
        Serial.println(" I2C device(s)");
        
        // Report missing critical devices
        if (!foundTCA9548A) {
            Serial.println("WARNING: TCA9548A I2C multiplexer not found - sensor access will be limited");
        }
        if (!foundMCP9600) {
            Serial.println("WARNING: MCP9600 temperature sensor not detected");
        }
        if (!foundNAU7802) {
            Serial.println("WARNING: NAU7802 load cell ADC not detected");
        }
        if (!foundINA219) {
            Serial.println("WARNING: INA219 power monitor not detected");
        }
        if (!foundMCP3421) {
            Serial.println("WARNING: MCP3421 18-bit ADC not detected");
        }
        if (!foundLCD) {
            Serial.println("WARNING: LCD/OLED display not detected");
        }
    }
    Serial.println("DEBUG: I2C scan complete");
    
    // Initialize system components one by one with debug output
    Serial.println("DEBUG: About to initialize components");
    
    // Initialize ConfigManager first to load settings from EEPROM
    Serial.println("DEBUG: Initializing configuration manager...");
    if (configManager.loadFromEEPROM()) {
        Serial.println("DEBUG: Configuration loaded from EEPROM successfully");
    } else {
        Serial.println("DEBUG: Using default configuration (EEPROM invalid or empty)");
    }
    
    Serial.println("DEBUG: Initializing monitor system...");
    monitorSystem.begin();
    Serial.println("DEBUG: Monitor system initialized");
    
    Serial.println("DEBUG: Initializing LCD display...");
    // Create temporary multiplexer instance for LCD initialization
    TCA9548A_Multiplexer tempMux(0x70);
    if (tempMux.begin()) {
        tempMux.selectChannel(7); // LCD_CHANNEL = 7
        if (lcdDisplay.begin()) {
            Serial.println("DEBUG: LCD display initialized successfully");
        } else {
            Serial.println("DEBUG: LCD display initialization failed or not present");
        }
        tempMux.disableAllChannels();
    } else {
        Serial.println("DEBUG: Could not access multiplexer for LCD initialization");
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
        Serial.println("For firmware updates, use USB connection or Arduino Cloud");
    }
    
    // Update telnet server
    if (networkManager.isWiFiConnected()) {
        telnetServer.update();
        
        // Update TFTP server
        tftpServer.update();
        
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