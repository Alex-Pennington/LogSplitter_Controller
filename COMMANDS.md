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
Commands: help, show, pins, set <param> <val>, relay R<n> ON|OFF
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

### 3. PINS
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

### 4. SET
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

### 5. RELAY
**Description**: Control hydraulic system relays
**Syntax**: `relay R<number> <state>`
**Access**: Serial + MQTT
**Range**: R1-R9 (relays 1-9)
**States**: ON, OFF (case-insensitive)

#### Relay Assignments:
- **R1**: Hydraulic Retract Control
- **R2**: Hydraulic Extend Control
- **R3-R9**: Additional relay outputs

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

The controller publishes real-time data to the following MQTT topics:

### Pressure Data (Published every 10 seconds)
- **r4/pressure**: Combined pressure readings (backward compatibility)
- **r4/pressure/hydraulic_system**: Hydraulic system pressure (A1 sensor)
- **r4/pressure/hydraulic_filter**: Hydraulic filter pressure (A5 sensor)
- **r4/pressure/status**: Pressure system status and alerts

### Sequence Control
- **r4/sequence/status**: Current sequence state (IDLE, EXTENDING, RETRACTING, etc.)
- **r4/sequence/event**: Sequence events and state changes
- **r4/sequence/state**: Detailed sequence controller state

### System Status
- **r4/example/pub**: General system telemetry

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
- **A1 (Pin A1)**: Hydraulic system pressure sensor (0-4.5V → 0-5000 PSI)
- **A5 (Pin A5)**: Hydraulic filter pressure sensor (0-4.5V → 0-5000 PSI)

### Digital Inputs
- **Pin 6**: Extend limit switch (INPUT_PULLUP)
- **Pin 7**: Retract limit switch (INPUT_PULLUP)

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
Commands: help, show, pins, set <param> <val>, relay R<n> ON|OFF

> show
Pressure: Main=0.0 PSI, Hydraulic=0.0 PSI
Sequence: IDLE, Safety: OK
Relays: R1=OFF(RETRACT) R2=OFF(EXTEND)
Safety: Manual Override=OFF, Pressure OK

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