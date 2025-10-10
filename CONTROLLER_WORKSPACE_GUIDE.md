# LogSplitter Controller Workspace Setup

## Overview
This workspace is configured for **controller-only development** of the LogSplitter system. The monitor system will be in a separate workspace when needed.

## Quick Start

### 1. Build and Deploy Controller
```bash
# Complete deployment workflow
make deploy

# Or step-by-step
make build    # Compile controller
make upload   # Upload to Arduino Uno R4 WiFi  
make monitor  # Start serial monitor for debugging
```

### 2. VS Code Tasks
Use **Ctrl+Shift+P** → **Tasks: Run Task** to access:
- 🔧 **Build Controller Program** - Compile controller code
- 📤 **Upload Controller Program** - Upload firmware to Arduino
- 🔍 **Monitor Controller Serial** - Debug with serial output
- 🧹 **Clean Build** - Clean build artifacts  
- 🔍 **Check Code** - Static code analysis
- 🏗️ **Build & Upload Controller** - Complete build and upload

### 3. Development Workflow
```bash
# Daily development cycle
make check        # Check code quality
make build        # Build controller
make debug        # Upload and monitor immediately

# Configuration management  
make secrets      # Setup arduino_secrets.h from template
make backup       # Backup current configuration
make restore      # List available backups
```

## Project Structure

### Controller-Specific Files
```
LogSplitter_Controller/
├── src/                    # Controller source code
│   ├── main.cpp           # Main controller program
│   ├── network_manager.cpp
│   ├── pressure_manager.cpp
│   ├── relay_controller.cpp
│   ├── safety_system.cpp
│   └── sequence_controller.cpp
├── include/               # Controller headers
│   ├── arduino_secrets.h  # WiFi and server configuration
│   ├── network_manager.h
│   ├── pressure_manager.h
│   └── relay_controller.h
├── platformio.ini         # Controller build configuration
└── .vscode/tasks.json     # Controller development tasks
```

### Key Configuration Files
- `platformio.ini` - Build configuration for Arduino Uno R4 WiFi
- `include/arduino_secrets.h` - WiFi credentials and server settings
- `.vscode/tasks.json` - VS Code tasks for controller development
- `Makefile` - Command-line build system

## Arduino Uno R4 WiFi Configuration

### Hardware Setup
- **Board**: Arduino Uno R4 WiFi
- **USB Connection**: Required for programming and serial monitoring
- **Power**: USB or external 7-12V supply
- **WiFi**: Built-in WiFi module for network communication

### Network Configuration
1. Copy `include/arduino_secrets.h.template` to `include/arduino_secrets.h`
2. Configure WiFi credentials and server settings:
```cpp
#define WIFI_SSID "YourWiFiNetwork"
#define WIFI_PASSWORD "YourWiFiPassword"  
#define SYSLOG_SERVER "192.168.1.100"
#define MQTT_SERVER "192.168.1.100"
```

## Development Environment

### Required Tools
- **PlatformIO**: Arduino development platform
- **VS Code**: Code editor with PlatformIO extension
- **Git**: Version control (recommended)

### Verification Steps
```bash
# Validate workspace setup
make validate

# Check project information
make info

# Test build system
make clean && make build
```

## Common Tasks

### Building Controller
```bash
# Standard build
make build

# Clean build (removes all artifacts first)
make clean && make build

# Build with static analysis
make check && make build
```

### Uploading and Monitoring
```bash
# Upload firmware
make upload

# Upload and immediately start monitoring
make debug

# Just monitor (if firmware already uploaded)
make monitor
```

### Configuration Management
```bash
# Setup secrets file from template
make secrets

# Create timestamped backup
make backup

# List available backups
make restore
```

## Troubleshooting

### Common Issues
1. **Upload Failed**: Check USB connection and Arduino port
2. **Build Errors**: Verify `arduino_secrets.h` exists and is configured
3. **Serial Monitor Issues**: Ensure no other programs are using the serial port

### Verification Commands
```bash
# Check PlatformIO installation
make validate

# Verify project structure
make info

# Test dependencies  
make deps
```

## Focus Areas

This workspace is optimized for:
- ✅ **Controller Development**: Full build, upload, debug cycle
- ✅ **Arduino Uno R4 WiFi**: Optimized for this specific hardware
- ✅ **Network Integration**: WiFi, MQTT, and syslog capabilities
- ✅ **Safety Systems**: Pressure monitoring and safety interlocks
- ✅ **Configuration Management**: EEPROM-based settings

Not included (separate workspace):
- ❌ Monitor system development
- ❌ Multi-board coordination  
- ❌ Monitor-specific documentation

---

**Ready to start controller development!** Use `make deploy` for a complete build and deployment cycle.