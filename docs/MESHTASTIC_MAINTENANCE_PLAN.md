# LogSplitter Meshtastic Network Maintenance Plan

## Overview
**Industrial Wireless Mesh Network for LogSplitter Controller System**
- **Firmware Version**: 2.7.11.ee68575 Alpha
- **Network Topology**: 3-Node Mesh (Controller ↔ Diagnostic PC ↔ Operator)
- **Frequency**: 433MHz (UA_433 Region)
- **Security**: Industrial PSK with encrypted channels

## Network Architecture

### Node Roles & Hardware
```
┌─────────────────┐    LoRa Mesh     ┌─────────────────┐    LoRa Mesh     ┌─────────────────┐
│ LogSplitter     │◄──────────────►  │ Diagnostic PC   │◄──────────────►  │ Field Operator  │
│ Controller      │                  │ (LCARS System)  │                  │ (Handheld)      │
│                 │                  │                 │                  │                 │
│ T-Beam (Base)   │                  │ T-Beam (Router) │                  │ T-Echo (Mobile) │
│ Role: CLIENT    │                  │ Role: ROUTER    │                  │ Role: CLIENT    │
│ GPS: Enabled    │                  │ GPS: Enabled    │                  │ GPS: Disabled   │
│ Power: Mains    │                  │ Power: USB      │                  │ Power: Battery  │
└─────────────────┘                  └─────────────────┘                  └─────────────────┘
         │                                    │                                    │
         ▼                                    ▼                                    ▼
   Serial Bridge                       LCARS Interface                    Operator Display
  (Binary Telemetry)                 (Emergency Wizard)                  (Status Monitor)
```

## Channel Configuration

### Channel 0: Primary Operations (SHORT_FAST)
- **Purpose**: Command & control, status updates, emergency alerts
- **Participants**: All nodes
- **Message Types**: Text commands, operational status, safety alerts
- **Encryption**: Enabled with custom PSK

### Channel 1: Binary Telemetry (LONG_FAST) 
- **Purpose**: High-throughput sensor data from LogSplitter
- **Participants**: Controller → Diagnostic PC
- **Message Types**: Binary protobuf (pressure, relays, inputs, errors)
- **Encryption**: Enabled with custom PSK

## Factory Reset & Configuration Procedure

### Prerequisites
- **Meshtastic CLI**: Version 2.7.x installed
- **USB Cables**: Quality cables for each device type
- **Documentation**: This maintenance plan and LCARS interface
- **Network Isolation**: Configure one device at a time

### Phase 1: Factory Reset All Devices

#### 1.1 Reset Controller T-Beam
```bash
meshtastic --port COM_X --factory-reset
# Wait 30 seconds for reboot
meshtastic --port COM_X --info  # Verify reset
```

#### 1.2 Reset Diagnostic PC T-Beam
```bash
meshtastic --port COM_Y --factory-reset
# Wait 30 seconds for reboot
meshtastic --port COM_Y --info  # Verify reset
```

#### 1.3 Reset Operator T-Echo
```bash
meshtastic --port COM_Z --factory-reset
# Wait 30 seconds for reboot
meshtastic --port COM_Z --info  # Verify reset
```

### Phase 2: Base Configuration (All Devices)

#### 2.1 Set LoRa Region (REQUIRED FIRST)
```bash
# All devices
meshtastic --port COM_X --set lora.region UA_433
```

#### 2.2 Configure Device Roles
```bash
# Controller T-Beam
meshtastic --port COM_X --set device.role CLIENT
meshtastic --port COM_X --set-owner "LogSplitter-Controller" --set-owner-short "LS-CTRL"

# Diagnostic PC T-Beam  
meshtastic --port COM_Y --set device.role ROUTER
meshtastic --port COM_Y --set-owner "LogSplitter-Diagnostics" --set-owner-short "LS-DIAG"

# Operator T-Echo
meshtastic --port COM_Z --set device.role CLIENT  
meshtastic --port COM_Z --set-owner "LogSplitter-Operator" --set-owner-short "LS-OP"
```

### Phase 3: Channel Configuration

#### 3.1 Configure Channel 0 (Primary Operations)
```bash
# All devices - Primary channel configuration
meshtastic --port COM_X --ch-set name "LogSplitter-Ops" --ch-index 0
meshtastic --port COM_X --ch-set psk [CUSTOM_PSK] --ch-index 0
meshtastic --port COM_X --set lora.modem_preset SHORT_FAST

# Repeat for all devices with same settings
```

#### 3.2 Configure Channel 1 (Binary Telemetry)  
```bash
# Controller and Diagnostic PC only
meshtastic --port COM_X --ch-add "Telemetry-Binary" --ch-index 1
meshtastic --port COM_X --ch-set psk [TELEMETRY_PSK] --ch-index 1

# Set telemetry-specific LoRa settings for reliability
meshtastic --port COM_X --set lora.modem_preset LONG_FAST --ch-index 1
```

### Phase 4: Device-Specific Configuration

#### 4.1 LogSplitter Controller T-Beam
```bash
# Serial module for telemetry bridge
meshtastic --port COM_X --set serial.enabled true
meshtastic --port COM_X --set serial.baud BAUD_115200
meshtastic --port COM_X --set serial.mode TEXTMSG

# GPS and positioning
meshtastic --port COM_X --set position.gps_enabled true
meshtastic --port COM_X --set position.fixed_position false
meshtastic --port COM_X --set position.position_broadcast_secs 900

# Power management for mains operation
meshtastic --port COM_X --set power.is_power_saving false
meshtastic --port COM_X --set power.wait_bluetooth_secs 60

# Device telemetry monitoring
meshtastic --port COM_X --set telemetry.device_update_interval 300
meshtastic --port COM_X --set telemetry.power_measurement_enabled true
```

#### 4.2 Diagnostic PC T-Beam (Router)
```bash
# Router configuration for mesh networking
meshtastic --port COM_Y --set device.rebroadcast_mode ALL
meshtastic --port COM_Y --set lora.hop_limit 3

# Store and forward for offline controller support
meshtastic --port COM_Y --set store_forward.enabled true
meshtastic --port COM_Y --set store_forward.is_server true
meshtastic --port COM_Y --set store_forward.records 50

# GPS for network positioning
meshtastic --port COM_Y --set position.gps_enabled true
meshtastic --port COM_Y --set position.position_broadcast_secs 1800

# Extended device monitoring
meshtastic --port COM_Y --set telemetry.device_update_interval 300
meshtastic --port COM_Y --set telemetry.power_measurement_enabled true
```

#### 4.3 Operator T-Echo (Handheld)
```bash
# Power saving for battery operation
meshtastic --port COM_Z --set power.is_power_saving true
meshtastic --port COM_Z --set power.ls_secs 300
meshtastic --port COM_Z --set power.wait_bluetooth_secs 30

# Disable GPS to save power
meshtastic --port COM_Z --set position.gps_enabled false
meshtastic --port COM_Z --set position.position_broadcast_secs 0

# Minimal telemetry for battery conservation
meshtastic --port COM_Z --set telemetry.device_update_interval 600
meshtastic --port COM_Z --set telemetry.power_measurement_enabled true

# Display optimization
meshtastic --port COM_Z --set display.screen_on_secs 30
meshtastic --port COM_Z --set display.auto_screen_carousel_secs 10
```

## Validation & Testing

### Network Health Checks
```bash
# Test connectivity from each device
meshtastic --port COM_X --nodes
meshtastic --port COM_X --sendtext "Controller online - $(date)"

# Test channel separation
meshtastic --port COM_X --ch-index 0 --sendtext "Operations channel test"
meshtastic --port COM_X --ch-index 1 --sendtext "Telemetry channel test"

# Range testing
meshtastic --port COM_X --set range_test.enabled true --set range_test.sender 60
```

### Performance Validation
- [ ] Message delivery rates >95%
- [ ] Typical mesh latency <2 seconds
- [ ] Battery life (T-Echo) >24 hours normal use
- [ ] GPS accuracy within 5 meters
- [ ] Channel isolation verified

## Troubleshooting Guide

### Common Issues
1. **No Mesh Connectivity**: Check LoRa region settings match
2. **High Power Usage**: Verify power saving settings on battery devices
3. **Message Loss**: Increase TX power or add repeater node
4. **GPS Issues**: Ensure clear sky view, wait for satellite lock

### Emergency Recovery
```bash
# Emergency factory reset
meshtastic --port COM_X --factory-reset --force

# Emergency channel reset  
meshtastic --port COM_X --ch-set psk default --ch-index 0
```

## Maintenance Schedule

### Daily (Automated via LCARS)
- [ ] Check node connectivity
- [ ] Monitor battery levels
- [ ] Validate message delivery

### Weekly
- [ ] Review telemetry data quality
- [ ] Check GPS position accuracy
- [ ] Test emergency procedures

### Monthly  
- [ ] Firmware update check
- [ ] Network range testing
- [ ] Backup device configurations

### Annually
- [ ] Complete network reconfiguration
- [ ] Hardware inspection
- [ ] Security key rotation

## LCARS Integration Points

The Emergency Diagnostic Wizard provides:
- **Real-time network health monitoring**
- **Automated node discovery and validation**
- **Emergency message broadcasting capability**
- **Performance metrics collection**
- **Troubleshooting workflow automation**

## Security Considerations

- **Custom PSK**: Use unique pre-shared keys for industrial security
- **Channel Isolation**: Separate operational and telemetry traffic
- **Access Control**: Limit administrative access to diagnostic PC
- **Regular Updates**: Monitor for firmware security patches

---
*This maintenance plan ensures reliable industrial mesh networking for LogSplitter operations with proper redundancy and monitoring capabilities.*