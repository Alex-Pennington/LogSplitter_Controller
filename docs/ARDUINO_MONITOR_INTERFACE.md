# Arduino R4 WiFi Monitor Interface Documentation

## Overview

This document defines the serial communication interface between the LogSplitter Controller (Arduino R4 WiFi) and a monitoring Arduino R4 WiFi that will relay telemetry to MQTT.

## Physical Connection

```
┌─────────────────────┐    A4(TX)     ┌─────────────────────┐    WiFi      ┌─────────────────┐
│   Controller R4     │──────────────▶│   Monitor R4 WiFi   │─────────────▶│  MQTT Broker    │
│  (Main System)      │   to RX       │  (MQTT Bridge)      │   802.11     │  (Mosquitto)    │
└─────────────────────┘               └─────────────────────┘              └─────────────────┘
```

**Physical Connection:**
- Controller Pin **A4** (TX) → Monitor Arduino **RX** pin (any digital pin)
- Controller Pin **GND** → Monitor Arduino **GND** (common ground required)
- **Baud Rate**: 115200
- **Format**: 8N1 (8 data bits, no parity, 1 stop bit)

**Notes**: 
- Controller A4 pin is configured as SoftwareSerial TX output for telemetry data
- Main USB Serial port is now free for user interaction and programming
- Monitor Arduino can use any available digital pin as RX to receive telemetry

## Output Frequency & Rate Limiting

The controller uses intelligent rate limiting to minimize serial traffic while ensuring critical data is sent immediately:

### Immediate Output (Real-time)
- **Sequence State Changes**: Sent immediately when sequence transitions occur
- **Input Pin Changes**: Button presses, limit switches, emergency stop events  
- **Relay Operations**: Valve control commands sent as they happen
- **Safety Events**: Emergency stops, safety violations (always immediate)
- **Critical/Error Messages**: System failures and warnings (always immediate)

### Rate Limited Output (30 second intervals)  
- **System Status**: General health information, uptime statistics
- **Pressure Readings**: Sent with sequence changes OR every 30 seconds maximum
- **Safety Status**: E-stop/engine status (only if unchanged for 30+ seconds)

**Result**: ~90% reduction in serial traffic compared to continuous streaming, while maintaining real-time responsiveness for operational data.

## Serial Output Format

All telemetry from the controller follows this format:

```
timestamp|LEVEL|message_content
```

### Message Structure

| Component | Description | Example |
|-----------|-------------|---------|
| `timestamp` | Milliseconds since controller boot | `45678` |
| `LEVEL` | Log severity (see table below) | `INFO` |
| `message_content` | Actual telemetry data | `Pressure: A1=245.2psi A5=12.1psi` |

### Log Levels

| Level | Always Sent | Description | Arduino Action |
|-------|-------------|-------------|----------------|
| `EMERG` | ✅ Yes | System unusable | **Always forward - Critical** |
| `ALERT` | ✅ Yes | Immediate action needed | **Always forward - Critical** |
| `CRIT` | ✅ Yes | Critical conditions | **Always forward - Critical** |
| `ERROR` | ✅ Yes | Error conditions | **Always forward - Critical** |
| `WARN` | 📡 Depends | Warning conditions | Forward if telemetry enabled |
| `NOTICE` | 📡 Depends | Normal but significant | Forward if telemetry enabled |
| `INFO` | 📡 Depends | Informational | Forward if telemetry enabled |
| `DEBUG` | 📡 Depends | Debug messages | Forward if telemetry enabled |

> **Important**: EMERG, ALERT, CRIT, and ERROR messages are **always sent** even when telemetry is disabled for safety reasons.

## Interactive Control Messages

The controller supports interactive telemetry control. The monitor Arduino should watch for these special messages:

### Telemetry Enable/Disable
```
*** TELEMETRY/DEBUG OUTPUT ENABLED (Ctrl+K to toggle) ***
*** TELEMETRY/DEBUG OUTPUT DISABLED (Ctrl+K to toggle) ***
```

**Monitor Arduino Action**: 
- Parse these messages to know if normal telemetry is active
- Critical messages (EMERG/ALERT/CRIT/ERROR) still come through when disabled

### Interactive Mode Messages
```
Interactive mode: keystrokes will be echoed
> 
```

**Monitor Arduino Action**: 
- These are user interface messages
- Should **not** be forwarded to MQTT
- Indicates controller is in interactive command mode

## Telemetry Message Types

### 1. System Status Messages

#### Startup Sequence
```
1234|INFO|System initializing...
1456|INFO|Pressure sensors initialized  
1678|INFO|Safety systems active
1890|INFO|System ready - entering main loop
```

#### Periodic Health Status
```
45678|INFO|System: state=READY estop=false pressure_ok=true
```

### 2. Pressure Data

#### Real-time Pressure Readings
```
12345|INFO|Pressure: A1=245.2psi A5=12.1psi
```

**Parsing Notes for Arduino**:
- Split on spaces after "Pressure:"
- Each sensor: `A1=245.2psi` format
- Extract sensor name and numeric value (remove "psi")

#### Pressure Events
```
23456|WARN|Pressure A1 high: 285.3psi (limit: 280.0psi)
34567|INFO|Pressure normalized: A1=255.1psi
```

### 3. Sequence Control Events

#### State Transitions
```
45678|INFO|Sequence: IDLE -> EXTEND_START
56789|INFO|Sequence: EXTEND_START -> EXTENDING
67890|INFO|Sequence: EXTENDING -> EXTEND_COMPLETE
```

**Parsing Notes**:
- Look for `->` to identify state transitions
- Extract `from_state` and `to_state`

#### Status Updates
```
78901|INFO|Sequence: type=AUTO state=EXTENDING elapsed=2340ms
```

### 4. Digital Input Changes

#### Pin State Changes
```
89012|INFO|Input: pin=6 state=true name=LIMIT_EXTEND
90123|INFO|Input: pin=12 state=true name=ESTOP_ACTIVE
```

**Parsing Notes**:
- Extract pin number, boolean state, and name
- Important pins: 6=LIMIT_EXTEND, 7=LIMIT_RETRACT, 12=ESTOP

#### Input Summary (Debug)
```
01234|DEBUG|Inputs: [2]=false [3]=true [4]=false [5]=false [6]=true [7]=false [12]=false
```

### 5. Relay Control Events

#### Individual Relay Changes
```
12345|INFO|Relay: R1=ON (Extend valve)
23456|INFO|Relay: R2=OFF (Retract valve)
```

**Parsing Notes**:
- Extract relay number (R1-R8) and state (ON/OFF)
- Description in parentheses is human-readable context

#### Relay Status Summary (Debug)
```
34567|DEBUG|Relays: R1=ON R2=OFF R3=OFF R4=ON R5=OFF R6=OFF R7=OFF R8=OFF
```

### 6. Safety System Events

#### Emergency Stop
```
45678|CRIT|Emergency stop activated - all sequences halted
56789|WARN|Emergency stop cleared - manual reset required
```

#### Safety Violations
```
67890|ERROR|Safety violation: pressure exceeded safe limits
78901|WARN|Safety system: auto-recovery attempted
```

### 7. Error Messages

#### Communication Errors
```
89012|ERROR|Relay controller communication timeout
90123|WARN|Pressure sensor A5 reading unstable
```

#### System Errors
```
01234|CRIT|Multiple system failures detected
12345|ERROR|Watchdog reset detected
```

## Sample Message Stream

Here's what the monitor Arduino would receive during a typical extend sequence:

```
# User presses extend button (immediate)
46234|INFO|Input: pin=2 state=true name=START_EXTEND

# Sequence starts (immediate sequence updates)
46235|INFO|Sequence: IDLE -> EXTEND_START
46236|INFO|Pressure: A1=156.7psi A5=8.2psi
46237|INFO|Relay: R1=ON (Extend valve)

# Next sequence transition (immediate)  
46890|INFO|Sequence: EXTEND_START -> EXTENDING
46891|INFO|Pressure: A1=234.1psi A5=9.8psi

# Limit switch reached (immediate input change)
58901|INFO|Input: pin=6 state=true name=LIMIT_EXTEND

# Sequence completes (immediate)
58902|INFO|Sequence: EXTENDING -> EXTEND_COMPLETE  
58903|INFO|Pressure: A1=278.5psi A5=12.1psi
58904|INFO|Relay: R1=OFF (Extend valve)

# Return to idle (immediate)
59234|INFO|Sequence: EXTEND_COMPLETE -> IDLE
59235|INFO|Pressure: A1=245.2psi A5=10.8psi

# System status summary (only every 30 seconds when quiet)
89234|INFO|System: state=READY estop=false pressure=stable
```

## Arduino Parsing Strategy

### Recommended Arduino Code Structure

```cpp
// In your monitor Arduino sketch:

void processControllerMessage(String message) {
    // Skip empty messages
    if (message.length() == 0) return;
    
    // Handle interactive control messages
    if (message.indexOf("TELEMETRY/DEBUG OUTPUT") >= 0) {
        handleTelemetryControl(message);
        return;
    }
    
    // Skip interactive prompts
    if (message.startsWith("> ") || message == ">") {
        return;
    }
    
    // Parse pipe-delimited format
    int firstPipe = message.indexOf('|');
    int secondPipe = message.indexOf('|', firstPipe + 1);
    
    if (firstPipe > 0 && secondPipe > firstPipe) {
        unsigned long timestamp = message.substring(0, firstPipe).toInt();
        String level = message.substring(firstPipe + 1, secondPipe);
        String content = message.substring(secondPipe + 1);
        
        // Process the parsed message
        handleTelemetryMessage(timestamp, level, content);
    } else {
        // Handle non-standard format as raw message
        handleRawMessage(message);
    }
}

void handleTelemetryMessage(unsigned long timestamp, String level, String content) {
    // Check if we should forward this message
    bool shouldForward = true;
    
    if (!telemetryEnabled) {
        // Only forward critical messages when telemetry disabled
        shouldForward = (level == "EMERG" || level == "ALERT" || 
                        level == "CRIT" || level == "ERROR");
    }
    
    if (shouldForward) {
        publishToMQTT(timestamp, level, content);
    }
}
```

### Message Filtering for Arduino

The monitor Arduino should implement basic filtering:

1. **Always Forward**: EMERG, ALERT, CRIT, ERROR messages
2. **Conditional Forward**: WARN, NOTICE, INFO, DEBUG (only when telemetry enabled)  
3. **Never Forward**: Interactive prompts ("> "), control messages
4. **Special Handling**: Parse structured data (pressure, relay, sequence) into separate MQTT topics

## MQTT Topic Recommendations

For the Arduino R4 WiFi monitor, suggest this topic structure:

```
logsplitter/
├── raw/{level}           # Raw messages by level
├── pressure/a1           # Individual pressure sensors  
├── pressure/a5
├── sequence/state        # Sequence state changes
├── relay/r1              # Individual relay states
├── relay/r2
├── input/pin6            # Individual input pins
├── input/pin12
├── system/status         # Overall system health
└── bridge/status         # Monitor Arduino health
```

This provides a clear interface specification for building the Arduino R4 WiFi monitoring system. 

**Next Steps**: Please clarify the connection method and specific requirements so I can provide more detailed Arduino code examples.