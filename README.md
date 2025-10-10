# LogSplitter Industrial Control System

A comprehensive Arduino-based control system for industrial log splitting operations, featuring integrated safety systems, sensor monitoring, LCD display, and remote management capabilities.

## 🏗️ System Architecture

The LogSplitter system is a unified Arduino UNO R4 WiFi controller that combines:

- **Industrial Control** - Main control unit with safety systems and relay management
- **Integrated Monitoring** - Built-in sensor monitoring with 20x4 LCD display and comprehensive data visualization
- **Network Integration** - WiFi connectivity with MQTT communication and telnet administration

### Key Features:
- **Comprehensive Control**: Industrial relay control with integrated safety monitoring
- **Real-time Display**: 4-line LCD showing sensor data, system status, and network connectivity
- **Unified Architecture**: Single controller handles both control operations and sensor monitoring
- **Network Management**: MQTT communication, syslog logging, and telnet administration

## ⚡ Quick Start

### 1. Controller Setup

```bash
# Build and upload the unified controller
pio run --target upload
```

See [README_REFACTORED.md](README_REFACTORED.md) for detailed controller setup.

### 2. System Integration

See [docs/DEPLOYMENT_GUIDE.md](docs/DEPLOYMENT_GUIDE.md) for complete system deployment.

## 🔧 Core Features

### Unified Logging System
- **RFC 3164 Compliant Syslog** - Enterprise-grade logging to centralized server
- **8-Level Severity** - EMERGENCY (0) to DEBUG (7) with proper prioritization
- **Dynamic Log Levels** - Runtime adjustable via `loglevel 0-7` commands
- **Intelligent Filtering** - Reduces noise while preserving critical information

### Network Integration
- **WiFi Connectivity** - Automatic connection with health monitoring
- **MQTT Communication** - Real-time pub/sub messaging between units
- **Telnet Administration** - Remote command-line access on port 23
- **Syslog Transmission** - UDP logging to 192.168.1.238:514

### Safety & Monitoring
- **Pressure Safety Systems** - Multi-threshold monitoring with emergency stops
- **Hardware Watchdog** - Automatic recovery from system faults
- **Sensor Integration** - Temperature, pressure, and weight monitoring
- **LCD Status Display** - Real-time system information

## 🏭 Industrial Applications

### Primary Use Case: Log Splitting Operations
- **Hydraulic Control** - Precision relay management for log splitting machinery with unified safety monitoring for both automatic sequences and manual operations
- **Safety Monitoring** - Pressure threshold enforcement, limit switch protection, and emergency stops
- **Remote Oversight** - Real-time monitoring and control via MQTT
- **Operational Logging** - Complete audit trail of all operations

### System Benefits
- **Reliability** - Industrial-grade Arduino R4 WiFi platform
- **Scalability** - Distributed architecture supports multiple units
- **Maintainability** - Comprehensive logging and remote diagnostics
- **Safety** - Multiple redundant safety systems and monitoring

## 📊 Component Overview

| Component | Purpose | Key Features |
|-----------|---------|--------------|
| **Controller** | Main control logic | Relay control, safety systems, sequence management |
| **Monitor** | Remote monitoring | Sensor readings, LCD display, weight measurement |
| **Shared Logger** | Unified logging | RFC 3164 syslog, dynamic levels, network transmission |
| **Network Stack** | Connectivity | WiFi management, MQTT pub/sub, telnet servers |

## 🌐 Network Architecture

```
┌─────────────────┐    ┌─────────────────┐
│   Controller    │    │     Monitor     │
│   (Main Unit)   │    │  (Remote Unit)  │
├─────────────────┤    ├─────────────────┤
│ • Relay Control │    │ • Sensor Data   │
│ • Safety Logic  │    │ • LCD Display   │
│ • Sequence Mgmt │    │ • Weight Scale  │
│ • Telnet :23    │    │ • Telnet :23    │
└─────────┬───────┘    └─────────┬───────┘
          │                      │
          └──────────┬───────────┘
                     │
            ┌────────▼────────┐
            │  Network Infra  │
            ├─────────────────┤
            │ • WiFi Router   │
            │ • MQTT Broker   │
            │ • Syslog Server │
            └─────────────────┘
```

## 🚀 Getting Started

### Prerequisites
- **PlatformIO** - Arduino development environment
- **Arduino UNO R4 WiFi** - Two units required
- **Network Infrastructure** - WiFi router, MQTT broker, syslog server

### Installation Steps

1. **Clone Repository**
   ```bash
   git clone <repository-url>
   cd LogSplitter_Controller
   ```

2. **Configure Secrets**
   ```bash
   # Controller (root directory)
   cp include/arduino_secrets.h.template include/arduino_secrets.h
   # Monitor  
   cp monitor/include/arduino_secrets.h.template monitor/include/arduino_secrets.h
   # Edit both files with your WiFi/MQTT credentials
   ```

3. **Build and Deploy**
   ```bash
   # Build controller (from root)
   pio run --target upload
   # Build monitor
   cd monitor && pio run --target upload
   ```

4. **Verify Operation**
   ```bash
   # Check serial output
   pio device monitor
   # Test telnet access
   telnet <controller-ip> 23
   telnet <monitor-ip> 23
   ```

## 📋 Command Interface

Both units support identical command interfaces with unified logging control:

### System Commands
```bash
help                    # Show available commands
show                    # Display current status
status                  # Detailed system information
network                 # Network health and connectivity
```

### Logging Commands
```bash
loglevel                # Show current log level
loglevel 0              # EMERGENCY - System unusable
loglevel 1              # ALERT - Immediate action required
loglevel 2              # CRITICAL - Critical conditions
loglevel 3              # ERROR - Error conditions  
loglevel 4              # WARNING - Warning conditions
loglevel 5              # NOTICE - Normal but significant
loglevel 6              # INFO - Informational (default)
loglevel 7              # DEBUG - Debug-level messages
```

### Testing Commands
```bash
test network            # Test network connectivity
test sensors            # Test sensor readings
syslog test             # Send test message to syslog server
syslog status           # Show syslog configuration
mqtt test               # Send test message to MQTT broker
mqtt status             # Show MQTT broker configuration
```

### Configuration Commands
```bash
set syslog <ip>         # Set syslog server IP
set mqtt <host>         # Set MQTT broker host
```

## 📚 Documentation

### Component Documentation
- **[Controller](README_REFACTORED.md)** - Detailed controller setup and operation (root directory)
- **[Monitor](monitor/README.md)** - Monitor system configuration with LCD and heartbeat animation
- **[Unified Architecture](docs/UNIFIED_ARCHITECTURE.md)** - Complete system architecture and coordination
- **[Logging System](docs/LOGGING_SYSTEM.md)** - Unified logging architecture
- **[Deployment Guide](docs/DEPLOYMENT_GUIDE.md)** - Complete system deployment

### Technical Documentation
- **[System Architecture](docs/SYSTEM_ARCHITECTURE.md)** - Technical system overview
- **[Network Integration](docs/NETWORK_INTEGRATION.md)** - MQTT and syslog setup
- **[Safety Systems](COMPREHENSIVE_REVIEW.md)** - Safety system details (root directory)
- **[Command Reference](COMMANDS.md)** - Complete command documentation

## 🔧 Development

### Building
```bash
# Build both components
make build-all

# Build individual components
pio run              # Controller (from root directory)
cd monitor && pio run  # Monitor
```

### Testing
```bash
# Run system tests
make test

# Test individual components
pio test              # Controller (from root directory)
cd monitor && pio test  # Monitor
```

### Debugging
```bash
# Monitor serial output
pio device monitor

# Enable debug logging
telnet <device-ip> 23
> loglevel 7
```

## 🔒 Security Considerations

- **Network Security** - Consider VPN for remote access in production
- **Authentication** - Telnet connections are unencrypted (secure network required)
- **Input Validation** - All commands undergo strict validation
- **Rate Limiting** - Built-in protection against command flooding

## 📈 Monitoring & Maintenance

### System Health
- **Uptime Tracking** - Both units report operational uptime
- **Connection Monitoring** - WiFi and MQTT health tracking
- **Error Reporting** - Comprehensive error logging and alerting
- **Performance Metrics** - Memory usage and response time monitoring

### Maintenance Tasks
- **Log Rotation** - Configure syslog server for log management
- **Firmware Updates** - OTA updates via PlatformIO
- **Configuration Backup** - EEPROM settings preservation
- **Hardware Monitoring** - Temperature and voltage monitoring

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## ⚠️ Safety Notice

This system controls industrial equipment. Always follow proper safety procedures:

- **Test thoroughly** before production deployment
- **Implement proper safety interlocks** for your specific application
- **Regular maintenance** of all safety systems
- **Emergency stop procedures** must be clearly documented and tested

---

**Version**: 2.0.0  
**Platform**: Arduino UNO R4 WiFi  
**Last Updated**: October 2025