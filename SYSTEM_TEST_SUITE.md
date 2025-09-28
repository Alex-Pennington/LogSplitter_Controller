# SystemTestSuite - Hardware Validation Framework

## Overview

The SystemTestSuite is a comprehensive testing framework designed to systematically validate all hardware components of the LogSplitter Controller before deployment. This interactive, safety-first system ensures that each component operates correctly and safely.

## Architecture

### Core Components

- **TestResult Enum**: PASS, FAIL, SKIP, PENDING, TIMEOUT
- **TestCase Struct**: Tracks name, result, details, duration, and criticality
- **SystemTestSuite Class**: Manages up to 15 test cases with full lifecycle support

### Safety Features

- **Critical Test Flagging**: Failures in safety-critical tests abort the entire suite
- **Emergency Abort**: Operators can immediately stop testing for safety
- **Safety Mode Enforcement**: All hydraulic relays remain OFF during testing
- **Interactive Timeouts**: 30-second timeouts prevent system hanging
- **User Confirmation**: Y/N prompts for each test step

## Test Categories

### Safety Input Tests (Critical)

These tests validate the core safety systems. Any failure aborts the test suite.

#### Emergency Stop Button Test
- Verifies E-Stop button operates correctly
- Tests both activation and latch behavior
- Confirms system response and safety state

#### Limit Switch Tests  
- Tests both retract and extend limit switches
- Validates NO (Normally Open) and NC (Normally Closed) operation
- Confirms system detection of activation/deactivation

#### Pressure Sensor Test
- Validates pressure readings are within expected range (0-5000 PSI)
- Tests stability over 5-second period (< 100 PSI variation)
- Confirms sensor provides valid, stable readings

#### Pressure Safety Threshold Test
- Verifies pressure safety system configuration
- Validates safety threshold settings (2750 PSI default)
- Confirms operator understanding of safety operation

### Output Device Tests (Critical)

These tests validate system output components.

#### Relay Controller Test
- Verifies all relays are in safety mode (OFF)
- Tests relay status reporting functionality  
- Confirms safety mode is properly enforced

#### Engine Stop Circuit Test
- Tests engine stop activation and restart
- Validates engine response to stop commands
- **WARNING**: This test temporarily stops the engine

### Integrated System Tests (Non-Critical)

These tests validate system integration. Failures do not abort the suite.

#### Network Communication Test
- Verifies WiFi connectivity
- Tests MQTT publish functionality
- Validates remote communication capability

#### Sequence Tests (Placeholder)
- Extend sequence validation (to be implemented)
- Retract sequence validation (to be implemented)  
- Pressure relief testing (to be implemented)

## Command Interface

### Available Commands

```text
test all        # Run complete system test suite
test safety     # Run safety input tests only  
test outputs    # Run output device tests only
test systems    # Run integrated system tests only
test report     # Generate comprehensive test report
test status     # Show current test progress
test progress   # Display progress bar
```

### Example Usage

```text
> test safety
Starting safety input tests...

===========================================
TEST: Emergency Stop Button Test
===========================================
This test verifies the emergency stop button functions correctly.

Step 1: Verify E-Stop is NOT pressed (released position)
Is the E-Stop button in the released (normal) position? [Y/N]: Y
Confirmed.

Step 2: Press and hold the E-Stop button
The system should immediately detect the activation.

Press the E-Stop button NOW and hold it.
Did the system respond immediately (relays clicked off)? [Y/N]: Y
Confirmed.

[PASS]
```

## Test Flow

### 1. Preparation Phase
- User confirmation to begin testing
- Safety mode activation
- System component validation

### 2. Test Execution
- Sequential execution by category
- Interactive user prompts for each step
- Real-time result tracking

### 3. Reporting Phase
- Comprehensive test report generation
- MQTT publishing of results
- Overall pass/fail determination

## Result States

- **PASS**: Test completed successfully
- **FAIL**: Test failed (critical tests abort suite)
- **SKIP**: Test skipped (user choice or unsafe conditions)  
- **PENDING**: Test not yet executed
- **TIMEOUT**: No user response within timeout period

## MQTT Integration

### Published Topics

- `r4/test/status` - Overall test suite status (PASS/FAIL/PENDING)
- `r4/test/result/{test_name}` - Individual test results with details
- `r4/test/summary` - Complete test summary with completion count

### Example MQTT Messages

```text
r4/test/status: PASS
r4/test/result/emergency_stop_button: PASS
r4/test/result/limit_switch_-_retract: PASS  
r4/test/summary: 7/11 tests completed - PASS
```

## Safety Considerations

### Before Testing

1. Ensure work area is clear and safe
2. Have emergency stop readily accessible
3. Understand each test procedure
4. Verify personnel are trained and authorized

### During Testing

1. Follow all interactive prompts carefully
2. Do not bypass safety confirmations
3. Stop testing immediately if unsafe conditions arise
4. Monitor system responses closely

### Emergency Procedures

1. **Emergency Abort**: Use `test` command interruption or physical E-Stop
2. **System Recovery**: Safety mode remains active after testing
3. **Fault Response**: Critical test failures prevent further testing

## Implementation Details

### File Structure

- `include/system_test_suite.h` - Framework interface and class definition
- `src/system_test_suite.cpp` - Complete implementation with all test methods

### Dependencies

- SafetySystem - Emergency stop and engine control
- RelayController - Hydraulic relay management  
- InputManager - Limit switch and button monitoring
- PressureManager - Pressure sensor readings
- SequenceController - Sequence state management
- NetworkManager - MQTT communication

### Memory Usage

- **Test Cases**: 15 slots with 128-byte detail strings
- **RAM Usage**: ~2KB for framework (included in 28% total usage)
- **Flash Usage**: ~12KB for implementation (included in 41.3% total usage)

## Extending the Framework

### Adding New Tests

1. Add test method declaration to `SystemTestSuite` class
2. Implement test method in `system_test_suite.cpp`
3. Add test case to `initializeTestCases()` method
4. Update test category execution as needed

### Test Method Template

```cpp
SystemTestSuite::TestResult SystemTestSuite::testNewComponent() {
    printTestHeader("New Component Test");
    
    if (!requiredDependency) {
        return FAIL;
    }
    
    Serial.println("Test description and safety notes");
    
    TestResult step1 = waitForUserConfirmation("Test step 1 question?");
    if (step1 != PASS) {
        return step1;
    }
    
    // Perform test operations
    
    Serial.println("Test completed successfully");
    return PASS;
}
```

## Troubleshooting

### Common Issues

**Test Timeout**
- Increase `USER_TIMEOUT_MS` if needed
- Ensure operator is ready before starting tests

**Critical Test Failures**
- Review failed test details in report
- Address hardware issues before retesting
- Check safety system status

**MQTT Publishing Failures**
- Verify network connectivity
- Check MQTT broker availability
- Review network manager status

### Debug Information

Enable debug output with:
```text
set debug on
```

This provides detailed logging of test execution and system state.

## Version History

- **v2.0**: Initial SystemTestSuite implementation
- **Features**: Complete interactive testing framework
- **Integration**: Full command processor and MQTT integration
- **Safety**: Comprehensive safety-first architecture

---

**Author**: Integrated with LogSplitter Controller v2.0  
**Date**: September 2025  
**Purpose**: Hardware validation and safety verification  
**Platform**: Arduino UNO R4 WiFi with PlatformIO