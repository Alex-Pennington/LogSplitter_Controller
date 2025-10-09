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

## System Commands

### help
**Description**: Display available commands
**Syntax**: `help`
**Access**: Serial + MQTT + Telnet

### show
**Description**: Display complete system status including pressure sensors, sequence controller, relays, and safety systems
**Syntax**: `show`
**Access**: Serial + MQTT + Telnet

### debug on|off
**Description**: Toggle debug mode
**Syntax**: `debug on` or `debug off`
**Access**: Serial + MQTT + Telnet

### reset
**Description**: Restart the system
**Syntax**: `reset`
**Access**: Serial + MQTT + Telnet

### error
**Description**: Show error status
**Syntax**: `error`
**Access**: Serial + MQTT + Telnet

### test
**Description**: Run system tests
**Syntax**: `test`
**Access**: Serial + MQTT + Telnet

### loglevel
**Description**: Set logging level
**Syntax**: `loglevel <0-7>`
**Access**: Serial + MQTT + Telnet
**Levels**: 0=EMERGENCY, 1=ALERT, 2=CRITICAL, 3=ERROR, 4=WARNING, 5=NOTICE, 6=INFO, 7=DEBUG

## Network Commands

### network status
**Description**: Show detailed network status
**Syntax**: `network status`
**Access**: Serial + MQTT + Telnet

### network reconnect
**Description**: Reconnect WiFi connection
**Syntax**: `network reconnect`
**Access**: Serial + MQTT + Telnet

### network mqtt_reconnect
**Description**: Reconnect MQTT broker connection
**Syntax**: `network mqtt_reconnect`
**Access**: Serial + MQTT + Telnet

### network syslog_test
**Description**: Test syslog connectivity
**Syntax**: `network syslog_test`
**Access**: Serial + MQTT + Telnet

## Relay Control Commands

### relay
**Description**: Control relay outputs
**Syntax**: `relay R<n> ON|OFF`
**Parameters**:
- `R<n>`: Relay number (e.g., R1, R2, R3, R4)
- `ON|OFF`: Relay state
**Access**: Serial + MQTT + Telnet
**Examples**:
- `relay R1 ON`
- `relay R4 OFF`

## Pin Configuration Commands

### pins
**Description**: Show current pin configuration
**Syntax**: `pins`
**Access**: Serial + Telnet only (restricted from MQTT for security)

### pin debounce
**Description**: Configure input pin debouncing
**Syntax**: `pin <6|7> debounce <low|med|high>`
**Parameters**:
- `6|7`: Pin number
- `low|med|high`: Debounce sensitivity
**Access**: Serial + Telnet only
**Examples**:
- `pin 6 debounce med`
- `pin 7 debounce high`

## Configuration Commands

### set
**Description**: Live configuration changes
**Syntax**: `set <param> <value>`
**Access**: Serial + MQTT + Telnet

**Available Parameters**:
- `syslog <server[:port]>` - Reconfigure syslog server (applied immediately)
- `mqtt <broker[:port]>` - Reconfigure MQTT broker (applied immediately)

**Examples**:
- `set syslog 192.168.1.100:514`
- `set mqtt 192.168.1.155:1883`

## Timing Analysis Commands

### timing report
**Description**: Show subsystem performance analysis
**Syntax**: `timing report`
**Access**: Serial + MQTT + Telnet

### timing status
**Description**: Show timing health status
**Syntax**: `timing status`
**Access**: Serial + MQTT + Telnet

### timing slowest
**Description**: Show performance bottlenecks
**Syntax**: `timing slowest`
**Access**: Serial + MQTT + Telnet

### timing reset
**Description**: Reset timing statistics
**Syntax**: `timing reset`
**Access**: Serial + MQTT + Telnet

### timing log
**Description**: Enable/disable timing logging
**Syntax**: `timing log`
**Access**: Serial + MQTT + Telnet

## OTA Firmware Update Commands

### ota start
**Description**: Start OTA (Over-The-Air) update server
**Syntax**: `ota start`
**Access**: Serial + MQTT + Telnet
**Response**: Shows OTA server URL and status

### ota stop
**Description**: Stop OTA update server
**Syntax**: `ota stop`
**Access**: Serial + MQTT + Telnet

### ota status
**Description**: Show OTA server status
**Syntax**: `ota status`
**Access**: Serial + MQTT + Telnet

### ota url
**Description**: Show OTA update URL
**Syntax**: `ota url`
**Access**: Serial + MQTT + Telnet

### ota reboot
**Description**: Reboot system after OTA update
**Syntax**: `ota reboot`
**Access**: Serial + MQTT + Telnet

## Safety System Commands

### bypass
**Description**: Bypass safety interlocks (use with caution)
**Syntax**: `bypass`
**Access**: Serial + MQTT + Telnet
**Warning**: This command bypasses safety systems and should only be used for maintenance

## Logging Commands

### syslog
**Description**: Send test syslog message or configure syslog
**Syntax**: `syslog`
**Access**: Serial + MQTT + Telnet

### mqtt
**Description**: Send test MQTT message or show MQTT status
**Syntax**: `mqtt`
**Access**: Serial + MQTT + Telnet

## Common Usage Examples

### Quick System Check:
```
show             # Complete system status
network status   # Network connectivity
timing status    # Performance health
error           # Check for errors
```

### Relay Control:
```
relay R1 ON      # Turn on relay 1
relay R2 OFF     # Turn off relay 2
show            # Verify relay states
```

### Network Configuration:
```
set syslog 192.168.1.100:514     # Configure syslog server
set mqtt 192.168.1.155:1883      # Configure MQTT broker
network reconnect                # Reconnect with new settings
```

### Performance Analysis:
```
timing report    # Show performance metrics
timing slowest   # Identify bottlenecks
timing reset     # Reset statistics
```

### OTA Firmware Update:
```
ota start        # Start OTA server
ota url          # Get update URL
ota status       # Check update progress
ota reboot       # Reboot after update
```

### Debugging:
```
debug on         # Enable debug output
loglevel 7       # Set maximum logging
timing log       # Enable timing logs
show            # Get detailed status
```

## Access Control Summary

- **Serial + Telnet Only**: `pins`, pin configuration commands
- **All Interfaces**: Most commands including system control, relay operations, network management
- **MQTT Restrictions**: PIN configuration commands are restricted for security

## Device Information

- **Hardware**: Arduino-compatible controller
- **Communication**: Serial (115200 baud), Telnet (port 23), MQTT
- **Network**: WiFi with MQTT and Syslog support
- **Features**: Relay control, pressure monitoring, safety systems, timing analysis, OTA updates
- **Safety**: Built-in safety interlocks and bypass capabilities