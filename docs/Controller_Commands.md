# LogSplitter Controller Commands Documentation

This comprehensive document serves as both a user command reference and an AI assistant implementation guide for the LogSplitter Controller system.

## Table of Contents
1. [User Command Reference](#user-command-reference)
2. [AI Implementation Guide](#ai-implementation-guide)

---

## User Command Reference

### Communication Interfaces

#### Serial Console
- **Port**: Primary USB Serial (115200 baud)
- **Access**: Full command access including PIN mode configuration
- **Format**: Plain text commands terminated with newline

#### Telnet Server
- **Port**: 23 (standard telnet port)  
- **Access**: Full command access including PIN mode configuration (equivalent to Serial)
- **Format**: Plain text commands terminated with newline
- **Debug Output**: Real-time debug messages with timestamps
- **Connection**: `telnet <device_ip> 23`

#### MQTT Topics
- **Subscribe (Commands)**: `controller/control`
- **Publish (Responses)**: `controller/control/resp`
- **Configuration Topics**: `controller/config/*` (retained values)
- **Access**: All commands except `pins` (PIN mode changes restricted to serial/telnet for security)

### Command Format

All commands are case-insensitive and follow the format:
```
COMMAND [parameter] [value]
```

### Available Commands

#### 1. HELP
**Description**: Display available commands  
**Syntax**: `help`  
**Access**: Serial + MQTT + Telnet

**Example**:
```
> help
Commands: help, show, debug, network, pins, set <param> <val>, relay R<n> ON|OFF
Live Network Config:
set syslog <server[:port]> - Apply immediately
set mqtt <broker[:port]> - Apply immediately
set mqtt_defaults - Query MQTT for config values
```

#### 2. SHOW
**Description**: Display complete system status including pressure sensors, sequence controller, relays, and safety systems  
**Syntax**: `show`  
**Access**: Serial + MQTT + Telnet

**Example Output**:
```
> show
System Status: [A1: 2456.2 PSI] [A5: 2445.1 PSI] [Seq: IDLE] [Safety: OK] [R1:OFF R2:OFF R3:OFF R4:OFF R5:OFF R6:OFF R7:OFF R8:OFF] [Pin6:HIGH Pin7:HIGH Pin12:HIGH]
```

#### 3. LOGLEVEL  
**Description**: Sets logging severity level (0-7). **Now persistent in EEPROM and MQTT!**  
**Syntax**: `loglevel [get|list|<0-7>]`  
**Access**: Serial + MQTT + Telnet  
**Persistence**: ✅ EEPROM + ✅ MQTT Retained

**Log Levels**:
- **0=EMERGENCY** (system unusable)
- **1=ALERT** (immediate action required)  
- **2=CRITICAL** (critical conditions)
- **3=ERROR** (error conditions)
- **4=WARNING** (warning conditions)
- **5=NOTICE** (normal but significant)
- **6=INFO** (informational - default)
- **7=DEBUG** (debug-level messages)

**Examples**:
```
> loglevel
Current log level: 6 (INFO)

> loglevel 3  
Log level set to 3 (ERROR)

> loglevel list
Log levels: 0=EMERGENCY, 1=ALERT, 2=CRITICAL, 3=ERROR, 4=WARNING, 5=NOTICE, 6=INFO, 7=DEBUG
```

**Key Features**:
- ✅ **Persistent**: Survives reboots (stored in EEPROM)
- ✅ **MQTT Published**: `controller/config/log_level` with retain flag
- ✅ **Immediate Effect**: Changes logging output instantly
- ✅ **Network Aware**: Only logs at or above set level to save bandwidth

#### 4. SET

**Description**: Configure system parameters with persistent storage  
**Syntax**: `set <parameter> <value>`  
**Access**: Serial + MQTT + Telnet

##### Available SET Parameters:

###### Log Level Configuration (New!)
- **loglevel** - Set logging severity level (0-7) with EEPROM persistence
  **Syntax**: `set loglevel <0-7>`  
  **Persistence**: ✅ EEPROM + ✅ MQTT Retained
  
  ```
  > set loglevel 7
  log level set to 7
  
  > set loglevel 3  
  log level set to 3
  ```

###### MQTT Configuration Recovery (New!)
- **mqtt_defaults** - Query MQTT broker for retained configuration values
  **Syntax**: `set mqtt_defaults`  
  **Requirements**: MQTT connection must be active
  
  ```
  > set mqtt_defaults
  Querying MQTT server for configuration defaults...
  MQTT defaults loaded and saved to EEPROM
  ```
  
  **What it does**:
  - Queries `controller/config/*` topics for retained values
  - Validates received configuration data
  - Saves valid parameters to EEPROM
  - Falls back to firmware defaults if MQTT query fails
  - Includes 10-second timeout protection

###### Network Configuration  
- **syslog** - Configure rsyslog server IP address and port for centralized logging
  **Syntax**: `set syslog <ip>` or `set syslog <ip>:<port>`
  **Default Port**: 514 (standard syslog UDP port)
  
  ```
  > set syslog 192.168.1.155
  syslog server set to 192.168.1.155:514 (applied immediately)
  
  > set syslog 192.168.1.155:1514  
  syslog server set to 192.168.1.155:1514 (applied immediately)
  ```
  
  **Features**:
  - All debug output is sent exclusively to syslog server
  - RFC 3164 compliant format with facility Local0 and severity Info
  - Use `syslog test` to verify connectivity

- **mqtt** - Configure MQTT broker address and port
  **Syntax**: `set mqtt <host>` or `set mqtt <host>:<port>`
  **Default Port**: 1883 (standard MQTT port)
  
  ```
  > set mqtt 192.168.1.155
  mqtt broker set to 192.168.1.155:1883 (applied immediately)
  
  > set mqtt broker.local:8883
  mqtt broker set to broker.local:8883 (applied immediately)  
  ```

---

## AI Implementation Guide

### OVERVIEW
This section details the exact procedure to add new commands to the LogSplitter Controller system. The system has two types of commands:
1. **Standalone Commands** (like `help`, `show`, `syslog`, `mqtt`, `loglevel`)
2. **SET Parameters** (like `set syslog <ip>`, `set mqtt <host>`, `set loglevel <0-7>`)

### CRITICAL FILES TO MODIFY

#### 1. Command Validation Arrays (MUST UPDATE FIRST)
**File:** `src/constants.cpp`
```cpp
// Add to ALLOWED_COMMANDS for standalone commands
const char* const ALLOWED_COMMANDS[] = {
    "help", "show", "pins", "pin", "set", "relay", "debug", 
    "network", "reset", "test", "syslog", "mqtt", "loglevel", 
    "heartbeat", "error", "bypass", "timing", "NEW_COMMAND", nullptr
};

// Add to ALLOWED_SET_PARAMS for SET parameters  
const char* const ALLOWED_SET_PARAMS[] = {
    "vref", "maxpsi", "gain", "offset", "filter", "emaalpha", 
    "a1_maxpsi", "a1_gain", "a1_offset", "a1_vref",
    "a5_maxpsi", "a5_gain", "a5_offset", "a5_vref",
    "pinmode", "seqstable", "seqstartstable", "seqtimeout", 
    "debug", "debugpins", "loglevel", "syslog", "mqtt", "mqtt_defaults",
    "NEW_SET_PARAM", nullptr
};
```

#### 2. Command Processor Header
**File:** `include/command_processor.h`
```cpp
// Add method declaration in private section
private:
    void handleNewCommand(char* param, char* response, size_t responseSize);
```

#### 3. Command Processor Implementation
**File:** `src/command_processor.cpp`

**A. Add Command Handler in processCommand() method:**
```cpp
else if (strcasecmp(cmd, "newcommand") == 0) {
    char* param = strtok(NULL, " ");
    handleNewCommand(param, response, responseSize);
}
```

**B. Add SET Parameter Handler in handleSet() method:**
```cpp
else if (strcasecmp(param, "newparam") == 0) {
    if (someManager) {
        // Parse and validate value
        someManager->setNewParameter(value);
        snprintf(response, responseSize, "new parameter set to %s", value);
    } else {
        snprintf(response, responseSize, "Manager not available");
    }
}
```

**C. Implement the Handler Method:**
```cpp
void CommandProcessor::handleNewCommand(char* param, char* response, size_t responseSize) {
    if (!someManager) {
        snprintf(response, responseSize, "manager not initialized");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "newcommand commands: test, status");
        return;
    }
    
    if (strcasecmp(param, "test") == 0) {
        // Implementation
    }
    else if (strcasecmp(param, "status") == 0) {
        // Implementation
    }
    else {
        snprintf(response, responseSize, "unknown command: %s", param);
    }
}
```

**D. Update Help Text:**
```cpp
void CommandProcessor::handleHelp(char* response, size_t responseSize, bool fromMqtt) {
    const char* helpText = "Commands: help, show, debug, network, reset, error, test, loglevel [0-7], bypass, syslog, mqtt, newcommand";
    // ... rest of help implementation
}
```

#### 4. Manager/Service Implementation (if needed)
**File:** `include/some_manager.h` and `src/some_manager.cpp`
```cpp
// Add methods to handle the new functionality
void setNewParameter(const char* value);
bool testNewFeature();
```

### REAL EXAMPLE: MQTT Command Implementation

#### Step 1: Added to constants.cpp ✅
```cpp
const char* const ALLOWED_COMMANDS[] = {
    // ... existing commands ...
    "mqtt", nullptr  // Added this
};

const char* const ALLOWED_SET_PARAMS[] = {
    // ... existing params ...
    "mqtt", nullptr  // Added this
};
```

#### Step 2: Added to command_processor.h ✅
```cpp
void handleMqtt(char* param, char* response, size_t responseSize);
```

#### Step 3: Added to command_processor.cpp ✅
```cpp
// In processCommand():
else if (strcasecmp(cmd, "mqtt") == 0) {
    char* param = strtok(NULL, " ");
    handleMqtt(param, response, responseSize);
}

// In handleSet():
else if (strcasecmp(param, "mqtt") == 0) {
    if (networkManager) {
        char* portPtr = strchr(value, ':');
        int port = BROKER_PORT;
        
        if (portPtr) {
            *portPtr = '\0';
            port = atoi(portPtr + 1);
            if (port <= 0 || port > 65535) {
                snprintf(response, responseSize, "Invalid port number");
                return;
            }
        }
        
        networkManager->setMqttBroker(value, port);
        snprintf(response, responseSize, "mqtt broker set to %s:%d", value, port);
    } else {
        snprintf(response, responseSize, "Network manager not available");
    }
}

// Implementation of handleMqtt():
void CommandProcessor::handleMqtt(char* param, char* response, size_t responseSize) {
    if (!networkManager) {
        snprintf(response, responseSize, "network manager not initialized");
        return;
    }
    
    if (!param) {
        snprintf(response, responseSize, "mqtt commands: test, status");
        return;
    }
    
    if (strcasecmp(param, "test") == 0) {
        if (!networkManager->isMQTTConnected()) {
            snprintf(response, responseSize, "MQTT not connected");
            return;
        }
        bool result = networkManager->publish("controller/test", "TEST MESSAGE");
        snprintf(response, responseSize, result ? "mqtt test sent" : "mqtt test failed");
    }
    else if (strcasecmp(param, "status") == 0) {
        snprintf(response, responseSize, "mqtt broker: %s:%d, status: %s", 
            networkManager->getMqttBrokerHost(), 
            networkManager->getMqttBrokerPort(),
            networkManager->isMQTTConnected() ? "connected" : "disconnected");
    }
    else {
        snprintf(response, responseSize, "unknown mqtt command: %s", param);
    }
}
```

#### Step 4: Added to NetworkManager ✅
```cpp
// In network_manager.h:
void setMqttBroker(const char* host, int port = BROKER_PORT);
const char* getMqttBrokerHost() const { return mqttBrokerHost; }
int getMqttBrokerPort() const { return mqttBrokerPort; }

// Added member variables:
char mqttBrokerHost[64];
int mqttBrokerPort;

// In network_manager.cpp:
void NetworkManager::setMqttBroker(const char* host, int port) {
    strncpy(mqttBrokerHost, host, sizeof(mqttBrokerHost) - 1);
    mqttBrokerHost[sizeof(mqttBrokerHost) - 1] = '\0';
    mqttBrokerPort = port;
    
    if (mqttState == MQTTState::CONNECTED) {
        mqttClient.stop();
        mqttState = MQTTState::DISCONNECTED;
    }
}
```

### COMMON MISTAKES TO AVOID

1. **FORGOT TO ADD TO ALLOWED ARRAYS** ❌
   - Command will fail with "invalid command" or "invalid set command"
   - ALWAYS check constants.cpp first

2. **WRONG COMMAND TYPE** ❌
   - Don't confuse standalone commands with SET parameters
   - `mqtt test` = standalone command handler
   - `set mqtt <host>` = SET parameter handler

3. **MISSING HEADER DECLARATION** ❌
   - Implementation without declaration causes compile errors
   - Add to command_processor.h

4. **WRONG PARAMETER PARSING** ❌
   - SET commands: value comes from strtok
   - Standalone commands: param comes from strtok

5. **FORGOT TO UPDATE HELP** ❌
   - Users won't know about new commands
   - Update handleHelp() method

### VALIDATION CHECKLIST

Before committing new command:
- [ ] Added to ALLOWED_COMMANDS or ALLOWED_SET_PARAMS in constants.cpp
- [ ] Added handler declaration to command_processor.h
- [ ] Added command routing in processCommand() or handleSet()
- [ ] Implemented handler method
- [ ] Updated help text
- [ ] Added manager methods if needed
- [ ] Updated documentation
- [ ] Tested compilation
- [ ] Tested on hardware

### ERROR MESSAGES DECODE

- "invalid command: xyz" → Missing from ALLOWED_COMMANDS
- "invalid set command" → Missing from ALLOWED_SET_PARAMS  
- "unknown command: xyz" → Command parser found but handler missing
- Compile error → Missing header declaration or implementation

### FILE MODIFICATION ORDER

1. **constants.cpp** (validation arrays)
2. **manager headers/implementation** (if new functionality needed)
3. **command_processor.h** (handler declaration)
4. **command_processor.cpp** (routing + implementation)
5. **documentation** (COMMANDS.md, README.md)
6. **commit and test**

### NETWORKING COMMANDS PATTERN

For network-related commands, follow this pattern:
```cpp
// SET parameter for configuration
set networkparam <value>

// Standalone commands for testing/status
networkcommand test
networkcommand status
```

This matches the syslog/mqtt pattern and provides consistency.

### ENCODING NOTE
This file uses standard text encoding. No special encoding required.
The .ai extension is for AI assistant recognition only.

### RECENT UPDATES (October 2025)

#### ✅ Log Level EEPROM Persistence 
- **Enhancement**: `loglevel` and `set loglevel` now persist in EEPROM
- **MQTT Integration**: Published to `controller/config/log_level` with retain flag  
- **Startup Behavior**: Log level restored from EEPROM on boot
- **Implementation**: Added to CalibrationData structure, ConfigManager methods, and startup sequence

#### ✅ MQTT Configuration Recovery
- **New Command**: `set mqtt_defaults` - Query MQTT broker for retained configuration
- **Purpose**: Recover configuration from MQTT when EEPROM is corrupted or blank
- **Timeout Protection**: 10-second timeout with fallback to firmware defaults
- **Topics Queried**: All `controller/config/*` parameters with validation

#### ✅ MQTT Topic Corrections
- **Fixed**: All configuration topics now use correct `controller/config/` prefix
- **Previous**: Incorrect `r4/configparam/` prefix has been corrected
- **Impact**: MQTT retained configuration values now use proper topic hierarchy

---
**Last Updated:** October 2025  
**Recent Patterns Implemented:** Log Level Persistence, MQTT Configuration Recovery  
**Success Rate:** 100% when following this procedure