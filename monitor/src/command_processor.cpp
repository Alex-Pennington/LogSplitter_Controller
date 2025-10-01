#include "command_processor.h"
#include "lcd_display.h"
#include "logger.h"
#include "heartbeat_animation.h"
#include <ctype.h>
#include <string.h>
#include <Wire.h>

extern void debugPrintf(const char* fmt, ...);
extern bool g_debugEnabled;
extern LCDDisplay* g_lcdDisplay;

// Static data for rate limiting
static unsigned long lastCommandTime = 0;
static const unsigned long COMMAND_RATE_LIMIT_MS = 100; // 10 commands/second max

CommandProcessor::CommandProcessor() :
    networkManager(nullptr),
    monitorSystem(nullptr) {
}

void CommandProcessor::begin(NetworkManager* network, MonitorSystem* monitor) {
    networkManager = network;
    monitorSystem = monitor;
    debugPrintf("CommandProcessor: Initialized\n");
}

bool CommandProcessor::processCommand(char* commandBuffer, bool fromMqtt, char* response, size_t responseSize) {
    // Initialize response
    response[0] = '\0';
    
    // Rate limiting
    if (!CommandValidator::checkRateLimit()) {
        snprintf(response, responseSize, "rate limited");
        return false;
    }
    
    // Sanitize input
    CommandValidator::sanitizeInput(commandBuffer, COMMAND_BUFFER_SIZE - 1);
    
    // Trim whitespace
    char* start = commandBuffer;
    while (*start == ' ' || *start == '\t') start++;
    
    // Find end and remove trailing whitespace
    char* end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        *end = '\0';
        end--;
    }
    
    // Copy trimmed command back
    if (start != commandBuffer) {
        memmove(commandBuffer, start, strlen(start) + 1);
    }
    
    debugPrintf("Command received: '%s' (length: %d)\n", commandBuffer, strlen(commandBuffer));
    
    // Tokenize command
    char* cmd = strtok(commandBuffer, " ");
    if (!cmd) {
        snprintf(response, responseSize, "empty command");
        return false;
    }
    
    debugPrintf("Parsed command: '%s'\n", cmd);
    
    // Validate command
    if (!CommandValidator::validateCommand(cmd)) {
        snprintf(response, responseSize, "invalid command: '%s'", cmd);
        return false;
    }
    
    // Handle specific commands
    if (strcasecmp(cmd, "help") == 0) {
        handleHelp(response, responseSize, fromMqtt);
    }
    else if (strcasecmp(cmd, "show") == 0) {
        handleShow(response, responseSize, fromMqtt);
    }
    else if (strcasecmp(cmd, "status") == 0) {
        handleStatus(response, responseSize);
    }
    else if (strcasecmp(cmd, "set") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        
        if (CommandValidator::validateSetCommand(param, value)) {
            handleSet(param, value, response, responseSize);
        } else {
            snprintf(response, responseSize, "invalid set command");
        }
    }
    else if (strcasecmp(cmd, "debug") == 0) {
        char* param = strtok(NULL, " ");
        handleDebug(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "network") == 0) {
        handleNetwork(response, responseSize);
    }
    else if (strcasecmp(cmd, "reset") == 0) {
        char* param = strtok(NULL, " ");
        handleReset(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "test") == 0) {
        char* param = strtok(NULL, " ");
        handleTest(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "syslog") == 0) {
        char* param = strtok(NULL, " ");
        handleSyslog(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "monitor") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        handleMonitor(param, value, response, responseSize);
    }
    else if (strcasecmp(cmd, "weight") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        handleWeight(param, value, response, responseSize);
    }
    else if (strcasecmp(cmd, "loglevel") == 0) {
        char* param = strtok(NULL, " ");
        handleLogLevel(param, response, responseSize);
    }
    else if (strcasecmp(cmd, "temp") == 0 || strcasecmp(cmd, "temperature") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        handleTemperature(param, value, response, responseSize);
    }
    else if (strcasecmp(cmd, "lcd") == 0) {
        char* param = strtok(NULL, " ");
        char* value = strtok(NULL, " ");
        handleLCD(param, value, response, responseSize);
    }
    else if (strcasecmp(cmd, "heartbeat") == 0) {
        char* param = strtok(NULL, " ");
        handleHeartbeat(param, response, responseSize);
    }
    else {
        snprintf(response, responseSize, "unknown command: %s", cmd);
        return false;
    }
    
    return true;
}

void CommandProcessor::handleHelp(char* response, size_t responseSize, bool fromMqtt) {
    snprintf(response, responseSize, 
        "Available commands:\r\n"
        "help           - Show this help\r\n" 
        "show           - Show sensor readings\r\n"
        "status         - Show system status\r\n"
        "network        - Show network status\r\n"
        "debug [on|off] - Toggle debug mode\r\n"
        "loglevel [0-7] - Set logging level (0=EMERGENCY, 7=DEBUG)\r\n"
        "monitor start  - Start monitoring\r\n"
        "monitor stop   - Stop monitoring\r\n"
        "weight read    - Read current weight\r\n"
        "weight tare    - Tare the scale\r\n"
        "weight zero    - Zero calibration\r\n"
        "weight calibrate <weight> - Scale calibration\r\n"
        "temp read      - Read temperature sensors (Fahrenheit)\r\n"
        "temp readc     - Read temperature sensors (Celsius)\r\n"
        "temp local|localc - Local temperature (F/C)\r\n"
        "temp remote|remotec - Remote temperature (F/C)\r\n"
        "temp status    - Show temperature sensor status\r\n"
        "temp offset <local> <remote> - Set temperature offsets\r\n"
        "lcd on|off     - Turn LCD display on/off\r\n"
        "lcd backlight on|off - Control LCD backlight\r\n"
        "lcd clear      - Clear LCD display\r\n"
        "heartbeat on|off - Control heartbeat animation\r\n"
        "heartbeat rate <bpm> - Set heart rate (30-200 BPM)\r\n"
        "heartbeat brightness <0-255> - Set brightness\r\n"
        "test network   - Test network connectivity\r\n"
        "syslog test    - Send test syslog message\r\n"
        "reset system   - Restart the device");
}

void CommandProcessor::handleShow(char* response, size_t responseSize, bool fromMqtt) {
    if (!monitorSystem) {
        snprintf(response, responseSize, "monitor system not available");
        return;
    }
    
    monitorSystem->getStatusString(response, responseSize);
}

void CommandProcessor::handleStatus(char* response, size_t responseSize) {
    if (!monitorSystem || !networkManager) {
        snprintf(response, responseSize, "system components not available");
        return;
    }
    
    char networkStatus[128];
    char monitorStatus[128];
    
    networkManager->getHealthString(networkStatus, sizeof(networkStatus));
    monitorSystem->getStatusString(monitorStatus, sizeof(monitorStatus));
    
    snprintf(response, responseSize, "network: %s | monitor: %s", 
        networkStatus, monitorStatus);
}

void CommandProcessor::handleSet(char* param, char* value, char* response, size_t responseSize) {
    if (!param || !value) {
        snprintf(response, responseSize, "Usage: set <param> <value>");
        return;
    }
    
    if (strcasecmp(param, "debug") == 0) {
        bool enabled;
        if (strcasecmp(value, "on") == 0 || strcasecmp(value, "1") == 0) {
            enabled = true;
        } else if (strcasecmp(value, "off") == 0 || strcasecmp(value, "0") == 0) {
            enabled = false;
        } else {
            snprintf(response, responseSize, "debug value must be ON|OFF|1|0");
            return;
        }
        
        g_debugEnabled = enabled;
        snprintf(response, responseSize, "debug %s", enabled ? "ON" : "OFF");
    }
    else if (strcasecmp(param, "loglevel") == 0) {
        if (!value) {
            // Show current log level
            LogLevel currentLevel = Logger::getLogLevel();
            snprintf(response, responseSize, "Current log level: %d (%s)", 
                currentLevel, 
                (currentLevel == LOG_EMERGENCY) ? "EMERGENCY" :
                (currentLevel == LOG_ALERT) ? "ALERT" :
                (currentLevel == LOG_CRITICAL) ? "CRITICAL" :
                (currentLevel == LOG_ERROR) ? "ERROR" :
                (currentLevel == LOG_WARNING) ? "WARNING" :
                (currentLevel == LOG_NOTICE) ? "NOTICE" :
                (currentLevel == LOG_INFO) ? "INFO" :
                (currentLevel == LOG_DEBUG) ? "DEBUG" : "UNKNOWN");
            return;
        }
        
        int level = atoi(value);
        if (level < 0 || level > 7) {
            snprintf(response, responseSize, "Invalid log level. Use 0-7 (0=EMERGENCY, 7=DEBUG)");
            return;
        }
        
        Logger::setLogLevel(static_cast<LogLevel>(level));
        snprintf(response, responseSize, "Log level set to %d", level);
    }
    else if (strcasecmp(param, "syslog") == 0) {
        if (networkManager) {
            // Parse syslog server address and optional port
            char* portPtr = strchr(value, ':');
            int port = SYSLOG_PORT;
            
            if (portPtr) {
                *portPtr = '\0'; // Split the string
                port = atoi(portPtr + 1);
                if (port <= 0 || port > 65535) {
                    snprintf(response, responseSize, "Invalid port number");
                    return;
                }
            }
            
            networkManager->setSyslogServer(value, port);
            if (portPtr) {
                snprintf(response, responseSize, "syslog server set to %s:%d", value, port);
            } else {
                snprintf(response, responseSize, "syslog server set to %s:%d", value, SYSLOG_PORT);
            }
        } else {
            snprintf(response, responseSize, "Network manager not available");
        }
    }
    else if (strcasecmp(param, "interval") == 0) {
        unsigned long interval = strtoul(value, NULL, 10);
        if (interval >= 1000 && interval <= 300000) { // 1 second to 5 minutes
            if (monitorSystem) {
                monitorSystem->setPublishInterval(interval);
                snprintf(response, responseSize, "publish interval set to %lu ms", interval);
            } else {
                snprintf(response, responseSize, "Monitor system not available");
            }
        } else {
            snprintf(response, responseSize, "Interval must be between 1000 and 300000 ms");
        }
    }
    else if (strcasecmp(param, "heartbeat") == 0) {
        unsigned long interval = strtoul(value, NULL, 10);
        if (interval >= 5000 && interval <= 600000) { // 5 seconds to 10 minutes
            if (monitorSystem) {
                monitorSystem->setHeartbeatInterval(interval);
                snprintf(response, responseSize, "heartbeat interval set to %lu ms", interval);
            } else {
                snprintf(response, responseSize, "Monitor system not available");
            }
        } else {
            snprintf(response, responseSize, "Heartbeat interval must be between 5000 and 600000 ms");
        }
    }
    else {
        snprintf(response, responseSize, "unknown parameter %s", param);
    }
}

void CommandProcessor::handleDebug(char* param, char* response, size_t responseSize) {
    if (!param) {
        // Show current debug status
        snprintf(response, responseSize, "debug %s", g_debugEnabled ? "ON" : "OFF");
        return;
    }
    
    if (strcasecmp(param, "on") == 0 || strcasecmp(param, "1") == 0) {
        g_debugEnabled = true;
        snprintf(response, responseSize, "debug ON");
    } else if (strcasecmp(param, "off") == 0 || strcasecmp(param, "0") == 0) {
        g_debugEnabled = false;
        snprintf(response, responseSize, "debug OFF");
    } else {
        snprintf(response, responseSize, "usage: debug [ON|OFF]");
    }
}

void CommandProcessor::handleNetwork(char* response, size_t responseSize) {
    if (networkManager) {
        networkManager->getHealthString(response, responseSize);
    } else {
        snprintf(response, responseSize, "network manager not available");
    }
}

void CommandProcessor::handleReset(char* param, char* response, size_t responseSize) {
    if (!param) {
        snprintf(response, responseSize, "usage: reset system|network");
        return;
    }
    
    if (strcasecmp(param, "system") == 0) {
        snprintf(response, responseSize, "system reset requested - restarting...");
        delay(100); // Allow response to be sent
        // Restart the system
        NVIC_SystemReset();
    } else if (strcasecmp(param, "network") == 0) {
        if (networkManager) {
            // Force network reconnection by setting state
            snprintf(response, responseSize, "network reset requested");
            // This would require exposing a reset method in NetworkManager
        } else {
            snprintf(response, responseSize, "network manager not available");
        }
    } else {
        snprintf(response, responseSize, "unknown reset parameter: %s", param);
    }
}

void CommandProcessor::handleTest(char* param, char* response, size_t responseSize) {
    if (!param) {
        snprintf(response, responseSize, "test commands: network, sensors, weight, temp, outputs, i2c, pins");
        return;
    }
    
    if (strcasecmp(param, "network") == 0) {
        if (networkManager) {
            bool wifiOk = networkManager->isWiFiConnected();
            bool mqttOk = networkManager->isMQTTConnected();
            snprintf(response, responseSize, "network test: wifi=%s mqtt=%s", 
                wifiOk ? "OK" : "FAIL", mqttOk ? "OK" : "FAIL");
        } else {
            snprintf(response, responseSize, "network manager not available");
        }
    }
    else if (strcasecmp(param, "sensors") == 0) {
        if (monitorSystem) {
            float localTempF = monitorSystem->getLocalTemperatureF();
            float remoteTempF = monitorSystem->getRemoteTemperatureF();
            snprintf(response, responseSize, "sensor test: local=%.1f°F remote=%.1f°F", localTempF, remoteTempF);
        } else {
            snprintf(response, responseSize, "monitor system not available");
        }
    }
    else if (strcasecmp(param, "weight") == 0) {
        if (monitorSystem) {
            bool ready = monitorSystem->isWeightSensorReady();
            NAU7802Status status = monitorSystem->getWeightSensorStatus();
            float weight = monitorSystem->getWeight();
            long raw = monitorSystem->getRawWeight();
            
            snprintf(response, responseSize, 
                "weight test: status=%s ready=%s weight=%.3f raw=%ld",
                (status == NAU7802_OK) ? "OK" : "ERROR",
                ready ? "YES" : "NO",
                weight, raw);
        } else {
            snprintf(response, responseSize, "monitor system not available");
        }
    }
    else if (strcasecmp(param, "temp") == 0 || strcasecmp(param, "temperature") == 0) {
        if (monitorSystem) {
            bool ready = monitorSystem->isTemperatureSensorReady();
            float localTempF = monitorSystem->getLocalTemperatureF();
            float remoteTempF = monitorSystem->getRemoteTemperatureF();
            char statusBuffer[256];
            monitorSystem->getTemperatureSensorStatus(statusBuffer, sizeof(statusBuffer));
            
            snprintf(response, responseSize, 
                "temp test: ready=%s local=%.2fF remote=%.2fF - %s",
                ready ? "YES" : "NO",
                localTempF, remoteTempF, statusBuffer);
        } else {
            snprintf(response, responseSize, "monitor system not available");
        }
    }
    else if (strcasecmp(param, "outputs") == 0) {
        if (monitorSystem) {
            // Toggle output pins briefly for testing
            monitorSystem->setDigitalOutput(DIGITAL_OUTPUT_1, true);
            delay(100);
            monitorSystem->setDigitalOutput(DIGITAL_OUTPUT_1, false);
            snprintf(response, responseSize, "output test: toggled output pins");
        } else {
            snprintf(response, responseSize, "monitor system not available");
        }
    }
    else if (strcasecmp(param, "i2c") == 0) {
        // Enhanced I2C scanner with diagnostics - using Wire1 for Qwiic connector
        int deviceCount = 0;
        char deviceList[200] = "";
        
        debugPrintf("DEBUG: Starting I2C scan on Wire1 (Qwiic connector)...\n");
        debugPrintf("DEBUG: Reinitializing Wire1 bus...\n");
        
        // Reinitialize Wire1 bus (Qwiic connector) with slower clock speed
        Wire1.end();
        Wire1.begin();
        Wire1.setClock(100000); // Set to 100kHz (standard I2C speed)
        debugPrintf("DEBUG: Wire1 bus reinitialized at 100kHz\n");
        
        // Check I2C pin states (R4 WiFi Qwiic connector)
        debugPrintf("DEBUG: Testing Wire1 communication...\n");
        
        // Try Wire1 I2C communication
        debugPrintf("DEBUG: Testing basic I2C communication on Wire1...\n");
        
        // Try a different I2C initialization approach
        debugPrintf("DEBUG: Testing basic I2C communication...\n");
        
        for (int address = 1; address < 127; address++) {
            Wire1.beginTransmission(address);
            int error = Wire1.endTransmission();
            
            if (error == 0) {
                deviceCount++;
                char addrStr[16];
                snprintf(addrStr, sizeof(addrStr), "0x%02X ", address);
                if (strlen(deviceList) + strlen(addrStr) < sizeof(deviceList) - 1) {
                    strcat(deviceList, addrStr);
                }
                
                debugPrintf("DEBUG: Found device at address 0x%02X on Wire1\n", address);
                
                // Check if this is the NAU7802 (default address 0x2A)
                if (address == 0x2A) {
                    debugPrintf("DEBUG: Found NAU7802 at address 0x2A on Wire1\n");
                }
            } else if (error == 2) {
                // Address not acknowledged - this is normal for unused addresses
            } else {
                // Other error - could indicate bus problem
                debugPrintf("DEBUG: Wire1 I2C error %d at address 0x%02X\n", error, address);
                // If we get consistent error 5, stop scanning early
                if (error == 5 && address > 10) {
                    debugPrintf("DEBUG: Multiple timeout errors detected - stopping scan\n");
                    break;
                }
            }
        }
        
        if (deviceCount == 0) {
            snprintf(response, responseSize, "i2c test: no devices found on Wire1 - check Qwiic connector");
            debugPrintf("DEBUG: No I2C devices detected on Wire1 (Qwiic)\n");
            debugPrintf("DEBUG: Ensure device is connected to Qwiic connector\n");
        } else {
            snprintf(response, responseSize, "i2c test: found %d device(s) on Wire1: %s", deviceCount, deviceList);
        }
    }
    else if (strcasecmp(param, "pins") == 0) {
        // Test I2C pin functionality (R4 WiFi dedicated I2C pins)
        debugPrintf("DEBUG: Testing I2C pin functionality\n");
        
        // Read current pin states
        int sda_state = digitalRead(SDA);
        int scl_state = digitalRead(SCL);
        
        // Set pins as outputs and test
        pinMode(SDA, OUTPUT);
        pinMode(SCL, OUTPUT);
        
        digitalWrite(SDA, HIGH);
        digitalWrite(SCL, HIGH);
        delay(10);
        int sda_high = digitalRead(SDA);
        int scl_high = digitalRead(SCL);
        
        digitalWrite(SDA, LOW);
        digitalWrite(SCL, LOW);
        delay(10);
        int sda_low = digitalRead(SDA);
        int scl_low = digitalRead(SCL);
        
        // Restore I2C functionality
        Wire.begin();
        
        snprintf(response, responseSize, 
            "pin test: SDA init=%d high=%d low=%d | SCL init=%d high=%d low=%d",
            sda_state, sda_high, sda_low, scl_state, scl_high, scl_low);
        
        debugPrintf("DEBUG: Pin test complete, I2C reinitialized\n");
    }
    else {
        snprintf(response, responseSize, "unknown test parameter: %s", param);
    }
}

void CommandProcessor::handleSyslog(char* param, char* response, size_t responseSize) {
    if (!networkManager) {
        snprintf(response, responseSize, "network manager not initialized");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "syslog commands: test, status");
        return;
    }
    
    if (strcasecmp(param, "test") == 0) {
        if (!networkManager->isWiFiConnected()) {
            snprintf(response, responseSize, "WiFi not connected - cannot send syslog");
            return;
        }
        
        // Send test message to syslog server
        bool result = networkManager->sendSyslog("SYSLOG TEST MESSAGE - LogSplitter Monitor");
        if (result) {
            snprintf(response, responseSize, "syslog test message sent successfully");
        } else {
            snprintf(response, responseSize, "syslog test message failed to send");
        }
    }
    else if (strcasecmp(param, "status") == 0) {
        snprintf(response, responseSize, 
            "syslog server: %s:%d, wifi: %s", 
            SYSLOG_SERVER, SYSLOG_PORT,
            networkManager->isWiFiConnected() ? "connected" : "disconnected");
    }
    else {
        snprintf(response, responseSize, "unknown syslog command: %s", param);
    }
}

void CommandProcessor::handleMonitor(char* param, char* value, char* response, size_t responseSize) {
    if (!monitorSystem) {
        snprintf(response, responseSize, "monitor system not available");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "monitor commands: start, stop, state, output");
        return;
    }
    
    if (strcasecmp(param, "start") == 0) {
        monitorSystem->setSystemState(SYS_MONITORING);
        snprintf(response, responseSize, "monitoring started");
    }
    else if (strcasecmp(param, "stop") == 0) {
        monitorSystem->setSystemState(SYS_MAINTENANCE);
        snprintf(response, responseSize, "monitoring stopped");
    }
    else if (strcasecmp(param, "state") == 0) {
        SystemState state = monitorSystem->getSystemState();
        const char* stateStr = "";
        switch (state) {
            case SYS_INITIALIZING: stateStr = "INITIALIZING"; break;
            case SYS_CONNECTING: stateStr = "CONNECTING"; break;
            case SYS_MONITORING: stateStr = "MONITORING"; break;
            case SYS_ERROR: stateStr = "ERROR"; break;
            case SYS_MAINTENANCE: stateStr = "MAINTENANCE"; break;
        }
        snprintf(response, responseSize, "monitor state: %s (%d)", stateStr, (int)state);
    }
    else if (strcasecmp(param, "output") == 0) {
        if (!value) {
            snprintf(response, responseSize, "usage: monitor output <1|2> <on|off>");
            return;
        }
        
        int outputNum = atoi(value);
        char* stateStr = strtok(NULL, " ");
        
        if (outputNum < 1 || outputNum > 2 || !stateStr) {
            snprintf(response, responseSize, "usage: monitor output <1|2> <on|off>");
            return;
        }
        
        bool state = (strcasecmp(stateStr, "on") == 0 || strcasecmp(stateStr, "1") == 0);
        uint8_t pin = (outputNum == 1) ? DIGITAL_OUTPUT_1 : DIGITAL_OUTPUT_2;
        
        monitorSystem->setDigitalOutput(pin, state);
        snprintf(response, responseSize, "output %d set to %s", outputNum, state ? "ON" : "OFF");
    }
    else {
        snprintf(response, responseSize, "unknown monitor command: %s", param);
    }
}

void CommandProcessor::handleWeight(char* param, char* value, char* response, size_t responseSize) {
    if (!monitorSystem) {
        snprintf(response, responseSize, "monitor system not available");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "weight commands: read, raw, tare, zero, calibrate, status");
        return;
    }
    
    if (strcasecmp(param, "read") == 0) {
        float weight = monitorSystem->getWeight();
        float filtered = monitorSystem->getFilteredWeight();
        snprintf(response, responseSize, "weight: %.3f (filtered: %.3f)", weight, filtered);
    }
    else if (strcasecmp(param, "raw") == 0) {
        long rawWeight = monitorSystem->getRawWeight();
        snprintf(response, responseSize, "raw weight: %ld", rawWeight);
    }
    else if (strcasecmp(param, "tare") == 0) {
        monitorSystem->tareWeightSensor();
        snprintf(response, responseSize, "weight sensor tared");
    }
    else if (strcasecmp(param, "zero") == 0) {
        if (monitorSystem->calibrateWeightSensorZero()) {
            snprintf(response, responseSize, "zero calibration completed");
        } else {
            snprintf(response, responseSize, "zero calibration failed");
        }
    }
    else if (strcasecmp(param, "calibrate") == 0) {
        if (!value) {
            snprintf(response, responseSize, "usage: weight calibrate <known_weight>");
            return;
        }
        
        float knownWeight = atof(value);
        if (knownWeight <= 0) {
            snprintf(response, responseSize, "known weight must be positive");
            return;
        }
        
        if (monitorSystem->calibrateWeightSensorScale(knownWeight)) {
            snprintf(response, responseSize, "scale calibrated with weight %.2f", knownWeight);
        } else {
            snprintf(response, responseSize, "scale calibration failed");
        }
    }
    else if (strcasecmp(param, "status") == 0) {
        NAU7802Status status = monitorSystem->getWeightSensorStatus();
        bool ready = monitorSystem->isWeightSensorReady();
        float weight = monitorSystem->getWeight();
        long raw = monitorSystem->getRawWeight();
        
        snprintf(response, responseSize, 
            "status: %s, ready: %s, weight: %.3f, raw: %ld",
            (status == NAU7802_OK) ? "OK" : "ERROR",
            ready ? "YES" : "NO",
            weight, raw);
    }
    else if (strcasecmp(param, "save") == 0) {
        if (monitorSystem->saveWeightCalibration()) {
            snprintf(response, responseSize, "weight calibration saved to EEPROM");
        } else {
            snprintf(response, responseSize, "failed to save weight calibration");
        }
    }
    else if (strcasecmp(param, "load") == 0) {
        if (monitorSystem->loadWeightCalibration()) {
            snprintf(response, responseSize, "weight calibration loaded from EEPROM");
        } else {
            snprintf(response, responseSize, "failed to load weight calibration");
        }
    }
    else {
        snprintf(response, responseSize, "unknown weight command: %s", param);
    }
}

void CommandProcessor::handleTemperature(char* param, char* value, char* response, size_t responseSize) {
    if (!monitorSystem) {
        snprintf(response, responseSize, "monitor system not available");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "temp commands: read, readc, local, remote, localc, remotec, status, offset");
        return;
    }
    
    if (strcasecmp(param, "read") == 0) {
        bool ready = monitorSystem->isTemperatureSensorReady();
        if (ready) {
            float localTempF = monitorSystem->getLocalTemperatureF();
            float remoteTempF = monitorSystem->getRemoteTemperatureF();
            snprintf(response, responseSize, "local: %.2fF, remote: %.2fF", localTempF, remoteTempF);
        } else {
            snprintf(response, responseSize, "temperature sensor not available");
        }
    }
    else if (strcasecmp(param, "readc") == 0) {
        bool ready = monitorSystem->isTemperatureSensorReady();
        if (ready) {
            float localTemp = monitorSystem->getLocalTemperature();
            float remoteTemp = monitorSystem->getRemoteTemperature();
            snprintf(response, responseSize, "local: %.2fC, remote: %.2fC", localTemp, remoteTemp);
        } else {
            snprintf(response, responseSize, "temperature sensor not available");
        }
    }
    else if (strcasecmp(param, "local") == 0) {
        float localTempF = monitorSystem->getLocalTemperatureF();
        snprintf(response, responseSize, "local temperature: %.2f°F", localTempF);
    }
    else if (strcasecmp(param, "localc") == 0) {
        float localTemp = monitorSystem->getLocalTemperature();
        snprintf(response, responseSize, "local temperature: %.2f°C", localTemp);
    }
    else if (strcasecmp(param, "remote") == 0) {
        float remoteTempF = monitorSystem->getRemoteTemperatureF();
        snprintf(response, responseSize, "remote temperature: %.2f°F", remoteTempF);
    }
    else if (strcasecmp(param, "remotec") == 0) {
        float remoteTemp = monitorSystem->getRemoteTemperature();
        snprintf(response, responseSize, "remote temperature: %.2f°C", remoteTemp);
    }
    else if (strcasecmp(param, "status") == 0) {
        char statusBuffer[256];
        monitorSystem->getTemperatureSensorStatus(statusBuffer, sizeof(statusBuffer));
        snprintf(response, responseSize, "%s", statusBuffer);
    }
    else if (strcasecmp(param, "offset") == 0) {
        if (!value) {
            snprintf(response, responseSize, "usage: temp offset <local_offset> <remote_offset>");
            return;
        }
        
        // Parse two float values
        char* localStr = value;
        char* remoteStr = strtok(NULL, " ");
        
        if (!remoteStr) {
            snprintf(response, responseSize, "usage: temp offset <local_offset> <remote_offset>");
            return;
        }
        
        float localOffset = atof(localStr);
        float remoteOffset = atof(remoteStr);
        
        monitorSystem->setTemperatureOffset(localOffset, remoteOffset);
        snprintf(response, responseSize, "temperature offsets set: local=%.2f°C, remote=%.2f°C", 
                localOffset, remoteOffset);
    }
    else {
        snprintf(response, responseSize, "unknown temperature command: %s", param);
    }
}

// CommandValidator implementation
bool CommandValidator::isValidCommand(const char* cmd) {
    if (!cmd || strlen(cmd) == 0 || strlen(cmd) > MAX_CMD_LENGTH) return false;
    
    // Check against whitelist
    for (int i = 0; ALLOWED_COMMANDS[i] != nullptr; i++) {
        if (strcasecmp(cmd, ALLOWED_COMMANDS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool CommandValidator::isValidSetParam(const char* param) {
    if (!param || strlen(param) == 0) return false;
    
    for (int i = 0; ALLOWED_SET_PARAMS[i] != nullptr; i++) {
        if (strcasecmp(param, ALLOWED_SET_PARAMS[i]) == 0) {
            return true;
        }
    }
    return false;
}

bool CommandValidator::validateCommand(const char* command) {
    return isValidCommand(command);
}

bool CommandValidator::validateSetCommand(const char* param, const char* value) {
    if (!isValidSetParam(param) || !value) return false;
    
    // Additional parameter-specific validation
    if (strcasecmp(param, "interval") == 0 || strcasecmp(param, "heartbeat") == 0) {
        unsigned long val = strtoul(value, NULL, 10);
        return (val >= 1000 && val <= 600000);
    }
    
    return true; // Allow other parameters with basic validation
}

void CommandValidator::sanitizeInput(char* input, size_t maxLength) {
    if (!input) return;
    
    size_t len = strlen(input);
    if (len > maxLength) {
        input[maxLength] = '\0';
        len = maxLength;
    }
    
    // Remove any non-printable characters
    for (size_t i = 0; i < len; i++) {
        if (input[i] < 32 || input[i] > 126) {
            input[i] = ' ';
        }
    }
}

bool CommandValidator::checkRateLimit() {
    unsigned long now = millis();
    if (now - lastCommandTime < COMMAND_RATE_LIMIT_MS) {
        return false; // Rate limited
    }
    lastCommandTime = now;
    return true;
}

void CommandProcessor::handleLCD(char* param, char* value, char* response, size_t responseSize) {
    if (!g_lcdDisplay) {
        snprintf(response, responseSize, "LCD display not available");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "LCD status: %s, backlight: %s", 
            g_lcdDisplay->isEnabled() ? "ON" : "OFF",
            g_lcdDisplay->isBacklightEnabled() ? "ON" : "OFF");
        return;
    }
    
    if (strcasecmp(param, "on") == 0) {
        g_lcdDisplay->setEnabled(true);
        snprintf(response, responseSize, "LCD display enabled");
    }
    else if (strcasecmp(param, "off") == 0) {
        g_lcdDisplay->setEnabled(false);
        snprintf(response, responseSize, "LCD display disabled");
    }
    else if (strcasecmp(param, "clear") == 0) {
        g_lcdDisplay->clear();
        snprintf(response, responseSize, "LCD display cleared");
    }
    else if (strcasecmp(param, "backlight") == 0) {
        if (!value) {
            snprintf(response, responseSize, "LCD backlight: %s", 
                g_lcdDisplay->isBacklightEnabled() ? "ON" : "OFF");
            return;
        }
        
        if (strcasecmp(value, "on") == 0) {
            g_lcdDisplay->setBacklight(true);
            snprintf(response, responseSize, "LCD backlight enabled");
        }
        else if (strcasecmp(value, "off") == 0) {
            g_lcdDisplay->setBacklight(false);
            snprintf(response, responseSize, "LCD backlight disabled");
        }
        else {
            snprintf(response, responseSize, "usage: lcd backlight on|off");
        }
    }
    else if (strcasecmp(param, "info") == 0) {
        if (value) {
            g_lcdDisplay->showInfo(value);
            snprintf(response, responseSize, "LCD info message displayed");
        } else {
            snprintf(response, responseSize, "usage: lcd info <message>");
        }
    }
    else {
        snprintf(response, responseSize, "usage: lcd [on|off|clear|backlight|info]");
    }
}

void CommandProcessor::handleLogLevel(const char* param, char* response, size_t responseSize) {
    if (!param || strcasecmp(param, "get") == 0) {
        // Show current log level
        LogLevel currentLevel = Logger::getLogLevel();
        const char* levelName;
        switch (currentLevel) {
            case LOG_EMERGENCY: levelName = "EMERGENCY"; break;
            case LOG_ALERT: levelName = "ALERT"; break;
            case LOG_CRITICAL: levelName = "CRITICAL"; break;
            case LOG_ERROR: levelName = "ERROR"; break;
            case LOG_WARNING: levelName = "WARNING"; break;
            case LOG_NOTICE: levelName = "NOTICE"; break;
            case LOG_INFO: levelName = "INFO"; break;
            case LOG_DEBUG: levelName = "DEBUG"; break;
            default: levelName = "UNKNOWN"; break;
        }
        snprintf(response, responseSize, "Current log level: %d (%s)", currentLevel, levelName);
    }
    else if (strcasecmp(param, "list") == 0) {
        snprintf(response, responseSize, 
            "Log levels: 0=EMERGENCY, 1=ALERT, 2=CRITICAL, 3=ERROR, 4=WARNING, 5=NOTICE, 6=INFO, 7=DEBUG");
    }
    else if (strcasecmp(param, "set") == 0) {
        snprintf(response, responseSize, "usage: loglevel set <0-7>");
    }
    else {
        // Try to parse as log level number
        int level = atoi(param);
        if (level >= 0 && level <= 7) {
            Logger::setLogLevel(static_cast<LogLevel>(level));
            const char* levelName;
            switch (level) {
                case LOG_EMERGENCY: levelName = "EMERGENCY"; break;
                case LOG_ALERT: levelName = "ALERT"; break;
                case LOG_CRITICAL: levelName = "CRITICAL"; break;
                case LOG_ERROR: levelName = "ERROR"; break;
                case LOG_WARNING: levelName = "WARNING"; break;
                case LOG_NOTICE: levelName = "NOTICE"; break;
                case LOG_INFO: levelName = "INFO"; break;
                case LOG_DEBUG: levelName = "DEBUG"; break;
                default: levelName = "UNKNOWN"; break;
            }
            snprintf(response, responseSize, "Log level set to %d (%s)", level, levelName);
        } else {
            snprintf(response, responseSize, "usage: loglevel [get|list|<0-7>]");
        }
    }
}

void CommandProcessor::handleHeartbeat(char* param, char* response, size_t responseSize) {
    if (!heartbeatAnimation) {
        snprintf(response, responseSize, "heartbeat animation not initialized");
        return;
    }

    if (!param || strcasecmp(param, "status") == 0) {
        // Show heartbeat status
        bool enabled = heartbeatAnimation->getEnabled();
        uint16_t bpm = heartbeatAnimation->getHeartRate();
        uint8_t brightness = heartbeatAnimation->getBrightness();
        snprintf(response, responseSize, "Heartbeat: %s, %d BPM, brightness %d", 
                 enabled ? "ENABLED" : "DISABLED", bpm, brightness);
                 
    } else if (strcasecmp(param, "on") == 0 || strcasecmp(param, "enable") == 0) {
        heartbeatAnimation->enable();
        snprintf(response, responseSize, "Heartbeat animation enabled");
        
    } else if (strcasecmp(param, "off") == 0 || strcasecmp(param, "disable") == 0) {
        heartbeatAnimation->disable();
        snprintf(response, responseSize, "Heartbeat animation disabled");
        
    } else if (strncasecmp(param, "rate", 4) == 0) {
        // Extract BPM value: "rate 72" or "rate72"
        char* bpmStr = param + 4;
        if (*bpmStr == ' ') bpmStr++; // Skip space if present
        if (*bpmStr == '\0') {
            // No value provided, get next token
            bpmStr = strtok(NULL, " ");
        }
        
        if (bpmStr) {
            int bpm = atoi(bpmStr);
            if (bpm >= 30 && bpm <= 200) {
                heartbeatAnimation->setHeartRate(bpm);
                snprintf(response, responseSize, "Heart rate set to %d BPM", bpm);
            } else {
                snprintf(response, responseSize, "Invalid heart rate (30-200 BPM)");
            }
        } else {
            snprintf(response, responseSize, "usage: heartbeat rate <30-200>");
        }
        
    } else if (strncasecmp(param, "brightness", 10) == 0) {
        // Extract brightness value: "brightness 128" or "brightness128"
        char* brightnessStr = param + 10;
        if (*brightnessStr == ' ') brightnessStr++; // Skip space if present
        if (*brightnessStr == '\0') {
            // No value provided, get next token
            brightnessStr = strtok(NULL, " ");
        }
        
        if (brightnessStr) {
            int brightness = atoi(brightnessStr);
            if (brightness >= 0 && brightness <= 255) {
                heartbeatAnimation->setBrightness(brightness);
                snprintf(response, responseSize, "Brightness set to %d", brightness);
            } else {
                snprintf(response, responseSize, "Invalid brightness (0-255)");
            }
        } else {
            snprintf(response, responseSize, "usage: heartbeat brightness <0-255>");
        }
        
    } else if (strncasecmp(param, "frame", 5) == 0) {
        // Extract frame number: "frame 2" or "frame2"
        char* frameStr = param + 5;
        if (*frameStr == ' ') frameStr++; // Skip space if present
        if (*frameStr == '\0') {
            // No value provided, get next token
            frameStr = strtok(NULL, " ");
        }
        
        if (frameStr) {
            int frame = atoi(frameStr);
            if (frame >= 0 && frame <= 3) {
                heartbeatAnimation->displayFrame(frame);
                snprintf(response, responseSize, "Displaying frame %d", frame);
            } else {
                snprintf(response, responseSize, "Invalid frame (0-3)");
            }
        } else {
            snprintf(response, responseSize, "usage: heartbeat frame <0-3>");
        }
        
    } else {
        snprintf(response, responseSize, 
                "usage: heartbeat [on|off|rate <bpm>|brightness <0-255>|frame <0-3>|status]");
    }
}