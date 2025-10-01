# LogSplitter Unified Controller-Monitor Architecture

## Overview

The LogSplitter system has evolved into a unified, distributed Arduino-based industrial control system featuring two specialized units working in coordination. This architecture provides comprehensive monitoring, safety systems, remote management, and visual feedback capabilities for industrial log splitting operations.

## System Architecture

### Unified Repository Structure

```
LogSplitter_Controller/
├── [Controller Project - Root Directory]
│   ├── src/                    # Controller source code
│   ├── include/                # Controller headers
│   ├── platformio.ini          # Controller build configuration
│   └── README_REFACTORED.md    # Controller documentation
├── monitor/                    # Monitor Project Subdirectory
│   ├── src/                    # Monitor source code
│   ├── include/                # Monitor headers
│   ├── platformio.ini          # Monitor build configuration
│   └── README.md               # Monitor documentation
├── docs/                       # Shared documentation
│   ├── UNIFIED_ARCHITECTURE.md # This document
│   ├── LOGGING_SYSTEM.md       # Unified logging system
│   └── DEPLOYMENT_GUIDE.md     # Complete deployment guide
└── [Shared Configuration Files]
    ├── COMMANDS.md             # Command reference
    ├── COMPREHENSIVE_REVIEW.md # System overview
    └── PINS.md                 # Hardware pin assignments
```

### Component Separation and Responsibilities

#### Controller (Root Directory)
**Purpose**: Industrial control and safety management
- **Primary Role**: Main control unit for log splitting operations
- **Hardware Control**: Relay management, pressure monitoring, sequence control
- **Safety Systems**: Emergency stops, pressure thresholds, hardware watchdog
- **Network Identity**: Hostname "LogSplitter", IP managed by DHCP
- **Command Interface**: Telnet server on port 23 for industrial control commands

**Key Features**:
- Hydraulic pressure monitoring with safety thresholds
- Multi-relay control for actuators and valves
- Sequence-based operation control
- Emergency stop systems
- MQTT communication for coordination
- Syslog integration for operational logging

#### Monitor (Subdirectory)
**Purpose**: Monitoring, sensing, and visual feedback
- **Primary Role**: Remote monitoring and data collection unit
- **Visual Systems**: LCD display and LED matrix heartbeat animation
- **Precision Sensing**: Weight measurement, temperature monitoring
- **Network Identity**: Hostname "LogMonitor", IP managed by DHCP
- **Command Interface**: Telnet server on port 23 for monitoring commands

**Key Features**:
- 20x4 LCD display for real-time status information
- 12x8 LED matrix with animated heartbeat (30-200 BPM, adjustable brightness)
- NAU7802 24-bit precision weight sensor
- MCP9600 thermocouple temperature sensor
- I2C device management via Wire1 bus
- MQTT data publishing and status reporting

## Hardware Architecture

### Arduino UNO R4 WiFi Platform (Both Units)
- **Processor**: RA4M1 48MHz ARM Cortex-M4
- **Memory**: 32KB RAM, 256KB Flash
- **WiFi**: Built-in 2.4GHz with automatic network management
- **I2C**: Dual I2C buses (Wire: A4/A5, Wire1: Qwiic connector)
- **LED Matrix**: Built-in 12x8 LED matrix (Monitor only)

### Controller Hardware Configuration
```
Pin Assignments:
A0-A3  - Analog sensors (pressure, voltage monitoring)
2-12   - Digital I/O (relays, limit switches, sensors)
13     - Status LED
A4/A5  - I2C bus (Wire) for local sensors
```

### Monitor Hardware Configuration
```
Pin Assignments:
A0-A3  - Analog sensors (temperature, voltage monitoring)
2-12   - Digital I/O (configurable inputs/outputs)
13     - Status LED
Built-in LED Matrix - 12x8 matrix for heartbeat animation
Wire1 (Qwiic):
  - LCD2004A Display (I2C address 0x27)
  - NAU7802 Weight Sensor (I2C address 0x2A)
  - MCP9600 Temperature Sensor (I2C address 0x67)
```

## Software Architecture

### Unified Logging System
Both units implement identical RFC 3164 compliant syslog integration:

- **Severity Levels**: EMERGENCY (0) to DEBUG (7)
- **Dynamic Control**: Runtime log level adjustment via `loglevel` commands
- **Centralized Server**: All logs sent to 192.168.1.238:514
- **Facility**: Local0 (16) with proper priority calculation
- **Application Tags**: "logsplitter" (Controller), "logmonitor" (Monitor)

### Network Integration
Both units share identical network infrastructure:

- **WiFi Configuration**: Shared credentials via `arduino_secrets.h`
- **MQTT Broker**: 159.203.138.46:1883 with authentication
- **Syslog Server**: 192.168.1.238:514 UDP
- **Telnet Access**: Both units provide port 23 command interface

### Command Interface Unification
Both units implement consistent command patterns:

#### Shared Commands (Available on Both Units)
```bash
# System Information
help                    # Show available commands
show                    # Display current readings
status                  # Detailed system information
network                 # Network health check

# Logging Control  
loglevel                # Show current log level
loglevel 0-7            # Set log level (EMERGENCY to DEBUG)
syslog test             # Send test syslog message
syslog status           # Show syslog configuration

# Testing
test network            # Test connectivity
test sensors            # Test sensor readings
reset system            # Restart unit
```

#### Controller-Specific Commands
```bash
# Industrial Control
relay R1 ON             # Control relay 1
relay R2 OFF            # Control relay 2
sequence start          # Start operation sequence
sequence stop           # Stop operation sequence
pressure                # Show pressure readings
safety                  # Show safety system status
emergency               # Emergency stop all operations
```

#### Monitor-Specific Commands
```bash
# Monitoring
weight read             # Get current weight
weight tare             # Tare scale
weight calibrate 400.0  # Calibrate with known weight
temp read               # Get temperature readings
monitor start           # Start monitoring mode

# Visual Systems
lcd on/off              # Control LCD display
lcd clear               # Clear LCD
heartbeat on/off        # Control heartbeat animation
heartbeat rate 72       # Set heart rate (30-200 BPM)
heartbeat brightness 255 # Set LED brightness (0-255)
heartbeat status        # Show heartbeat status
```

## Development Workflow

### Building Both Projects

#### Controller (Root Directory)
```bash
# From repository root
pio run                 # Build Controller
pio run --target upload # Upload Controller firmware
pio device monitor      # Monitor Controller serial output
```

#### Monitor (Subdirectory)
```bash
# From repository root
cd monitor/
pio run                 # Build Monitor
pio run --target upload # Upload Monitor firmware
pio device monitor      # Monitor Monitor serial output
```

### Configuration Management
Both projects share configuration pattern:

1. **Copy Template**: `cp include/arduino_secrets.h.template include/arduino_secrets.h`
2. **Edit Credentials**: Update WiFi and MQTT credentials
3. **Apply to Both**: Ensure both Controller and Monitor have identical credentials

### Testing and Deployment

#### Individual Unit Testing
```bash
# Test Controller
telnet <controller-ip> 23
> test network
> test sensors
> show

# Test Monitor  
telnet <monitor-ip> 23
> test network
> test sensors
> heartbeat status
> weight read
```

#### System Integration Testing
```bash
# Verify unified logging
tail -f /var/log/syslog | grep -E "(logsplitter|logmonitor)"

# Test MQTT coordination
mosquitto_sub -h 159.203.138.46 -p 1883 -u <user> -P <pass> -t "r4/+/+"
```

## Visual Feedback Systems

### LED Matrix Heartbeat Animation (Monitor Only)
The Monitor unit features a sophisticated LED matrix heartbeat animation system:

- **Display**: 12x8 LED matrix built into Arduino R4 WiFi
- **Animation**: 4-frame heart pattern with realistic cardiac timing
- **Heart Rate**: Configurable 30-200 BPM (default 72 BPM)
- **Brightness**: Adjustable 0-255 (default 128)
- **Control**: Full command interface for real-time adjustment
- **Visual Health**: Provides immediate visual feedback of system status

#### Heartbeat Animation Features
- **Realistic Timing**: Frame transitions mimic actual cardiac cycle
- **System Health Indicator**: Animation rate can reflect system load
- **Remote Control**: Full telnet command interface
- **Energy Efficient**: Optimized for continuous operation
- **Non-blocking**: Animation update doesn't interfere with monitoring

### LCD Status Display (Monitor Only)
- **Size**: 20x4 character display via I2C
- **Real-time Data**: Weight, temperature, system status
- **Backlight Control**: Configurable via commands
- **Status Messages**: System state and error notifications

## Network Communication Patterns

### MQTT Topic Architecture

#### Controller Publishing Pattern
```
r4/controller/status     # System operational status
r4/controller/pressure   # Pressure sensor readings
r4/controller/safety     # Safety system status
r4/controller/sequence   # Operation sequence state
r4/controller/relay      # Relay state changes
r4/controller/heartbeat  # Operational heartbeat
```

#### Monitor Publishing Pattern
```
r4/monitor/status        # Monitoring system status
r4/monitor/weight        # Weight sensor readings
r4/monitor/temperature   # Temperature readings
r4/monitor/heartbeat     # Monitor heartbeat (not LED animation)
r4/monitor/sensors       # All sensor data summary
```

#### Cross-Unit Coordination
```
r4/coordination/control  # Cross-unit command channel
r4/coordination/status   # Unified system status
r4/emergency/stop        # Emergency coordination channel
```

## Deployment Considerations

### Network Requirements
- **WiFi Coverage**: Both units must have reliable WiFi access
- **MQTT Broker**: Must be accessible from both units
- **Syslog Server**: Centralized logging server required
- **IP Management**: DHCP recommended, but static IPs supported

### Physical Installation
- **Controller**: Mount near hydraulic control systems
- **Monitor**: Position for easy LCD visibility and sensor access
- **Cabling**: Ensure proper power and sensor connections
- **Environmental**: Consider temperature and humidity for electronics

### Security Considerations
- **Network Security**: Secure WiFi WPA2/WPA3 required
- **Telnet Access**: Consider VPN for remote access in production
- **Credential Management**: Protect `arduino_secrets.h` files
- **Update Security**: Secure firmware update procedures

## Maintenance and Monitoring

### Health Monitoring
Both units provide comprehensive health reporting:

- **Uptime Tracking**: System operational time
- **Memory Usage**: RAM utilization monitoring
- **Network Health**: WiFi signal strength and connectivity
- **Sensor Status**: Individual sensor health and calibration
- **Error Logging**: Comprehensive error tracking and reporting

### Performance Metrics
- **Controller**: Response time, relay operation count, safety events
- **Monitor**: Sensor reading frequency, display updates, animation smoothness
- **Network**: MQTT message rates, syslog transmission success
- **System**: Memory usage, CPU utilization, error rates

## Future Enhancement Opportunities

### Potential Expansions
- **Additional Sensors**: More monitoring points for comprehensive data
- **Enhanced Displays**: Color LCD or larger displays for better visualization
- **Advanced Animations**: Additional LED matrix patterns and status indicators
- **Data Analytics**: Historical data collection and trend analysis
- **Mobile Interface**: Smartphone app for remote monitoring
- **Voice Alerts**: Audio notifications for critical events

### Scalability Considerations
- **Multi-Unit Support**: Framework supports additional Controller/Monitor pairs
- **Cloud Integration**: MQTT bridge to cloud services for remote access
- **Database Logging**: Historical data storage and analysis
- **Web Interface**: Browser-based monitoring and control
- **API Development**: RESTful APIs for third-party integration

---

**Document Version**: 1.0  
**Last Updated**: October 2025  
**System Version**: Controller 2.0.0, Monitor 1.1.0  
**Platform**: Arduino UNO R4 WiFi