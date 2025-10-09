# Custom OTA Usage Guide for LogMonitor

## 🚀 Development Workflow

### 1. Build New Firmware
```bash
cd LogSplitter_Controller
pio run
# Creates: .pio/build/uno_r4_wifi/firmware.bin
```

### 2. Start OTA Server (via Telnet)
```bash
# Connect to device
telnet 192.168.1.155 23

# Start OTA server
ota start
# Response: OTA server started on http://192.168.1.155/update

# Check status
ota status
# Response: OTA Status: IDLE, Progress: 0.0%, Error: None
```

### 3. Upload Firmware

#### Option A: Web Browser
1. Open: `http://192.168.1.155/update`
2. Select: `.pio/build/uno_r4_wifi/firmware.bin`
3. Click: "🚀 Upload Firmware"
4. Wait for completion

#### Option B: Command Line (curl)
```bash
curl -X POST -F "firmware=@.pio/build/uno_r4_wifi/firmware.bin" \
     http://192.168.1.155/update
```

#### Option C: PowerShell
```powershell
$uri = "http://192.168.1.155/update"
$filePath = ".pio\build\uno_r4_wifi\firmware.bin"
$form = @{
    firmware = Get-Item -Path $filePath
}
Invoke-RestMethod -Uri $uri -Method Post -Form $form
```

### 4. Monitor Progress (via Telnet)
```bash
# Check upload progress
ota status
# Response: OTA Status: RECEIVING, Progress: 45.2%, Error: None

# When complete
ota status  
# Response: OTA Status: SUCCESS, Progress: 100.0%, Error: None

# Reboot with new firmware
ota reboot
# Response: System rebooting in 3 seconds...
```

## 🔧 Integration with Existing System

### Telnet Commands Added:
- `ota status` - Show OTA server status and progress
- `ota start` - Start HTTP server for firmware uploads
- `ota stop` - Stop OTA server
- `ota url` - Show upload URL and curl example
- `ota reboot` - Restart system (useful after update)

### Syslog Integration:
- All OTA activities logged to syslog server
- Security events (unauthorized access, rate limiting)
- Upload progress and validation results
- Error conditions and recovery actions

### Safety Features:
- ✅ Atomic updates (complete or rollback)
- ✅ Firmware validation before flashing
- ✅ Local network access only
- ✅ Rate limiting (3 requests/minute per IP)
- ✅ Size limits (max 200KB firmware)
- ✅ CRC32 verification
- ✅ ARM vector table validation

## 📊 Monitoring & Debugging

### View OTA Logs:
```bash
# Check syslog server for OTA events
tail -f /var/log/syslog | grep "OTA:"

# Example log entries:
# Oct 09 14:30:15 LogMonitor: OTA: Server started for firmware updates
# Oct 09 14:32:22 LogMonitor: OTA: Upload started from 192.168.1.100
# Oct 09 14:32:45 LogMonitor: OTA: Firmware validation passed
# Oct 09 14:32:46 LogMonitor: OTA: Flash programming completed successfully
```

### Error Recovery:
```bash
# If upload fails, device remains on current firmware
ota status
# Response: OTA Status: ERROR, Progress: 23.4%, Error: CRC validation failed

# Stop and restart OTA server
ota stop
ota start

# Try upload again with correct firmware
```

## ⚡ Performance Impact

- **Memory Usage**: +2-3KB RAM for HTTP server
- **Flash Usage**: +8-10KB for OTA code
- **Network**: HTTP server runs only when needed
- **Update Time**: ~30-60 seconds for full firmware
- **Concurrent Operations**: OTA server doesn't interfere with monitoring

## 🛡️ Security Considerations

1. **Network Isolation**: Only local subnet access allowed
2. **No Authentication**: Suitable for lab/development environments
3. **Rate Limiting**: Prevents abuse/flooding
4. **Firmware Validation**: Prevents corrupted uploads
5. **Audit Logging**: All activities logged to syslog

## 🔄 Backup Strategy

The system maintains current firmware during update process:
1. New firmware uploaded to temporary buffer
2. Validation performed on buffer
3. Only after validation, flash programming begins
4. If flash fails, device boots from existing firmware
5. Watchdog timer ensures recovery from hang conditions