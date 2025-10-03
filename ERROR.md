# LogSplitter Controller - Error Code Documentation

## Overview

This document provides comprehensive information about all system errors that can activate the mill lamp (System Error LED on Pin 9) in the LogSplitter Controller. Each error includes severity classification, root causes, source code locations, and troubleshooting guidance.

## Mill Lamp Behavior Summary

| LED Pattern | Error Count | Severity | Description |
|-------------|-------------|----------|-------------|
| **OFF** | 0 errors | Normal | No system errors detected |
| **SOLID ON** | 1 unacknowledged | Standard | Single error requiring attention |
| **SLOW BLINK** | Multiple or acknowledged | Standard/Warning | Multiple errors or acknowledged errors |
| **FAST BLINK** | Critical errors present | Critical | Hardware/memory/EEPROM failures |

## Error Type Classification

### 🔴 Critical Errors (Fast Blink)
These errors trigger immediate fast blinking and require urgent attention:

#### ERROR_EEPROM_CRC (0x01)
- **Severity**: CRITICAL
- **Mill Lamp**: Fast Blink (1Hz)
- **Log Level**: LOG_CRITICAL
- **Description**: EEPROM data integrity failure - stored configuration is corrupted
- **Causes**:
  - Power failure during EEPROM write operation
  - Memory corruption due to electrical interference
  - Hardware failure of EEPROM storage
  - Firmware bug corrupting stored data
- **Source Locations**:
  ```cpp
  File: src/config_manager.cpp
  Line: ~85 - bool ConfigManager::validateCRC32()
  Line: ~45 - bool ConfigManager::loadFromEEPROM()
  
  Error Generation:
  systemErrorManager.setError(ERROR_EEPROM_CRC, "EEPROM CRC validation failed");
  ```
- **Impact**: System cannot trust stored configuration, may use defaults
- **Resolution**: 
  - Reset configuration: `error clear 0x01`
  - Save new configuration: Use telnet commands to reconfigure
  - If persistent: Hardware replacement may be required

#### ERROR_MEMORY_LOW (0x20)
- **Severity**: CRITICAL  
- **Mill Lamp**: Fast Blink (1Hz)
- **Log Level**: LOG_CRITICAL
- **Description**: Available RAM has fallen below critical threshold
- **Causes**:
  - Memory leak in application code
  - Excessive buffer allocation
  - Network buffer overflow
  - String manipulation without proper cleanup
- **Source Locations**:
  ```cpp
  File: src/main.cpp
  Line: ~180 - void checkSystemHealth()
  Line: ~195 - if (ESP.getFreeHeap() < CRITICAL_MEMORY_THRESHOLD)
  
  Error Generation:
  systemErrorManager.setError(ERROR_MEMORY_LOW, "Critical memory shortage");
  ```
- **Impact**: System instability, potential crashes, network failures
- **Resolution**:
  - Restart system: `reset system`
  - Monitor memory usage: `status` command
  - If persistent: Firmware investigation required

#### ERROR_HARDWARE_FAULT (0x40)
- **Severity**: CRITICAL
- **Mill Lamp**: Fast Blink (1Hz) 
- **Log Level**: LOG_CRITICAL
- **Description**: General hardware malfunction detected
- **Causes**:
  - I2C bus failure
  - SPI communication errors
  - GPIO pin hardware failure
  - Power supply instability
  - External hardware not responding
- **Source Locations**:
  ```cpp
  File: src/relay_controller.cpp
  Line: ~125 - void RelayController::checkHardwareHealth()
  Line: ~140 - Hardware diagnostic failures
  
  File: src/pressure_sensor.cpp  
  Line: ~95 - void PressureSensor::checkSensorHealth()
  Line: ~110 - Sensor communication failures
  
  Error Generation:
  systemErrorManager.setError(ERROR_HARDWARE_FAULT, "Hardware diagnostic failure");
  ```
- **Impact**: Unreliable operation, potential safety issues
- **Resolution**:
  - Check all connections and wiring
  - Verify power supply stability
  - Test individual components
  - Hardware replacement if fault persists

### 🟡 Standard Errors (Solid/Slow Blink)
These errors trigger solid or slow blinking patterns:

#### ERROR_EEPROM_SAVE (0x02)
- **Severity**: ERROR
- **Mill Lamp**: Solid ON (single) / Slow Blink (multiple)
- **Log Level**: LOG_ERROR  
- **Description**: Failed to write configuration data to EEPROM
- **Causes**:
  - EEPROM write protection enabled
  - Power interruption during write
  - EEPROM wear/failure
  - Write operation timeout
- **Source Locations**:
  ```cpp
  File: src/config_manager.cpp
  Line: ~120 - bool ConfigManager::saveToEEPROM()
  Line: ~135 - EEPROM.commit() failure detection
  
  Error Generation:
  systemErrorManager.setError(ERROR_EEPROM_SAVE, "EEPROM save operation failed");
  ```
- **Impact**: Configuration changes not persisted, will revert on restart
- **Resolution**:
  - Retry save operation
  - Check EEPROM health
  - Reduce write frequency if excessive

#### ERROR_SENSOR_FAULT (0x04)  
- **Severity**: WARNING
- **Mill Lamp**: Solid ON (single) / Slow Blink (multiple)
- **Log Level**: LOG_WARNING
- **Description**: Pressure sensor malfunction or communication failure
- **Causes**:
  - Sensor disconnected or damaged
  - I2C communication failure
  - Sensor calibration drift
  - Power supply issues to sensor
  - Environmental interference
- **Source Locations**:
  ```cpp
  File: src/pressure_sensor.cpp
  Line: ~75 - bool PressureSensor::readPressure()
  Line: ~85 - I2C communication error detection
  Line: ~165 - void PressureSensor::checkSensorHealth()
  
  Error Generation:
  systemErrorManager.setError(ERROR_SENSOR_FAULT, "Pressure sensor malfunction");
  ```
- **Impact**: Pressure readings unavailable, safety systems may engage
- **Resolution**:
  - Check sensor wiring and connections
  - Verify sensor power supply
  - Recalibrate sensor if needed
  - Replace sensor if permanently failed

#### ERROR_NETWORK_PERSISTENT (0x08)
- **Severity**: WARNING
- **Mill Lamp**: Solid ON (single) / Slow Blink (multiple)  
- **Log Level**: LOG_WARNING
- **Description**: Network connection repeatedly failing despite retry attempts
- **Causes**:
  - WiFi access point unavailable
  - MQTT broker unreachable
  - Network credentials incorrect
  - Router/network infrastructure failure
  - Signal strength too low
- **Source Locations**:
  ```cpp
  File: src/network_manager.cpp
  Line: ~185 - void NetworkManager::checkConnectionHealth()
  Line: ~200 - Persistent failure detection after retries
  Line: ~220 - MQTT connection failure tracking
  
  Error Generation:
  systemErrorManager.setError(ERROR_NETWORK_PERSISTENT, 
                            "Network connection persistently failed");
  ```
- **Impact**: Remote monitoring unavailable, no MQTT/telnet access
- **Resolution**:
  - Check WiFi signal strength
  - Verify network credentials
  - Check router/access point status
  - Test MQTT broker connectivity

#### ERROR_CONFIG_INVALID (0x10)
- **Severity**: WARNING
- **Mill Lamp**: Solid ON (single) / Slow Blink (multiple)
- **Log Level**: LOG_WARNING  
- **Description**: Configuration parameters are outside valid ranges
- **Causes**:
  - Manual configuration with invalid values
  - Configuration corruption (partial)
  - Firmware version mismatch
  - User input validation failure
- **Source Locations**:
  ```cpp
  File: src/config_manager.cpp
  Line: ~65 - bool ConfigManager::validateConfiguration()
  Line: ~70 - Parameter range validation
  
  File: src/command_processor.cpp
  Line: ~280 - Configuration command validation
  Line: ~295 - Parameter bounds checking
  
  Error Generation:
  systemErrorManager.setError(ERROR_CONFIG_INVALID, "Configuration parameters invalid");
  ```
- **Impact**: System using default values, some features may be disabled
- **Resolution**:
  - Review configuration via `status` command
  - Reconfigure invalid parameters
  - Reset to defaults if necessary

## Error Status Commands

### Viewing Error Status
```bash
# Show all active errors
> error list
0x04:Pressure sensor malfunction, 0x08:(ACK)Network connection persistently failed

# Show error summary with LED state  
> error status
Errors: 2 active (1 unacked), uptime: 1547s, LED: SLOW

# Show complete system status
> status
System Status: RUNNING
Active Errors: 2 (ERROR_SENSOR_FAULT, ERROR_NETWORK_PERSISTENT)
Mill Lamp: SLOW_BLINK
Uptime: 25m 47s
```

### Error Management Commands
```bash
# Acknowledge specific error (changes LED pattern)
> error ack 0x04
ERROR 0x04 acknowledged

# Clear resolved error  
> error clear 0x04
ERROR 0x04 cleared

# Clear all errors (use with caution)
> error clear all
All errors cleared

# Get detailed error description
> error describe 0x08
ERROR_NETWORK_PERSISTENT (0x08): Network connection persistently failed
Severity: WARNING, LED: Contributes to SLOW_BLINK pattern
```

## Error Log Integration

### Syslog Messages
All errors are automatically logged to the central syslog server with RFC 3164 compliance:

```
<130>Oct  3 15:42:10 LogSplitter logsplitter: SystemErrorManager: ERROR 0x04 - Pressure sensor malfunction
<132>Oct  3 15:43:15 LogSplitter logsplitter: SystemErrorManager: ERROR 0x08 - Network connection persistently failed  
<130>Oct  3 15:44:20 LogSplitter logsplitter: SystemErrorManager: ERROR 0x01 - EEPROM CRC validation failed
```

### MQTT Error Publishing
```
Topic: r4/system/error
Payload: 0x04: Pressure sensor malfunction

Topic: r4/system/error_count  
Payload: 2
```

## Error Severity Mapping

| Error Code | Error Name | Severity | Log Level | Syslog Priority |
|------------|------------|----------|-----------|-----------------|
| 0x01 | ERROR_EEPROM_CRC | CRITICAL | LOG_CRITICAL | 130 |
| 0x02 | ERROR_EEPROM_SAVE | ERROR | LOG_ERROR | 131 |
| 0x04 | ERROR_SENSOR_FAULT | WARNING | LOG_WARNING | 132 |
| 0x08 | ERROR_NETWORK_PERSISTENT | WARNING | LOG_WARNING | 132 |
| 0x10 | ERROR_CONFIG_INVALID | WARNING | LOG_WARNING | 132 |
| 0x20 | ERROR_MEMORY_LOW | CRITICAL | LOG_CRITICAL | 130 |
| 0x40 | ERROR_HARDWARE_FAULT | CRITICAL | LOG_CRITICAL | 130 |

## Mill Lamp Logic Implementation

### Source Code Location
```cpp
File: src/system_error_manager.cpp
Line: ~95 - ErrorLedPattern SystemErrorManager::getLedPattern() const

Decision Logic:
1. No errors (0x00): LED_OFF
2. Critical errors present: LED_FAST_BLINK  
3. Multiple unacknowledged: LED_SLOW_BLINK
4. Single unacknowledged: LED_SOLID
5. All acknowledged: LED_SLOW_BLINK
```

### LED Update Frequency
```cpp
File: src/main.cpp
Line: ~160 - void updateSystem()
Line: ~170 - systemErrorManager.updateLED()

Update Rate: Every 100ms in main loop
LED Timing: Hardware timer-based patterns
```

## Troubleshooting Quick Reference

### Fast Blinking Mill Lamp
1. Check system memory: `status` → look for free memory
2. Verify EEPROM: `error list` → look for 0x01
3. Test hardware: Use built-in diagnostics
4. **Immediate Action**: Consider system restart for critical errors

### Solid Mill Lamp  
1. Single error present: `error list` → identify specific error
2. Address root cause based on error type
3. Acknowledge when fixed: `error ack 0x##`

### Slow Blinking Mill Lamp
1. Multiple errors or acknowledged errors
2. Review error list: `error list`
3. Address each error systematically
4. Clear resolved errors: `error clear 0x##`

### No Mill Lamp Activity
- Normal operation (preferred state)
- Use `error status` to confirm no active errors
- Mill lamp should remain OFF during normal operation

## Error Prevention

### Regular Maintenance
- Monitor memory usage trends
- Check EEPROM health periodically  
- Verify sensor calibration
- Maintain stable power supply
- Keep firmware updated

### Network Reliability
- Ensure strong WiFi signal
- Monitor MQTT broker health
- Use uninterruptible power supply
- Implement network redundancy if critical

### Hardware Health
- Regular visual inspections
- Cable and connection checks
- Environmental monitoring (temperature, humidity)
- Preventive component replacement

---

**Mill Lamp Hardware**: Pin 9, Active HIGH, External LED recommended for visibility  
**Error Storage**: Persistent across reboots until cleared  
**Safety Integration**: Independent of critical safety systems  
**Update Frequency**: 100ms LED pattern updates, real-time error detection