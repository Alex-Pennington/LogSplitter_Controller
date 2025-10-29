# Meshtastic Integration for LogSplitter Controller

## Overview

This document describes the integration of Meshtastic mesh networking with the LogSplitter Controller system, enabling wireless telemetry transport over long-range LoRa networks. The integration provides industrial-grade wireless communication for remote hydraulic log splitter operations.

## 🎯 **Key Benefits**

### **Wireless Range**
- **LoRa Technology**: Multi-kilometer range in open areas
- **Mesh Networking**: Multi-hop routing through intermediate nodes  
- **Industrial Reliability**: Robust communication in harsh environments
- **Battery Friendly**: Low power consumption for remote operations

### **Zero Protocol Changes**
- **Perfect Compatibility**: Existing binary telemetry (7-19 bytes) unchanged
- **Transparent Transport**: All 8 message types (0x10-0x17) work unchanged
- **Development Continuity**: Existing tools remain compatible
- **Plug-and-Play**: Simple hardware addition to existing system

### **Network Resilience**
- **Automatic Routing**: Mesh network finds optimal paths
- **Redundant Links**: Multiple communication paths
- **Self-Healing**: Network adapts to node failures
- **Range Extension**: Intermediate nodes extend coverage

## 🔧 **Hardware Architecture**

### **System Topology**

```
LogSplitter Controller ──► Meshtastic Bridge Node ──► LoRa Mesh ──► Monitor Node ──► Computer
┌─────────────────────┐   ┌─────────────────────────┐   ┌─────────┐   ┌─────────────┐   ┌──────────┐
│ Arduino UNO R4 WiFi │   │    Meshtastic Device    │   │  LoRa   │   │ Meshtastic  │   │ Monitoring│
│                     │   │                         │   │ Network │   │   Device    │   │  Station  │
│ Pin A4 (TX) ────────┼───┼─► RX Pin                 │   │         │   │             │   │           │
│ Pin A5 (RX) ────────┼───┼─► TX Pin                 │   │         │   │ USB ────────┼───┼─► Computer │
│ 5V/GND ─────────────┼───┼─► Power                  │   │         │   │             │   │           │
│                     │   │                         │   │         │   │             │   │           │
│ Binary Telemetry    │   │ Serial Bridge @ 115200  │   │         │   │ USB Bridge  │   │ Python    │
│ 7-19 byte packets   │   │ LoRa Transport          │   │         │   │ 115200 baud │   │ Receiver  │
└─────────────────────┘   └─────────────────────────┘   └─────────┘   └─────────────┘   └──────────┘
```

### **Bridge Node (Connects to Arduino)**
- **Role**: Transport binary telemetry from Arduino to mesh network
- **Connection**: Serial bridge at 115200 baud  
- **Power**: Can be powered from Arduino 5V or external source
- **Location**: Near LogSplitter controller (weatherproof enclosure)

### **Monitor Node (Connects to Computer)**
- **Role**: Receive mesh telemetry and forward to monitoring station
- **Connection**: USB to computer, no serial bridge needed
- **Power**: USB powered from computer
- **Location**: Monitoring station or office

### **Intermediate Nodes (Optional)**
- **Role**: Extend mesh range and provide redundancy
- **Connection**: No external connections needed
- **Power**: Battery powered for remote placement
- **Location**: Strategic points for optimal coverage

## 🔌 **Hardware Connections**

### **Bridge Node Wiring**

```
Arduino UNO R4 WiFi          Meshtastic Device (e.g., T-Beam, Heltec)
┌─────────────────┐           ┌─────────────────────────────────────┐
│                 │           │                                     │
│ Pin A4 (TX) ────┼───────────┼─► GPIO Pin (RX) - Configurable     │
│ Pin A5 (RX) ────┼───────────┼─► GPIO Pin (TX) - Configurable     │
│ 5V ─────────────┼───────────┼─► VIN (5V input)                   │
│ GND ────────────┼───────────┼─► GND                               │
│                 │           │                                     │
│ Serial (Debug)  │           │ USB (Configuration)                 │
│ Serial1 (Relay) │           │ LoRa Antenna                        │
└─────────────────┘           │ GPS Antenna (optional)              │
                              │ OLED Display (optional)             │
                              └─────────────────────────────────────┘
```

### **Pin Configuration Examples**

#### **T-Beam v1.1**
```
Arduino A4 → T-Beam GPIO 13 (RX)
Arduino A5 → T-Beam GPIO 14 (TX)  
Arduino 5V → T-Beam VIN
Arduino GND → T-Beam GND
```

#### **Heltec WiFi LoRa 32 V3**
```
Arduino A4 → Heltec GPIO 17 (RX)
Arduino A5 → Heltec GPIO 18 (TX)
Arduino 5V → Heltec VIN  
Arduino GND → Heltec GND
```

#### **LilyGO T3S3**
```
Arduino A4 → T3S3 GPIO 44 (RX)
Arduino A5 → T3S3 GPIO 43 (TX)
Arduino 5V → T3S3 VIN
Arduino GND → T3S3 GND
```

### **Power Considerations**

- **Arduino Power**: 7-12V input recommended for stable 5V output
- **Meshtastic Power**: 3.3-5V input, ~100-200mA typical consumption
- **LoRa Transmission**: Brief 100-500mA spikes during transmission
- **Power Supply**: Use quality regulated supply for both devices

## ⚙️ **Software Configuration**

### **Step 1: Install Meshtastic CLI**

```bash
# Install Meshtastic Python tools
pip install meshtastic

# Verify installation
meshtastic --version
```

### **Step 2: Configure Bridge Node**

```bash
# Connect bridge node via USB
python meshtastic_config.py --setup-bridge

# Manual configuration alternative:
meshtastic --set-owner "LogSplitter Bridge"
meshtastic --set lora.use_preset LONG_FAST
meshtastic --set lora.tx_power 22
meshtastic --ch-set psk "LogSplitterIndustrial2025!" --ch-index 1
meshtastic --ch-set name "LogSplitter" --ch-index 1
meshtastic --set serial.enabled true
meshtastic --set serial.baud 115200
```

### **Step 3: Configure Monitor Node**

```bash  
# Connect monitor node via USB
python meshtastic_config.py --setup-receiver

# Manual configuration alternative:
meshtastic --set-owner "LogSplitter Monitor"
meshtastic --set lora.use_preset LONG_FAST
meshtastic --set lora.tx_power 22
meshtastic --ch-set psk "LogSplitterIndustrial2025!" --ch-index 1
meshtastic --ch-set name "LogSplitter" --ch-index 1
meshtastic --set serial.enabled false
```

### **Step 4: Test Configuration**

```bash
# Check node status
python meshtastic_config.py --status

# Test connectivity
python meshtastic_config.py --test

# Monitor mesh network
meshtastic --nodes
```

## 📡 **Telemetry Transport**

### **Binary Protocol Preservation**

The Meshtastic integration preserves the exact binary telemetry protocol:

```
Original Arduino Output:     Meshtastic Transport:        Receiver Output:
[SIZE][MSG][PAYLOAD]   ──►  [MESH_HEADER][SIZE][MSG]  ──► [SIZE][MSG][PAYLOAD]
     7-19 bytes               LoRa Packet ~50 bytes           7-19 bytes
```

### **Message Types Supported**

All existing message types transport unchanged:

| Type | ID   | Description          | Size     | Mesh Compatible |
|------|------|---------------------|----------|-----------------|
| Digital Input  | 0x10 | DI state changes    | 11 bytes | ✅ Yes          |
| Digital Output | 0x11 | DO state changes    | 10 bytes | ✅ Yes          |
| Relay Control  | 0x12 | Relay operations    | 10 bytes | ✅ Yes          |
| Pressure       | 0x13 | Hydraulic pressure  | 15 bytes | ✅ Yes          |
| System Error   | 0x14 | Error conditions    | 9-33 bytes | ✅ Yes        |
| Safety Event   | 0x15 | Safety system       | 10 bytes | ✅ Yes          |
| System Status  | 0x16 | Heartbeat/status    | 19 bytes | ✅ Yes          |
| Sequence Event | 0x17 | Sequence control    | 11 bytes | ✅ Yes          |

### **Performance Characteristics**

- **Latency**: Sub-second for single hop, 1-3 seconds multi-hop
- **Throughput**: ~100 messages/minute sustained
- **Range**: 1-10 km depending on terrain and obstructions
- **Reliability**: >95% packet delivery in good conditions

## 🛠️ **Monitoring and Debugging**

### **Enhanced Telemetry Receiver**

Use the enhanced receiver that supports both direct and mesh modes:

```bash
# Auto-detect connection mode
python meshtastic_telemetry_receiver.py

# Force direct Arduino connection  
python meshtastic_telemetry_receiver.py --mode direct --port COM3

# Force Meshtastic bridge mode
python meshtastic_telemetry_receiver.py --mode meshtastic --port COM4
```

### **Sample Output**

```
🚀 LogSplitter Enhanced Telemetry Receiver
╔══════════════════════════════════════════════════════════════╗
║         LOGSPLITTER ENHANCED TELEMETRY MONITOR               ║
║              Direct Arduino + Meshtastic Bridge              ║
╠══════════════════════════════════════════════════════════════╣
║  Mode: MESHTASTIC       Port: COM4               ║
║  Expected: Binary protobuf messages from LogSplitter        ║
║  Format: [SIZE][MSG_TYPE][SEQ][TIMESTAMP][DATA...]          ║
╚══════════════════════════════════════════════════════════════╝

📡 [14:23:45.123] Type:0x13 Seq: 42 RSSI:-85dBm Hops:1 Pressure A1: 1234.5 PSI
📡 [14:23:45.856] Type:0x10 Seq: 43 RSSI:-85dBm Input Pin 6: HIGH
📡 [14:23:46.234] Type:0x15 Seq: 44 RSSI:-82dBm Safety Event: Type 1, Severity 2
```

### **Network Status Monitoring**

```bash
# Show mesh network status
meshtastic --nodes

# Monitor signal strength and routing
meshtastic --info

# View message history
meshtastic --export-config
```

### **LCARS Emergency Diagnostic Integration**

The emergency diagnostic wizard includes Meshtastic connectivity testing:

```bash
# Start documentation server with emergency wizard
python lcars_docs_server.py

# Navigate to: http://localhost:3000/emergency_diagnostic
# Select: "Mesh Network Issues" 
```

The wizard will:
- Auto-detect Meshtastic nodes
- Test mesh connectivity 
- Validate signal strength
- Provide troubleshooting guidance

## 📊 **Range Testing**

### **Range Testing Tool**

```bash
# Run range testing between nodes
python meshtastic_range_test.py --bridge-node COM3 --monitor-node COM4 --duration 300

# Example output:
🔗 Starting 5-minute range test...
📍 Position logging enabled
📊 Test Results:
   Distance: 2.3 km
   Packets sent: 150
   Packets received: 147  
   Success rate: 98%
   Average RSSI: -89 dBm
   Average latency: 1.2 seconds
```

### **Range Optimization Tips**

1. **Antenna Placement**
   - Mount as high as possible
   - Clear line of sight preferred
   - Avoid metal obstructions
   - Use quality antennas (gain 3-8 dBi)

2. **Power Settings**
   - Use maximum legal power (22 dBm)
   - Consider higher power modules for extreme range
   - Balance power vs battery life

3. **Network Topology**
   - Place intermediate nodes strategically
   - Create redundant paths
   - Consider terrain and obstacles

## 🔒 **Security Configuration**

### **Channel Encryption**

LogSplitter uses AES encryption with industrial PSK:

```bash
# Set secure pre-shared key
meshtastic --ch-set psk "LogSplitterIndustrial2025!" --ch-index 1

# Verify encryption is active
meshtastic --ch-index 1
```

### **Network Isolation**

- **Dedicated Channel**: LogSplitter operates on private channel 1
- **Unique PSK**: Custom pre-shared key prevents unauthorized access  
- **Node Authentication**: Only configured nodes can join network
- **Payload Encryption**: All telemetry data encrypted in transit

## 🚨 **Troubleshooting**

### **Common Issues**

#### **No Mesh Connectivity**
```bash
# Check node configuration
python meshtastic_config.py --status

# Verify channel settings match
meshtastic --ch-index 1

# Test local connectivity  
python meshtastic_config.py --test
```

#### **Poor Signal Quality**
- Check antenna connections
- Verify antenna placement
- Test with nodes closer together
- Consider intermediate relay nodes

#### **Missing Telemetry Data**
- Verify Arduino→Meshtastic wiring
- Check serial bridge configuration
- Monitor Arduino debug output
- Test direct connection first

#### **High Latency**
- Check mesh hop count
- Optimize node placement
- Reduce network congestion
- Consider LoRa parameter tuning

### **Diagnostic Commands**

```bash
# Node information
meshtastic --info

# Network topology  
meshtastic --nodes --export-config

# Signal strength monitoring
meshtastic --debug

# Reset to defaults
meshtastic --factory-reset
```

## 📈 **Performance Monitoring**

### **Key Metrics**

Monitor these metrics for optimal performance:

- **RSSI**: Signal strength (-120 to -80 dBm range)
- **SNR**: Signal-to-noise ratio (>0 dB preferred)  
- **Packet Loss**: <5% for reliable operation
- **Latency**: <3 seconds for multi-hop networks
- **Battery Voltage**: Monitor for remote nodes

### **Performance Dashboard**

The enhanced receiver provides real-time performance metrics:

```
📊 Meshtastic Bridge Statistics:
   Messages: 1247
   Bytes: 18,450
   Signal Quality:
     RSSI: -87 dBm
     SNR: 8.3 dB  
     Hop Count: 2
```

## 🔄 **Integration Workflow**

### **Complete Setup Process**

1. **Hardware Assembly**
   ```
   ✓ Connect Arduino A4/A5 to Meshtastic bridge node
   ✓ Power both devices from stable supply
   ✓ Install antennas and GPS (if equipped)
   ✓ Place in weatherproof enclosure
   ```

2. **Software Configuration**
   ```
   ✓ Install Meshtastic CLI tools
   ✓ Configure bridge node (near Arduino)
   ✓ Configure monitor node (at monitoring station)
   ✓ Set matching channel and encryption
   ✓ Test connectivity between nodes
   ```

3. **Network Deployment**
   ```
   ✓ Position nodes for optimal coverage
   ✓ Test range and signal quality
   ✓ Deploy intermediate nodes if needed
   ✓ Verify end-to-end telemetry transport
   ```

4. **Monitoring Setup**
   ```
   ✓ Start enhanced telemetry receiver
   ✓ Verify all message types received
   ✓ Configure LCARS emergency diagnostic
   ✓ Establish performance baselines
   ```

### **Maintenance Schedule**

- **Daily**: Monitor signal strength and packet loss
- **Weekly**: Check battery levels on remote nodes  
- **Monthly**: Update firmware and verify configuration
- **Quarterly**: Range test and antenna inspection
- **Annually**: Replace batteries and weatherproofing

## 📚 **Additional Resources**

### **Documentation Links**
- [Meshtastic Official Documentation](https://meshtastic.org/docs/)
- [LoRa Range Calculator](https://www.rfwireless-world.com/calculators/LoRa-range-calculator.html)
- [Arduino UNO R4 WiFi Pinout](https://docs.arduino.cc/hardware/uno-r4-wifi/)

### **Hardware Vendors**
- **Meshtastic Devices**: LilyGO, Heltec, RAK Wireless
- **Antennas**: Pulse Electronics, Linx Technologies  
- **Enclosures**: Pelican, Hoffman, Bud Industries

### **Community Support**
- **Meshtastic Discord**: [discord.gg/meshtastic](https://discord.gg/meshtastic)
- **LogSplitter GitHub**: [Issues and discussions](https://github.com/Alex-Pennington/LogSplitter_Controller)
- **Arduino Forums**: [Hardware troubleshooting](https://forum.arduino.cc/)

---

**Status**: Production-ready Meshtastic integration with comprehensive wireless telemetry transport for industrial LogSplitter operations.