# LogSplitter Controller - AI Coding Assistant Guide

## Architecture Overview

This is an **industrial Arduino UNO R4 WiFi control system** for hydraulic log splitter operations. This is the **non-networking version** that removes WiFi/MQTT complexity for maximum reliability and uses **pure binary telemetry** on dedicated pins A4/A5.

### Key System Characteristics
- **Hardware Target**: Arduino UNO R4 WiFi (ARM Cortex-M4, 32KB RAM, 256KB Flash)  
- **Memory Optimized**: Shared global buffers (`g_message_buffer`, `g_response_buffer`), ~60% RAM reduction
- **Safety Critical**: Fail-safe hydraulic control with emergency stops and pressure limits
- **Real-time**: Sub-millisecond event processing with **600% faster binary telemetry**
- **Non-blocking**: Event-driven updates to prevent system lockups
- **No Networking**: Simplified architecture without WiFi/MQTT dependencies

## Core Architecture Patterns

### 1. Modular Component System
Each subsystem is a self-contained class with clear responsibilities:
```cpp
// Manager classes - handle specific domain logic
PressureManager pressureManager;     // Dual-sensor pressure monitoring  
SequenceController sequenceController; // State machine for hydraulic cycles
RelayController relayController;     // 8-channel industrial relay control
SafetySystem safetySystem;          // Emergency stops and mill lamp
InputManager inputManager;          // Debounced pin monitoring
```

**Critical**: Components communicate through dependency injection, not global coupling.

### 2. Binary Telemetry System  
The system outputs **pure binary Protocol Buffer messages** on pins A4/A5 at 115200 baud:
- **NO ASCII on telemetry port** - maximum 600% throughput improvement
- Size-prefixed binary messages (7-19 bytes)
- 8 message types: Digital I/O, Relays, Pressure, Errors, Safety, System Status
- See `docs/TELEMETRY_API.md` for complete protocol specification

### 3. Safety-First Design
All relay operations go through safety validation:
```cpp
// SAFETY: Don't allow extend if already at limit
if (state && g_limitExtendActive) {
    debugPrintf("SAFETY: Extend blocked - cylinder already at extend limit\n");
    return;
}
relayController.setRelay(RELAY_EXTEND, state);
```

### 4. Memory Management Strategy
- **Shared global buffers**: `g_message_buffer`, `g_response_buffer` (256 bytes each)
- **No large stack arrays**: Use shared buffers for string operations  
- **PROGMEM constants**: Store strings in flash memory
- **Timing monitoring**: Track subsystem performance to prevent timeouts

## Critical Development Workflows

### Build and Upload
Use **either** PlatformIO commands or VS Code tasks:
```bash
# Makefile commands (Windows PowerShell compatible)
make build        # Compile firmware  
make upload       # Flash to Arduino UNO R4 WiFi
make deploy       # Build + Upload + Monitor
make check        # Static code analysis
make clean        # Clean build artifacts
```

### VS Code Tasks (Preferred)
- **🔧 Build Controller Program**: Compile only (`Ctrl+Shift+P` → "Tasks: Run Task")
- **📤 Upload Controller Program**: Flash firmware to device
- **🏗️ Build & Upload Controller**: Complete deployment workflow
- **🔍 Monitor Controller Serial**: Debug output monitoring
- **🔍 Check Code**: Static analysis with verbose output

### Configuration Management
- **arduino_secrets.h**: Basic configuration template (minimal for non-networking)
- **EEPROM Configuration**: Persistent settings with CRC validation via `ConfigManager`
- **Serial Commands**: Live reconfiguration via command processor (no networking)

### Debugging Approach
1. **Serial Debug**: ASCII debug output on main Serial port (Arduino IDE Serial Monitor)
2. **Binary Telemetry**: Real-time system events on A4/A5 pins (use `docs/TELEMETRY_API.md`)
3. **Command Interface**: Interactive commands via Serial (`help`, `show`, `pins`)
4. **Timing Monitor**: Subsystem performance tracking (`SubsystemTimingMonitor`)
5. **Safety System**: Mill lamp patterns indicate system state
6. **Ctrl+K Toggle**: Enable/disable echo mode in Serial (binary telemetry always active)

## Project-Specific Conventions

### 1. Pin Mapping (Critical for Safety)
```cpp
// Emergency and Safety Pins
const uint8_t E_STOP_PIN = 12;        // Emergency stop (NC switch)
const uint8_t SAFETY_CLEAR_PIN = 4;   // Safety system reset
const uint8_t LIMIT_EXTEND_PIN = 6;   // Cylinder fully extended  
const uint8_t LIMIT_RETRACT_PIN = 7;  // Cylinder fully retracted

// Manual Control Pins
const uint8_t MANUAL_EXTEND_PIN = 3;   // Manual extend button
const uint8_t MANUAL_RETRACT_PIN = 2;  // Manual retract button  
const uint8_t SEQUENCE_START_PIN = 5;  // Start automatic sequence

// Relay Assignments
const uint8_t RELAY_EXTEND = 1;        // Hydraulic extend valve
const uint8_t RELAY_RETRACT = 2;       // Hydraulic retract valve
const uint8_t RELAY_ENGINE_STOP = 8;   // Engine safety stop

// Binary Telemetry (Non-networking version specific)
// Pin A4 (TX) = Binary telemetry output (SoftwareSerial)
// Pin A5 (RX) = Not used but required by SoftwareSerial constructor
```

### 2. Logging Conventions
Use severity-appropriate logging macros:
```cpp
LOG_CRITICAL("Safety activated");     // Emergency conditions
LOG_ERROR("Sensor communication failed"); // Error recovery needed  
LOG_WARN("Pressure approaching limit");   // Performance warnings
LOG_INFO("Sequence started");         // Normal operations
LOG_DEBUG("Pin 6 state changed");     // Detailed debugging
```

### 3. Command Processing Pattern
All user commands follow validation → processing → response:
```cpp
bool CommandProcessor::processCommand(char* cmd, bool fromMqtt, char* response, size_t responseSize) {
    // 1. Validate command against ALLOWED_COMMANDS array
    // 2. Parse parameters with bounds checking  
    // 3. Execute command with safety checks
    // 4. Generate structured response
}
```

### 4. State Machine Design
The `SequenceController` uses explicit state transitions:
```cpp
enum SequenceState {
    SEQ_IDLE, SEQ_STAGE1_ACTIVE, SEQ_STAGE1_WAIT_LIMIT,
    SEQ_STAGE2_ACTIVE, SEQ_STAGE2_WAIT_LIMIT, SEQ_COMPLETE, SEQ_ABORT
};
```

## Critical Files for AI Agents

- **`src/main.cpp`**: System initialization and main loop coordination
- **`include/constants.h`**: Pin mappings, timing constants, system limits
- **`docs/TELEMETRY_API.md`**: Complete binary protocol specification
- **`include/sequence_controller.h`**: Hydraulic cycle state machine
- **`src/command_processor.cpp`**: Command validation and processing logic

## Safety Constraints

1. **Never bypass safety checks** - all relay operations must validate limit switches
2. **Emergency stop takes precedence** - E-stop handling comes before all other logic  
3. **Timeout protection** - sequences must have abort timeouts (30 seconds default)
4. **Pressure limits** - automatic shutdown at 2500 PSI for main hydraulic system
5. **Memory bounds** - use shared buffers, never create large stack arrays

## Integration Points

- **Binary Telemetry**: A4/A5 pins output Protocol Buffer messages
- **Serial1**: Relay board communication (115200 baud)
- **Serial Commands**: `controller/` hierarchy for direct control via Serial
- **EEPROM**: Persistent configuration with CRC validation
- **Command Interface**: Text-based commands for configuration and control

## Memory Management Patterns

### Shared Global Buffers
```cpp
// Critical: Use these shared buffers instead of local arrays
extern char g_message_buffer[SHARED_BUFFER_SIZE];    // 256 bytes
extern char g_response_buffer[SHARED_BUFFER_SIZE];   // 256 bytes
extern char g_command_buffer[COMMAND_BUFFER_SIZE];   // 80 bytes

// Example usage in functions:
void myFunction() {
    // WRONG: char localBuffer[256]; // Uses stack
    // RIGHT: Use global shared buffer
    snprintf(g_message_buffer, SHARED_BUFFER_SIZE, "Status: %d", value);
}
```

### Memory Constraints
- **Total RAM**: 32KB (current usage ~20.8%)
- **Flash**: 256KB (current usage ~35.5%)
- **Never create large stack arrays**: Use shared global buffers
- **PROGMEM constants**: Store strings in flash with `PROGMEM`

## Component Dependencies and Initialization Order

### Critical Startup Sequence (main.cpp)
```cpp
// 1. Hardware and basic systems
Serial.begin(115200);
telemetrySerial.begin(115200);
telemetryManager.begin(&telemetrySerial);

// 2. Configuration first (other components depend on it)
configManager.begin();

// 3. Core subsystems
pressureManager.begin();
relayController.begin();
configManager.applyToRelayController(relayController);

// 4. Safety and control systems
safetySystem.setRelayController(&relayController);
safetySystem.begin();

// 5. Command processor (needs references to all components)
commandProcessor.setConfigManager(&configManager);
commandProcessor.setPressureManager(&pressureManager);
// ... other setters
```

### Component Communication Pattern
```cpp
// Components communicate via dependency injection, not globals
class SequenceController {
    SystemErrorManager* errorManager = nullptr;
    InputManager* inputManager = nullptr;
    SafetySystem* safetySystem = nullptr;
    
public:
    void setErrorManager(SystemErrorManager* em) { errorManager = em; }
    // Components call methods, don't access internals directly
};
```

## Non-Networking Version Specifics

### What's Removed
- `NetworkManager` class and all WiFi/MQTT code
- Telnet server functionality  
- MQTT publish/subscribe methods
- WiFi credential management
- Network timeout handling

### What Remains
- **Binary Telemetry**: Enhanced protobuf output on A4/A5
- **Serial Commands**: Full command interface via main Serial port
- **Configuration**: EEPROM storage and management
- **All Control Logic**: Sequences, safety, pressure monitoring unchanged

### Key Files for Non-Networking
- `src/main.cpp`: No network initialization or update calls
- `src/telemetry_manager.cpp`: Pure binary output, no MQTT
- `src/command_processor.cpp`: Serial-only command interface
- `arduino_secrets.h`: Minimal template, no WiFi credentials needed

## LCARS Documentation System & Emergency Diagnostic Wizard

### Documentation Server (`lcars_docs_server.py`)
The project includes a **comprehensive Flask-based documentation system** that provides:

- **Smart Document Discovery**: Auto-scans all `.md` files and categorizes them (Emergency, Hardware, Operations, etc.)
- **Emergency Dashboard**: Priority access to critical documentation during repairs
- **Interactive Search**: Query expansion for repair terms and troubleshooting
- **LCARS-Style Interface**: Clean, responsive design optimized for technical work

### Emergency Diagnostic Wizard (`/emergency_diagnostic`)
**Critical Tool**: Interactive troubleshooting system with:

- **Visual Assessment**: Mill lamp and engine status identification
- **Progressive Diagnosis**: AI-powered analysis of symptoms → root cause
- **Live Serial Commands**: Direct controller communication via USB/Serial
- **Safety Warnings**: Hydraulic operation confirmations and area-clear protocols
- **Session Logging**: Complete diagnostic sessions saved to `wizard_logs/`
- **Report Generation**: PDF/CSV export for maintenance handoff

### Usage Workflows

#### Start Documentation Server
```bash
python lcars_docs_server.py
# Opens http://localhost:3000
# Emergency button → /emergency_diagnostic
```

#### Diagnostic Wizard Features
1. **Mill Lamp Assessment**: Visual identification (OFF/SOLID/SLOW_BLINK/FAST_BLINK)
2. **Engine Status Check**: Running/Stopped/Unknown states  
3. **Progressive Commands**: Auto-suggested commands based on findings
4. **Live Serial Interface**: Real-time controller communication
5. **Safety Protocols**: Hydraulic movement warnings and confirmations
6. **Session Export**: Complete diagnostic logs for maintenance teams

#### Emergency Command API (`/api/run_command`)
- **Auto-detects Arduino COM port** (CH340, USB Serial adapters)
- **115200 baud connection** with 3-second timeout
- **Command validation** and **response parsing**
- **Error handling** for serial communication failures
- **Session logging** to `wizard_logs/*.jsonl`

### Integration with Controller
The diagnostic wizard **directly interfaces** with the LogSplitter Controller:
- **Serial Commands**: `show`, `errors`, `inputs`, `relay status`
- **Hydraulic Safety**: Area-clear confirmations before cylinder movement
- **Real-time Analysis**: Progressive diagnosis based on command responses
- **Cross-referencing**: Compares multiple command outputs for patterns

### Key Documentation Categories
- **Emergency** (🚨): ERROR.md, MILL_LAMP.md, safety procedures
- **Hardware** (⚙️): PINS.md, pressure sensors, relay configuration  
- **Operations** (🔧): Commands, deployment, setup procedures
- **Monitoring** (📊): TELEMETRY_API.md, system testing
- **Development** (💻): API interfaces, protocols
- **Reference** (📖): General documentation and guides

## LCARS Documentation System & Emergency Diagnostic Wizard

### Documentation Server (`lcars_docs_server.py`)
The project includes a **comprehensive Flask-based documentation system** that provides:

- **Smart Document Discovery**: Auto-scans all `.md` files and categorizes them (Emergency, Hardware, Operations, etc.)
- **Emergency Dashboard**: Priority access to critical documentation during repairs
- **Interactive Search**: Query expansion for repair terms and troubleshooting
- **LCARS-Style Interface**: Clean, responsive design optimized for technical work

### Emergency Diagnostic Wizard (`/emergency_diagnostic`)
**Critical Tool**: Interactive troubleshooting system with:

- **Visual Assessment**: Mill lamp and engine status identification
- **Progressive Diagnosis**: AI-powered analysis of symptoms → root cause
- **Live Serial Commands**: Direct controller communication via USB/Serial
- **Safety Warnings**: Hydraulic operation confirmations and area-clear protocols
- **Session Logging**: Complete diagnostic sessions saved to `wizard_logs/`
- **Report Generation**: PDF/CSV export for maintenance handoff

### Usage Workflows

#### Start Documentation Server
```bash
python lcars_docs_server.py
# Opens http://localhost:3000
# Emergency button → /emergency_diagnostic
```

#### Diagnostic Wizard Features
1. **Mill Lamp Assessment**: Visual identification (OFF/SOLID/SLOW_BLINK/FAST_BLINK)
2. **Engine Status Check**: Running/Stopped/Unknown states  
3. **Progressive Commands**: Auto-suggested commands based on findings
4. **Live Serial Interface**: Real-time controller communication
5. **Safety Protocols**: Hydraulic movement warnings and confirmations
6. **Session Export**: Complete diagnostic logs for maintenance teams

#### Emergency Command API (`/api/run_command`)
- **Auto-detects Arduino COM port** (CH340, USB Serial adapters)
- **115200 baud connection** with 3-second timeout
- **Command validation** and **response parsing**
- **Error handling** for serial communication failures
- **Session logging** to `wizard_logs/*.jsonl`

### Integration with Controller
The diagnostic wizard **directly interfaces** with the LogSplitter Controller:
- **Serial Commands**: `show`, `errors`, `inputs`, `relay status`
- **Hydraulic Safety**: Area-clear confirmations before cylinder movement
- **Real-time Analysis**: Progressive diagnosis based on command responses
- **Cross-referencing**: Compares multiple command outputs for patterns

### Key Documentation Categories
- **Emergency** (🚨): ERROR.md, MILL_LAMP.md, safety procedures
- **Hardware** (⚙️): PINS.md, pressure sensors, relay configuration  
- **Operations** (🔧): Commands, deployment, setup procedures
- **Monitoring** (📊): TELEMETRY_API.md, system testing
- **Development** (💻): API interfaces, protocols
- **Reference** (📖): General documentation and guides

## Telemetry Test Receiver (`telemetry_test_receiver.py`)

### Binary Telemetry Validation Tool
**Critical Development Tool**: Python script for validating binary protobuf telemetry:

- **Auto-detects Arduino ports**: Finds CH340, USB Serial adapters automatically
- **Real-time Protocol Buffer decoding**: Validates 8 message types (Digital I/O, Relays, Pressure, etc.)
- **Message integrity checking**: Sequence validation and error detection
- **LCD Simulation**: Displays controller status in simulated LCD format
- **Performance monitoring**: Tracks message rates and throughput

### Usage Examples
```bash
python telemetry_test_receiver.py         # Auto-detect Arduino port
python telemetry_test_receiver.py COM3    # Specific port
```

### Integration with Development
- **Real-time debugging**: Monitor telemetry during controller development
- **Protocol validation**: Ensure binary messages conform to protobuf spec
- **Performance testing**: Verify 600% throughput improvement over ASCII
- **System integration**: Test external monitoring systems

## Meshtastic Integration Architecture

### Wireless Telemetry via Meshtastic Node
**Future Enhancement**: The binary telemetry system is perfectly designed for Meshtastic integration:

#### Hardware Configuration
```
Arduino UNO R4 WiFi → Meshtastic Node → Mesh Network
┌─────────────────────────────────────────────────────────┐
│ Pin A4 (TX) ────────► Meshtastic RX  ────────► LoRa    │
│ Pin A5 (RX) ────────► Meshtastic TX  ◄────────◄ LoRa    │
│                                                         │
│ Serial  ────────────► Debug/Local (unchanged)          │
│ Serial1 ────────────► Relay Board (unchanged)          │
└─────────────────────────────────────────────────────────┘
```

#### Integration Benefits
- **Zero Protocol Changes**: Existing binary telemetry (7-19 bytes) fits perfectly in Meshtastic packets
- **Industrial Range**: LoRa provides kilometers of range for remote log splitter operations
- **Mesh Resilience**: Multi-hop routing through intermediate Meshtastic nodes
- **Battery Friendly**: Optimized binary protocol minimizes transmission power
- **Real-time Performance**: Sub-second latency for safety-critical events

#### Meshtastic Node Configuration
```python
# Meshtastic node setup for LogSplitter telemetry transport
serial_config = {
    'enabled': True,
    'baud': 115200,
    'timeout': 1000,
    'mode': 'TEXTMSG',  # Forward binary data as text payload
    'echo': False
}

# Channel configuration for industrial telemetry
channel_config = {
    'name': 'LogSplitter',
    'psk': 'AQ==',  # Custom PSK for industrial network
    'role': 'PRIMARY',
    'uplink_enabled': True,
    'downlink_enabled': True
}
```

#### Message Routing
- **Telemetry Messages**: A4→Meshtastic→Mesh→Monitoring Station
- **Command Messages**: Monitoring Station→Mesh→Meshtastic→A5→Controller
- **Emergency Broadcasts**: Safety events propagate to all mesh nodes instantly
- **Local Fallback**: Serial debug port remains available for direct USB connection

#### Development Integration
- **Existing Tools Compatible**: `telemetry_test_receiver.py` works through Meshtastic bridge
- **Protocol Unchanged**: All 8 message types (0x10-0x17) transport transparently
- **Range Testing**: Use Meshtastic utilities to validate signal strength and routing
- **Network Debugging**: Mesh network visualization tools for troubleshooting

#### Safety Considerations
- **Redundant Paths**: Multiple mesh routes ensure critical safety data delivery
- **Local Override**: Direct USB connection always available for emergency access
- **Message Priority**: Safety events (Type 0x15) can be configured for priority routing
- **Mesh Health**: Monitor mesh connectivity as part of system health checks

When implementing new features, prioritize safety validation, use existing memory management patterns, and ensure compatibility with the binary telemetry system.