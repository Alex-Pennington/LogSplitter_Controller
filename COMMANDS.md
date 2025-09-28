# LogSplitter Controller Command Reference

This document details all available commands for the LogSplitter Controller via Serial Console and MQTT.

## Communication Interfaces

### Serial Console
- **Port**: Primary USB Serial (115200 baud)
- **Access**: Full command access including PIN mode configuration
- **Format**: Plain text commands terminated with newline

### MQTT Topics
- **Subscribe (Commands)**: `r4/example/sub` and `r4/control`
- **Publish (Responses)**: `r4/control/resp`
- **Access**: All commands except `pins` (PIN mode changes restricted to serial for security)

## Command Format

All commands are case-insensitive and follow the format:
```
COMMAND [parameter] [value]
```

## Available Commands

### 1. HELP
**Description**: Display available commands
**Syntax**: `help`
**Access**: Serial + MQTT

**Example**:
```
> help
Commands: help, show, debug, network, pins, set <param> <val>, relay R<n> ON|OFF
```

### 2. SHOW
**Description**: Display complete system status including pressure sensors, sequence controller, relays, and safety systems
**Syntax**: `show`
**Access**: Serial + MQTT

**Example Output**:
```
> show
Pressure: Main=2450.5 PSI, Hydraulic=2340.2 PSI
Sequence: IDLE, Safety: OK
Relays: R1=OFF(RETRACT) R2=OFF(EXTEND)
Safety: Manual Override=OFF, Pressure OK
```

### 3. DEBUG
**Description**: Enable or disable debug output to serial console
**Syntax**: `debug [ON|OFF]`
**Access**: Serial + MQTT

**Examples**:
```
> debug
debug OFF

> debug ON
debug ON

> debug off
debug OFF
```

**Note**: Debug output is **disabled by default** to reduce serial console noise. When enabled, the system outputs detailed diagnostic information including:
- Input pin state changes
- Limit switch activations  
- MQTT message details
- Pressure sensor initialization
- Command processing status

### 4. NETWORK
**Description**: Display network health statistics and connection status
**Syntax**: `network`
**Access**: Serial + MQTT

**Example Output**:
```
> network
wifi=OK mqtt=OK stable=YES disconnects=2 fails=0 uptime=1247s
```

**Status Fields**:
- **wifi**: OK/DOWN - WiFi connection state
- **mqtt**: OK/DOWN - MQTT broker connection state  
- **stable**: YES/NO - Connection stable for >30 seconds
- **disconnects**: Total number of connection losses
- **fails**: Failed MQTT publish attempts
- **uptime**: Current connection uptime in seconds

**Network Failsafe Operation**:
- ✅ **Hydraulic control NEVER blocked by network issues**
- ✅ **Non-blocking reconnection** - system continues operating during network problems
- ✅ **Automatic recovery** with exponential backoff
- ✅ **Connection stability monitoring** prevents flapping
- ✅ **Health metrics** for diagnostics and troubleshooting

### 5. RESET
**Description**: Reset system components from fault states
**Syntax**: `reset <component>`
**Access**: Serial + MQTT

#### Available RESET Components:

##### Emergency Stop (E-Stop) Reset
**Parameter**: `estop`
**Function**: Clear emergency stop latch and restore system operation

**Requirements**:
- Emergency stop button must NOT be currently pressed
- System must be in emergency stop state (SYS_EMERGENCY_STOP)

**Examples**:
```
> reset estop
E-Stop reset successful - system operational

> reset estop
E-Stop reset failed: E-Stop button still pressed

> reset estop  
E-Stop not latched - no reset needed
```

**Safety Notes**:
- ⚠️ **CRITICAL**: E-Stop reset requires manual verification that all hazards are clear
- ⚠️ **VERIFY**: Ensure E-Stop button is physically released before attempting reset
- ⚠️ **CONFIRM**: All personnel are clear of hydraulic equipment before reset
- ✅ Reset only clears the software latch - hardware E-Stop must be manually released
- ✅ Safety system integration prevents unsafe operation

### 6. PINS
**Description**: Display current PIN mode configuration for all Arduino pins
**Syntax**: `pins`
**Access**: Serial ONLY (Security restriction)

**Example Output**:
```
> pins
Pin 0: INPUT_PULLUP
Pin 1: OUTPUT
Pin 2: INPUT
...
Pin 13: OUTPUT
```

### 7. SET
**Description**: Configure system parameters with EEPROM persistence
**Syntax**: `set <parameter> <value>`
**Access**: Serial + MQTT

#### Available SET Parameters:

##### Pressure Sensor Configuration
- **vref** - ADC reference voltage (volts, default: 4.5)
  ```
  > set vref 4.5
  set vref=4.5
  ```

- **maxpsi** - Maximum pressure scale (PSI, default: 5000)
  ```
  > set maxpsi 5000
  set maxpsi=5000
  ```

- **gain** - Pressure sensor gain multiplier (default: 1.0)
  ```
  > set gain 1.0
  set gain=1.0
  ```

- **offset** - Pressure sensor offset (PSI, default: 0.0)
  ```
  > set offset 0.0
  set offset=0.0
  ```

- **filter** - Digital filter coefficient (0.0-1.0, default: 0.8)
  ```
  > set filter 0.8
  set filter=0.8
  ```

- **emaalpha** - Exponential Moving Average alpha (0.0-1.0, default: 0.1)
  ```
  > set emaalpha 0.1
  set emaalpha=0.1
  ```

##### Individual Sensor Configuration
Configure sensors independently with sensor-specific parameters:

**A1 System Pressure Sensor (4-20mA Current Loop)**:
- **a1_maxpsi** - Maximum pressure range (PSI, default: 5000)
- **a1_vref** - ADC reference voltage (volts, default: 5.0)
- **a1_gain** - Sensor gain multiplier (default: 1.0)
- **a1_offset** - Sensor offset (PSI, default: 0.0)

**A5 Filter Pressure Sensor (0-4.5V Voltage Output)**:
- **a5_maxpsi** - Maximum pressure range (PSI, default: 30 for 0-30 PSI absolute sensor)
- **a5_vref** - ADC reference voltage (volts, default: 5.0)
- **a5_gain** - Sensor gain multiplier (default: 1.0, note: sensor outputs 0-4.5V for full scale)
- **a5_offset** - Sensor offset (PSI, default: 0.0)

Examples:
```
> set a1_maxpsi 5000
A1 maxpsi set 5000

> set a5_maxpsi 3000
A5 maxpsi set 3000

> set a5_gain 1.2
A5 gain set 1.200000

> set a5_offset -10.5
A5 offset set -10.500000
```

##### Sequence Controller Configuration
- **seqstable** - Sequence stability time in milliseconds (default: 1000)
  ```
  > set seqstable 1000
  set seqstable=1000
  ```

- **seqstartstable** - Sequence start stability time in milliseconds (default: 500)
  ```
  > set seqstartstable 500
  set seqstartstable=500
  ```

- **seqtimeout** - Sequence timeout in milliseconds (default: 30000)
  ```
  > set seqtimeout 30000
  set seqtimeout=30000
  ```

##### PIN Configuration (Serial Only)
- **pinmode** - Configure Arduino pin mode
  **Syntax**: `set pinmode <pin> <mode>`
  **Modes**: INPUT, OUTPUT, INPUT_PULLUP
  ```
  > set pinmode 13 OUTPUT
  set pinmode pin=13 mode=OUTPUT
  
  > set pinmode 2 INPUT_PULLUP
  set pinmode pin=2 mode=INPUT_PULLUP
  ```

##### Debug Control
- **debug** - Enable/disable debug output (1/0, ON/OFF, default: OFF)
  ```
  > set debug ON
  set debug=ON
  
  > set debug 0
  set debug=OFF
  ```

### 8. ERROR
**Description**: System error management for diagnostics and maintenance
**Syntax**: `error <command> [parameter]`
**Access**: Serial + MQTT

#### Error Commands:

##### List Active Errors
**Command**: `error list`
**Function**: Display all currently active system errors with acknowledgment status

**Example Output**:
```
> error list
Active errors: 0x01:(ACK)EEPROM_CRC, 0x10:CONFIG_INVALID
```

##### Acknowledge Error
**Command**: `error ack <error_code>`
**Function**: Acknowledge a specific error (changes LED pattern but doesn't clear error)

**Error Codes**:
- `0x01` - EEPROM CRC validation failed
- `0x02` - EEPROM save operation failed  
- `0x04` - Pressure sensor malfunction
- `0x08` - Network connection persistently failed
- `0x10` - Configuration parameters invalid
- `0x20` - Memory allocation issues
- `0x40` - General hardware fault

**Examples**:
```
> error ack 0x01
Error 0x01 acknowledged

> error ack 0x10
Error 0x10 acknowledged
```

##### Clear All Errors
**Command**: `error clear`
**Function**: Clear all system errors (only if underlying faults are resolved)

**Example**:
```
> error clear
All errors cleared
```

**System Error LED (Pin 9)**:
- **OFF**: No errors
- **Solid ON**: Single error or all errors acknowledged
- **Slow Blink (1Hz)**: Multiple errors, some unacknowledged
- **Fast Blink (5Hz)**: Critical errors (EEPROM CRC, memory, hardware)

**MQTT Error Reporting**:
- Error details published to `r4/system/error` topic
- Includes error code, description, and timestamp
- Automatic error reporting on detection

### 9. RELAY
**Description**: Control hydraulic system relays
**Syntax**: `relay R<number> <state>`
**Access**: Serial + MQTT
**Range**: R1-R9 (relays 1-9)
**States**: ON, OFF (case-insensitive)

#### Relay Assignments:
- **R1**: Hydraulic Extend Control
- **R2**: Hydraulic Retract Control
- **R3-R9**: Additional relay outputs

#### Manual Operation Safety:
- **R1 (Extend)**: Automatically turns OFF when extend limit switch (Pin 6) is reached
- **R2 (Retract)**: Automatically turns OFF when retract limit switch (Pin 7) is reached
- **Safety Message**: "SAFETY: [Extend/Retract] operation stopped - limit switch reached"
- **Protection**: Prevents hydraulic cylinder over-travel damage during manual operation

**Examples**:
```
> relay R1 ON
relay R1 ON

> relay R2 OFF
relay R2 OFF

> relay r3 on
relay r3 on
```

## MQTT Data Topics

The controller publishes real-time data to MQTT topics optimized for database integration. Each topic contains a single data value as the payload for streamlined data storage and analysis.

### Pressure Data (Published every 10 seconds)
- **r4/pressure/hydraulic_system** → Hydraulic system pressure (PSI)
- **r4/pressure/hydraulic_filter** → Hydraulic filter pressure (PSI)
- **r4/pressure/hydraulic_system_voltage** → A1 sensor voltage (V)
- **r4/pressure/hydraulic_filter_voltage** → A5 sensor voltage (V)
- **r4/pressure** → Main pressure (backward compatibility)

### System Data (Published every 10 seconds)
- **r4/data/system_uptime** → System uptime (milliseconds)
- **r4/data/safety_active** → Safety system active (1/0)
- **r4/data/estop_active** → E-Stop button pressed (1/0)
- **r4/data/estop_latched** → E-Stop latched state (1/0)
- **r4/data/limit_extend** → Extend limit switch (1/0)
- **r4/data/limit_retract** → Retract limit switch (1/0)
- **r4/data/relay_r1** → Extend relay state (1/0)
- **r4/data/relay_r2** → Retract relay state (1/0)
- **r4/data/splitter_operator** → Splitter operator signal (1/0)

### Sequence Data (Published every 10 seconds)
- **r4/data/sequence_stage** → Current sequence stage (0-2)
- **r4/data/sequence_active** → Sequence running (1/0)
- **r4/data/sequence_elapsed** → Sequence elapsed time (milliseconds)

### Sequence Events (Published on state changes)
- **r4/sequence/event** → Sequence events (start, complete, abort, etc.)
- **r4/sequence/state** → Sequence state changes (start, complete, abort)

### Legacy Topics (Backward Compatibility)
- **r4/sequence/status** → Complex sequence status string
- **r4/example/pub** → Timestamp heartbeat
- **r4/control/resp** → Command responses and system messages

### Database Integration Benefits
- **Simple Values**: Each topic contains only the data value (no parsing required)
- **Consistent Format**: Numeric values as strings, boolean values as 1/0
- **Clear Naming**: Topic name directly indicates the data type
- **Efficient Storage**: Minimal payload overhead for database insertion
- **Scalable**: Easy to add new data points as individual topics

## Safety Features

### Manual Override
The system includes manual safety override capabilities:
- Pressure threshold monitoring (Mid-stroke: 2750 PSI, At limits: 2950 PSI)
- Automatic sequence abort on over-pressure conditions
- Manual override toggle for emergency situations

### Rate Limiting
Commands are rate-limited to prevent system overload:
- Maximum command frequency enforced
- Rate limit violations return "rate limited" response

### Input Validation
All commands undergo strict validation:
- Parameter range checking
- Invalid commands return "invalid command" response
- Malformed relay commands return "relay command failed"

## Hardware Configuration

### Pressure Sensors
- **A1 (Pin A1)**: System hydraulic pressure sensor (4-20mA current loop, 1-5V → 0-5000 PSI)
- **A5 (Pin A5)**: Filter hydraulic pressure sensor (0-5V voltage output → 0-5000 PSI)

### Digital Inputs
- **Pin 6**: Extend limit switch (INPUT_PULLUP)
- **Pin 7**: Retract limit switch (INPUT_PULLUP)
- **Pin 8**: Splitter operator signal (INPUT_PULLUP)
- **Pin 12**: Emergency stop (E-Stop) button (INPUT_PULLUP)

### Relay Outputs
- **Serial1**: Hardware relay control interface
- **R1-R9**: Available relay channels

## Error Responses

| Error | Description |
|-------|-------------|
| `invalid command` | Command not recognized or malformed |
| `invalid parameter` | SET parameter not in allowed list |
| `invalid value` | Parameter value out of range or wrong type |
| `relay command failed` | Relay number invalid or communication error |
| `rate limited` | Commands sent too frequently |
| `pins command not available via MQTT` | PIN commands restricted to serial |

## Example Session

```
> help
Commands: help, show, debug, network, pins, set <param> <val>, relay R<n> ON|OFF

> network
wifi=OK mqtt=OK stable=YES disconnects=0 fails=0 uptime=1247s

> debug
debug OFF

> debug on
debug ON
Debug output enabled

> show
Pressure: Main=0.0 PSI, Hydraulic=0.0 PSI
Sequence: IDLE, Safety: OK
Relays: R1=OFF(RETRACT) R2=OFF(EXTEND)
Safety: Manual Override=OFF, Pressure OK

> set debug OFF
debug OFF

> set maxpsi 4000
set maxpsi=4000

> relay R2 ON
relay R2 ON

> relay R2 OFF  
relay R2 OFF
```

## Development Notes

- All configuration changes are saved to EEPROM for persistence across power cycles
- The system supports both uppercase and lowercase commands
- MQTT responses are published to `r4/control/resp` topic
- Serial responses are sent directly to the console
- PIN mode changes are restricted to serial interface for security