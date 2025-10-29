# LCARS Meshtastic Network Overview

## Network Status Dashboard

### 🛰️ **LogSplitter Mesh Network Status** 

**Network Health**: `INITIALIZING` | **Nodes Active**: `0/3` | **Last Update**: `---`

---

## Node Configuration Status

### 📡 **Controller Node (T-Beam)**
- **Status**: `PENDING SETUP`
- **Role**: CLIENT (Base Station)  
- **Location**: LogSplitter Equipment
- **Channels**: 0 (Operations), 1 (Telemetry)
- **Serial Bridge**: Enabled for telemetry
- **Last Seen**: `---`

### 💻 **Diagnostic PC Node (T-Beam)**  
- **Status**: `PENDING SETUP`
- **Role**: ROUTER (Mesh Repeater)
- **Location**: LCARS Diagnostic Station
- **Channels**: 0 (Operations), 1 (Telemetry)
- **Store & Forward**: Enabled
- **Last Seen**: `---`

### 👤 **Operator Node (T-Echo)**
- **Status**: `PENDING SETUP` 
- **Role**: CLIENT (Handheld)
- **Location**: Field Operations
- **Channels**: 0 (Operations Only)
- **Power Mode**: Battery Optimized
- **Last Seen**: `---`

---

## Quick Configuration Commands

### **Factory Reset Sequence**
```bash
# Reset all devices to factory defaults
meshtastic --port COM8 --factory-reset   # Controller
meshtastic --port COM9 --factory-reset   # Diagnostic PC  
meshtastic --port COM10 --factory-reset  # Operator
```

### **Basic Network Setup**
```bash
# Set LoRa region (REQUIRED FIRST STEP)
meshtastic --port COM8 --set lora.region UA_433

# Configure device roles
meshtastic --port COM8 --set device.role CLIENT
meshtastic --port COM8 --set-owner "LogSplitter-Controller" --set-owner-short "LS-CTRL"

# Setup primary channel
meshtastic --port COM8 --ch-set name "LogSplitter-Ops" --ch-index 0
meshtastic --port COM8 --set lora.modem_preset SHORT_FAST
```

### **Emergency Recovery**
```bash
# Emergency reset if network fails
meshtastic --port COM8 --factory-reset --force
meshtastic --port COM8 --set lora.region UA_433
meshtastic --port COM8 --ch-set psk default --ch-index 0
```

---

## Network Health Monitoring

### **Connectivity Tests**
- **Ping Test**: Send test message between all nodes
- **Range Test**: Validate signal strength and packet delivery  
- **Channel Test**: Verify operations and telemetry channel separation
- **Emergency Test**: Confirm emergency broadcast capability

### **Performance Metrics**
- **Message Delivery Rate**: Target >95%
- **Network Latency**: Target <2 seconds
- **Battery Status**: Monitor T-Echo power levels
- **Signal Quality**: RSSI and SNR monitoring

### **Automated Diagnostics**
The LCARS Emergency Diagnostic Wizard provides:
- Real-time node discovery and health checks
- Automated configuration validation
- Network performance testing
- Emergency message broadcasting
- Maintenance alert generation

---

## Configuration Parameters

### **Channel 0: Log Traffic** 
- **Name**: LogSplitter-LogTraffic
- **Purpose**: Operational messages (WARN level and above)
- **Participants**: All nodes (Operator receive-only)
- **Modem**: SHORT_FAST (global setting)
- **Encryption**: Custom PSK

### **Channel 1: Binary Telemetry**
- **Name**: Telemetry-Binary  
- **Purpose**: Protobuf sensor data (pressure, relays, inputs)
- **Participants**: Controller → Diagnostic PC only
- **Modem**: SHORT_FAST (global setting)
- **Protocol**: Binary protobuf

### **Security Settings**
- **Region**: UA_433 (433MHz)
- **TX Power**: 10dBm (T-Echo), 30dBm (T-Beam)
- **Modem Preset**: SHORT_FAST (global, all channels)
- **GPS**: Disabled (time sync only)
- **Node Announcements**: Every 300 seconds
- **Encryption**: AES-256 with custom PSK

---

## Emergency Procedures

### **🚨 Network Failure Recovery**

1. **Immediate Actions**
   - Switch to direct USB connection for controller
   - Verify power status on all nodes  
   - Check antenna connections

2. **Diagnostic Steps**
   - Run LCARS network health check
   - Test individual node connectivity
   - Verify LoRa region settings

3. **Recovery Actions**
   - Factory reset affected nodes
   - Reconfigure with known-good settings
   - Validate network connectivity

### **📞 Emergency Contact Protocol**

If mesh network fails during critical operations:
- **Primary**: Direct USB to LogSplitter Controller
- **Secondary**: Manual operation via controller panel
- **Backup**: Radio/cell contact to maintenance team

---

## Maintenance Integration

### **Daily Automated Checks** (via LCARS)
- Node connectivity verification
- Battery level monitoring
- Message delivery testing
- Performance metric collection

### **Alert Conditions**
- **RED**: Node offline >5 minutes
- **YELLOW**: Battery <20% or poor signal quality
- **GREEN**: All systems operational

### **Maintenance Log**
All network maintenance activities are logged in the LCARS system with:
- Configuration changes
- Performance metrics
- Error conditions
- Recovery actions taken

---

**🔧 For detailed setup procedures, see: `MESHTASTIC_MAINTENANCE_PLAN.md`**

**📊 Network monitoring available at: LCARS Emergency Diagnostic Wizard**