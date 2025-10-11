# LogSplitter Controller Pin Assignment Documentation

This document provides comprehensive details of all pin assignments for the Arduino UNO R4 WiFi LogSplitter Controller.

## Pin Assignment Summary

| Pin | Type | Function | Description | Active State |
|-----|------|----------|-------------|--------------|
| A0 | Analog In | Reserved | Future analog input | N/A |
| A1 | Analog In | Main Hydraulic Pressure | Primary pressure sensor (0-5000 PSI) | 0-4.5V |
| A2 | Analog In | Reserved | Future analog input | N/A |
| A3 | Analog In | Reserved | Future analog input | N/A |
| A4 | Analog In | Reserved | Future analog input | N/A |
| A5 | Analog In | Hydraulic Filter Pressure | Filter pressure sensor (0-30 PSI) | 0-4.5V |
| 0 | Digital I/O | Serial RX | USB Serial communication | N/A |
| 1 | Digital I/O | Serial TX | USB Serial communication | N/A |
| 2 | Digital In | Manual Extend Input | Manual extend control button | Active LOW |
| 3 | Digital In | Manual Retract Input | Manual retract control button | Active LOW |
| 4 | Digital In | Safety Reset/Clear Input | Saftay Stop State Reset/Clear Button | Active LOW |
| 5 | Digital In | Sequence Start Input | Auto sequence start button | Active LOW |
| 6 | Digital In | Extend Limit Switch | Hydraulic cylinder extend limit | Active LOW |
| 7 | Digital In | Retract Limit Switch | Hydraulic cylinder retract limit | Active LOW |
| 8 | Digital In | Splitter Operator Signal | Splitter operator input for basket exchange | Active LOW |
| 9 | Digital Out | Mill Lamp (Yellow) | Mill lamp indicator controlled by Relay 9 | Active HIGH |
| 10 | Digital I/O | Reserved | Future digital I/O | N/A |
| 11 | Digital I/O | Reserved | Future digital I/O | N/A |
| 12 | Digital In | **Emergency Stop (E-Stop)** | Emergency stop input | Active LOW |
| 13 | Digital Out | Status LED | Built-in LED status indicator | Active HIGH |
| Serial1 | UART | Relay Control | Hardware relay controller communication | 115200 baud |

## Detailed Pin Descriptions

### Analog Inputs

#### A1 - System Hydraulic Pressure Sensor (4-20mA Current Loop)
- **Function**: Primary hydraulic system pressure monitoring
- **Range**: 0-5000 PSI (4mA-20mA)
- **Voltage**: 1-5V input (4mA×250Ω to 20mA×250Ω)
- **Circuit**: 24V supply → Sensor → 250Ω resistor → GND
- **Sampling**: 100ms intervals with digital filtering
- **Safety**: Critical for over-pressure protection
- **MQTT Topic**: `r4/pressure/hydraulic_system`

#### A5 - Filter Hydraulic Pressure Sensor (0-5V Voltage Output)
- **Function**: Hydraulic filter pressure monitoring  
- **Range**: 0-30 PSI (filter pressure monitoring)
- **Voltage**: 0-5V input (direct voltage output)
- **Circuit**: Sensor power → 5V, Signal → A5, Ground → GND
- **Sampling**: 100ms intervals with digital filtering
- **Purpose**: Filter condition monitoring and diagnostics
- **MQTT Topic**: `r4/pressure/hydraulic_filter`

### Digital Inputs

#### Pin 2 - Manual Extend Input
- **Function**: Manual control for hydraulic cylinder extension
- **Configuration**: INPUT_PULLUP (normally open button to ground)
- **Active State**: LOW (button pressed)
- **Debouncing**: 15ms software debounce
- **Purpose**: Allows operator to manually extend cylinder independent of automatic sequence
- **Safety Integration**: Automatically stops when extend limit (Pin 6) is reached
- **Response**: Activates extend relay (R1) while button is held

#### Pin 3 - Manual Retract Input
- **Function**: Manual control for hydraulic cylinder retraction
- **Configuration**: INPUT_PULLUP (normally open button to ground)
- **Active State**: LOW (button pressed)
- **Purpose**: Allows operator to manually retract cylinder independent of automatic sequence
- **Safety Integration**: Automatically stops when retract limit (Pin 7) is reached
- **Response**: Activates retract relay (R2) while button is held

#### Pin 4 - Safety Clear Input
- **Function**: Safety system reset for operational recovery
- **Configuration**: INPUT_PULLUP (normally open safety clear button)
- **Active State**: LOW (safety clear button pressed)
- **Priority**: Management override - clears safety system but preserves error history
- **Response**: Clears safety lockouts to restore normal operation, mill light errors remain active
- **Usage**: Allows managing operator to restore operation after reviewing/addressing fault causes
- **Note**: Error history must be cleared separately via `error clear` command after fault resolution

#### Pin 5 - Sequence Start Input
- **Function**: Initiates automatic hydraulic sequence (extend then retract)
- **Configuration**: INPUT_PULLUP (normally open button to ground)
- **Active State**: LOW (button pressed momentarily)
- **Purpose**: Single-button start for complete automatic cycle
- **Safety Checks**: Verifies system is safe before starting sequence
- **Response**: Begins automatic extend → retract → idle cycle

#### Pin 6 - Extend Limit Switch
- **Function**: Detects when hydraulic cylinder reaches full extension
- **Configuration**: INPUT_PULLUP (normally open switch to ground)
- **Active State**: LOW (switch closed/activated)
- **Debouncing**: Hardware debouncing recommended + 10ms software debounce
- **Safety**: Critical for sequence control and over-travel protection
- **Manual Safety**: Automatically stops extend relay (R1) when limit reached
- **Response**: Triggers sequence state change, safety checks, and manual operation protection

#### Pin 7 - Retract Limit Switch
- **Function**: Detects when hydraulic cylinder reaches full retraction
- **Configuration**: INPUT_PULLUP (normally open switch to ground)
- **Active State**: LOW (switch closed/activated)
- **Debouncing**: Hardware debouncing recommended + 10ms software debounce
- **Safety**: Critical for sequence control and over-travel protection
- **Manual Safety**: Automatically stops retract relay (R2) when limit reached
- **Response**: Triggers sequence state change, safety checks, and manual operation protection

#### Pin 8 - Splitter Operator Signal
- **Function**: Input signal from splitter operator to notify loader operator for basket exchange
- **Configuration**: INPUT_PULLUP (normally open button or switch to ground)
- **Active State**: LOW (button pressed/switch activated)
- **Purpose**: Communication between splitter and loader operators
- **MQTT**: Publishes immediate status changes to `r4/data/splitter_operator`
- **Response**: Triggers MQTT notification and serial console message

#### Pin 12 - Emergency Stop (E-Stop)
- **Function**: Emergency shutdown of all hydraulic operations
- **Configuration**: INPUT_PULLUP (normally open E-Stop button)
- **Active State**: LOW (E-Stop button pressed)
- **Priority**: Highest priority safety input
- **Response**: Immediate system shutdown, all relays OFF
- **Reset**: Manual reset required after E-Stop activation
- **Standards**: Should meet IEC 60947-5-5 Category 0 stop requirements

### Digital Outputs

#### Pin 9 - Mill Lamp (Yellow)
- **Function**: Mill lamp indicator controlled by Relay 9 (relay board power control)
- **Configuration**: OUTPUT (drives mill lamp circuit)
- **States**:
  - **OFF**: Normal operation, no mill lamp indication needed
  - **ON**: Mill lamp activated (indicates operational status or alerts)
- **Control**: Controlled via Relay 9 commands (`relay R9 ON/OFF`)
- **Circuit**: Pin 9 controls Relay 9 which switches mill lamp power
- **Purpose**: Visual indicator for mill operational status

#### Pin 13 - Status LED
- **Function**: System status indication
- **Configuration**: OUTPUT (built-in LED)
- **States**:
  - **Solid ON**: System operational, all connections good
  - **Slow Blink (1Hz)**: Network disconnected, system operational
  - **Fast Blink (5Hz)**: Safety warning or system error
  - **OFF**: System fault or not initialized

### Communication Interfaces

#### Serial (USB) - Pins 0/1
- **Baud Rate**: 115200
- **Function**: Primary command interface and diagnostics
- **Access**: Full system control including PIN configuration
- **Commands**: All commands available (help, show, debug, network, pins, set, relay)

#### Serial1 (Hardware UART)
- **Baud Rate**: 9600
- **Function**: Relay controller communication
- **Protocol**: Simple ASCII commands (R1 ON, R2 OFF, etc.)
- **Relays**: Controls R1-R8 external relay board
- **Safety**: Hardware relay control for hydraulic valves and engine systems

#### WiFi (Built-in)
- **Function**: MQTT communication and remote monitoring
- **Topics**: Status publishing and command reception
- **Failsafe**: Network issues cannot affect hydraulic control
- **Security**: PIN commands restricted to Serial interface only

## Relay Controller System

The LogSplitter Controller uses an 8-channel relay board connected via Serial1 (Hardware UART) for controlling high-power industrial equipment. All relays are optically isolated and capable of switching AC/DC loads.

### Relay Output Assignments

| Relay | Function | Load Type | Safety Critical | Default State |
|-------|----------|-----------|-----------------|---------------|
| R1 | Hydraulic Extend | 24V DC Solenoid | Yes | OFF |
| R2 | Hydraulic Retract | 24V DC Solenoid | Yes | OFF |
| R3 | Reserved | TBD | No | OFF |
| R4 | Reserved | TBD | No | OFF |
| R5 | Reserved | TBD | No | OFF |
| R6 | Reserved | TBD | No | OFF |
| R7 | Splitter Operator Signal Buzzer | 12V DC Control Signal | No | OFF |
| R8 | Engine Enable/Disable | 12V DC Control Signal | Yes | OFF |
| R9 | Mill Lamp (Yellow) / Relay Board Power | Mill Lamp Circuit | No | OFF |

### Detailed Relay Descriptions

#### Relay 1 (R1) - Hydraulic Extend
- **Function**: Controls hydraulic cylinder extension valve
- **Load**: 24V DC hydraulic solenoid valve (extend direction)
- **Current Rating**: Up to 10A continuous
- **Safety Integration**: 
  - Automatically disabled when extend limit switch (Pin 6) is reached
  - Pressure monitoring integration prevents over-pressure operation
  - Emergency stop (Pin 12) immediately disables
- **Manual Control**: Via `relay R1 ON/OFF` commands or Pin 2 manual extend button
- **Automatic Control**: Controlled during hydraulic sequences
- **MQTT Topic**: `r4/relays/R1` (status publishing)

#### Relay 2 (R2) - Hydraulic Retract  
- **Function**: Controls hydraulic cylinder retraction valve
- **Load**: 24V DC hydraulic solenoid valve (retract direction)
- **Current Rating**: Up to 10A continuous
- **Safety Integration**:
  - Automatically disabled when retract limit switch (Pin 7) is reached
  - Pressure monitoring integration prevents over-pressure operation
  - Emergency stop (Pin 12) immediately disables
- **Manual Control**: Via `relay R2 ON/OFF` commands or Pin 3 manual retract button
- **Automatic Control**: Controlled during hydraulic sequences
- **MQTT Topic**: `r4/relays/R2` (status publishing)

#### Relay 8 (R8) - Engine Enable/Disable
- **Function**: Controls engine enable/disable signal for log splitter engine
- **Load**: 12V DC control signal to engine control module
- **Current Rating**: Up to 5A continuous
- **Safety Integration**:
  - Emergency stop (Pin 12) immediately disables engine
  - Automatic shutdown during system faults
  - Integrated with hydraulic safety systems
- **Control**: Via `relay R8 ON/OFF` commands for engine enable/disable
- **Default State**: OFF (engine disabled) for safety
- **MQTT Topic**: `r4/relays/R8` (status publishing)

#### Relays 3-6 (R3-R6) - Reserved

- **Function**: Available for future expansion
- **Load**: Configurable based on application needs
- **Current Rating**: Up to 10A per relay
- **Control**: Via `relay R3-R6 ON/OFF` commands
- **Default State**: OFF
- **Applications**: Future auxiliary equipment, lighting, cooling systems, etc.

#### Relay 7 (R7) - Splitter Operator Signal Buzzer

- **Function**: Audio warning signal to splitter operator for high pressure conditions
- **Load**: 12V DC buzzer or alarm horn (up to 5A)
- **Current Rating**: Up to 10A continuous
- **Safety Integration**:
  - Activates when hydraulic pressure exceeds warning threshold
  - Provides 10-second audible warning before automatic system shutdown
  - Emergency stop (Pin 12) immediately disables buzzer
  - Integrated with pressure monitoring system for predictive warnings
- **Control**: 
  - Automatic activation via pressure monitoring system
  - Manual control via `relay R7 ON/OFF` commands for testing
- **Warning Sequence**:
  1. Pressure exceeds configurable warning threshold (default: 90% of max pressure)
  2. R7 activates buzzer for 10-second warning period
  3. After 10 seconds, if pressure still high, hydraulic system shuts down
  4. Buzzer continues until pressure drops below threshold or manual reset
- **Default State**: OFF
- **MQTT Topic**: `r4/relays/R7` (status publishing)
- **Configuration**: Warning pressure threshold configurable via `set pressure_warning_psi` command

### Relay Board Specifications

#### Electrical Specifications
- **Control Voltage**: 5V DC (Arduino logic level)
- **Control Current**: ~15mA per relay coil
- **Contact Rating**: 10A @ 250V AC, 10A @ 30V DC
- **Isolation**: 2500V optical isolation between control and load circuits
- **Response Time**: ~10ms activation, ~5ms deactivation
- **Operating Temperature**: -10°C to +70°C
- **Humidity**: 5% to 85% RH (non-condensing)

#### Communication Protocol
- **Interface**: Serial1 (Hardware UART)
- **Baud Rate**: 9600 bps, 8N1
- **Commands**: ASCII text format
  - `R1 ON` - Turn relay 1 ON
  - `R1 OFF` - Turn relay 1 OFF
  - `ALL OFF` - Turn all relays OFF (emergency)
- **Response**: Echo confirmation or error message
- **Timeout**: 100ms command timeout with retry logic

### Relay Board Wiring

#### Control Connection (Serial1)
```
Arduino Pin 0 (RX1)     → Relay Board TX
Arduino Pin 1 (TX1)     → Relay Board RX  
Arduino 5V              → Relay Board VCC
Arduino GND             → Relay Board GND
```

#### Power Supply Requirements
```
Relay Board Power:
24V DC Power Supply (+) → Relay Board VIN
24V DC Power Supply (-) → Relay Board GND
Arduino GND             → 24V Power Supply GND (common ground)

Load Power (separate supplies recommended):
Hydraulic Solenoids:    24V DC @ 10A supply
Engine Control:         12V DC @ 5A supply
```

#### Hydraulic Solenoid Connections
```
Extend Solenoid Wiring:
24V Supply (+)          → Solenoid Valve (+)
Relay R1 Common (C)     → 24V Supply (+)
Relay R1 NO Contact     → Solenoid Valve (-)
Solenoid Ground         → 24V Supply (-)

Retract Solenoid Wiring:
24V Supply (+)          → Solenoid Valve (+)  
Relay R2 Common (C)     → 24V Supply (+)
Relay R2 NO Contact     → Solenoid Valve (-)
Solenoid Ground         → 24V Supply (-)
```

#### Engine Control Connection
```
Engine Enable Wiring:
12V Supply (+)          → Engine Control Module (+12V)
Relay R8 Common (C)     → 12V Supply (+)
Relay R8 NO Contact     → Engine Control Enable Input
Engine Control Ground   → 12V Supply (-)
```

#### Splitter Operator Buzzer Connection (R7)
```
Buzzer Wiring:
12V Supply (+)          → Buzzer/Alarm Horn (+12V)
Relay R7 Common (C)     → 12V Supply (+)
Relay R7 NO Contact     → Buzzer/Alarm Horn (-)
Buzzer Ground           → 12V Supply (-)

Recommended Buzzer Specifications:
- Operating Voltage: 12V DC
- Current Draw: 0.5-2A typical
- Sound Level: 85-105 dB @ 1 meter
- IP Rating: IP65 or higher for outdoor use
- Mounting: Panel mount or enclosed housing
```

### Safety Systems Integration

#### Emergency Stop Behavior
When Emergency Stop (Pin 12) is activated:
1. **Immediate Response**: Serial1 sends `ALL OFF` command to relay board
2. **Hydraulic Safety**: R1 and R2 immediately turn OFF, stopping all hydraulic movement
3. **Engine Safety**: R8 immediately turns OFF, disabling engine
4. **Buzzer Safety**: R7 immediately turns OFF, silencing pressure warning buzzer
5. **System Lock**: All relays remain OFF until manual system reset
6. **Status Logging**: Emergency stop event logged with relay states

#### Limit Switch Integration
- **Extend Limit (Pin 6)**: Automatically sends `R1 OFF` when limit reached during extend operation
- **Retract Limit (Pin 7)**: Automatically sends `R2 OFF` when limit reached during retract operation
- **Override Protection**: Manual override flag prevents accidental relay activation during limit conditions

#### Pressure Safety Integration
- **Over-Pressure Warning**: R7 buzzer activates when pressure exceeds warning threshold (90% of max)
- **10-Second Warning Period**: Buzzer provides 10-second warning before automatic system shutdown
- **Over-Pressure Shutdown**: Automatically disables R1/R2 when pressure exceeds safety limits after warning period
- **Under-Pressure**: Prevents hydraulic operation when insufficient pressure detected
- **Pressure Monitoring**: Continuous monitoring with relay control integration
- **Buzzer Reset**: R7 automatically turns OFF when pressure drops below warning threshold

### Command Examples

#### Manual Relay Control
```bash
# Manual hydraulic extend
> relay R1 ON
relay R1 ON

# Manual hydraulic retract  
> relay R2 ON
relay R2 ON

# Engine enable
> relay R8 ON
relay R8 ON

# Buzzer test (manual activation)
> relay R7 ON
relay R7 ON

# Buzzer off
> relay R7 OFF
relay R7 OFF

# Emergency all off
> relay ALL OFF
ALL RELAYS OFF
```

#### Status Monitoring
```bash
# Check relay status
> show
Relays: R1=OFF(EXTEND) R2=OFF(RETRACT) R3=OFF R4=OFF R5=OFF R6=OFF R7=OFF(BUZZER) R8=OFF(ENGINE)

# MQTT status topics
r4/relays/R1 → 0 (OFF) or 1 (ON)
r4/relays/R2 → 0 (OFF) or 1 (ON)
r4/relays/R7 → 0 (OFF) or 1 (ON)
r4/relays/R8 → 0 (OFF) or 1 (ON)

# Pressure warning example
> # When pressure exceeds warning threshold (automatic)
PRESSURE WARNING: 4750 PSI exceeds threshold (4500 PSI)
R7 BUZZER ON - 10 second warning before shutdown
# [10 seconds later if pressure still high]
PRESSURE SHUTDOWN: Hydraulic system disabled
R1 OFF, R2 OFF (hydraulic valves closed)
```

### Troubleshooting

#### Common Relay Issues

##### Relay Not Responding
- **Symptom**: Relay command sent but no physical relay activation
- **Check**: 
  - Serial1 wiring (TX/RX, power, ground)
  - Relay board power supply (24V)
  - Command syntax and relay number
- **Debug**: Use `debug ON` to see Serial1 communication

##### Hydraulic Solenoid Not Operating
- **Symptom**: Relay activates but hydraulic valve doesn't operate
- **Check**:
  - 24V solenoid power supply
  - Solenoid wiring to relay contacts
  - Solenoid coil continuity
  - Hydraulic system pressure
- **Test**: Measure voltage across solenoid when relay is ON

##### Engine Control Not Responding
- **Symptom**: R8 relay activates but engine doesn't enable
- **Check**:
  - 12V control signal power supply
  - Engine control module wiring
  - Engine control module configuration
  - Engine safety interlocks
- **Test**: Measure 12V signal at engine control input when R8 is ON

##### Buzzer Not Operating (R7)
- **Symptom**: R7 relay activates but buzzer doesn't sound
- **Check**:
  - 12V buzzer power supply
  - Buzzer wiring to relay contacts
  - Buzzer continuity and polarity
  - Buzzer current draw (should be < 5A)
- **Test**: Measure 12V across buzzer when R7 is ON
- **Manual Test**: Use `relay R7 ON` to test buzzer operation

##### Pressure Warning Not Triggering
- **Symptom**: High pressure but buzzer doesn't activate automatically
- **Check**:
  - Pressure sensor readings (`show` command)
  - Warning threshold configuration (`set pressure_warning_psi`)
  - Pressure monitoring system functionality
  - R7 relay operation (manual test)
- **Debug**: Monitor pressure values and threshold settings

#### Debug Commands
```bash
> debug ON              # Enable detailed relay communication logging
> show                  # Display all relay states
> relay R1 ON           # Test individual relay
> relay ALL OFF         # Emergency all relays off
```

## Safety System Integration

### Critical Input Monitoring
The safety system continuously monitors these inputs with high priority:

1. **Emergency Stop (Pin 12)** - Highest priority
2. **Pressure Sensors (A1, A5)** - Over-pressure protection
3. **Limit Switches (Pins 6, 7)** - Over-travel protection

### Emergency Stop Behavior
When E-Stop is activated (Pin 12 goes LOW):
1. **Immediate Response**: All relays turn OFF within 10ms
2. **Sequence Abort**: Current hydraulic sequence immediately terminated
3. **Safety Lock**: System enters EMERGENCY_STOP state
4. **Manual Reset**: E-Stop must be released and system manually restarted
5. **Status Indication**: LED fast blink pattern
6. **Logging**: E-Stop event logged with timestamp

### Manual Operation Safety
The system provides automatic protection during manual hydraulic operations:

#### Extend Limit Safety (Pin 6)
- **Trigger**: When extend limit switch (Pin 6) activates during manual operation
- **Action**: Automatically turns OFF extend relay (R1) to prevent over-travel
- **Logging**: "SAFETY: Extend operation stopped - limit switch reached"
- **Override**: Uses manual override flag to ensure operation during safety conditions

#### Retract Limit Safety (Pin 7)
- **Trigger**: When retract limit switch (Pin 7) activates during manual operation
- **Action**: Automatically turns OFF retract relay (R2) to prevent over-travel
- **Logging**: "SAFETY: Retract operation stopped - limit switch reached"
- **Override**: Uses manual override flag to ensure operation during safety conditions

#### Safety Operation Flow
```
Manual Command: relay R1 ON     # User starts extend
System Response: relay R1 ON    # Relay activates
[Hydraulic cylinder extends...]
Limit Reached: Pin 6 ACTIVE     # Limit switch triggers
Safety Action: R1 OFF           # Automatic relay shutdown
User Feedback: "SAFETY: Extend operation stopped - limit switch reached"
```

### Input Prioritization
1. **Emergency Stop** - Overrides all other inputs
2. **Limit Switches** - Override sequence commands and stop manual operations
3. **Pressure Sensors** - Trigger safety shutdowns
4. **Splitter Operator Signal** - Communication between operators
5. **Network Commands** - Lowest priority, fail-safe

## Electrical Specifications

### Digital Input Requirements
- **Voltage Levels**: 5V CMOS (0V = LOW, 5V = HIGH)
- **Input Impedance**: ~50kΩ with internal pullup enabled
- **Maximum Voltage**: 5.5V (do not exceed)
- **Current Consumption**: ~100µA per input with pullup

### Analog Input Requirements
- **Voltage Range**: 0-5V (1-5V for 4-20mA current loop sensors)
- **Resolution**: 10-bit (1024 levels, ~4.9mV per step)
- **Input Impedance**: ~100MΩ
- **Sampling Rate**: Configurable, default 10 samples/second per channel
- **Current Loop**: 4-20mA with 250Ω shunt resistor (1V-5V = 0-5000 PSI)

### Digital Output Specifications
- **Output Voltage**: 0V (LOW) to 5V (HIGH)
- **Current Capacity**: 20mA per pin maximum
- **Total Current**: 200mA total for all I/O pins

## Wiring Recommendations

### System Pressure Sensor (A1 - 4-20mA Current Loop)
```
24V Power Supply (+)    → Pressure Sensor Pin 1 (Power)
Pressure Sensor Pin 2  → 250Ω Resistor → Arduino GND
Arduino A1              → 250Ω Resistor (measure voltage drop)
24V Power Supply (-)    → Arduino GND
Shield/Case             → GND (if metallic)

Current Loop Operation:
- 4mA (0 PSI) × 250Ω = 1.0V at Arduino A1
- 20mA (5000 PSI) × 250Ω = 5.0V at Arduino A1
- Voltage range: 1V-5V represents 0-5000 PSI
```

### Filter Pressure Sensor (A5 - 0-5V Voltage Output)
```
Sensor Pin 1 (Power)    → 5V
Sensor Pin 2 (Ground)   → GND  
Sensor Pin 3 (Signal)   → Arduino A5
Shield/Case             → GND (if metallic)

Voltage Output:
- 0V = 0 PSI
- 5V = 5000 PSI (configurable maximum)
- Direct voltage output, no current loop
```

### Limit Switches
```
Switch Common          → Pin 6 or 7
Switch NO Contact      → GND
Shield (if used)       → GND
```

### Emergency Stop
```
E-Stop NC Contact 1    → Pin 12
E-Stop NC Contact 2    → GND
E-Stop Case           → GND (safety ground)
```

### Splitter Operator Signal
```
Signal Switch NO       → Pin 8 (internal pullup enabled)
Signal Switch Common   → GND
Shield (if used)       → GND
```

### Status LED (External)
```
LED Anode (+)         → Pin 13
LED Cathode (-)       → 220Ω Resistor → GND
```

## Configuration Commands

### PIN Mode Configuration (Serial Only)
```
> set pinmode 6 INPUT_PULLUP    # Configure extend limit
> set pinmode 7 INPUT_PULLUP    # Configure retract limit  
> set pinmode 12 INPUT_PULLUP   # Configure E-Stop input
> set pinmode 13 OUTPUT         # Configure status LED
```

### View Current PIN Configuration
```
> pins
Pin 0: INPUT_PULLUP
Pin 1: OUTPUT
...
Pin 12: INPUT_PULLUP
Pin 13: OUTPUT
```

## Troubleshooting

### Common Issues

#### Limit Switch Problems
- **Symptom**: Sequence doesn't stop at limits
- **Check**: Wiring, switch type (NO vs NC), pin configuration
- **Debug**: Use `debug ON` to see input state changes

#### E-Stop Not Working
- **Symptom**: E-Stop press doesn't stop system
- **Check**: Wiring, switch contacts, pin 12 configuration
- **Test**: Monitor pin state with `debug ON` while pressing E-Stop

#### Pressure Sensor Issues
- **Symptom**: Incorrect pressure readings
- **Check**: Sensor power supply, signal wiring, calibration
- **Commands**: Use `show` to see current readings, `set vref` to calibrate

### Debug Commands
```
> debug ON              # Enable detailed diagnostics
> show                  # Display all system status
> network               # Check network connectivity
> pins                  # Show PIN configurations
```

## Safety Certifications

### Recommended Standards Compliance
- **IEC 61508**: Functional Safety for Electrical Systems
- **IEC 60947-5-5**: Emergency Stop Device Requirements
- **OSHA 29 CFR 1910**: Occupational Safety Standards
- **NFPA 79**: Electrical Standard for Industrial Machinery

### Installation Notes
- Install E-Stop within easy reach of operator
- Use Category 0 stop function (immediate power removal)
- Ensure E-Stop button is red with yellow background
- Provide manual reset capability after E-Stop activation
- Test E-Stop function before each use

---

**Document Version**: 1.0  
**Last Updated**: September 28, 2025  
**Hardware**: Arduino UNO R4 WiFi  
**Firmware**: LogSplitter Controller v2.0