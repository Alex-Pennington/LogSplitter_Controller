# LogSplitter Controller - Syslog Logging System

## Overview

The LogSplitter Controller implements a comprehensive RFC 3164 compliant syslog logging system that provides structured, severity-based logging to a centralized rsyslog server. This system enables real-time monitoring, debugging, and operational oversight of the industrial control system.

## Architecture

### Core Components

- **Logger Class** (`src/logger.cpp`, `include/logger.h`) - Central logging management
- **NetworkManager Integration** - Handles UDP syslog transmission
- **Severity-Based Filtering** - Configurable message filtering by importance
- **Fallback Mechanisms** - Serial output when syslog unavailable

### RFC 3164 Compliance

The system implements standard syslog protocols:

- **Facility**: Local0 (16) for custom application logs
- **Format**: `<priority>timestamp hostname tag: message`
- **Transport**: UDP over port 514
- **Priority Calculation**: `(facility * 8) + severity`

## Log Levels

The system supports 5 primary log levels based on RFC 3164 severity codes:

| Level | Severity | Usage | Examples |
|-------|----------|-------|----------|
| **CRITICAL** | 2 | Safety issues, hardware failures | Safety system activation, relay failures |
| **ERROR** | 3 | Error conditions, failures | Communication timeouts, sensor errors |
| **WARNING** | 4 | Performance issues, delays | Slow MQTT operations, network delays |
| **INFO** | 6 | Normal operations | System initialization, connections |
| **DEBUG** | 7 | Detailed debugging | State changes, pin transitions |

## API Usage

### Basic Logging Functions

```cpp
#include "logger.h"

// Convenience macros for easy logging
LOG_CRITICAL("SAFETY ACTIVATED: %s (pressure=%.1f PSI)", reason, pressure);
LOG_ERROR("Relay R%d command failed after %d retries", relayNumber, retries);
LOG_WARN("MQTT connect took %lums", duration);
LOG_INFO("RelayController initialized");
LOG_DEBUG("Pin %d: %s -> %s", pin, oldState, newState);
```

### Advanced Logging

```cpp
// Direct logger class usage
Logger::log(LOG_ERROR, "Custom error message: %d", errorCode);
Logger::setLogLevel(LOG_WARNING);  // Only show WARN and above
LogLevel current = Logger::getLogLevel();
```

### Backward Compatibility

Existing `debugPrintf()` calls continue to work and are automatically routed to `LOG_DEBUG` level:

```cpp
debugPrintf("Legacy debug message\n");  // -> LOG_DEBUG
```

## Configuration

### Runtime Configuration

Configure log levels via serial/telnet/MQTT commands:

```bash
# Direct loglevel commands (numeric 0-7)
loglevel                    # Show current log level
loglevel get                # Get current log level
loglevel list               # List all available log levels
loglevel 0                  # Set to EMERGENCY level
loglevel 1                  # Set to ALERT level
loglevel 2                  # Set to CRITICAL level
loglevel 3                  # Set to ERROR level
loglevel 4                  # Set to WARNING level
loglevel 5                  # Set to NOTICE level
loglevel 6                  # Set to INFO level (normal operation)
loglevel 7                  # Set to DEBUG level (development)

# Legacy set commands (still supported)
set loglevel 0              # Set log level using numeric value
set loglevel 7              # Set to maximum debug level

# Configure syslog server
set syslog 192.168.1.238       # Default port 514
set syslog 192.168.1.238:1514  # Custom port

# Test syslog connectivity
syslog test
syslog status
```

### Default Configuration

- **Default Server**: `192.168.1.238:514`
- **Default Level**: `LOG_INFO`
- **Hostname**: `LogSplitter`
- **Application Tag**: `logsplitter`

## Network Integration

### Syslog Server Setup

The system sends logs to a centralized rsyslog server. Example rsyslog configuration:

```bash
# /etc/rsyslog.d/50-logsplitter.conf
# Listen on UDP port 514
$ModLoad imudp
$UDPServerRun 514

# LogSplitter specific logging
:hostname, isequal, "LogSplitter" /var/log/logsplitter.log
& stop
```

### Message Format

Syslog messages follow RFC 3164 format:

```text
<134>Jan  1 00:15:23 LogSplitter logsplitter: SAFETY ACTIVATED: pressure_exceeded (pressure=2100.0 PSI)
```

Where:

- `<134>` = Priority (facility 16 * 8 + severity 6 = 134)
- `Jan  1 00:15:23` = Simplified timestamp
- `LogSplitter` = Hostname
- `logsplitter` = Application tag
- Message content follows

## Implementation Details

### System Integration

The logger is initialized during system startup:

```cpp
void initializeSystem() {
    // ... network initialization ...
    if (networkManager.begin()) {
        // Initialize logger after network is available
        Logger::begin(&networkManager);
        Logger::setLogLevel(LOG_DEBUG);
        LOG_INFO("Logger initialized with network manager");
    }
}
```

### Memory Management

- **Shared Buffer**: 512-byte static buffer for message formatting
- **Zero Allocation**: No dynamic memory allocation in logging path
- **Thread Safe**: Atomic operations for log level changes

### Fallback Behavior

When syslog transmission fails:

- CRITICAL and ERROR messages always appear on Serial
- Fallback messages include `[SYSLOG_FAIL]` prefix
- Timestamp and severity level included for debugging

## Monitoring and Debugging

### Log Level Effects

| Log Level | Messages Sent | Use Case |
|-----------|---------------|----------|
| **CRITICAL** | ~2/day | Emergency monitoring only |
| **ERROR** | ~5-10/day | Production error tracking |
| **WARNING** | ~20-50/day | Performance monitoring |
| **INFO** | ~100-200/day | Operational oversight |
| **DEBUG** | ~1000+/day | Development debugging |

### Performance Impact

- **Network Overhead**: ~50-200 bytes per message
- **CPU Impact**: Minimal (~1-2% at DEBUG level)
- **Memory Usage**: 512 bytes static buffer
- **Filtering**: Messages below threshold are discarded before network transmission

## Migration Guide

### From debugPrintf()

Replace existing debug calls with appropriate severity:

```cpp
// OLD
debugPrintf("CRITICAL: Safety activated\n");
debugPrintf("Relay initialized\n");
debugPrintf("WARNING: Slow response\n");

// NEW
LOG_CRITICAL("Safety activated");
LOG_INFO("Relay initialized");
LOG_WARN("Slow response");
```

### Categorization Guidelines

- **CRITICAL**: Safety shutdowns, hardware failures
- **ERROR**: Communication failures, sensor errors, timeouts
- **WARNING**: Performance degradation, resource limits
- **INFO**: System lifecycle events, connections
- **DEBUG**: State changes, detailed flow control

## Troubleshooting

### Common Issues

1. **No syslog messages received**:
   - Check network connectivity: `network`
   - Test syslog: `syslog test`
   - Verify server: `syslog status`

2. **Too many/few messages**:
   - Adjust log level: `set loglevel info`
   - Check current level: `debug` (shows both debug flag and log level)

3. **Messages only on Serial**:
   - Network disconnected - syslog falls back to Serial
   - Look for `[SYSLOG_FAIL]` prefix

### Debug Commands

```bash
debug                    # Show current debug status
syslog status           # Show syslog server configuration
syslog test             # Send test message
network                 # Check network connectivity
```

## Security Considerations

- **No Authentication**: Standard syslog UDP (RFC 3164) has no authentication
- **Network Exposure**: Messages sent in plain text over UDP
- **Rate Limiting**: No built-in rate limiting (implement at rsyslog server)
- **Buffer Overflow**: Fixed-size buffers prevent message truncation

## Future Enhancements

- **TLS Support**: Encrypted syslog transmission (RFC 5425)
- **Rate Limiting**: Client-side message throttling
- **Log Rotation**: Local storage with rotation
- **Structured Logging**: JSON format support
- **Multiple Servers**: Redundant syslog destinations

---

*This logging system provides enterprise-grade operational visibility for the LogSplitter Controller while maintaining embedded system efficiency and reliability.*
