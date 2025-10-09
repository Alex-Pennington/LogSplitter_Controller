# MCP3421 ADC Sensor Integration Summary

## Overview
Successfully integrated MCP3421 18-bit I2C ADC sensor into the LogSplitter Monitor Program with address 0x68.

## Hardware Setup
- **Sensor**: MCP3421 18-bit ADC with I2C interface
- **I2C Address**: 0x68 (7-bit address)
- **Interface**: I2C (Wire1/Qwiic interface)
- **Resolution**: 12, 14, 16, or 18-bit selectable
- **Sample Rates**: 240, 60, 15, or 3.75 SPS respectively
- **Gain**: 1x, 2x, 4x, or 8x programmable gain amplifier

## Files Created/Modified

### New Files Created
1. **monitor/include/mcp3421_sensor.h**
   - Complete MCP3421 sensor class definition
   - I2C register definitions and communication
   - Multiple resolution and gain settings
   - Continuous and one-shot conversion modes
   - Configuration and status reporting functions

2. **monitor/src/mcp3421_sensor.cpp**
   - Full implementation of MCP3421 sensor class
   - I2C register read/write operations
   - ADC value conversion to voltage
   - Error handling and debugging support
   - Configurable reference voltage (default 2.048V)

### Files Modified
1. **monitor/include/monitor_system.h**
   - Added MCP3421_Sensor include
   - Added ADC monitoring function declarations
   - Added ADC sensor member variables

2. **monitor/src/monitor_system.cpp**
   - Updated constructor with ADC sensor variables
   - Added MCP3421 initialization in begin()
   - Implemented readAdcSensor() function
   - Added ADC reading to update() loop
   - Added ADC sensor getter functions

3. **monitor/include/constants.h**
   - Added ADC_READ_INTERVAL_MS constant (1500ms)
   - Added MCP3421 MQTT topics for voltage, raw, and status

4. **monitor/src/command_processor.cpp**
   - Added "adc" command handling
   - Implemented handleAdc() function with subcommands:
     - `adc read` - Read voltage and raw value
     - `adc voltage` - Read voltage only
     - `adc raw` - Read raw ADC value only
     - `adc status` - Show sensor status and configuration
     - `adc config` - Configure resolution (12/14/16/18-bit)

5. **monitor/include/command_processor.h**
   - Added handleAdc() function declaration

6. **monitor/src/constants.cpp**
   - Added "adc" to ALLOWED_COMMANDS array

## ADC Monitoring Features

### MQTT Topics
- `r4/monitor/adc/voltage` - ADC voltage reading (high precision)
- `r4/monitor/adc/raw` - Raw ADC value (signed integer)
- `r4/monitor/adc/status` - Comprehensive sensor status

### Telnet Commands
- `adc read` - Display voltage and raw value
- `adc voltage` - Display voltage reading only
- `adc raw` - Display raw ADC value only
- `adc status` - Display sensor status and configuration
- `adc config` - Show current configuration
- `adc config 12bit` - Set 12-bit resolution (240 SPS)
- `adc config 14bit` - Set 14-bit resolution (60 SPS)
- `adc config 16bit` - Set 16-bit resolution (15 SPS)
- `adc config 18bit` - Set 18-bit resolution (3.75 SPS)

### System Integration
- **Reading Interval**: 1.5 seconds (configurable)
- **Default Configuration**: 16-bit resolution, 15 SPS, 1x gain, continuous mode
- **Reference Voltage**: 2.048V (configurable)
- **Error Handling**: Graceful degradation if sensor not present
- **Memory Usage**: Minimal overhead added to existing system

## Build Results
- **RAM Usage**: 31.1% (10,192 bytes / 32,768 bytes)
- **Flash Usage**: 49.0% (128,336 bytes / 262,144 bytes)
- **Build Status**: SUCCESS ✅

## Technical Specifications

### MCP3421 Capabilities
- **Resolution**: 12, 14, 16, or 18-bit ADC
- **Sample Rates**: 240, 60, 15, or 3.75 SPS
- **Gain Settings**: 1x, 2x, 4x, or 8x PGA
- **Input Range**: ±2.048V (with 1x gain and 2.048V reference)
- **Conversion Modes**: Continuous or one-shot
- **I2C Interface**: Standard I2C with configurable address

### Voltage Calculation
```
Voltage = (RawValue / MaxValue) × (Vref / Gain)
```

Where:
- RawValue: Signed ADC reading
- MaxValue: Maximum positive value for resolution
- Vref: Reference voltage (default 2.048V)
- Gain: Programmable gain amplifier setting

## Usage Examples

### Telnet Commands
```
> adc read
0.125634V, raw: 4095

> adc status
MCP3421 @ 0x68: READY, 16-bit, Gain=1x, 15 SPS, Vref=2.048V, Last=0.125634V

> adc config 18bit
ADC set to 18-bit, 3.75 SPS

> adc config
MCP3421: 18-bit, Gain=1x, Vref=2.048V
```

### MQTT Data
```
r4/monitor/adc/voltage: "0.125634"
r4/monitor/adc/raw: "4095"
r4/monitor/adc/status: "ready: YES, voltage: 0.125634V, raw: 4095, resolution: 16-bit"
```

## Benefits
1. **High-Resolution ADC**: 18-bit precision for accurate analog measurements
2. **Flexible Configuration**: Selectable resolution, gain, and sample rate
3. **Remote Monitoring**: MQTT integration for remote ADC data collection
4. **Command Interface**: Interactive ADC configuration via telnet
5. **System Integration**: Seamless integration with existing monitor infrastructure
6. **Programmable Gain**: Built-in amplifier for low-level signal measurement

## Applications
- **Precision Voltage Monitoring**: High-accuracy voltage measurements
- **Sensor Interface**: Connect analog sensors with digital readout
- **Data Acquisition**: Remote analog data collection via MQTT
- **Process Monitoring**: Industrial process variable measurement
- **Battery Monitoring**: Precise battery voltage and current sensing

## Next Steps
- Connect MCP3421 ADC sensor to monitor system I2C bus (address 0x68)
- Configure appropriate reference voltage for measurement range
- Test ADC measurements with actual analog signals
- Set up ADC-based alerts or automation rules
- Integrate ADC data into monitoring dashboards