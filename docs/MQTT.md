# LogSplitter Controller MQTT Topics

## Document Purpose

This document describes all MQTT topics published by the LogSplitter Controller, their data formats, update frequencies, and usage. This is the authoritative reference for building interfaces and monitoring systems.

**Controller**: Arduino UNO R4 WiFi LogSplitter Controller  
**Firmware**: LogSplitter Controller v2.0  
**MQTT Prefix**: `controller/`  
**Broker**: Configurable in `arduino_secrets.h` (default: 192.168.1.155:1883)

---

## Topic Hierarchy

```
controller/
├── control/              # Command interface
├── inputs/               # Digital input states
├── relays/               # Relay output states
├── pressure/             # Pressure sensor readings
├── safety/               # Safety system status
├── sequence/             # Hydraulic sequence status
├── engine/               # Engine control status
├── system/               # System health and diagnostics
└── config/               # Configuration parameters (retained)
```

---

## 1. Control Interface Topics

### `controller/control`
**Direction**: Subscribe (commands IN)  
**Purpose**: Command input topic - controller subscribes to receive commands  
**Format**: Plain text command strings  
**Examples**:
- `help`
- `show`
- `relay R1 ON`
- `set vref 4.75`

**Note**: This is a command INPUT topic. The controller listens for commands here.

---

### `controller/control/resp`
**Direction**: Publish (responses OUT)  
**Purpose**: Command response and feedback messages  
**Format**: Plain text response strings  
**Update**: On command execution or event  
**Retained**: No  

**Example Messages**:
```
relay R1 ON
E-STOP: ACTIVATED - System shutdown
SAFETY: pressure normalized - manual clear required
SAFETY: deactivated
```

---

## 2. Digital Input Topics

All digital input pins publish two values per state change:

### `controller/inputs/{PIN}/state`
**Direction**: Publish  
**Purpose**: Numeric state value  
**Format**: `0` (inactive) or `1` (active)  
**Update**: On debounced state change  
**Retained**: No  

### `controller/inputs/{PIN}/active`
**Direction**: Publish  
**Purpose**: Human-readable state  
**Format**: `true` or `false`  
**Update**: On debounced state change  
**Retained**: No  

---

### Input Pin Mapping

| Pin | Topic | Function | Active When |
|-----|-------|----------|-------------|
| 2 | `controller/inputs/2/state`<br>`controller/inputs/2/active` | Manual Extend Button | Button pressed (LOW) |
| 3 | `controller/inputs/3/state`<br>`controller/inputs/3/active` | Manual Retract Button | Button pressed (LOW) |
| 4 | `controller/inputs/4/state`<br>`controller/inputs/4/active` | Safety Reset/Clear Button | Button pressed (LOW) |
| 5 | `controller/inputs/5/state`<br>`controller/inputs/5/active` | Sequence Start Button | Button pressed (LOW) |
| 6 | `controller/inputs/6/state`<br>`controller/inputs/6/active` | Extend Limit Switch | Limit reached (LOW) |
| 7 | `controller/inputs/7/state`<br>`controller/inputs/7/active` | Retract Limit Switch | Limit reached (LOW) |
| 12 | `controller/inputs/12/state`<br>`controller/inputs/12/active` | Emergency Stop | E-Stop pressed (LOW) |

**Debounce Timing**:
- Pins 2,3,4,5: 15ms (button debounce)
- Pins 6,7: 10ms (limit switch debounce, configurable)
- Pin 12: 5ms (default debounce)

**Example Messages**:
```
controller/inputs/6/state → 1
controller/inputs/6/active → true
controller/inputs/2/state → 0
controller/inputs/2/active → false
```

---

## 3. Relay Output Topics

Each relay publishes two values per state change:

### `controller/relays/{NUM}/state`
**Direction**: Publish  
**Purpose**: Numeric relay state  
**Format**: `0` (OFF) or `1` (ON)  
**Update**: On relay state change  
**Retained**: No  

### `controller/relays/{NUM}/active`
**Direction**: Publish  
**Purpose**: Human-readable relay state  
**Format**: `true` or `false`  
**Update**: On relay state change  
**Retained**: No  

---

### Relay Mapping

| Relay | Topic | Function | Control |
|-------|-------|----------|---------|
| 1 | `controller/relays/1/state`<br>`controller/relays/1/active` | Hydraulic Extend | Manual (Pin 2) or Sequence |
| 2 | `controller/relays/2/state`<br>`controller/relays/2/active` | Hydraulic Retract | Manual (Pin 3) or Sequence |
| 3-6 | `controller/relays/3-6/state`<br>`controller/relays/3-6/active` | Reserved | Command only |
| 7 | `controller/relays/7/state`<br>`controller/relays/7/active` | Pressure Warning Buzzer | Automatic or Command |
| 8 | `controller/relays/8/state`<br>`controller/relays/8/active` | Engine Enable/Disable | Command or Safety |
| 9 | `controller/relays/9/state`<br>`controller/relays/9/active` | Mill Lamp / Board Power | Command |

---

### Relay System Status

### `controller/relays/board_powered`
**Direction**: Publish  
**Purpose**: Relay board power status  
**Format**: `true` or `false`  
**Update**: Periodic or state change  
**Retained**: No  

### `controller/relays/safety_mode`
**Direction**: Publish  
**Purpose**: Safety system active status  
**Format**: `true` or `false`  
**Update**: On safety state change  
**Retained**: No  

---

## 4. Pressure Sensor Topics

### Main Hydraulic System Pressure (A1)

### `controller/pressure/hydraulic_system`
**Direction**: Publish  
**Purpose**: Main hydraulic pressure in PSI  
**Format**: Floating point string (e.g., `1234.5`)  
**Range**: 0-5000 PSI (4-20mA current loop)  
**Update**: 100ms sampling, published on significant change  
**Retained**: No  
**Sensor**: Analog pin A1, 4-20mA current loop (1-5V)

### `controller/pressure/hydraulic_system_voltage`
**Direction**: Publish  
**Purpose**: Raw voltage reading from A1  
**Format**: Floating point string (e.g., `3.456`)  
**Range**: 1.0-5.0V (4mA-20mA)  
**Update**: With pressure updates  
**Retained**: No  

### `controller/pressure` *(Legacy)*
**Direction**: Publish  
**Purpose**: Backward compatibility for main pressure  
**Format**: Same as `hydraulic_system`  
**Note**: Maintained for legacy systems, use `hydraulic_system` for new code  

---

### Hydraulic Filter Pressure (A5)

### `controller/pressure/hydraulic_filter`
**Direction**: Publish  
**Purpose**: Filter pressure in PSI  
**Format**: Floating point string (e.g., `12.3`)  
**Range**: 0-30 PSI (0-5V voltage output)  
**Update**: 100ms sampling, published on significant change  
**Retained**: No  
**Sensor**: Analog pin A5, 0-5V direct voltage

### `controller/pressure/hydraulic_filter_voltage`
**Direction**: Publish  
**Purpose**: Raw voltage reading from A5  
**Format**: Floating point string (e.g., `2.123`)  
**Range**: 0-5.0V  
**Update**: With filter pressure updates  
**Retained**: No  

---

## 5. Safety System Topics

### `controller/safety/active`
**Direction**: Publish  
**Purpose**: Safety system activation state  
**Format**: `0` (inactive) or `1` (active)  
**Update**: On safety state change  
**Retained**: No  

### `controller/safety/estop`
**Direction**: Publish  
**Purpose**: Emergency stop status  
**Format**: `0` (not pressed) or `1` (pressed)  
**Update**: Immediate on E-Stop state change  
**Retained**: No  
**Priority**: Highest priority safety input  

### `controller/safety/engine`
**Direction**: Publish  
**Purpose**: Engine operational status  
**Format**: `STOPPED` or `RUNNING`  
**Update**: On engine state change  
**Retained**: No  

### `controller/safety/pressure_current`
**Direction**: Publish  
**Purpose**: Current pressure reading (PSI) for safety monitoring  
**Format**: Floating point string  
**Update**: Periodic during safety monitoring  
**Retained**: No  

### `controller/safety/pressure_threshold`
**Direction**: Publish  
**Purpose**: Safety pressure limit (PSI)  
**Format**: Floating point string (default: `2500.0`)  
**Update**: On threshold configuration change  
**Retained**: No  

### `controller/safety/high_pressure_active`
**Direction**: Publish  
**Purpose**: High pressure warning state  
**Format**: `0` (normal) or `1` (high pressure)  
**Update**: On pressure threshold crossing  
**Retained**: No  

### `controller/safety/high_pressure_elapsed`
**Direction**: Publish  
**Purpose**: Time (ms) at high pressure before shutdown  
**Format**: Integer string (e.g., `5000`)  
**Update**: During high pressure condition  
**Retained**: No  
**Note**: System shuts down after configured duration  

---

## 6. Engine Control Topics

### `controller/engine/stopped`
**Direction**: Publish  
**Purpose**: Engine stopped state (inverse of running)  
**Format**: `true` or `false`  
**Update**: On engine state change via Relay R8  
**Retained**: No  

### `controller/engine/running`
**Direction**: Publish  
**Purpose**: Engine running state  
**Format**: `true` or `false`  
**Update**: On engine state change via Relay R8  
**Retained**: No  

---

## 7. Hydraulic Sequence Topics

### `controller/sequence/state`
**Direction**: Publish  
**Purpose**: Current sequence state  
**Format**: String values  
**Update**: On state transitions  
**Retained**: No  

**Possible Values**:
- `idle` - No sequence running
- `start` - Sequence initiated
- `extending` - Cylinder extending
- `retracting` - Cylinder retracting  
- `complete` - Sequence completed successfully
- `abort` - Sequence aborted (error or E-Stop)
- `manual_extend` - Manual extend active
- `manual_retract` - Manual retract active
- `limit_reached` - Limit switch triggered
- `stopped` - Manual operation stopped

---

### `controller/sequence/event`
**Direction**: Publish  
**Purpose**: Sequence events and transitions  
**Format**: String event descriptions  
**Update**: On specific events  
**Retained**: No  

**Possible Values**:
- `started_R1` - Extend relay activated
- `switched_to_R2_pressure_or_limit` - Transition to retract
- `complete_pressure_or_limit` - Sequence completed
- `manual_extend_started` - Manual extend began
- `manual_retract_started` - Manual retract began
- `manual_stopped` - Manual operation stopped
- `manual_extend_limit_reached` - Extend limit during manual
- `manual_retract_limit_reached` - Retract limit during manual

---

### `controller/sequence/active`
**Direction**: Publish  
**Purpose**: Sequence running status  
**Format**: `0` (not running) or `1` (running)  
**Update**: Periodic during sequence  
**Retained**: No  

### `controller/sequence/stage`
**Direction**: Publish  
**Purpose**: Current sequence stage number  
**Format**: Integer string (e.g., `0`, `1`, `2`)  
**Update**: Periodic during sequence  
**Retained**: No  

**Stage Values**:
- `0` - Idle
- `1` - Extending
- `2` - Retracting

### `controller/sequence/elapsed`
**Direction**: Publish  
**Purpose**: Elapsed time in current sequence (ms)  
**Format**: Integer string (e.g., `12500`)  
**Update**: Periodic during sequence  
**Retained**: No  

### `controller/sequence/disabled`
**Direction**: Publish  
**Purpose**: Sequence start disabled by conditions  
**Format**: `0` (enabled) or `1` (disabled)  
**Update**: On condition evaluation  
**Retained**: No  

---

## 8. System Status Topics

### `controller/system/uptime`
**Direction**: Publish  
**Purpose**: System uptime in milliseconds  
**Format**: Integer string (e.g., `3600000` for 1 hour)  
**Update**: Periodic (with status updates)  
**Retained**: No  

### `controller/system/memory_free`
**Direction**: Publish  
**Purpose**: Free RAM in bytes  
**Format**: Integer string  
**Update**: Periodic (with status updates)  
**Retained**: No  

### `controller/system/network_connected`
**Direction**: Publish  
**Purpose**: Network connection status  
**Format**: `0` (disconnected) or `1` (connected)  
**Update**: On network state change  
**Retained**: No  

### `controller/system/error` *(Legacy r4)*
**Direction**: Publish  
**Purpose**: System error messages  
**Format**: String error description  
**Update**: On error occurrence  
**Retained**: No  
**Note**: Legacy topic still using `r4/` prefix, may be updated in future

### `controller/system/error_count` *(Legacy r4)*
**Direction**: Publish  
**Purpose**: Total error count  
**Format**: Integer string  
**Update**: On error occurrence  
**Retained**: No  
**Note**: Legacy topic still using `r4/` prefix, may be updated in future

---

## 9. Configuration Topics (Retained)

All configuration topics use the **retained flag**, meaning the MQTT broker stores the last value and delivers it to new subscribers.

**Update**: On configuration changes via `set` commands  
**Retained**: Yes (all config topics)  
**Direction**: Publish  

---

### Pressure Sensor A1 Configuration

| Topic | Purpose | Format | Default |
|-------|---------|--------|---------|
| `controller/config/a1_max_pressure` | A1 max pressure (PSI) | Float | `5000.0` |
| `controller/config/a1_adc_vref` | A1 ADC reference voltage | Float | `5.0` |
| `controller/config/a1_sensor_gain` | A1 calibration gain | Float | `1.0` |
| `controller/config/a1_sensor_offset` | A1 calibration offset | Float | `0.0` |

---

### Pressure Sensor A5 Configuration

| Topic | Purpose | Format | Default |
|-------|---------|--------|---------|
| `controller/config/a5_max_pressure` | A5 max pressure (PSI) | Float | `30.0` |
| `controller/config/a5_adc_vref` | A5 ADC reference voltage | Float | `5.0` |
| `controller/config/a5_sensor_gain` | A5 calibration gain | Float | `1.0` |
| `controller/config/a5_sensor_offset` | A5 calibration offset | Float | `0.0` |

---

### Sequence Timing Configuration

| Topic | Purpose | Format | Default |
|-------|---------|--------|---------|
| `controller/config/seq_stable_ms` | Sequence stable time (ms) | Integer | `15` |
| `controller/config/seq_start_stable_ms` | Sequence start stable time (ms) | Integer | `100` |
| `controller/config/seq_timeout_ms` | Sequence timeout (ms) | Integer | `30000` |

---

### Filter Configuration

| Topic | Purpose | Format | Default |
|-------|---------|--------|---------|
| `controller/config/ema_alpha` | EMA filter alpha coefficient | Float | `0.1` |
| `controller/config/filter_mode` | Filter type | Integer | `2` (EMA) |

**Filter Mode Values**:
- `0` = No filtering
- `1` = Median3 filter
- `2` = EMA (Exponential Moving Average)

---

### System Configuration

| Topic | Purpose | Format | Default |
|-------|---------|--------|---------|
| `controller/config/relay_echo` | Relay command echo | `true`/`false` | `false` |
| `controller/config/debug_enabled` | Debug output enabled | `true`/`false` | `false` |
| `controller/config/log_level` | Syslog level (0-7) | Integer | `6` |
| `controller/config/pin_modes_bitmap` | Pin configuration bitmap | Integer | System dependent |

---

### Pin Configuration

Each monitored pin (2,3,4,5,6,7,12) has a configuration topic:

`controller/config/pin{N}_mode` where N is the pin number

**Format**: String  
**Values**: `INPUT`, `INPUT_PULLUP`, `OUTPUT`

**Examples**:
- `controller/config/pin2_mode` → `INPUT_PULLUP`
- `controller/config/pin6_mode` → `INPUT_PULLUP`
- `controller/config/pin12_mode` → `INPUT_PULLUP`

---

### Legacy Configuration Topics

| Topic | Purpose | Format | Note |
|-------|---------|--------|------|
| `controller/config/legacy_adc_vref` | Legacy ADC reference | Float | Backward compatibility |
| `controller/config/legacy_max_pressure` | Legacy max pressure | Float | Backward compatibility |

---

### Configuration Metadata

| Topic | Purpose | Format |
|-------|---------|--------|
| `controller/config/config_valid` | Configuration CRC valid | `true`/`false` |
| `controller/config/config_magic` | Configuration magic number | Hex string |

---

## Topic Update Frequencies

| Category | Update Rate | Notes |
|----------|-------------|-------|
| Inputs | Event-driven | On debounced state change |
| Relays | Event-driven | On relay command execution |
| Pressure | ~100ms | Filtered, published on significant change |
| Safety | Event-driven | Immediate on safety events |
| Sequence | Variable | State: on transition, Status: periodic during sequence |
| Engine | Event-driven | On R8 relay state change |
| System | Periodic | With status command, ~1-5 second intervals |
| Config | On change | When `set` commands modify values |

---

## Data Type Formats

| Type | Format | Examples |
|------|--------|----------|
| Boolean (numeric) | `0` or `1` | `0`, `1` |
| Boolean (text) | `true` or `false` | `true`, `false` |
| Integer | Decimal string | `123`, `30000`, `-5` |
| Float | Decimal string with period | `1234.5`, `3.14`, `0.1` |
| String | Plain text | `RUNNING`, `manual_extend`, `limit_reached` |
| State | Descriptive string | `idle`, `extending`, `complete` |

---

## Quality of Service (QoS)

**Default QoS**: 0 (At most once)  
**Retained Topics**: All `controller/config/*` topics use retained flag  
**Non-Retained**: All other topics (inputs, relays, pressure, safety, sequence)

---

## Message Size Limits

**Maximum Topic Length**: 64 characters  
**Maximum Payload Length**: 256 characters (typically much smaller)  
**Typical Payload Sizes**:
- Boolean: 1-5 bytes
- Integer: 1-10 bytes
- Float: 3-12 bytes
- String: 5-50 bytes

---

## MQTT Connection Parameters

**Default Broker**: 192.168.1.155:1883 (configurable in `arduino_secrets.h`)  
**Client ID**: `logsplitter_<MAC_ADDRESS>`  
**Authentication**: Optional (configure in `arduino_secrets.h`)  
**Reconnect Interval**: 5 seconds  
**Max Retries**: 3 attempts before extended backoff  

---

## Network Behavior

### Connection Management
- **WiFi Timeout**: 20 seconds
- **MQTT Reconnect**: 5 seconds between attempts
- **Max Reconnect Attempts**: 3 (then extended backoff)
- **Network Bypass Mode**: Automatic on excessive delays (>500ms publish or poll)

### Performance Protection
- **Publish Timeout Warning**: >100ms
- **Poll Timeout Warning**: >50ms
- **Auto-Bypass Trigger**: >500ms delay in MQTT operations
- **Bypass Mode**: Suppresses MQTT publishing to maintain control loop timing

---

## Special Behaviors

### Safety System Integration
- E-Stop activation (Pin 12) → Immediate `controller/safety/estop` = `1`
- All relays turn OFF → Published immediately
- Engine stops → `controller/engine/running` = `false`
- Safety active → `controller/safety/active` = `1`

### Sequence Operation
- State changes publish to both `state` and `event` topics
- Manual operations publish distinct event types
- Limit switches trigger immediate state updates

### Input Debouncing
- All inputs debounced before publishing
- Separate debounce timings for buttons vs limit switches
- Only publishes on stable state changes (no noise/glitches)

### Configuration Persistence
- All `controller/config/*` topics published with retain flag
- MQTT broker stores values for recovery
- `set mqtt_defaults` command queries broker for stored values
- EEPROM stores values with CRC32 validation

---

## Testing and Debugging

### Test Topics
`r4/test` - Legacy test message topic (may be updated)

### Debug Commands
Enable detailed MQTT logging via serial:
```
> debug ON
> mqtt
```

### Status Query
Get comprehensive system status:
```
> show
```
Publishes current values to multiple topics

---

## Legacy Topics

Some topics still use the `r4/` prefix from earlier versions:

| Legacy Topic | Status | Replacement |
|--------------|--------|-------------|
| `r4/test` | Active | N/A (test only) |
| `r4/system/error` | Active | May migrate to `controller/system/error` |
| `r4/system/error_count` | Active | May migrate to `controller/system/error_count` |
| `r4/configparam/*` | **Deprecated** | Now `controller/config/*` |

**Note**: All configuration topics have been migrated to `controller/config/*` with retained flag support.

---

## Integration Examples

### Monitor Input State (Python)
```python
import paho.mqtt.client as mqtt

def on_message(client, userdata, msg):
    if msg.topic == "controller/inputs/12/active":
        if msg.payload.decode() == "true":
            print("EMERGENCY STOP ACTIVATED!")

client = mqtt.Client()
client.on_message = on_message
client.connect("192.168.1.155", 1883)
client.subscribe("controller/inputs/12/active")
client.loop_forever()
```

### Query Configuration (Retained Values)
```python
def on_connect(client, userdata, flags, rc):
    client.subscribe("controller/config/#")  # Receives all retained config

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect("192.168.1.155", 1883)
```

### Send Command
```python
client.publish("controller/control", "relay R1 ON")
```

### Monitor Response
```python
client.subscribe("controller/control/resp")  # Receive command responses
```

---

## Document Version

**Version**: 1.0  
**Date**: October 14, 2025  
**Controller Firmware**: LogSplitter Controller v2.0  
**Hardware**: Arduino UNO R4 WiFi  

---

## Related Documents

- **PINS.md** - Hardware pin assignments and specifications
- **Controller_Commands.md** - Command reference and usage
- **Testing_TEMP.md** - Testing interface specification (work in progress)
- **SYSLOG.md** - Syslog integration documentation
