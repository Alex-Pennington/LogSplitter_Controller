# LogSplitter Meshtastic Quick Setup Guide

## Current Status: 1 Node Configured ✅

**LogSplitter-Base (LS-B)** on COM8 is ready. Need 2 additional devices for complete network.

## Next Steps: Complete Network Setup

### 1. **Hardware Preparation**
Connect all 3 devices:
- ✅ **Controller T-Beam** (COM8) - LogSplitter-Base - CONFIGURED
- 🔌 **Diagnostic PC T-Beam** - Need to connect
- 🔌 **Operator T-Echo** - Need to connect

### 2. **Generate Custom PSK Keys**
For security, generate unique pre-shared keys:

```bash
# Generate random PSK for log traffic (Channel 0)
python -c "import secrets; print('LOG_PSK:', secrets.token_hex(16))"

# Generate random PSK for telemetry (Channel 1) 
python -c "import secrets; print('TELEMETRY_PSK:', secrets.token_hex(16))"
```

### 3. **Automated Configuration**
Run the setup wizard:

```bash
python meshtastic_config_wizard.py
```

**Manual Assignment:**
1. Controller T-Beam → Already configured on COM8
2. Diagnostic PC T-Beam → Assign to detected port
3. Operator T-Echo → Assign to detected port

### 4. **Configuration Summary**

#### Network Architecture
```
Controller T-Beam ←→ Diagnostic PC T-Beam ←→ Operator T-Echo
    (Bidirectional)      (Bidirectional)      (Receive Only)
    
Channel 0: Log Traffic (WARN+ messages)
Channel 1: Binary Telemetry (Controller → Diagnostic only)
```

#### Device Roles
- **Controller**: CLIENT, SoftwareSerial bridge to Arduino, max TX power
- **Diagnostic**: CLIENT (not router), receives telemetry, max TX power  
- **Operator**: CLIENT, receive-only, battery optimized, reduced TX power

#### Key Settings
- **Modem Preset**: SHORT_FAST (global, all channels)
- **Region**: UA_433 (433MHz)
- **GPS**: Disabled (time sync only)
- **Node Announcements**: Every 300 seconds
- **Device Telemetry**: Enabled on all units

### 5. **Validation Commands**

After setup, test network connectivity:

```bash
# Test from Controller (COM8)
meshtastic --port COM8 --nodes
meshtastic --port COM8 --sendtext "Controller online - network test"

# Test from Diagnostic PC  
meshtastic --port COM_DIAG --nodes
meshtastic --port COM_DIAG --sendtext "Diagnostic PC online - network test"

# Check Operator receives messages (should see both test messages)
meshtastic --port COM_OP --nodes
```

### 6. **Arduino Integration**

The Controller T-Beam is configured for SoftwareSerial bridge:
- **Baud**: 115200
- **Mode**: TEXTMSG 
- **Pins**: Connect to Arduino A4 (TX) / A5 (RX)

Arduino will send binary telemetry which gets forwarded to Diagnostic PC via Channel 1.

### 7. **LCARS Integration**

Once configured, the LCARS Emergency Diagnostic Wizard will:
- Auto-detect all 3 nodes
- Monitor network health in real-time
- Provide troubleshooting interface
- Log maintenance activities

### 8. **Emergency Procedures**

If network fails:
1. **Primary**: Direct USB to Controller T-Beam (bypass mesh)
2. **Secondary**: Manual Arduino operation via Serial console
3. **Recovery**: Factory reset and reconfigure using this guide

## Configuration Files Reference

- **Setup Wizard**: `meshtastic_config_wizard.py` 
- **Manual Procedures**: `docs/MESHTASTIC_MAINTENANCE_PLAN.md`
- **LCARS Integration**: `docs/MESHTASTIC_OVERVIEW.md`
- **Parameters**: `docs/MESHTASTIC_CONFIG_QUESTIONS.md`

## Ready to Proceed?

1. Connect remaining 2 devices (Diagnostic PC T-Beam + Operator T-Echo)
2. Generate custom PSK keys
3. Run configuration wizard
4. Validate network connectivity
5. Test Arduino integration

**Current Controller is ready for network completion!** 🚀