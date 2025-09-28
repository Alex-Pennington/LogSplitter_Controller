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
| A5 | Analog In | Hydraulic Filter Pressure | Filter pressure sensor (0-5000 PSI) | 0-4.5V |
| 0 | Digital I/O | Serial RX | USB Serial communication | N/A |
| 1 | Digital I/O | Serial TX | USB Serial communication | N/A |
| 2 | Digital In | Reserved | Future digital input | N/A |
| 3 | Digital I/O | Reserved | Future digital I/O | N/A |
| 4 | Digital I/O | Reserved | Future digital I/O | N/A |
| 5 | Digital I/O | Reserved | Future digital I/O | N/A |
| 6 | Digital In | Extend Limit Switch | Hydraulic cylinder extend limit | Active LOW |
| 7 | Digital In | Retract Limit Switch | Hydraulic cylinder retract limit | Active LOW |
| 8 | Digital In | Splitter Operator Signal | Splitter operator input for basket exchange | Active LOW |
| 9 | Digital Out | System Error LED | Malfunction indicator lamp for non-critical faults | Active HIGH |
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
- **Range**: 0-5000 PSI (configurable)
- **Voltage**: 0-5V input (direct voltage output)
- **Circuit**: Sensor power → 5V, Signal → A5, Ground → GND
- **Voltage**: 0-4.5V input (linear mapping)
- **Sampling**: 100ms intervals with digital filtering
- **Purpose**: Filter condition monitoring and diagnostics
- **MQTT Topic**: `r4/pressure/hydraulic_filter`

### Digital Inputs

#### Pin 6 - Extend Limit Switch
- **Function**: Detects when hydraulic cylinder reaches full extension
- **Configuration**: INPUT_PULLUP (normally closed switch to ground)
- **Active State**: LOW (switch closed/activated)
- **Debouncing**: Hardware debouncing recommended
- **Safety**: Critical for sequence control and over-travel protection
- **Manual Safety**: Automatically stops extend relay (R1) when limit reached
- **Response**: Triggers sequence state change, safety checks, and manual operation protection

#### Pin 7 - Retract Limit Switch
- **Function**: Detects when hydraulic cylinder reaches full retraction
- **Configuration**: INPUT_PULLUP (normally closed switch to ground)
- **Active State**: LOW (switch closed/activated)
- **Debouncing**: Hardware debouncing recommended
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

#### Pin 9 - System Error LED (Malfunction Indicator Lamp)
- **Function**: Indicates non-critical system faults requiring maintenance attention
- **Configuration**: OUTPUT (external LED recommended)
- **States**:
  - **OFF**: No errors detected
  - **Solid ON**: Single error (e.g., configuration issue)
  - **Slow Blink (1Hz)**: Multiple errors present
  - **Fast Blink (5Hz)**: Critical errors (EEPROM CRC, memory issues)
- **Error Types**: EEPROM faults, sensor malfunctions, network issues, configuration errors
- **MQTT**: Error details published to `r4/system/error` topic
- **Acknowledgment**: Errors can be acknowledged but LED indicates until fault is cleared

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
- **Relays**: Controls R1-R9 external relay board
- **Safety**: Hardware relay control for hydraulic valves

#### WiFi (Built-in)
- **Function**: MQTT communication and remote monitoring
- **Topics**: Status publishing and command reception
- **Failsafe**: Network issues cannot affect hydraulic control
- **Security**: PIN commands restricted to Serial interface only

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
- **Resolution**: 14-bit (16384 levels, ~0.3mV per step)
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