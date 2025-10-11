# LogSplitter Controller Command Reference

This document details all available commands for the LogSplitter Controller via Serial Console, Telnet, and MQTT.

## Communication Interfaces

### Serial Console
- **Port**: Primary USB Serial (115200 baud)
- **Access**: Full command access including PIN mode configuration
- **Format**: Plain text commands terminated with newline

### Telnet Server
- **Port**: 23 (standard telnet port)
- **Access**: Full command access including PIN mode configuration (equivalent to Serial)
- **Format**: Plain text commands terminated with newline
- **Debug Output**: Real-time debug messages with timestamps
- **Connection**: `telnet <device_ip> 23`

### MQTT Topics
- **Subscribe (Commands)**: `r4/example/sub` and `r4/control`
- **Publish (Responses)**: `r4/control/resp`
- **Access**: All commands except `pins` (PIN mode changes restricted to serial/telnet for security)

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
**Description**: Reset system components from fault states or perform complete system reboot
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

##### Complete System Reset
**Parameter**: `system`
**Function**: Perform complete system reboot equivalent to power cycling

**Examples**:
```
> reset system
System reset initiated - rebooting...
[System reboots immediately]
```

**Reset Effects**:
- **Complete Hardware Reset**: Equivalent to power off/on cycle
- **All Variables Reset**: All runtime variables return to default values
- **Network Reconnection**: WiFi and MQTT connections will be re-established
- **Configuration Preserved**: EEPROM settings remain intact
- **Immediate Effect**: System reboots within ~100ms of command execution

**Use Cases**:
- Recovery from system lockup or unresponsive state
- Clearing memory corruption or stack overflow issues
- Full system refresh after configuration changes
- Emergency recovery when other reset methods fail

**Safety Notes**:
- ⚠️ **WARNING**: All active hydraulic operations will stop immediately
- ⚠️ **VERIFY**: Ensure hydraulic cylinders are in safe position before reset
- ✅ Safety systems will reinitialize with full protection on reboot
- ✅ E-Stop state will be re-evaluated on startup

### 6. PINS
**Description**: Display current PIN mode configuration for all Arduino pins
**Syntax**: `pins`
**Access**: Serial + Telnet ONLY (Security restriction)

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
- **seqstable** - Sequence stability time in milliseconds (default: 15)
  ```
  > set seqstable 15
  set seqstable=15
  ```

- **seqstartstable** - Sequence start stability time in milliseconds (default: 100)
  ```
  > set seqstartstable 100
  set seqstartstable=100
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

##### Syslog Configuration
- **syslog** - Configure rsyslog server IP address and port for centralized logging
  **Syntax**: `set syslog <ip>` or `set syslog <ip>:<port>`
  ```
  > set syslog 192.168.1.155
  syslog server set to 192.168.1.155:514
  
  > set syslog 192.168.1.155:1514
  syslog server set to 192.168.1.155:1514
  ```
  
  **Notes**:
  - Default port is 514 (standard syslog UDP port)
  - All debug output is sent exclusively to syslog server
  - RFC 3164 compliant format with facility Local0 and severity Info
  - Use `syslog test` to verify connectivity

##### MQTT Broker Configuration
- **mqtt** - Configure MQTT broker host and port for real-time telemetry
  **Syntax**: `set mqtt <host>` or `set mqtt <host>:<port>`
  ```
  > set mqtt 192.168.1.155
  mqtt broker set to 192.168.1.155:1883
  
  > set mqtt broker.example.com:8883
  mqtt broker set to broker.example.com:8883
  ```
  
  **Notes**:
  - Default port is 1883 (standard MQTT port)
  - Change takes effect immediately (disconnects and reconnects)
  - Use `mqtt test` to verify connectivity
  - Use `mqtt status` to check current configuration

### 8. SYSLOG
**Description**: Syslog server configuration and testing for centralized logging
**Syntax**: `syslog <command>`
**Access**: Serial + MQTT

#### Syslog Commands:

##### Test Syslog Connection
**Command**: `syslog test`
**Function**: Send a test message to the configured syslog server

**Example**:
```
> syslog test
syslog test message sent successfully to 192.168.1.155:514

> syslog test
syslog test message failed to send to 192.168.1.155:514
```

##### Check Syslog Status
**Command**: `syslog status`
**Function**: Display current syslog server configuration and connectivity

**Example Output**:
```
> syslog status
syslog server: 192.168.1.155:514, wifi: connected, local IP: 192.168.1.100
```

**Syslog Configuration**:
- **Primary Configuration**: Set `SYSLOG_SERVER_HOST` in `arduino_secrets.h`
- **Default Fallback**: 192.168.1.155:514 (defined in constants.h)
- **Protocol**: RFC 3164 compliant UDP syslog
- **Facility**: Local0 (16) for custom application logs
- **Severity**: Info (6) for normal operational messages
- **Hostname**: "LogSplitter" for easy identification
- **Tag**: "logsplitter" for application identification

**Debug Output Integration**:
- All debugPrintf() messages are sent exclusively to the syslog server
- No local debug output to Serial or Telnet (cleaner operation)
- Runtime configuration: `set syslog <ip>` or `set syslog <ip>:<port>`
- Test connectivity with `syslog test` command

### 9. MQTT
**Description**: MQTT broker configuration and testing for real-time telemetry
**Syntax**: `mqtt <command>`
**Access**: Serial + MQTT

#### MQTT Commands:

##### Test MQTT Connection
**Command**: `mqtt test`
**Function**: Send a test message to the configured MQTT broker

**Example**:
```
> mqtt test
mqtt test message sent successfully to 192.168.1.155:1883

> mqtt test
MQTT not connected to broker 192.168.1.155:1883
```

##### Check MQTT Status
**Command**: `mqtt status`
**Function**: Display current MQTT broker configuration and connectivity

**Example Output**:
```
> mqtt status
mqtt broker: 192.168.1.155:1883, wifi: connected, mqtt: connected, local IP: 192.168.1.100
```

**MQTT Configuration**:
- **Primary Configuration**: Set `MQTT_BROKER_HOST` in `arduino_secrets.h`
- **Default Fallback**: 192.168.1.155:1883 (defined in constants.h)
- **Protocol**: MQTT v3.1.1 over TCP
- **Client ID**: Automatically generated from MAC address (format: r4-XXXXXX)
- **Topics**: Hierarchical structure under `/controller` namespace
- **Authentication**: Set `MQTT_USER`/`MQTT_PASS` in `arduino_secrets.h`

**Runtime Configuration**:
- Runtime override: `set mqtt <host>` or `set mqtt <host>:<port>`
- Changes take effect immediately (disconnects and reconnects to new broker)
- Test connectivity with `mqtt test` command
- Monitor status with `mqtt status` command

### 10. ERROR
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
- `0x80` - Sequence operation timeout

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

##### Safety Clear vs Error Clear
**Safety Clear (Pin 4 Button)**:
- **Purpose**: Operational recovery - allows resuming normal operation
- **Action**: Clears safety system lockouts and re-enables sequence controller after timeout
- **Error History**: Preserves all error records for management review
- **Sequence Control**: Re-enables sequence controller if disabled due to timeout errors
- **Use Case**: Managing operator restores operation after addressing fault causes

**Error Clear (Command)**:
- **Purpose**: Error history management - clears error records
- **Action**: Removes error entries from system error list
- **Safety State**: Does not affect safety system lockouts
- **Use Case**: Management clears error history after review and documentation

**Typical Recovery Sequence**:
1. Fault occurs → Safety system activates → Mill light indicates errors
2. **Sequence Timeout**: If timeout occurs, sequence controller is disabled
3. Operator addresses root cause of fault condition
4. Manager presses Safety Clear (Pin 4) → System operational, sequence re-enabled, errors still logged
5. Manager reviews error list → Documents incident → Issues `error clear` command

**System Error LED (Pin 9)**:
- **OFF**: No errors
- **Solid ON**: Single error or all errors acknowledged
- **Slow Blink (1Hz)**: Multiple errors, some unacknowledged
- **Fast Blink (5Hz)**: Critical errors (EEPROM CRC, memory, hardware)

**MQTT Error Reporting**:
- Error details published to `r4/system/error` topic
- Includes error code, description, and timestamp
- Automatic error reporting on detection

### 10. RELAY
**Description**: Control hydraulic system relays with integrated safety monitoring
**Syntax**: `relay R<number> <state>`
**Access**: Serial + MQTT
**Range**: R1-R9 (relays 1-9)
**States**: ON, OFF (case-insensitive)

#### Relay Assignments:
- **R1**: Hydraulic Extend Control (Safety-Monitored)
- **R2**: Hydraulic Retract Control (Safety-Monitored)
- **R3-R9**: Direct relay outputs

#### Enhanced Manual Operation Safety (R1/R2):
**Unified Sequence-Based Control**: R1 and R2 commands now use the same intelligent safety system as automatic sequences, providing comprehensive protection against over-travel and pressure damage.

**Pre-Operation Safety Checks:**
- **Limit Validation**: Commands blocked if already at target limit switch
- **Pressure Validation**: Commands blocked if pressure already at safety limit
- **System Status**: Commands blocked during active sequences or system faults

**Real-Time Protection During Operation:**
- **Automatic Limit Shutoff**: 
  - R1 (Extend) stops when Pin 6 (extend limit) activates
  - R2 (Retract) stops when Pin 7 (retract limit) activates
- **Pressure Limit Protection**:
  - R1 stops when pressure reaches `EXTEND_PRESSURE_LIMIT_PSI`
  - R2 stops when pressure reaches `RETRACT_PRESSURE_LIMIT_PSI`
- **Timeout Protection**: Operations abort if timeout exceeded (configurable)

**Enhanced User Feedback:**
```bash
# Successful operation start
> relay R1 ON
manual extend started (safety-monitored)

# Safety blocking examples
> relay R1 ON
manual extend blocked - check limits/pressure/status

# Manual stop
> relay R1 OFF
manual operation stopped

# Non-hydraulic relay (direct control)
> relay R3 ON
relay R3 ON
```

**Safety Messages and Logging:**
- All operations logged with safety status
- MQTT publishing for remote monitoring
- Clear feedback on why operations are blocked
- Integration with system error management

**Backward Compatibility:**
- Existing command syntax unchanged
- R3-R9 relays operate as direct control
- All monitoring and telemetry preserved

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
- PIN mode changes are restricted to serial and telnet interfaces for security