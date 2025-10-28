# Pressure Sensor System Documentation

## Overview

The LogSplitter Controller implements a comprehensive dual-pressure monitoring system with both operational control and safety protection capabilities. The system uses two independent pressure sensors with different measurement ranges and technologies.

## Physical Sensor Configuration

### Primary Hydraulic Pressure Sensor (A1)
- **Pin**: A1 (Analog Input)
- **Type**: 4-20mA Current Loop Sensor
- **Range**: 0-5000 PSI (configurable via `DEFAULT_A1_MAX_PRESSURE_PSI`)
- **Voltage Conversion**: 4-20mA → 1-5V via 250Ω precision resistor
- **Resolution**: ~1.2 PSI per ADC count (5000 PSI / 4096 counts)
- **Accuracy**: ±1% typical (sensor dependent)
- **Purpose**: Main hydraulic system pressure monitoring

### Secondary Hydraulic Oil Pressure Sensor (A5)
- **Pin**: A5 (Analog Input)
- **Type**: 0-5V Voltage Output Sensor
- **Range**: 0-30 PSI (configurable via `DEFAULT_A5_MAX_PRESSURE_PSI`)
- **Voltage Range**: 0-5V direct
- **Resolution**: ~0.007 PSI per ADC count (30 PSI / 4096 counts)
- **Purpose**: Hydraulic oil pressure monitoring

## System Architecture

### Data Flow Pipeline

```
Hardware Sensor → ADC Sampling → Filtering → Averaging → Calibration → System Usage
```

1. **Hardware Sensing**: Physical pressure → electrical signal
2. **ADC Sampling**: Arduino samples at `SAMPLE_INTERVAL_MS` intervals
3. **Digital Filtering**: Median3, EMA, or no filtering (configurable)
4. **Running Average**: Averages over `SAMPLE_WINDOW_COUNT` samples
5. **Calibration**: ADC counts → voltage → PSI conversion
6. **System Integration**: Used by sequence controller and safety system

### Software Components

#### PressureSensorChannel Class
- **Sampling**: Continuous ADC reading with configurable intervals
- **Filtering**: Multiple filter modes (FILTER_MEDIAN3, FILTER_EMA, FILTER_NONE)
- **Buffering**: Rolling average over configurable sample window
- **Calibration**: Voltage-to-pressure conversion with gain/offset

#### PressureManager Class
- **Multi-Sensor**: Manages both hydraulic sensors simultaneously
- **Binary Telemetry**: Real-time data streaming via telemetry protocol
- **System Interface**: Provides unified pressure access to other systems

## Operational Integration

### Sequence Controller Pressure Logic

The sequence controller uses pressure monitoring as an alternative to physical limit switches:

#### Extend Stage Implementation
```cpp
// EXTEND STAGE - Dual detection method
bool extendLimitReached = g_limitExtendActive;  // Physical switch (Pin 6)

// Parallel pressure check (independent of physical switch)
if (pressureManager.isReady()) {
    float currentPressure = pressureManager.getHydraulicPressure();
    if (currentPressure >= EXTEND_PRESSURE_LIMIT_PSI) {  // 2300 PSI
        extendLimitReached = true;  // Pressure-based limit reached
        debugPrintf("[SEQ] Pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                   currentPressure, EXTEND_PRESSURE_LIMIT_PSI);
    }
}
```

#### Retract Stage Implementation
```cpp
// RETRACT STAGE - Dual detection method
bool retractLimitReached = g_limitRetractActive;  // Physical switch (Pin 7)

// Parallel pressure check (independent of physical switch)
if (pressureManager.isReady()) {
    float currentPressure = pressureManager.getHydraulicPressure();
    if (currentPressure >= RETRACT_PRESSURE_LIMIT_PSI) {  // 2300 PSI
        retractLimitReached = true;  // Pressure-based limit reached
        debugPrintf("[SEQ] Retract pressure limit reached: %.1f PSI >= %.1f PSI\n", 
                   currentPressure, RETRACT_PRESSURE_LIMIT_PSI);
    }
}
```

#### Key Operational Features
- **Redundant Detection**: Physical switches OR pressure thresholds
- **Independent Logic**: Both methods work simultaneously (not conditional)
- **Stability Requirements**: Must maintain limit condition for `stableTimeMs` (15ms default)
- **Real-time Logging**: Detailed debug output for diagnostics

### Safety System Pressure Monitoring

The safety system provides independent pressure monitoring for emergency protection:

#### Safety Logic Implementation
```cpp
// Called every main loop: safetySystem.update(pressureManager.getPressure())
void SafetySystem::checkPressure(float pressure, bool atLimitSwitch) {
    if (pressure >= SAFETY_THRESHOLD_PSI) {  // 2500 PSI
        if (!safetyActive) {
            const char* reason = atLimitSwitch ? "pressure_at_limit" : "pressure_threshold";
            activate(reason);  // Trigger emergency shutdown
        }
    } else if (pressure < (SAFETY_THRESHOLD_PSI - SAFETY_HYSTERESIS_PSI)) {  // 2490 PSI
        if (safetyActive) {
            // Pressure normalized but safety REMAINS ACTIVE
            debugPrintf("Pressure normalized: %.1f PSI below threshold - safety remains active (manual clear required)\n", pressure);
            // Manual safety clear button required - NO automatic clearing
        }
    }
}
```

#### Safety Features
- **Independent Monitoring**: Separate from sequence controller logic
- **Manual-Clear-Only**: No automatic safety clearing when pressure drops
- **Hysteresis**: 10 PSI deadband prevents oscillation
- **Emergency Response**: <100ms from spike detection to system shutdown

## Configuration Constants

### Pressure Thresholds

| **System Component** | **Constant** | **Value** | **Purpose** |
|---------------------|--------------|-----------|-------------|
| Sequence Extend Limit | `EXTEND_PRESSURE_LIMIT_PSI` | 2300 PSI | Alternative to physical extend limit switch |
| Sequence Retract Limit | `RETRACT_PRESSURE_LIMIT_PSI` | 2300 PSI | Alternative to physical retract limit switch |
| Safety Activation | `SAFETY_THRESHOLD_PSI` | 2500 PSI | Emergency shutdown trigger |
| Safety Hysteresis | `SAFETY_HYSTERESIS_PSI` | 10 PSI | Prevents safety system oscillation |

### Sensor Configuration

| **Parameter** | **A1 (Main Hydraulic)** | **A5 (Oil Pressure)** |
|---------------|-------------------------|------------------------|
| Max Pressure | `DEFAULT_A1_MAX_PRESSURE_PSI` (5000 PSI) | `DEFAULT_A5_MAX_PRESSURE_PSI` (30 PSI) |
| Voltage Reference | `DEFAULT_A1_ADC_VREF` (5.0V) | `DEFAULT_A5_ADC_VREF` (5.0V) |
| Sensor Gain | `DEFAULT_A1_SENSOR_GAIN` (1.0) | `DEFAULT_A5_SENSOR_GAIN` (1.0) |
| Sensor Offset | `DEFAULT_A1_SENSOR_OFFSET` (0.0) | `DEFAULT_A5_SENSOR_OFFSET` (0.0) |

### Sampling Configuration

| **Parameter** | **Constant** | **Value** | **Description** |
|---------------|--------------|-----------|-----------------|
| Sample Interval | `SAMPLE_INTERVAL_MS` | Configurable | Time between ADC readings |
| Sample Window | `SAMPLE_WINDOW_COUNT` | Configurable | Number of samples in rolling average |
| ADC Resolution | `ADC_RESOLUTION_BITS` | 12-bit | Arduino UNO R4 WiFi ADC resolution |
| Voltage Reference | `DEFAULT_ADC_VREF` | 5.0V | Arduino UNO R4 WiFi reference voltage |

## Performance Characteristics

### Response Times
- **ADC Sampling**: ~1-10ms per main loop cycle
- **Pressure Calculation**: <1ms computational overhead
- **Safety Response**: <100ms from pressure spike to emergency shutdown
- **Sequence Response**: 15ms stability timer + processing time
- **Binary Telemetry**: Every 100ms (real-time streaming)

### Accuracy and Precision
- **ADC Resolution**: 12-bit (4096 levels)
- **A1 Resolution**: ~1.2 PSI per count (5000 PSI range)
- **A5 Resolution**: ~0.007 PSI per count (30 PSI range)
- **System Accuracy**: Dependent on sensor calibration (typically ±1%)
- **Stability**: Digital filtering reduces noise and false triggers

## Binary Telemetry Integration

### Real-time Streaming

#### Automatic Publishing
- **Interval**: Every 10 seconds
- **Topics**: 
  - `r4/pressure/hydraulic_system` → Main hydraulic pressure (PSI)
  - `r4/pressure/hydraulic_filter` → Oil pressure (PSI)
- **Trigger**: Immediate publish on sequence state changes

#### Data Format
```json
{
  "hydraulic_system": 1234.5,
  "hydraulic_filter": 15.2,
  "timestamp": "2025-10-04T12:34:56Z"
}
```

### Debug Output
- **Real-time Logging**: Pressure values in debug console
- **Sequence Events**: Pressure limit detection logging
- **Safety Events**: Pressure threshold violations
- **Calibration Data**: Voltage and pressure correlation

## Diagnostic Features

### Status Reporting
- **Sensor Ready Status**: `pressureManager.isReady()`
- **Current Readings**: `pressureManager.getHydraulicPressure()`
- **Voltage Monitoring**: Raw voltage readings available
- **System Health**: Sensor connectivity and calibration status

### Error Detection
- **Sensor Disconnection**: Detectable via voltage range checking
- **Calibration Drift**: Comparative analysis with known reference points
- **Signal Noise**: Filter effectiveness monitoring
- **Range Violations**: Out-of-bounds pressure detection

## Calibration Procedures

### Field Calibration
1. **Zero Calibration**: Adjust offset with system at 0 PSI
2. **Span Calibration**: Adjust gain using known pressure reference
3. **Verification**: Compare readings with certified pressure gauge
4. **Documentation**: Record calibration coefficients in system

### Configuration Updates
```cpp
// Example calibration update
pressureManager.getSensor(SENSOR_HYDRAULIC).setSensorGain(1.05f);
pressureManager.getSensor(SENSOR_HYDRAULIC).setSensorOffset(-2.3f);
```

## Safety Considerations

### Hardware Safety
- **Sensor Redundancy**: Dual pressure monitoring systems
- **Independent Monitoring**: Safety system isolated from operational logic
- **Fail-Safe Design**: System defaults to safe state on sensor failure
- **Manual Override**: Safety clear button for emergency recovery

### Software Safety
- **Input Validation**: Pressure range checking and bounds verification
- **Filtering**: Digital noise reduction prevents false alarms
- **Hysteresis**: Prevents rapid cycling of safety systems
- **Logging**: Complete audit trail of pressure events

### Operational Safety
- **Manual Clear Only**: No automatic safety system clearing
- **Pressure Limits**: Conservative thresholds with safety margins
- **Emergency Response**: Immediate system shutdown on threshold violation
- **Management Control**: Safety clear requires supervisor intervention

## Maintenance and Troubleshooting

### Regular Maintenance
- **Sensor Inspection**: Visual check of connections and mounting
- **Calibration Verification**: Periodic accuracy checks
- **Filter Replacement**: If applicable to sensor type
- **Connection Integrity**: Electrical continuity testing

### Common Issues
- **Pressure Spikes**: Check for mechanical system issues
- **Sensor Drift**: Recalibration may be required
- **Noise/Fluctuation**: Verify filtering configuration
- **Communication Errors**: Check serial connections and binary telemetry stream

### Diagnostic Commands
- `show` - Display current pressure readings
- `debug pressure on` - Enable detailed pressure logging
- `calibrate pressure` - Enter calibration mode
- `pressure status` - Show sensor health and configuration

---

*This document provides comprehensive documentation of the pressure sensor system implementation in the LogSplitter Controller. For technical support or system modifications, refer to the source code in the pressure_manager and safety_system modules.*