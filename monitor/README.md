# LogSplitter Monitor

## Overview

The LogSplitter Monitor is a companion system to the LogSplitter Controller, designed to provide remote monitoring, data collection, and visual feedback capabilities. It features integrated NAU7802 precision weight sensing for load monitoring, LCD display for real-time status, and LED matrix heartbeat animation for visual system health indication. The Monitor connects to the same network infrastructure and logs to the same rsyslog server for centralized monitoring.

## Features

### Visual Display Systems
- **LCD Display**: 20x4 character LCD with real-time system status display
- **LED Matrix Heartbeat Animation**: 12x8 LED matrix displaying animated heartbeat pattern
- **Configurable Heartbeat**: Adjustable heart rate (30-200 BPM) and brightness (0-255)
- **Visual Health Indicator**: Heartbeat animation provides immediate visual feedback of system status

### Networking
- **WiFi Connectivity**: Automatic connection to the same network as the Controller
- **MQTT Integration**: Publishes monitoring data and receives commands
- **Syslog Logging**: Sends all debug output to centralized rsyslog server (192.168.1.238:514)
- **Hostname**: Automatically sets hostname to "LogMonitor" for easy identification

### Telnet Server
- **Remote Access**: Telnet server on port 23 for remote command execution
- **Interactive Commands**: Full command-line interface for monitoring and control
- **Real-time Response**: Live command execution with immediate feedback

### Monitoring Capabilities
- **Weight Sensing**: Precision 24-bit NAU7802 ADC with load cell support
- **Temperature Monitoring**: MAX6656 I2C temperature sensor with local and remote sensing
- **LCD Display**: 20x4 character LCD with real-time system status display
- **Sensor Reading**: Temperature, voltage, and digital input monitoring
- **Weight Calibration**: Zero-point and scale calibration with EEPROM persistence
- **Temperature Filtering**: Advanced filtering for stable temperature readings
- **System Health**: Memory usage, uptime, and connection status tracking
- **Data Publishing**: Automated MQTT publishing of sensor data and system status
- **Status LED**: Visual indication of system state

### Digital I/O
- **Digital Inputs**: Configurable digital input monitoring with state change detection
- **Digital Outputs**: Remote controllable digital outputs for external devices
- **Real-time Notification**: Immediate MQTT publication of input state changes

## Hardware Configuration

### Arduino UNO R4 WiFi Platform
- **Processor**: RA4M1 48MHz ARM Cortex-M4
- **Memory**: 32KB RAM, 256KB Flash
- **WiFi**: Built-in WiFi with automatic hostname setting
- **I2C**: Dual I2C buses (Wire: A4/A5, Wire1: Qwiic connector)
- **Weight Sensor**: NAU7802 24-bit precision ADC via Qwiic connector
- **LED Matrix**: Built-in 12x8 LED matrix for heartbeat animation

### Pin Assignments
```
A0  - Temperature sensor (optional)
A1  - Voltage monitoring
A2  - Spare analog input
2   - Digital input 1 (configurable)
3   - Digital input 2 (configurable)
4   - Digital output 1 (controllable)
5   - Digital output 2 (controllable)
6-9 - Additional watch pins (configurable)
13  - Status LED (built-in)
Built-in LED Matrix - 12x8 matrix for heartbeat animation
Qwiic - LCD2004A I2C display (SDA1/SCL1 via Wire1)
Qwiic - NAU7802 weight sensor (SDA1/SCL1 via Wire1)
```

## I2C Device Configuration

All I2C devices use **Wire1** (Qwiic connector) for consistency and proper Arduino R4 WiFi integration:
- **NAU7802 Weight Sensor**: Address 0x2A
- **LCD2004A Display**: Address 0x27 (default I2C backpack)
- **MAX6656 Temperature Sensor**: Address 0x4C (default, configurable 0x4C-0x4F)

**Important**: Wire1 is used instead of Wire to utilize the Arduino R4 WiFi's Qwiic connector and maintain a clean I2C bus architecture.

## MQTT Topics

### Published Topics (Monitoring Data)
```
r4/monitor/status        - Comprehensive system status
r4/monitor/heartbeat     - Periodic heartbeat with uptime
r4/monitor/temperature   - Temperature sensor reading (°C) - Local sensor
r4/monitor/temperature/local   - Local temperature from MAX6656 (°C)
r4/monitor/temperature/remote  - Remote temperature from MAX6656 (°C)
r4/monitor/voltage       - Voltage monitoring (V)
r4/monitor/weight        - Weight readings (calibrated)
r4/monitor/weight/raw    - Raw ADC values from NAU7802
r4/monitor/weight/status - Weight sensor comprehensive status
r4/monitor/uptime        - System uptime (seconds)
r4/monitor/memory        - Free memory (bytes)
r4/monitor/input/X       - Digital input X state changes (1/0)
r4/monitor/error         - System error messages
```

### Subscribed Topics (Command Input)
```
r4/monitor/control       - Command input topic
```

### Response Topics
```
r4/monitor/control/resp  - Command responses
```

## Telnet Commands

### Connection
```bash
telnet <monitor_ip> 23
```

### Available Commands

#### System Information
```
help                     # Show available commands
show                     # Show current sensor readings and status
status                   # Show detailed system and network status
```

#### Network Management
```
network                  # Show network connection health
syslog test              # Send test message to rsyslog server
syslog status            # Show syslog server configuration
```

#### Monitoring Control
```
monitor start            # Start monitoring mode
monitor stop             # Enter maintenance mode
monitor state            # Show current monitoring state
monitor output 1 on      # Set digital output 1 ON
monitor output 2 off     # Set digital output 2 OFF
```

#### Configuration
```
set debug on             # Enable debug output
set debug off            # Disable debug output
set syslog 192.168.1.238 # Set rsyslog server IP
set interval 5000        # Set status publish interval (ms)
set heartbeat 30000      # Set heartbeat interval (ms)
```

#### Logging Control
```
loglevel                 # Show current log level
loglevel get             # Get current log level
loglevel list            # List all available log levels
loglevel 0               # Set log level to EMERGENCY (0)
loglevel 1               # Set log level to ALERT (1)
loglevel 2               # Set log level to CRITICAL (2)
loglevel 3               # Set log level to ERROR (3)
loglevel 4               # Set log level to WARNING (4)
loglevel 5               # Set log level to NOTICE (5)
loglevel 6               # Set log level to INFO (6)
loglevel 7               # Set log level to DEBUG (7)
```

#### Testing
```
test network             # Test network connectivity
test sensors             # Test sensor readings
test weight              # Test weight sensor (NAU7802)
test outputs             # Test digital outputs
```

#### Weight Sensor (NAU7802)
```
weight read              # Get current weight reading
weight raw               # Get raw ADC value
weight status            # Get comprehensive sensor status
weight tare              # Tare scale at current load
weight zero              # Zero calibration (no load)
weight calibrate 400.0   # Scale calibration with known weight
weight save              # Save calibration to EEPROM
weight load              # Load calibration from EEPROM
```

#### Temperature Sensor (MAX6656)
```
temp read                # Get both local and remote temperatures
temp local               # Get local temperature only
temp remote              # Get remote temperature only
temp status              # Get comprehensive sensor status
temp offset 0.5 -1.0     # Set temperature offsets (local, remote)
```

#### LCD Display Control
```
lcd                      # Get LCD status
lcd on                   # Turn LCD display on
lcd off                  # Turn LCD display off
lcd clear                # Clear LCD display
lcd backlight on         # Turn LCD backlight on
lcd backlight off        # Turn LCD backlight off
lcd info <message>       # Display custom message
```

#### Heartbeat Animation Control
```
heartbeat                # Show heartbeat status
heartbeat status         # Show heartbeat status (ENABLED/DISABLED, BPM, brightness)
heartbeat on             # Enable heartbeat animation
heartbeat off            # Disable heartbeat animation
heartbeat enable         # Enable heartbeat animation (alias for 'on')
heartbeat disable        # Disable heartbeat animation (alias for 'off')
heartbeat rate 72        # Set heart rate to 72 BPM (30-200 range)
heartbeat brightness 255 # Set LED brightness (0-255 range)
heartbeat frame 2        # Display specific frame (0-3 for debugging)
```

#### System Control
```
reset system             # Restart the monitor
reset network            # Reset network connections
```

## Installation

### 1. Hardware Setup
1. Connect Arduino UNO R4 WiFi to power
2. Connect NAU7802 breakout board to Qwiic connector
3. Connect load cell to NAU7802 (E+, E-, A+, A- terminals)
4. Connect LCD2004A display to I2C pins (A4=SDA, A5=SCL)
5. Connect optional sensors to analog pins
6. Connect digital inputs/outputs as needed

### 2. Software Configuration
1. Copy `include/arduino_secrets.h.template` to `include/arduino_secrets.h`
2. Edit `arduino_secrets.h` with your WiFi and MQTT credentials:
   ```cpp
   #define SECRET_SSID "your_wifi_network"
   #define SECRET_PASS "your_wifi_password"
   #define MQTT_USER "your_mqtt_username"
   #define MQTT_PASS "your_mqtt_password"
   ```

### 3. Build and Upload
```bash
pio run --target upload
```

### 4. Monitor Serial Output
```bash
pio device monitor
```

## Operation

### Startup Sequence
1. **Initialization**: System components initialize
2. **Network Connection**: WiFi connection established
3. **MQTT Connection**: MQTT broker connection established
4. **Telnet Server**: Telnet server starts on port 23
5. **Monitoring**: Continuous sensor reading and data publishing

### LED Status Indicators
- **Fast Blink**: Initializing
- **Slow Blink**: Connecting to network
- **Solid ON**: Monitoring (normal operation)
- **Very Fast Blink**: System error
- **Alternating**: Maintenance mode

### Data Publishing
- **Status**: Every 10 seconds (configurable)
- **Heartbeat**: Every 30 seconds (configurable)
- **Sensor Data**: Every 5 seconds
- **Input Changes**: Immediate

## Network Integration

### Same Infrastructure as Controller
- Uses identical WiFi credentials as LogSplitter Controller
- Connects to same MQTT broker (159.203.138.46:1883)
- Logs to same rsyslog server (192.168.1.238:514)

### Hostname Configuration
- Automatically sets hostname to "LogMonitor"
- Easy to identify on network
- Consistent with Controller's "LogSplitter" hostname

### Syslog Integration
- RFC 3164 compliant syslog messages with proper priority levels
- Facility: Local0 (16), Severity: 0-7 (EMERGENCY to DEBUG)
- Priority calculation: Facility * 8 + Severity Level
- Application tag: "logmonitor"
- Dynamic log level control via loglevel commands
- Intelligent error filtering for clean operational logs
- All log output sent to remote syslog server (192.168.1.238:514)

## Memory Usage

**Current Build Statistics:**
- **Platform**: Arduino UNO R4 WiFi
- **Memory**: Optimized for embedded operation
- **Shared Buffers**: Efficient memory management
- **Real-time Operation**: Non-blocking network operations

## Development

### Building
```bash
pio run
```

### Uploading
```bash
pio run --target upload
```

### Monitoring
```bash
pio device monitor
```

### Debug Mode
Enable debug output for troubleshooting:
```
telnet <monitor_ip> 23
> set debug on
debug ON
```

## Troubleshooting

### Connection Issues
1. Check WiFi credentials in `arduino_secrets.h`
2. Verify network connectivity: `telnet <ip> 23`
3. Check MQTT broker accessibility
4. Use `network` command to check connection health

### Sensor Issues
1. Use `test sensors` command to verify readings
2. Check analog sensor connections
3. Verify sensor power supply

### Weight Sensor Issues
1. Use `test weight` command to verify NAU7802 status
2. Check Qwiic connector connection (Wire1 I2C bus)
3. Verify load cell wiring: E+, E-, A+, A- to NAU7802
4. Use `weight status` to check sensor state
5. Perform calibration if readings are incorrect:
   - Remove all load: `weight zero`
   - Add known weight: `weight calibrate <weight>`
   - Save calibration: `weight save`

### Command Issues
1. Use `help` command to see available commands
2. Check command syntax and parameters
3. Use `status` for detailed system information

---

**Version**: 1.0.0  
**Platform**: Arduino UNO R4 WiFi  
**Compatibility**: LogSplitter Controller v2.3.0+