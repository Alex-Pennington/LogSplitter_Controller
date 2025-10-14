# **LogSplitter Controller MQTT Output Documentation**

## **Overview**
The LogSplitter Controller is an Arduino-based hydraulic log splitter automation system that publishes real-time operational data to MQTT topics using a hierarchical structure under `controller/`. All values are published when they change or periodically for monitoring.

**System Context**: This controller manages hydraulic cylinder operations, pressure monitoring, safety systems, and input/output control for an automated log splitting machine. The MQTT interface allows remote monitoring and control integration.

**Hardware Platform**: Arduino UNO R4 WiFi with relay control board, pressure sensors, limit switches, and safety inputs.

---

## **🎛️ Command & Control Topics**

### **`controller/control`**
- **Direction**: Input (Controller subscribes)
- **Purpose**: Command input topic for controlling the system
- **Format**: Text commands (e.g., "extend", "retract", "stop", "status")
- **Example**: `extend` - Start extend sequence

### **`controller/control/resp`**
- **Direction**: Output (Controller publishes)  
- **Purpose**: Command responses and system notifications
- **Format**: Human-readable text responses
- **Example**: `"Extend sequence started"` or `"cli: relay 1 on"`

---

## **🔧 Pressure System Topics**

### **`controller/pressure`**
- **Direction**: Output
- **Purpose**: Main hydraulic system pressure (backward compatibility)
- **Format**: Float (1 decimal place)
- **Units**: PSI (Pounds per Square Inch)
- **Example**: `2450.5`

### **`controller/pressure/hydraulic_system`**
- **Direction**: Output
- **Purpose**: Primary hydraulic system pressure reading
- **Format**: Float (1 decimal place)
- **Units**: PSI
- **Range**: 0.0 - 5000.0 PSI (configurable)
- **Example**: `2450.5`

### **`controller/pressure/hydraulic_filter`**
- **Direction**: Output
- **Purpose**: Hydraulic oil/filter pressure reading
- **Format**: Float (1 decimal place)
- **Units**: PSI
- **Range**: 0.0 - 30.0 PSI (typical)
- **Example**: `12.3`

### **`controller/pressure/hydraulic_system_voltage`**
- **Direction**: Output
- **Purpose**: Raw voltage from hydraulic pressure sensor (for calibration)
- **Format**: Float (2 decimal places)
- **Units**: Volts
- **Range**: 0.00 - 5.00V
- **Example**: `3.45`

### **`controller/pressure/hydraulic_filter_voltage`**
- **Direction**: Output
- **Purpose**: Raw voltage from hydraulic filter pressure sensor
- **Format**: Float (2 decimal places)
- **Units**: Volts
- **Range**: 0.00 - 5.00V
- **Example**: `1.23`

---

## **🛡️ Safety System Topics**

### **`controller/safety/active`**
- **Direction**: Output
- **Purpose**: Overall safety system status
- **Format**: Boolean string
- **Values**: `"1"` (active/engaged) or `"0"` (inactive/normal)
- **Example**: `"0"` - Safety system not engaged

### **`controller/safety/estop`**
- **Direction**: Output
- **Purpose**: Emergency stop button status
- **Format**: Boolean string
- **Values**: `"1"` (E-Stop activated) or `"0"` (E-Stop released)
- **Example**: `"0"` - E-Stop not pressed

### **`controller/safety/engine`**
- **Direction**: Output
- **Purpose**: Engine stop relay status
- **Format**: String
- **Values**: `"STOPPED"` or `"RUNNING"`
- **Example**: `"RUNNING"` - Engine is running normally

### **`controller/safety/pressure_current`**
- **Direction**: Output
- **Purpose**: Current pressure reading used by safety system
- **Format**: Float (1 decimal place)
- **Units**: PSI
- **Example**: `2450.5`

### **`controller/safety/pressure_threshold`**
- **Direction**: Output
- **Purpose**: Pressure threshold that triggers safety system
- **Format**: Float (1 decimal place)
- **Units**: PSI
- **Default**: `2500.0`
- **Example**: `2500.0`

### **`controller/safety/high_pressure_active`**
- **Direction**: Output
- **Purpose**: High pressure monitoring state
- **Format**: Boolean string
- **Values**: `"1"` (monitoring high pressure) or `"0"` (normal pressure)
- **Example**: `"0"` - Pressure is normal

### **`controller/safety/high_pressure_elapsed`**
- **Direction**: Output
- **Purpose**: Time elapsed since high pressure condition started
- **Format**: Integer
- **Units**: Milliseconds
- **Example**: `5430` - High pressure for 5.43 seconds

---

## **⚙️ Sequence Controller Topics**

### **`controller/sequence/active`**
- **Direction**: Output
- **Purpose**: Whether any sequence is currently running
- **Format**: Boolean string
- **Values**: `"1"` (sequence active) or `"0"` (no sequence)
- **Example**: `"1"` - Sequence is running

### **`controller/sequence/stage`**
- **Direction**: Output
- **Purpose**: Current stage of active sequence
- **Format**: Integer
- **Values**: Stage number (0 = idle, 1+ = active stages)
- **Example**: `"2"` - Currently in stage 2

### **`controller/sequence/elapsed`**
- **Direction**: Output
- **Purpose**: Time elapsed since sequence started
- **Format**: Integer
- **Units**: Milliseconds
- **Example**: `"15430"` - Sequence running for 15.43 seconds

### **`controller/sequence/disabled`**
- **Direction**: Output
- **Purpose**: Whether sequence execution is disabled
- **Format**: Boolean string
- **Values**: `"1"` (disabled) or `"0"` (enabled)
- **Example**: `"0"` - Sequences are enabled

### **`controller/sequence/event`**
- **Direction**: Output
- **Purpose**: Latest sequence event that occurred
- **Format**: String
- **Values**: Event descriptions
- **Examples**:
  - `"started_R1"` - Extend sequence started
  - `"switched_to_R2_pressure_or_limit"` - Switched to retract
  - `"complete_pressure_or_limit"` - Sequence completed
  - `"manual_extend_started"` - Manual extend began
  - `"manual_extend_limit_reached"` - Manual extend hit limit

### **`controller/sequence/state`**
- **Direction**: Output
- **Purpose**: Current sequence state
- **Format**: String
- **Values**: State descriptions
- **Examples**:
  - `"start"` - Sequence starting
  - `"complete"` - Sequence finished
  - `"manual_extend"` - Manual extend mode
  - `"manual_retract"` - Manual retract mode
  - `"stopped"` - Sequence stopped
  - `"limit_reached"` - Limit switch triggered
  - `"abort"` - Sequence aborted

---

## **🔌 Relay Controller Topics**

### **`controller/relays/{N}/state`** (where N = 1-9)
- **Direction**: Output
- **Purpose**: Individual relay on/off state
- **Format**: Boolean integer
- **Values**: `"1"` (relay on) or `"0"` (relay off)
- **Example**: `controller/relays/1/state` → `"1"` (Relay 1 is ON)

### **`controller/relays/{N}/active`** (where N = 1-9)
- **Direction**: Output
- **Purpose**: Individual relay active status (human-readable)
- **Format**: Boolean string
- **Values**: `"true"` (relay active) or `"false"` (relay inactive)
- **Example**: `controller/relays/2/active` → `"false"` (Relay 2 is OFF)

### **`controller/relays/board_powered`**
- **Direction**: Output
- **Purpose**: Overall relay board power status
- **Format**: Boolean string
- **Values**: `"true"` (board powered) or `"false"` (board unpowered)
- **Example**: `"true"` - Relay board has power

### **`controller/relays/safety_mode`**
- **Direction**: Output
- **Purpose**: Whether relays are in safety mode (all hydraulic relays forced OFF)
- **Format**: Boolean string
- **Values**: `"true"` (safety mode active) or `"false"` (normal operation)
- **Example**: `"false"` - Normal relay operation

---

## **📊 Input Monitoring Topics**

### **`controller/inputs/{PIN}/state`** (where PIN = 2,3,4,5,6,7,12)
- **Direction**: Output
- **Purpose**: Digital state of input pins
- **Format**: Boolean integer
- **Values**: `"1"` (pin HIGH/activated) or `"0"` (pin LOW/not activated)
- **Example**: `controller/inputs/12/state` → `"0"` (E-Stop pin not pressed)

### **`controller/inputs/{PIN}/active`** (where PIN = 2,3,4,5,6,7,12)
- **Direction**: Output
- **Purpose**: Human-readable input pin status
- **Format**: Boolean string
- **Values**: `"true"` (pin activated) or `"false"` (pin not activated)
- **Example**: `controller/inputs/6/active` → `"true"` (Extend limit switch activated)

---

## **📍 Pin Mapping Reference**

| Pin | Purpose | Topic Examples |
|-----|---------|----------------|
| 2 | General Input | `controller/inputs/2/state`, `controller/inputs/2/active` |
| 3 | General Input | `controller/inputs/3/state`, `controller/inputs/3/active` |
| 4 | Safety Clear Button | `controller/inputs/4/state`, `controller/inputs/4/active` |
| 5 | General Input | `controller/inputs/5/state`, `controller/inputs/5/active` |
| 6 | Extend Limit Switch | `controller/inputs/6/state`, `controller/inputs/6/active` |
| 7 | Retract Limit Switch | `controller/inputs/7/state`, `controller/inputs/7/active` |
| 12 | Emergency Stop Button | `controller/inputs/12/state`, `controller/inputs/12/active` |

---

## **🔧 Relay Mapping Reference**

| Relay | Purpose | Topic Examples |
|-------|---------|----------------|
| 1 | Hydraulic Extend Valve | `controller/relays/1/state`, `controller/relays/1/active` |
| 2 | Hydraulic Retract Valve | `controller/relays/2/state`, `controller/relays/2/active` |
| 8 | Engine Stop Relay | `controller/relays/8/state`, `controller/relays/8/active` |
| 9 | Relay Board Power Control | `controller/relays/9/state`, `controller/relays/9/active` |

---

## **📡 Publication Frequency**

- **Input Changes**: Published immediately when pin states change
- **Relay Changes**: Published immediately when relay states change  
- **Pressure Values**: Published every 10 seconds during normal operation
- **Safety Status**: Published every 10 seconds or immediately on state changes
- **Sequence Events**: Published immediately when events occur
- **Command Responses**: Published immediately after command processing

---

## **🔍 Monitoring Examples**

### **Monitor All Pressure Data**
Subscribe to: `controller/pressure/+`

### **Monitor Safety System**
Subscribe to: `controller/safety/+`

### **Monitor All Inputs**
Subscribe to: `controller/inputs/+/+`

### **Monitor Specific Relay**
Subscribe to: `controller/relays/1/+`

### **Monitor All System Events**
Subscribe to: `controller/+/+`

---

## **⚠️ Important Notes**

1. **Retain Flag**: Most topics are published without the retain flag, so you'll only see current data
2. **Connection Dependencies**: Topics are only published when WiFi and MQTT are connected
3. **Safety Priority**: Safety-related topics have the highest publication priority
4. **Data Types**: All values are published as strings for maximum compatibility
5. **Backward Compatibility**: `controller/pressure` is maintained alongside the newer specific pressure topics

This MQTT structure provides comprehensive monitoring of all LogSplitter Controller systems with clear, hierarchical topic organization suitable for integration with any MQTT-based monitoring system.

---

## **🤖 AI Integration Context**

### **System Understanding for AI Assistants**
This LogSplitter Controller operates as an industrial automation system with the following key components:

- **Hydraulic System**: Controls extend/retract cylinder operations via solenoid valves
- **Pressure Monitoring**: Dual pressure sensors monitor hydraulic system and filter pressures
- **Safety Systems**: Emergency stop, pressure limits, and engine control for operator safety
- **Sequence Control**: Automated extend/retract cycles with pressure and limit-based termination
- **Input Monitoring**: Limit switches, buttons, and safety inputs with debouncing
- **Relay Control**: 9-channel relay board controlling hydraulic valves and safety systems

### **Typical Operation Flow**
1. System starts in safety mode with all hydraulic relays OFF
2. Operator initiates extend sequence via MQTT command or manual control
3. Hydraulic extend valve (Relay 1) activates, cylinder extends
4. System monitors pressure and limit switches during operation
5. When extend limit reached or pressure threshold met, switches to retract
6. Hydraulic retract valve (Relay 2) activates, cylinder retracts
7. Sequence completes when retract limit reached
8. All status published via MQTT throughout operation

### **Critical Safety Features**
- Emergency stop button immediately halts all operations
- Pressure-based safety shutdown at 2500 PSI threshold
- Engine stop relay for emergency situations
- All hydraulic operations disabled in safety mode
- Comprehensive error reporting and status monitoring

This context enables AI systems to understand operational states, troubleshoot issues, and provide intelligent monitoring and control recommendations.