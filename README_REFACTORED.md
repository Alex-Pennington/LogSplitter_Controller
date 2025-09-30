# LogSplitter Controller v2.0 - Refactored

## Overview

This is a complete refactor of the original monolithic LogSplitter Controller code. The system has been redesigned with a modular architecture that improves maintainability, reliability, and performance.

## Key Improvements

### 1. **Modular Architecture**
- **Original**: 1000+ lines in single `main.cpp` file
- **Refactored**: Split into 8 focused modules with clear responsibilities

### 2. **Memory Optimization**
- **Issue Fixed**: Large stack arrays (256+ bytes) that could cause stack overflow
- **Solution**: Shared global buffers with controlled sizes
- **Benefit**: Reduced memory usage by ~60%

### 3. **Network Reliability**
- **Issue Fixed**: Blocking delays and infinite reconnection attempts
- **Solution**: Non-blocking reconnection with retry limits and timeouts
- **Benefit**: System remains responsive during network issues

### 4. **Safety Systems**
- **Enhancement**: Comprehensive safety system with pressure monitoring
- **Features**: Emergency stop, system health monitoring, watchdog timer
- **Benefit**: Industrial-grade reliability and fail-safe operation

### 5. **Input Validation & Security**
- **Issue Fixed**: No validation of MQTT commands
- **Solution**: Command whitelisting, parameter validation, rate limiting
- **Benefit**: Protection against malicious or malformed commands

### 6. **State Machine Design**
- **Issue Fixed**: Complex sequence logic mixed with other code
- **Solution**: Clean state machine with defined states and transitions
- **Benefit**: Easier debugging and more predictable behavior

## File Structure

```
├── include/
│   ├── constants.h           # System constants and configuration
│   ├── network_manager.h     # WiFi/MQTT connectivity
│   ├── sequence_controller.h # Sequence state machine
│   ├── pressure_sensor.h     # Pressure monitoring and filtering
│   ├── relay_controller.h    # Relay control and Serial1 communication
│   ├── config_manager.h      # EEPROM configuration management
│   ├── input_manager.h       # Input debouncing and pin monitoring
│   ├── safety_system.h       # Safety monitoring and emergency stop
│   └── command_processor.h   # Command validation and processing
├── src/
│   ├── main.cpp             # Main application (200 lines vs 1000+)
│   ├── network_manager.cpp
│   ├── sequence_controller.cpp
│   ├── pressure_sensor.cpp
│   ├── relay_controller.cpp
│   ├── config_manager.cpp
│   ├── input_manager.cpp
│   ├── safety_system.cpp
│   ├── command_processor.cpp
│   └── constants.cpp
```

## Module Summaries

### Pressure Sensing

- **Purpose**: Dual-channel (A1 main hydraulic, A5 filter/oil) sampling with filtering & calibration
- **Features**: Circular buffer averaging, Median3 / EMA filters, configurable ADC reference
- **Extended Scaling (A1)**: 0–5.0 V electrical span represents -25% .. +125% of nominal (3000 PSI) but output is CLAMPED to 0..3000 PSI for safety & display
- **Benefit**: Head-room for sensor over‑range / calibration shift while keeping operator & safety logic within a stable nominal window

### RelayController

- **Purpose**: Serial1 communication with relay board
- **Features**: State tracking, power management, command validation
- **Improvements**: Centralized relay logic, safety integration

### ConfigManager

- **Purpose**: EEPROM storage and configuration management
- **Features**: Validation, defaults, cross-module configuration
- **Improvements**: Robust configuration with validation and recovery

### SafetySystem

- **Purpose**: System safety monitoring and emergency procedures
- **Features**: Pressure monitoring, emergency stop, system health checks
- **Improvements**: Centralized safety logic, multiple trigger conditions

### InputManager

- **Purpose**: Pin monitoring with debouncing
- **Features**: Configurable pin modes (NO/NC), callback system
- **Improvements**: Separated from main loop, cleaner debounce logic

### SequenceController

- **Purpose**: Cylinder sequence state machine (extend/retract workflow)
- **Features**: Stable limit detection timers, timeout handling, abort path
- **Improvements**: Deterministic transitions; reduced false limit triggers

### CommandProcessor

- **Purpose**: Command validation and processing (Serial + Telnet + MQTT)
- **Features**: Input validation, rate limiting, security checks, shorthand relay commands (e.g. `R1 ON`)
- **Improvements**: Protection against malformed commands, structured processing, compact `show` output

### NetworkManager

- **Purpose**: WiFi + MQTT connectivity management + Syslog logging
- **Features**: Non-blocking reconnect, publish helper, status tracking, automatic hostname setting, UDP syslog support
- **Improvements**: Avoids main loop stalls during outages
- **Hostname**: Automatically sets device hostname as `LogSplitter` for easier telnet server discovery
- **Syslog**: Sends debug messages to configurable rsyslog server (RFC 3164 format over UDP)

### Pressure Scaling Details (A1)

Electrical span: 0–5.0 V (configured constant)

Logical span: -0.25 * P_nom .. +1.25 * P_nom (P_nom = 3000 PSI) => 1.5 * P_nom total

Mapping formula (before clamp):

```text
rawPsi = (V / 5.0) * (1.5 * P_nom) - 0.25 * P_nom
```

Clamp applied:

```text
rawPsi_clamped = min( max(rawPsi, 0), P_nom )
```

Reasons:

1. Headroom for slight sensor over‑range / calibration shift without saturating ADC early
2. Negative region (below 0) absorbed by clamp—prevents underflow noise
3. Safety logic and UI operate only on clamped nominal range (predictable thresholds)
4. No change to MQTT payload schema; downstream consumers unaffected

If future needs arise (publishing raw unclamped value, configurable span, or auto-calibration), the code has clear constants (`MAIN_PRESSURE_EXT_*`) ready for parameterization.

## Usage

### Serial / Telnet / MQTT Commands

```text
help                             # Show available commands
show                             # Compact status line (pressures, sequence, relays, safety)
pins                             # (Serial only) Detailed pin mapping & modes
set vref 3.3                     # Set ADC reference (used for ADC->Voltage)
set maxpsi 5000                  # Set nominal max (non-extended channels / legacy path)
set filter median3               # Filter: none | median3 | ema
set gain 1.02                    # Apply scalar gain to raw pressure (legacy single-sensor path)
set offset -12.5                 # Apply offset (" ")
set pinmode 6 NC                 # Configure limit / input as NO or NC
set syslog 192.168.1.100         # Set rsyslog server IP address
set syslog 192.168.1.100:514     # Set rsyslog server IP and port
syslog test                      # Send test message to rsyslog server
syslog status                    # Show rsyslog server status and connectivity
R1 ON                            # Shorthand relay control (works over MQTT & Serial)
relay R2 OFF                     # Long form relay control
```

**Telnet Access**: The device automatically sets its hostname to `LogSplitter`. This makes it easier to find and connect to the telnet server on your network. The telnet server runs on port 23 and displays the hostname and IP address in the welcome message.

**Syslog Integration**: All debug messages are sent exclusively to the configured rsyslog server using RFC 3164 format over UDP port 514. Configure with `set syslog <server_ip>` or `set syslog <server_ip>:<port>`. Debug output has been removed from Serial and Telnet for cleaner operation.

Example `show` response (single line):

```text
hydraulic=1234.5 hydraulic_oil=1180.2 seq=IDLE stage=NONE relays=1:ON,2:OFF safe=OK
```
Exact tokens may vary; order kept stable for easy parsing.

### MQTT Topics

**Subscribe (Control)**:
- `r4/control` - Command input
- `r4/example/sub` - General messages

**Publish (Status / Telemetry)**:

- `r4/control/resp` - Command responses
- `r4/pressure` - (Backward compat) Main hydraulic pressure (clamped)
- `r4/pressure/hydraulic_system` - Main hydraulic pressure (clamped 0..3000)
- `r4/pressure/hydraulic_filter` - Filter/oil pressure
- `r4/pressure/status` - Key/value status line (pressures)
- `r4/sequence/status` - Sequence state
- `r4/sequence/event` - Sequence transitions / notable events
- `r4/inputs/{pin}` - Input state changes

## Safety Features

1. **Pressure Safety**: Automatic shutdown if clamped main pressure > 2500 PSI (A1 extended scaling still clamps before this check)
2. **Sequence Timeouts**: Automatic abort if sequence takes too long
3. **System Health**: Watchdog monitoring of main loop execution
4. **Emergency Stop / Reset**: Safety reset on pin 4; single start on pin 5; manual controls on pins 2 & 3
5. **Input Validation**: Protection against malformed commands & rate spikes

## Memory Usage

- **Current Usage**: RAM: 34.5% (11,308 bytes), Flash: 43.3% (113,432 bytes)
- **Shared Buffers**: 256-byte message buffer, 64-byte topic buffer
- **Stack Safety**: No large local arrays in functions
- **PROGMEM**: Constants stored in flash memory
- **Estimated Savings**: ~60% reduction in RAM usage vs original monolithic design

## Compilation

To compile this project:

1. Ensure PlatformIO is installed and in PATH
2. Open terminal in project directory
3. Run: `pio run`
4. Upload: `pio run --target upload`

## Migration Notes

- **EEPROM**: Existing configurations automatically loaded
- **Commands**: Legacy commands preserved; added shorthand `R<n> ON|OFF`
- **MQTT**: Existing topics preserved; added explicit hydraulic system/filter topics & pressure status line
- **Pins**: Updated logic: pin 4 now dedicated Safety Reset, pin 5 single Start button (replaces multi-button requirement), pins 2/3 manual action inputs
- **Pressure Scaling**: Main channel (A1) now uses extended 0–5V mapping to -25%..+125% of nominal then clamps to 0..3000 for safety & display; no telemetry format changes required
- **Behavior**: Improved reliability; sequence limit stability timing reduces false transitions

## Troubleshooting

### Build Issues

- Ensure all header files are present in `include/` directory
- Check that `arduino_secrets.h` contains WiFi credentials
- Verify PlatformIO dependencies are installed

### Runtime Issues

- Check serial output for initialization messages
- Verify WiFi credentials and network connectivity
- Use `show` command to check system status
- Monitor MQTT topics for system telemetry

## SystemTestSuite - Hardware Validation Framework

### Overview

A comprehensive testing framework that allows programmers to systematically test each system component for correct operation and safety before deployment. The SystemTestSuite provides interactive, safety-first validation of all hardware components.

### Key Features

- **Safety-First Architecture**: Critical test failures abort the entire suite
- **Interactive User Guidance**: 30-second timeout prompts with Y/N confirmations
- **Comprehensive Coverage**: Tests all safety inputs, outputs, and integrated systems
- **Professional Reporting**: Detailed test reports with MQTT publishing
- **Remote Monitoring**: Test status available via MQTT for remote oversight

### Test Categories

**Safety Input Tests (Critical):**
- Emergency Stop Button validation with latch testing
- Limit Switch testing (both retract and extend switches)
- Pressure Sensor accuracy and stability validation  
- Pressure Safety Threshold verification

**Output Device Tests (Critical):**
- Relay Controller safety mode verification
- Engine Stop Circuit functional testing

**Integrated System Tests (Non-Critical):**
- Network Communication testing
- Extend/Retract/Pressure Relief sequences

### Test Commands

```text
test all        # Run complete system test suite
test safety     # Run safety input tests only  
test outputs    # Run output device tests only
test systems    # Run integrated system tests only
test report     # Generate comprehensive test report
test status     # Show current test progress
test progress   # Display progress bar
```

### Example Test Flow

1. **Preparation**: Ensure area is clear and you're ready to operate controls
2. **Safety Tests**: Interactive validation of emergency stop, limit switches, pressure sensor
3. **Output Tests**: Verify relay controller and engine stop circuit operation
4. **System Tests**: Validate network communication and integrated sequences
5. **Reporting**: Generate comprehensive test report with pass/fail status

### Test Results

The framework provides multiple result states:
- **PASS**: Test completed successfully
- **FAIL**: Test failed (critical tests abort suite)
- **SKIP**: Test skipped (user choice or unsafe conditions)
- **PENDING**: Test not yet run
- **TIMEOUT**: No user response within 30 seconds

### Safety Features

- **Emergency Abort**: Operators can stop testing immediately
- **Safety Mode Enforcement**: All relays remain OFF during testing
- **Critical Test Flagging**: Failures in safety tests prevent further testing
- **Timeout Protection**: Prevents hanging on user input

### MQTT Integration

Test results are published to MQTT topics:
- `r4/test/status` - Overall test suite status
- `r4/test/result/{test_name}` - Individual test results
- `r4/test/summary` - Complete test summary

### Benefits

1. **Hardware Validation**: Systematic verification before deployment
2. **Safety Assurance**: Critical safety systems thoroughly tested
3. **Documentation**: Professional test reports for compliance
4. **Remote Monitoring**: MQTT integration for remote test oversight
5. **Maintainable**: Modular architecture for easy test additions

## Future Enhancements

Potential areas for continued improvement:

1. **OTA Updates**: Over-the-air firmware updates
2. **Web Interface**: Built-in web server for configuration
3. **Data Logging**: Local storage of pressure and sequence data
4. **Advanced Safety**: Multi-level safety with configurable thresholds
5. **Diagnostics**: Built-in system diagnostics and error reporting
6. **Test Automation**: Automated test scheduling and execution
7. **Extended Test Coverage**: Additional system component validation

---

**Author**: Refactored from original monolithic design  
**Date**: September 2025  
**Version**: 2.3.0 (Hostname configuration, syslog integration, SystemTestSuite framework, extended pressure scaling, shorthand relay commands)  
**Compatibility**: Arduino UNO R4 WiFi with PlatformIO
