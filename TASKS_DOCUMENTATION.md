# VS Code Task Documentation

## Available Tasks for Monitor Development

### 🔧 Build Monitor
- **Purpose**: Compile the monitor firmware
- **Command**: PlatformIO build
- **Output**: `monitor\.pio\build\uno_r4_wifi\firmware.bin`

### 📤 Upload Monitor via USB
- **Purpose**: Upload firmware via USB cable
- **Dependencies**: Requires successful build
- **Hardware**: Requires physical USB connection to device

### 🚀 Build and Upload via TFTP *(Recommended)*
- **Purpose**: Build firmware and upload via network
- **Script**: `scripts\build_and_upload_simple.ps1`
- **Requirements**: Device must be on network (192.168.1.225)
- **Process**:
  1. Build firmware using PlatformIO
  2. Start TFTP server on device via telnet
  3. Upload firmware via TFTP protocol
  4. Display upload statistics

### 🌐 Upload via TFTP Only
- **Purpose**: Upload pre-built firmware via TFTP
- **Requirements**: Firmware must already be built
- **Use case**: Quick re-upload without rebuild

## Usage Instructions

### Using VS Code Tasks
1. **Open Command Palette**: `Ctrl+Shift+P`
2. **Run Task**: Type "Tasks: Run Task"
3. **Select Task**: Choose desired task from list

### Manual Script Execution
```powershell
# Build and upload via TFTP
.\scripts\build_and_upload_simple.ps1 -TargetHost "192.168.1.225"

# Production deployment
.\deploy_production.ps1 -TargetHost "192.168.1.225"
```

### Prerequisites for TFTP Upload
1. **Device Network Connection**: Device must be accessible at specified IP
2. **TFTP Server**: Device TFTP server should be running
3. **Network Access**: Host must have access to device on port 69 (TFTP)

### Manual TFTP Server Management
```bash
# Connect to device
telnet 192.168.1.225 23

# Start TFTP server
> tftp start

# Check status
> tftp status

# Stop server
> tftp stop
```

## Performance Metrics

### Typical Upload Performance
- **Transfer Rate**: ~2,500-3,500 bytes/second
- **131KB Firmware**: ~40-52 seconds
- **Memory Usage**: Device RAM stays at 37% during transfer
- **Reliability**: 100% success rate for valid firmware

### Build Performance
- **Compilation Time**: ~15-20 seconds
- **Total Process**: Build + Upload = ~60-75 seconds
- **Memory Efficiency**: Streaming upload with 8KB buffer

## Troubleshooting

### Common Issues
1. **"Device not found"**: Check IP address and network connectivity
2. **"TFTP upload failed"**: Ensure TFTP server is running on device
3. **"Build failed"**: Check PlatformIO configuration and code syntax
4. **"Permission denied"**: Run PowerShell as administrator if needed

### Debug Steps
1. **Test Connectivity**: `telnet 192.168.1.225 23`
2. **Check TFTP Status**: `tftp status` via telnet
3. **Manual Upload**: Use TFTP command directly
4. **Check Logs**: Monitor device serial output

### Recovery Procedures
1. **USB Recovery**: Use USB upload if TFTP fails
2. **Device Reset**: Power cycle device if unresponsive
3. **Network Reset**: Check WiFi connectivity on device
4. **Manual TFTP**: Start TFTP server manually via telnet

This task system provides a streamlined development workflow for the LogSplitter Monitor with efficient network-based firmware updates.