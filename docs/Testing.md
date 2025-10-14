# LogSplitter Controller - Operator Pre-Shift Testing Interface

## Document Purpose

This document specifies the requirements for an operator testing interface to be implemented by another AI. The interface will guide operators through pre-shift safety verification testing of all LogSplitter Controller inputs and outputs.

## Testing Philosophy

All tests follow a standardized pattern to ensure consistency and ease of use:
- Clear operator instructions
- Real-time MQTT monitoring for verification
- Visual/physical confirmation where applicable
- Pass/Fail status with timestamps
- Test report generation capability

## Hardware Under Test

**Controller**: Arduino UNO R4 WiFi LogSplitter Controller
**Firmware**: LogSplitter Controller v2.0
**MQTT Prefix**: `controller/`

## Test Categories

### Category 1: Safety Critical Inputs
1. Engine Run/Stop Relay (R8)
2. Emergency Stop Button (Pin 12)
3. Limit Switches (Pins 6 & 7)

### Category 2: Operational Inputs
4. Safety Reset/Clear Button (Pin 4)
5. Manual Extend Button (Pin 2)
6. Manual Retract Button (Pin 3)
7. Sequence Start Button (Pin 5)
8. Splitter Operator Signal (Pin 8)

### Category 3: Visual Outputs
9. Mill Lamp LED (Pin 9 / Relay R9)
10. Status LED (Pin 13)

### Category 4: Sensor Inputs
11. System Hydraulic Pressure Sensor (A1)
12. Filter Pressure Sensor (A5)

### Category 5: Hydraulic System
13. Complete Hydraulic Sequence Test

---

## Detailed Test Specifications

### Test 1: Engine Run/Stop Relay (R8)

**Purpose**: Verify engine enable/disable relay operation
**Hardware**: Relay R8, 12V DC control signal
**MQTT Topics**: 
- `controller/relays/R8` (status: 0=OFF, 1=ON)
- `controller/relay_states` (full relay status)

**Test Procedure**:
1. **Initial State Verification**
   - Display current relay state: "Engine Relay (R8) is currently: OFF"
   - Verify MQTT topic `controller/relays/R8` shows `0`
   - Operator confirms engine is disabled

2. **Enable Test**
   - Prompt: "Test will now ENABLE the engine relay (R8 ON)"
   - Send command: `relay R8 ON`
   - Monitor MQTT topic `controller/relays/R8` for value `1`
   - Display: "Engine Relay (R8): ON - Engine should now be enabled"
   - Operator confirms: "Is engine control signal active?" (Yes/No)

3. **Disable Test**
   - Prompt: "Test will now DISABLE the engine relay (R8 OFF)"
   - Send command: `relay R8 OFF`
   - Monitor MQTT topic `controller/relays/R8` for value `0`
   - Display: "Engine Relay (R8): OFF - Engine should now be disabled"
   - Operator confirms: "Is engine control signal inactive?" (Yes/No)

4. **Result Recording**
   - Pass: Both enable and disable operations confirmed by operator
   - Fail: Any operation not confirmed or MQTT mismatch
   - Log: Timestamp, test result, operator ID

---

### Test 2: Emergency Stop Button (Pin 12)

**Purpose**: Verify emergency stop functionality and system safety response
**Hardware**: Pin 12 (INPUT_PULLUP, Active LOW)
**Safety**: Highest priority safety input
**MQTT Topics**: 
- `controller/safety/emergency_stop` (0=inactive, 1=active)
- `controller/relays/R8` (should go to 0 when E-Stop active)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Emergency Stop is currently: INACTIVE (not pressed)"
   - Monitor Pin 12 state: HIGH (not pressed)
   - All relays should be in normal state

2. **Activation Test**
   - Prompt: "PRESS and HOLD the Emergency Stop button"
   - Monitor Pin 12 for transition to LOW (button pressed)
   - Verify MQTT topic `controller/safety/emergency_stop` shows `1`
   - Verify ALL relays turn OFF (especially R1, R2, R8)
   - Display: "EMERGENCY STOP ACTIVE - All hydraulic operations halted"
   - Operator confirms: "Are all operations stopped?" (Yes/No)

3. **Release Test**
   - Prompt: "RELEASE the Emergency Stop button"
   - Monitor Pin 12 for transition to HIGH (button released)
   - Verify MQTT topic `controller/safety/emergency_stop` shows `0`
   - Display: "Emergency Stop released - System ready for reset"
   - Operator confirms: "Is E-Stop button released?" (Yes/No)

4. **Reset Verification**
   - Note: System may require manual reset command after E-Stop
   - If needed: Send command `safety reset`
   - Verify system returns to operational state

5. **Result Recording**
   - Pass: E-Stop activation halts all operations, release allows reset
   - Fail: System doesn't stop or doesn't reset properly
   - Log: Timestamp, test result, operator ID
   - **Critical**: This is a safety-critical test - failures must be addressed immediately

---

### Test 3A: Extend Limit Switch (Pin 6)

**Purpose**: Verify extend limit switch detection and safety cutoff
**Hardware**: Pin 6 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/extend_limit` (0=not reached, 1=at limit)
- `controller/relays/R1` (should turn OFF at limit)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Extend Limit Switch is currently: NOT ACTIVE"
   - Monitor Pin 6 state: HIGH (limit not reached)
   - Verify MQTT topic `controller/inputs/extend_limit` shows `0`

2. **Limit Activation Test**
   - Prompt: "Manually ACTIVATE the extend limit switch (press/move to limit)"
   - Monitor Pin 6 for transition to LOW (limit reached)
   - Verify MQTT topic `controller/inputs/extend_limit` shows `1`
   - If R1 was active, verify it automatically turns OFF
   - Display: "EXTEND LIMIT REACHED - Relay R1 safety cutoff active"
   - Operator confirms: "Is limit switch activated?" (Yes/No)

3. **Limit Release Test**
   - Prompt: "RELEASE the extend limit switch (move away from limit)"
   - Monitor Pin 6 for transition to HIGH (limit not reached)
   - Verify MQTT topic `controller/inputs/extend_limit` shows `0`
   - Display: "Extend limit switch released - Normal operation restored"
   - Operator confirms: "Is limit switch released?" (Yes/No)

4. **Result Recording**
   - Pass: Limit switch activation detected, R1 safety cutoff works
   - Fail: Limit switch not detected or safety cutoff doesn't work
   - Log: Timestamp, test result, operator ID

---

### Test 3B: Retract Limit Switch (Pin 7)

**Purpose**: Verify retract limit switch detection and safety cutoff
**Hardware**: Pin 7 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/retract_limit` (0=not reached, 1=at limit)
- `controller/relays/R2` (should turn OFF at limit)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Retract Limit Switch is currently: NOT ACTIVE"
   - Monitor Pin 7 state: HIGH (limit not reached)
   - Verify MQTT topic `controller/inputs/retract_limit` shows `0`

2. **Limit Activation Test**
   - Prompt: "Manually ACTIVATE the retract limit switch (press/move to limit)"
   - Monitor Pin 7 for transition to LOW (limit reached)
   - Verify MQTT topic `controller/inputs/retract_limit` shows `1`
   - If R2 was active, verify it automatically turns OFF
   - Display: "RETRACT LIMIT REACHED - Relay R2 safety cutoff active"
   - Operator confirms: "Is limit switch activated?" (Yes/No)

3. **Limit Release Test**
   - Prompt: "RELEASE the retract limit switch (move away from limit)"
   - Monitor Pin 7 for transition to HIGH (limit not reached)
   - Verify MQTT topic `controller/inputs/retract_limit` shows `0`
   - Display: "Retract limit switch released - Normal operation restored"
   - Operator confirms: "Is limit switch released?" (Yes/No)

4. **Result Recording**
   - Pass: Limit switch activation detected, R2 safety cutoff works
   - Fail: Limit switch not detected or safety cutoff doesn't work
   - Log: Timestamp, test result, operator ID

---

### Test 4: Safety Reset/Clear Button (Pin 4)

**Purpose**: Verify safety system reset functionality
**Hardware**: Pin 4 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/safety_reset` (0=not pressed, 1=pressed)
- `controller/safety/state` (safety system state)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Safety Reset Button is currently: NOT PRESSED"
   - Monitor Pin 4 state: HIGH (not pressed)
   - Verify MQTT topic `controller/inputs/safety_reset` shows `0`

2. **Button Press Test**
   - Prompt: "PRESS the Safety Reset/Clear button"
   - Monitor Pin 4 for transition to LOW (button pressed)
   - Verify MQTT topic `controller/inputs/safety_reset` shows `1`
   - Display: "Safety Reset activated - Clearing safety lockouts"
   - Operator confirms: "Is safety reset button pressed?" (Yes/No)
   - Note: System should clear safety lockouts (preserving error history)

3. **Button Release Test**
   - Prompt: "RELEASE the Safety Reset/Clear button"
   - Monitor Pin 4 for transition to HIGH (button released)
   - Verify MQTT topic `controller/inputs/safety_reset` shows `0`
   - Display: "Safety reset button released"
   - Operator confirms: "Is button released?" (Yes/No)

4. **Result Recording**
   - Pass: Button press/release detected, safety system responds
   - Fail: Button not detected or safety system doesn't respond
   - Log: Timestamp, test result, operator ID

---

### Test 5: Manual Extend Button (Pin 2)

**Purpose**: Verify manual extend button operation and relay control
**Hardware**: Pin 2 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/manual_extend` (0=not pressed, 1=pressed)
- `controller/relays/R1` (extend relay, should activate when pressed)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Manual Extend Button is currently: NOT PRESSED"
   - Monitor Pin 2 state: HIGH (not pressed)
   - Verify MQTT topic `controller/inputs/manual_extend` shows `0`
   - Verify Relay R1 is OFF

2. **Button Press Test**
   - Prompt: "PRESS and HOLD the Manual Extend button"
   - Monitor Pin 2 for transition to LOW (button pressed)
   - Verify MQTT topic `controller/inputs/manual_extend` shows `1`
   - Verify Relay R1 activates (turns ON)
   - Display: "Manual Extend ACTIVE - Relay R1 ON"
   - Operator confirms: "Is extend button pressed and R1 active?" (Yes/No)

3. **Button Release Test**
   - Prompt: "RELEASE the Manual Extend button"
   - Monitor Pin 2 for transition to HIGH (button released)
   - Verify MQTT topic `controller/inputs/manual_extend` shows `0`
   - Verify Relay R1 deactivates (turns OFF)
   - Display: "Manual Extend released - Relay R1 OFF"
   - Operator confirms: "Is button released and R1 off?" (Yes/No)

4. **Result Recording**
   - Pass: Button activates R1 when pressed, deactivates when released
   - Fail: Button not detected or R1 doesn't respond correctly
   - Log: Timestamp, test result, operator ID

---

### Test 6: Manual Retract Button (Pin 3)

**Purpose**: Verify manual retract button operation and relay control
**Hardware**: Pin 3 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/manual_retract` (0=not pressed, 1=pressed)
- `controller/relays/R2` (retract relay, should activate when pressed)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Manual Retract Button is currently: NOT PRESSED"
   - Monitor Pin 3 state: HIGH (not pressed)
   - Verify MQTT topic `controller/inputs/manual_retract` shows `0`
   - Verify Relay R2 is OFF

2. **Button Press Test**
   - Prompt: "PRESS and HOLD the Manual Retract button"
   - Monitor Pin 3 for transition to LOW (button pressed)
   - Verify MQTT topic `controller/inputs/manual_retract` shows `1`
   - Verify Relay R2 activates (turns ON)
   - Display: "Manual Retract ACTIVE - Relay R2 ON"
   - Operator confirms: "Is retract button pressed and R2 active?" (Yes/No)

3. **Button Release Test**
   - Prompt: "RELEASE the Manual Retract button"
   - Monitor Pin 3 for transition to HIGH (button released)
   - Verify MQTT topic `controller/inputs/manual_retract` shows `0`
   - Verify Relay R2 deactivates (turns OFF)
   - Display: "Manual Retract released - Relay R2 OFF"
   - Operator confirms: "Is button released and R2 off?" (Yes/No)

4. **Result Recording**
   - Pass: Button activates R2 when pressed, deactivates when released
   - Fail: Button not detected or R2 doesn't respond correctly
   - Log: Timestamp, test result, operator ID

---

### Test 7: Sequence Start Button (Pin 5)

**Purpose**: Verify automatic sequence start button operation
**Hardware**: Pin 5 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/sequence_start` (0=not pressed, 1=pressed)
- `controller/sequence/state` (sequence state: IDLE, EXTENDING, RETRACTING, etc.)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Sequence Start Button is currently: NOT PRESSED"
   - Monitor Pin 5 state: HIGH (not pressed)
   - Verify MQTT topic `controller/inputs/sequence_start` shows `0`
   - Verify sequence state is IDLE

2. **Button Press Test**
   - Prompt: "PRESS the Sequence Start button (momentary press)"
   - Monitor Pin 5 for transition to LOW (button pressed)
   - Verify MQTT topic `controller/inputs/sequence_start` shows `1`
   - Verify sequence state changes from IDLE
   - Display: "Sequence Start activated - Automatic cycle beginning"
   - Operator confirms: "Did automatic sequence start?" (Yes/No)

3. **Button Release Test**
   - Prompt: "RELEASE the Sequence Start button"
   - Monitor Pin 5 for transition to HIGH (button released)
   - Verify MQTT topic `controller/inputs/sequence_start` shows `0`
   - Note: Sequence should continue running even after button release
   - Display: "Sequence start button released - Sequence continues"
   - Operator confirms: "Is button released?" (Yes/No)

4. **Result Recording**
   - Pass: Button press initiates sequence, release doesn't stop sequence
   - Fail: Button not detected or sequence doesn't start
   - Log: Timestamp, test result, operator ID
   - Note: Allow sequence to complete or manually stop for next test

---

### Test 8: Splitter Operator Signal (Pin 8)

**Purpose**: Verify splitter operator communication signal
**Hardware**: Pin 8 (INPUT_PULLUP, Active LOW)
**MQTT Topics**: 
- `controller/inputs/splitter_operator` (0=not signaling, 1=signaling)
- `controller/data/splitter_operator` (signal status with timestamp)

**Test Procedure**:
1. **Initial State Verification**
   - Display: "Splitter Operator Signal is currently: INACTIVE"
   - Monitor Pin 8 state: HIGH (not signaling)
   - Verify MQTT topic `controller/inputs/splitter_operator` shows `0`

2. **Signal Activation Test**
   - Prompt: "ACTIVATE the Splitter Operator Signal (press button/switch)"
   - Monitor Pin 8 for transition to LOW (signal active)
   - Verify MQTT topic `controller/inputs/splitter_operator` shows `1`
   - Verify MQTT notification published to `controller/data/splitter_operator`
   - Display: "Splitter Operator Signal ACTIVE - Basket exchange requested"
   - Operator confirms: "Is signal button/switch activated?" (Yes/No)

3. **Signal Deactivation Test**
   - Prompt: "DEACTIVATE the Splitter Operator Signal (release button/switch)"
   - Monitor Pin 8 for transition to HIGH (signal inactive)
   - Verify MQTT topic `controller/inputs/splitter_operator` shows `0`
   - Display: "Splitter Operator Signal released"
   - Operator confirms: "Is signal button/switch released?" (Yes/No)

4. **Result Recording**
   - Pass: Signal activation/deactivation detected and published via MQTT
   - Fail: Signal not detected or MQTT notification not sent
   - Log: Timestamp, test result, operator ID

---

### Test 9: Mill Lamp LED (Pin 9 / Relay R9)

**Purpose**: Verify mill lamp visual indicator operation
**Hardware**: Pin 9 (OUTPUT), Relay R9
**MQTT Topics**: 
- `controller/relays/R9` (0=OFF, 1=ON)

**Test Procedure**:
1. **Locate Mill Lamp**
   - Display: "Please locate the Mill Lamp (Yellow indicator light)"
   - Prompt: "Can you see the Mill Lamp?" (Yes/No)
   - If No: Provide guidance on lamp location

2. **Flash Test**
   - Display: "The Mill Lamp will now flash 5 times"
   - Execute sequence:
     - Send command: `relay R9 ON`
     - Wait 500ms
     - Send command: `relay R9 OFF`
     - Wait 500ms
     - Repeat 5 times
   - Monitor MQTT topic `controller/relays/R9` during flashing
   - Verify values alternate between `1` (ON) and `0` (OFF)

3. **Operator Confirmation**
   - Prompt: "Did you observe the Mill Lamp flashing 5 times?" (Yes/No)
   - If Yes: Display "Mill Lamp visual test PASSED"
   - If No: Display "Mill Lamp visual test FAILED - Check lamp and wiring"

4. **Final State**
   - Send command: `relay R9 OFF`
   - Verify Mill Lamp is OFF
   - Verify MQTT topic `controller/relays/R9` shows `0`

5. **Result Recording**
   - Pass: Operator confirms visual flashing observed
   - Fail: Operator does not observe flashing or MQTT mismatch
   - Log: Timestamp, test result, operator ID

---

### Test 10: Status LED (Pin 13)

**Purpose**: Verify built-in status LED operation
**Hardware**: Pin 13 (OUTPUT, built-in LED on Arduino board)

**Test Procedure**:
1. **Locate Status LED**
   - Display: "Please locate the built-in Status LED on the Arduino board (Pin 13)"
   - Prompt: "Can you see the Status LED on the controller board?" (Yes/No)
   - If No: Provide guidance - "Look for built-in LED labeled 'L' or near Pin 13"

2. **Flash Test**
   - Display: "The Status LED will now flash 5 times"
   - Execute sequence:
     - Set Pin 13 HIGH (LED ON)
     - Wait 500ms
     - Set Pin 13 LOW (LED OFF)
     - Wait 500ms
     - Repeat 5 times
   - Note: This is direct pin control, not relay-based

3. **Operator Confirmation**
   - Prompt: "Did you observe the Status LED flashing 5 times?" (Yes/No)
   - If Yes: Display "Status LED visual test PASSED"
   - If No: Display "Status LED visual test FAILED - Check LED or board"

4. **Final State**
   - Set Pin 13 to normal operating mode
   - Restore status LED to system-controlled state
   - Display: "Status LED returned to normal operation"

5. **Result Recording**
   - Pass: Operator confirms visual flashing observed
   - Fail: Operator does not observe flashing
   - Log: Timestamp, test result, operator ID

---

### Test 11: System Hydraulic Pressure Sensor (A1)

**Purpose**: Verify main hydraulic pressure sensor readings and calibration
**Hardware**: Analog Pin A1 (4-20mA current loop, 0-5000 PSI range)
**MQTT Topics**: 
- `controller/pressure/hydraulic_system` (pressure in PSI)
- `controller/sensors/a1_raw` (raw ADC value, 0-1023)
- `controller/sensors/a1_voltage` (voltage reading, 1.0-5.0V)

**Test Procedure**:
1. **Initial Reading**
   - Display: "System Hydraulic Pressure Sensor Test"
   - Read current pressure from MQTT topic `controller/pressure/hydraulic_system`
   - Display: "Current pressure reading: XXX PSI"
   - Read raw ADC value and voltage
   - Display: "Raw ADC: XXX, Voltage: X.XXV"

2. **Zero Pressure Test**
   - Prompt: "Ensure hydraulic system is at ZERO pressure (no load)"
   - Operator confirms: "Is system at zero pressure?" (Yes/No)
   - Read pressure value
   - Expected: ~0 PSI (tolerance: ±50 PSI)
   - Expected voltage: ~1.0V (4mA current loop minimum)
   - Display reading and tolerance check

3. **Operating Pressure Test**
   - Prompt: "Apply normal operating pressure to hydraulic system"
   - Operator confirms: "Is system at operating pressure?" (Yes/No)
   - Operator inputs expected pressure: "Enter expected pressure (PSI):"
   - Read actual pressure value
   - Calculate difference: |Actual - Expected|
   - Tolerance: ±100 PSI or ±5% of reading
   - Display: "Expected: XXX PSI, Actual: XXX PSI, Difference: ±XX PSI"

4. **Sensor Health Check**
   - Verify voltage range is within 1.0-5.0V (valid 4-20mA range)
   - Check for sensor disconnection (voltage < 0.5V or > 5.5V)
   - Verify readings are stable (no excessive noise/fluctuation)
   - Display sensor health status

5. **Result Recording**
   - Pass: Readings within tolerance, sensor health good
   - Fail: Readings out of tolerance or sensor health issues
   - Log: Timestamp, pressure readings, tolerance check, operator ID

---

### Test 12: Filter Pressure Sensor (A5)

**Purpose**: Verify hydraulic filter pressure sensor readings
**Hardware**: Analog Pin A5 (0-5V voltage output, 0-30 PSI range)
**MQTT Topics**: 
- `controller/pressure/hydraulic_filter` (pressure in PSI)
- `controller/sensors/a5_raw` (raw ADC value, 0-1023)
- `controller/sensors/a5_voltage` (voltage reading, 0-5.0V)

**Test Procedure**:
1. **Initial Reading**
   - Display: "Filter Pressure Sensor Test"
   - Read current pressure from MQTT topic `controller/pressure/hydraulic_filter`
   - Display: "Current filter pressure reading: XX PSI"
   - Read raw ADC value and voltage
   - Display: "Raw ADC: XXX, Voltage: X.XXV"

2. **Zero Pressure Test**
   - Prompt: "Ensure hydraulic system is at ZERO pressure (no flow)"
   - Operator confirms: "Is system at zero pressure?" (Yes/No)
   - Read pressure value
   - Expected: ~0 PSI (tolerance: ±2 PSI)
   - Expected voltage: ~0.0V
   - Display reading and tolerance check

3. **Filter Pressure Test**
   - Prompt: "Start hydraulic system and allow pressure to build"
   - Operator confirms: "Is system running with flow through filter?" (Yes/No)
   - Read actual pressure value
   - Normal filter pressure: 5-15 PSI (clean filter)
   - Warning threshold: >20 PSI (filter may need replacement)
   - Critical threshold: >25 PSI (filter replacement required)
   - Display: "Filter pressure: XX PSI - Status: [NORMAL/WARNING/CRITICAL]"

4. **Sensor Health Check**
   - Verify voltage range is within 0-5.0V (valid sensor range)
   - Check for sensor disconnection (unstable or out-of-range readings)
   - Verify readings are stable (no excessive noise/fluctuation)
   - Display sensor health status

5. **Filter Condition Assessment**
   - Based on pressure readings, assess filter condition:
     - 0-10 PSI: Filter clean, good condition
     - 10-20 PSI: Filter normal wear, monitor
     - 20-25 PSI: Filter needs replacement soon
     - >25 PSI: Filter replacement required immediately
   - Display filter condition recommendation

6. **Result Recording**
   - Pass: Sensor functioning, readings within expected range
   - Fail: Sensor malfunction or out-of-range readings
   - Log: Timestamp, pressure readings, filter condition, operator ID

---

### Test 13: Complete Hydraulic Sequence Test

**Purpose**: Verify full automatic hydraulic sequence operation
**Hardware**: All hydraulic components (R1, R2, Pins 6, 7, pressure sensors)
**MQTT Topics**: 
- `controller/sequence/state` (IDLE, EXTENDING, RETRACTING, COMPLETE, ERROR)
- `controller/relays/R1` (extend relay)
- `controller/relays/R2` (retract relay)
- `controller/inputs/extend_limit` (extend limit switch)
- `controller/inputs/retract_limit` (retract limit switch)
- `controller/pressure/hydraulic_system` (system pressure)

**Test Procedure**:
1. **Pre-Test Safety Checks**
   - Verify all previous tests passed
   - Verify emergency stop is not active
   - Verify system pressure is adequate (>500 PSI recommended)
   - Verify both limit switches are in normal state (not triggered)
   - Display: "Pre-test safety checks complete"
   - Operator confirms: "Is area clear and safe to run sequence?" (Yes/No)

2. **Initiate Sequence**
   - Display: "Starting automatic hydraulic sequence..."
   - Send command via Pin 5 (Sequence Start) or command: `sequence start`
   - Verify MQTT topic `controller/sequence/state` changes from IDLE
   - Record start timestamp

3. **Monitor EXTENDING Phase**
   - Display: "Phase 1: EXTENDING cylinder"
   - Verify `controller/sequence/state` shows EXTENDING
   - Verify Relay R1 activates (ON)
   - Verify Relay R2 remains OFF
   - Monitor system pressure (should remain within safe limits)
   - Monitor extend limit switch (Pin 6)
   - Wait for extend limit to trigger (Pin 6 goes LOW)
   - Verify R1 automatically turns OFF when limit reached
   - Display: "Extend limit reached - transitioning to retract"
   - Operator observes: "Did cylinder extend to full limit?" (Yes/No)

4. **Monitor RETRACTING Phase**
   - Display: "Phase 2: RETRACTING cylinder"
   - Verify `controller/sequence/state` shows RETRACTING
   - Verify Relay R2 activates (ON)
   - Verify Relay R1 remains OFF
   - Monitor system pressure (should remain within safe limits)
   - Monitor retract limit switch (Pin 7)
   - Wait for retract limit to trigger (Pin 7 goes LOW)
   - Verify R2 automatically turns OFF when limit reached
   - Display: "Retract limit reached - sequence complete"
   - Operator observes: "Did cylinder retract to full limit?" (Yes/No)

5. **Sequence Completion**
   - Verify `controller/sequence/state` shows COMPLETE or IDLE
   - Verify both R1 and R2 are OFF
   - Record end timestamp
   - Calculate total sequence time
   - Display: "Sequence completed in XX.X seconds"

6. **Sequence Validation**
   - Verify no errors occurred during sequence
   - Verify pressure remained within safe limits throughout
   - Verify both limit switches triggered at appropriate times
   - Verify proper relay state transitions (R1→OFF, R2→ON, R2→OFF)
   - Display sequence validation results

7. **Result Recording**
   - Pass: Complete sequence executed without errors, all safety checks passed
   - Fail: Sequence error, safety limit exceeded, or improper operation
   - Log: Timestamp, sequence duration, pressure min/max, operator observations, operator ID

---

## Test Report Format

The testing interface shall generate a comprehensive test report containing:

### Report Header
- **Test Date/Time**: ISO 8601 timestamp
- **Operator Name/ID**: Operator identification
- **Controller Serial Number**: Hardware identification
- **Firmware Version**: Software version under test
- **Test Session ID**: Unique test session identifier

### Individual Test Results
For each test (1-13):
- **Test Number & Name**
- **Test Status**: PASS / FAIL / SKIPPED
- **Start Time**: Test initiation timestamp
- **End Time**: Test completion timestamp
- **Duration**: Test execution time
- **Operator Confirmations**: All Yes/No responses recorded
- **MQTT Data**: Key topic values captured during test
- **Notes**: Any operator observations or anomalies

### Summary Statistics
- **Total Tests**: 13
- **Tests Passed**: X
- **Tests Failed**: X
- **Tests Skipped**: X
- **Pass Rate**: XX.X%
- **Total Test Duration**: XX minutes
- **Overall Status**: PASS / FAIL

### Failure Details
For any failed tests:
- **Test Number & Name**
- **Failure Reason**: Detailed explanation
- **Expected Behavior**: What should have happened
- **Actual Behavior**: What actually happened
- **Recommended Action**: Troubleshooting steps

### Safety Critical Summary
Special section highlighting safety-critical test results:
- Test 1: Engine Relay - [PASS/FAIL]
- Test 2: Emergency Stop - [PASS/FAIL]
- Test 3A: Extend Limit - [PASS/FAIL]
- Test 3B: Retract Limit - [PASS/FAIL]
- **Safety Status**: All safety tests must PASS for equipment operation

### Report Footer
- **Operator Signature**: (Electronic/Digital confirmation)
- **Supervisor Review**: (If required)
- **Next Test Due**: (Based on maintenance schedule)
- **Report Generated**: Timestamp

---

## Implementation Requirements

### MQTT Connection
- Connect to MQTT broker using credentials from `arduino_secrets.h`
- Subscribe to all relevant topics listed in test procedures
- Implement connection health monitoring
- Handle disconnections gracefully with operator notification

### User Interface Requirements
- Clear, readable display for operator instructions
- Large buttons for operator confirmations (Yes/No, Pass/Fail)
- Real-time MQTT data display during tests
- Progress indicator showing test completion percentage
- Ability to pause/resume testing (except during active sequences)
- Emergency stop button accessible at all times

### Data Logging
- All test results saved to persistent storage
- MQTT data captured during test execution
- Timestamp accuracy: ±1 second
- Export capability: PDF, CSV, JSON formats
- Historical test reports accessible for review

### Error Handling
- MQTT connection failures
- Timeout detection for operator responses
- Test sequence interruptions
- Data validation and range checking
- Graceful degradation if non-critical features fail

### Safety Features
- Prominent emergency stop indication
- Warning messages for safety-critical test failures
- Automatic test sequence abort on safety violations
- Require supervisor override for failed safety tests
- Equipment lockout recommendation for critical failures

---

## Testing Interface Design Notes

### Target Users
- Equipment operators (pre-shift testing)
- Maintenance technicians (troubleshooting)
- Supervisors (certification and review)

### Skill Level Assumptions
- Basic understanding of hydraulic equipment
- Ability to read and follow instructions
- No programming or technical expertise required
- Familiar with basic safety procedures

### Accessibility
- Large, clear fonts (minimum 14pt)
- High contrast color scheme
- Touch-friendly interface (minimum 44px touch targets)
- Audio feedback optional
- Multi-language support consideration

### Platform Recommendations
- Web-based interface (browser accessible)
- Tablet-optimized (10" screen minimum recommended)
- Mobile responsive design
- Offline capability with data sync when connected
- Cross-platform compatibility (Windows, Linux, Android, iOS)

---

**Document Version**: 1.0  
**Created**: October 14, 2025  
**Author**: System Architect  
**Purpose**: Specification for AI-implemented operator testing interface  
**Hardware**: Arduino UNO R4 WiFi LogSplitter Controller  
**Firmware**: LogSplitter Controller v2.0
