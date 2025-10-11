# LogSplitter Unified Logging System

## Overview

The LogSplitter system implements a comprehensive, RFC 3164 compliant logging infrastructure that provides enterprise-grade operational visibility across both Controller and Monitor units. This unified approach ensures consistent log formatting, centralized collection, and intelligent severity-based filtering.

## Architecture

### Core Components

The logging system consists of several integrated components:

- **Shared Logger Class** (`shared/logger/`) - Centralized logging management
- **NetworkManager Integration** - UDP syslog transmission with RFC 3164 compliance
- **Dynamic Severity Control** - Runtime adjustable log levels (0-7)
- **Intelligent Fallback** - Serial output when network unavailable
- **Unified Command Interface** - Consistent `loglevel` commands across both units

### System Topology

```
┌─────────────────┐    ┌─────────────────┐
│   Controller    │    │     Monitor     │
│                 │    │                 │
│ ┌─────────────┐ │    │ ┌─────────────┐ │
│ │   Logger    │ │    │ │   Logger    │ │
│ │   Class     │ │    │ │   Class     │ │
│ └─────┬───────┘ │    │ └─────┬───────┘ │
│       │         │    │       │         │
│ ┌─────▼───────┐ │    │ ┌─────▼───────┐ │
│ │ NetworkMgr  │ │    │ │ NetworkMgr  │ │
│ │ (UDP Client)│ │    │ │ (UDP Client)│ │
│ └─────┬───────┘ │    │ └─────┬───────┘ │
└───────┼─────────┘    └───────┼─────────┘
        │                      │
        └──────────┬───────────┘
                   │
        ┌──────────▼──────────┐
        │   Syslog Server     │
        │  (192.168.1.155)    │
        │     Port 514        │
        └─────────────────────┘
```

## RFC 3164 Compliance

### Message Format

All syslog messages follow the RFC 3164 standard format:

```
<PRI>TIMESTAMP HOSTNAME TAG: MESSAGE
```

**Example Messages:**
```
<131>Oct  1 12:34:56 LogSplitter logsplitter: System initialized successfully
<134>Oct  1 12:35:01 LogMonitor logmonitor: Temperature reading: 23.5C
<131>Oct  1 12:35:05 LogSplitter logsplitter: Relay R1 activated
<133>Oct  1 12:35:10 LogSplitter logsplitter: Pressure threshold warning: 2800 PSI
```

### Priority Calculation

Priority (PRI) is calculated using the standard formula:

```
Priority = Facility × 8 + Severity
```

**LogSplitter Configuration:**
- **Facility**: 16 (local0)
- **Severity**: 0-7 (EMERGENCY to DEBUG)

**Priority Examples:**
- `<128>` = Emergency (16 × 8 + 0 = 128)
- `<131>` = Error (16 × 8 + 3 = 131)
- `<134>` = Info (16 × 8 + 6 = 134)
- `<135>` = Debug (16 × 8 + 7 = 135)

## Severity Levels

The system implements all 8 RFC 3164 severity levels:

| Level | Value | Name | Usage | Examples |
|-------|-------|------|-------|----------|
| **EMERGENCY** | 0 | System unusable | Critical hardware failures | Power loss, CPU fault |
| **ALERT** | 1 | Immediate action required | Safety system activation | Emergency stop engaged |
| **CRITICAL** | 2 | Critical conditions | Hardware malfunctions | Sensor failure, relay fault |
| **ERROR** | 3 | Error conditions | Operational failures | MQTT timeout, I2C error |
| **WARNING** | 4 | Warning conditions | Performance issues | High pressure, slow network |
| **NOTICE** | 5 | Normal but significant | Important events | System startup, configuration |
| **INFO** | 6 | Informational | Normal operations | Status updates, connections |
| **DEBUG** | 7 | Debug-level messages | Detailed debugging | State changes, pin transitions |

### Level Selection Guidelines

**Production Environment (INFO level 6):**
- Normal operational oversight
- ~100-200 messages per day
- Focus on system lifecycle and errors

**Development Environment (DEBUG level 7):**
- Detailed troubleshooting information
- ~1000+ messages per day
- Complete system activity logging

**Emergency Monitoring (ERROR level 3):**
- Critical issues only
- ~5-10 messages per day
- Focus on failures requiring attention

## API Usage

### Include Headers

```cpp
#include "shared/logger/logger.h"
```

### Basic Logging

```cpp
// Convenience macros (recommended)
LOG_CRITICAL("Safety system activated: %s", reason);
LOG_ERROR("Relay R%d failed after %d retries", relayNum, retries);
LOG_WARN("Network latency high: %lums", latency);
LOG_INFO("System initialized successfully");
LOG_DEBUG("Pin %d state: %s -> %s", pin, oldState, newState);
```

### Advanced Usage

```cpp
// Direct logger class usage
Logger::setLogLevel(LOG_WARNING);  // Set minimum level
LogLevel current = Logger::getLogLevel();  // Get current level
Logger::log(LOG_ERROR, "Custom message: %d", errorCode);  // Direct logging
```

### Initialization

```cpp
void setup() {
    // Initialize network first
    if (networkManager.begin()) {
        // Initialize logger with network manager
        Logger::begin(&networkManager);
        Logger::setLogLevel(LOG_INFO);  // Set default level
        LOG_INFO("Logger initialized with syslog server");
    }
}
```

## Command Interface

Both Controller and Monitor support identical logging commands:

### Direct Commands

```bash
# Show current log level
loglevel
loglevel get

# List all available levels  
loglevel list

# Set specific levels
loglevel 0              # EMERGENCY
loglevel 1              # ALERT  
loglevel 2              # CRITICAL
loglevel 3              # ERROR
loglevel 4              # WARNING
loglevel 5              # NOTICE
loglevel 6              # INFO (recommended for production)
loglevel 7              # DEBUG (development/troubleshooting)
```

### Legacy Support (Controller)

```bash
# Backward compatible commands
set loglevel 0-7        # Numeric interface
```

### Testing Commands

```bash
# Test syslog connectivity
syslog test             # Send test message
syslog status           # Show configuration

# Network diagnostics
network                 # Show connection health
```

## Network Integration

### Syslog Server Configuration

**Rsyslog Setup (Ubuntu/CentOS):**

```bash
# Enable UDP reception
sudo nano /etc/rsyslog.conf
```

```conf
# Add to rsyslog.conf
$ModLoad imudp
$UDPServerRun 514
$UDPServerAddress 0.0.0.0

# LogSplitter specific logging
:hostname, isequal, "LogSplitter" /var/log/logsplitter-controller.log
:hostname, isequal, "LogMonitor" /var/log/logsplitter-monitor.log
& stop
```

```bash
# Restart rsyslog
sudo systemctl restart rsyslog
```

### Message Routing

**By Priority:**
```conf
# Emergency and Alert messages to separate file
*.emerg;*.alert /var/log/emergency.log

# Error and warning messages  
*.err;*.warning /var/log/errors.log

# Informational messages
*.info /var/log/info.log
```

**By Hostname:**
```conf
# Controller messages
:hostname, isequal, "LogSplitter" /var/log/controller.log

# Monitor messages  
:hostname, isequal, "LogMonitor" /var/log/monitor.log
```

## Performance and Tuning

### Message Volume by Level

| Log Level | Daily Messages | Network Overhead | Use Case |
|-----------|----------------|------------------|----------|
| EMERGENCY (0) | 0-1 | ~50 bytes | Production alerts |
| ALERT (1) | 0-2 | ~100 bytes | Safety monitoring |
| CRITICAL (2) | 1-5 | ~250 bytes | Hardware monitoring |
| ERROR (3) | 5-20 | ~1 KB | Error tracking |
| WARNING (4) | 20-100 | ~5 KB | Performance monitoring |
| NOTICE (5) | 50-200 | ~10 KB | Event tracking |
| INFO (6) | 100-500 | ~25 KB | Operational oversight |
| DEBUG (7) | 1000+ | ~100+ KB | Development debugging |

### Memory Usage

**Static Allocations:**
- **Log Buffer**: 512 bytes per unit
- **Network Buffer**: Shared with other operations
- **Zero Dynamic Allocation**: No malloc/free in logging path

**Performance Impact:**
- **CPU Overhead**: <1% at INFO level, ~2% at DEBUG level
- **Network Latency**: ~10-50ms per message (depending on network)
- **Memory Footprint**: ~1KB total logging infrastructure

### Optimization Recommendations

**Production Environment:**
```cpp
Logger::setLogLevel(LOG_INFO);  // Balance visibility and performance
```

**Development Environment:**
```cpp
Logger::setLogLevel(LOG_DEBUG);  // Maximum visibility
```

**Emergency Monitoring:**
```cpp
Logger::setLogLevel(LOG_ERROR);  // Critical issues only
```

## Fallback Mechanisms

### Network Failure Handling

When syslog transmission fails:

1. **Critical/Error Messages**: Always output to Serial console
2. **Retry Logic**: Automatic retry on next message
3. **Fallback Indicator**: `[SYSLOG_FAIL]` prefix on Serial output
4. **Network Recovery**: Automatic resumption when network restored

**Example Fallback Output:**
```
[12345] [ERROR] [SYSLOG_FAIL] Relay R2 command timeout
[12678] [CRITICAL] [SYSLOG_FAIL] Safety system activated
```

### Serial Console Output

Serial output includes:
- **Timestamp**: Milliseconds since boot
- **Severity**: Human-readable level string
- **Failure Indicator**: When syslog unavailable
- **Message**: Original log message

## Troubleshooting

### Common Issues

**No Syslog Messages Received:**
```bash
# Check network connectivity
telnet <device-ip> 23
> network

# Test syslog directly
> syslog test

# Verify server configuration
> syslog status
```

**Too Many/Few Messages:**
```bash
# Adjust log level
> loglevel 6          # Set to INFO level
> loglevel get        # Verify current level
```

**Messages Only on Serial:**
```bash
# Network connectivity issue
> network             # Check WiFi/MQTT status

# Look for [SYSLOG_FAIL] prefix in Serial output
```

### Debug Commands

```bash
# Show current logging status
loglevel get

# List all available levels
loglevel list

# Test network connectivity
network

# Send test syslog message
syslog test

# Show syslog server configuration
syslog status

# Enable debug logging
loglevel 7
```

### Log Analysis

**Grep Examples:**
```bash
# Find all error messages
grep -E "<13[0-9]>" /var/log/logsplitter-*.log

# Find messages from specific device
grep "LogSplitter" /var/log/logsplitter-controller.log

# Find high-priority messages (0-3)
grep -E "<12[8-9]>|<13[0-3]>" /var/log/logsplitter-*.log
```

## Security Considerations

### Network Security

- **Encryption**: RFC 3164 UDP is unencrypted (use VPN for sensitive environments)
- **Authentication**: No built-in authentication (implement at network level)
- **Filtering**: Configure firewall to restrict UDP 514 access

### Message Content

- **Sensitive Data**: Avoid logging passwords or API keys
- **Data Classification**: Consider message sensitivity when setting log levels
- **Retention**: Configure appropriate log rotation and retention policies

## Migration Guide

### From debugPrintf()

**Old Code:**
```cpp
debugPrintf("System starting\n");
debugPrintf("ERROR: Sensor failed\n");
debugPrintf("WARNING: High pressure\n");
```

**New Code:**
```cpp
LOG_INFO("System starting");
LOG_ERROR("Sensor failed");
LOG_WARN("High pressure");
```

### Benefits of Migration

- **Structured Logging**: Consistent format across all messages
- **Remote Collection**: Centralized log aggregation
- **Severity Filtering**: Runtime adjustable verbosity
- **Performance**: Filtered messages don't consume network bandwidth
- **Production Ready**: Enterprise-grade logging infrastructure

---

This unified logging system provides the foundation for reliable, scalable monitoring and debugging of the LogSplitter industrial control system.