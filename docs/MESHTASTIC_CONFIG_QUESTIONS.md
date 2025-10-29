# LogSplitter Meshtastic Configuration Questions

## Network Architecture & Requirements

Before proceeding with the configuration, please confirm these critical parameters:

### 1. **Device Roles & Deployment**

#### Controller T-Beam
- **Location**: Where will this be mounted? (Equipment shed, control panel, weatherproof enclosure?)
- **Power**: Mains powered or battery backup required?
- **Antenna**: Indoor or outdoor antenna placement?
- **Range Requirements**: How far from diagnostic PC? (meters/kilometers)

#### Diagnostic PC T-Beam  
- **Location**: Permanent workstation or mobile diagnostic cart?
- **Role Preference**: ROUTER (mesh repeater) or CLIENT (endpoint only)?
- **Store & Forward**: Should it cache messages when controller is offline?
- **Range Extension**: Will this serve as a mesh network repeater?

#### Operator T-Echo
- **Usage Pattern**: Continuous monitoring or periodic checks?
- **Battery Life Target**: 8 hours, 24 hours, or multi-day operation?
- **Display Requirements**: Critical alerts only or full status information?
- **Durability Needs**: Industrial environment, outdoor use, moisture protection?

### 2. **LoRa Configuration Parameters**

#### Regional Settings
- **Confirmed**: UA_433 region (433MHz)
- **TX Power**: 
  - T-Beam nodes: 30dBm (max power) or reduced for battery life?
  - T-Echo: 10dBm (battery optimized) confirmed?
- **Duty Cycle**: Default regulatory limits or custom restrictions?

#### Modem Presets
- **Channel 0 (Operations)**: SHORT_FAST confirmed for low latency
- **Channel 1 (Telemetry)**: LONG_FAST for reliability, or MEDIUM_FAST for balance?
- **Bandwidth**: Default per preset or custom optimization?
- **Spread Factor**: Auto or manual tuning for environment?

### 3. **Channel & Security Configuration**

#### Channel 0: Primary Operations
- **Name**: "LogSplitter-Ops" confirmed
- **Participants**: All 3 nodes confirmed  
- **Message Types**: 
  - Emergency alerts (safety critical)
  - Operational commands (start/stop sequences)
  - Status updates (pressure, temperature)
  - Maintenance notifications
- **PSK**: Custom industrial key or default for testing?
- **Encryption**: AES-256 enabled confirmed?

#### Channel 1: Binary Telemetry
- **Name**: "Telemetry-Binary" confirmed
- **Participants**: Controller → Diagnostic PC only confirmed
- **Data Rate**: High-frequency updates (1Hz) or periodic (10s intervals)?
- **Message Priority**: Should telemetry have transmission priority?
- **Compression**: Raw binary or compressed protobuf messages?

### 4. **GPS & Positioning**

#### Location Services
- **Controller**: Fixed position after installation or continuous GPS?
- **Diagnostic PC**: GPS for mobile diagnostics or fixed workstation?
- **Operator**: GPS disabled for battery savings confirmed?
- **Privacy**: Position sharing within mesh or broadcast disabled?

#### Position Broadcasting
- **Update Intervals**: 
  - Fixed nodes: Every 15 minutes sufficient?
  - Mobile nodes: Smart broadcasting based on movement?
- **Precision**: Full GPS precision or reduced for privacy?
- **Location Logging**: Save position history for analysis?

### 5. **Power Management & Battery**

#### T-Echo Operator Device
- **Power Saving**: Aggressive battery optimization confirmed?
- **Wake Behavior**: 
  - Wake on message reception?
  - Scheduled wake intervals?
  - Manual wake only?
- **Low Battery Actions**: 
  - Automatic power-down at 10%?
  - Emergency-only mode at 20%?
- **Charging**: USB-C charging during operation acceptable?

#### T-Beam Base Stations
- **Mains Power**: Continuous operation assumed?
- **Battery Backup**: Should they continue on battery during power outages?
- **Power Monitoring**: Report power status to diagnostic system?

### 6. **Mesh Network Behavior**

#### Message Routing
- **Hop Limit**: 3 hops confirmed (covers most industrial sites)?
- **Rebroadcast**: All nodes rebroadcast or selective routing?
- **Route Discovery**: Automatic mesh topology or manual optimization?
- **Redundancy**: Multiple paths for critical messages?

#### Network Health
- **Heartbeat Intervals**: How often should nodes announce presence?
- **Timeout Detection**: Mark node offline after how many missed messages?
- **Recovery Behavior**: Automatic reconnection or manual intervention?
- **Performance Logging**: Track message delivery rates and latency?

### 7. **Industrial Integration**

#### Serial Bridge (Controller)
- **Baud Rate**: 115200 confirmed for telemetry bridge
- **Mode**: TEXTMSG for compatibility or SIMPLE for raw data?
- **Echo**: Disabled to prevent feedback loops?
- **Timeout**: How long to wait for controller responses?

#### Emergency Procedures
- **Failsafe**: If mesh fails, fall back to direct USB connection?
- **Alert Escalation**: Multiple notification methods for critical events?
- **Manual Override**: Physical buttons for emergency mesh reset?
- **Maintenance Access**: Diagnostic port always available via USB?

### 8. **Security & Access Control**

#### Network Security
- **PSK Management**: Separate keys for operations vs telemetry channels?
- **Key Rotation**: Scheduled security key updates required?
- **Admin Access**: Which devices can modify network configuration?
- **Audit Logging**: Track all configuration changes and access?

#### Industrial Compliance
- **Regulatory**: FCC/CE compliance for industrial wireless?
- **Documentation**: Maintain radio license documentation?
- **Safety**: RF exposure limits in work areas?
- **EMI**: Interference prevention with other industrial equipment?

---

## Configuration Validation Questions

Please confirm or modify these proposed settings:

### Basic Network
```
Region: UA_433 (433MHz)
Channel 0: LogSplitter-Ops, SHORT_FAST, All nodes
Channel 1: Telemetry-Binary, LONG_FAST, Controller+Diagnostic only
Hop Limit: 3
TX Power: 30dBm (T-Beam), 10dBm (T-Echo)
```

### Node Roles
```
Controller: CLIENT, GPS enabled, Serial bridge enabled
Diagnostic: ROUTER, Store&Forward enabled, GPS enabled  
Operator: CLIENT, Power saving enabled, GPS disabled
```

### Security
```
Encryption: AES-256 enabled
PSK: Custom industrial keys (not default)
Admin access: Diagnostic PC only
```

---

## Next Steps

1. **Review & Confirm**: Verify all parameters above
2. **Hardware Preparation**: Ensure all devices are connected and accessible
3. **Factory Reset**: Reset all 3 devices to clean state
4. **Sequential Configuration**: Configure one device at a time
5. **Network Testing**: Validate connectivity and performance
6. **Documentation**: Record final configuration for maintenance

**Ready to proceed with factory reset and configuration once parameters are confirmed.**