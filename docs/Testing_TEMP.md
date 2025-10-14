# LogSplitter Controller - Operator Testing Interface Specification

## Document Purpose

This document specifies requirements for another AI to build an operator testing interface. The interface will communicate with the LogSplitter Controller to test all inputs, outputs, and sensors during pre-shift safety verification.

**Important Notes:**
- This is a specification document for AI implementation
- A separate backend interface will be created on the controller side
- The backend will allow the testing program to:
  - Put controller in test mode
  - Disable safety alarms during testing
  - Query input states
  - Control outputs for verification
  - Monitor MQTT topics

---

## Hardware Under Test

**Controller**: Arduino UNO R4 WiFi LogSplitter Controller
**Firmware**: LogSplitter Controller v2.0
**MQTT Prefix**: `controller/`

---

## Standard Switch Test Pattern

**This pattern will be defined once we complete Test 2 (Emergency Stop), then applied to all switch tests.**

### Test Components:
1. Initial state verification (check MQTT topic for current state)
2. Activation test (operator presses/activates switch)
3. MQTT monitoring (verify topic value changes)
4. Release test (operator releases switch)
5. Return state verification (confirm return to initial state)
6. Pass/Fail determination

---

## Test 1: Engine Run/Stop Relay (R8)

**Type**: Output Test (Relay)
**Purpose**: Verify engine enable/disable relay operation
**Hardware**: Relay R8, 12V DC control signal

**MQTT Topics**:
- `controller/relays/R8` (status: 0=OFF, 1=ON)

**Test Procedure**:
1. **Initial State**: Verify R8 is OFF (MQTT shows 0)
2. **Enable Test**: 
   - Send command to controller: `relay R8 ON`
   - Verify MQTT `controller/relays/R8` changes to 1
   - Operator confirms engine control signal active
3. **Disable Test**:
   - Send command: `relay R8 OFF`
   - Verify MQTT `controller/relays/R8` changes to 0
   - Operator confirms engine control signal inactive
4. **Result**: Pass if both enable/disable confirmed

**Status**: ✅ Complete

---

## Test 2: Emergency Stop Button (Pin 12)

**Type**: Input Test (Switch)
**Purpose**: Verify emergency stop functionality
**Hardware**: Pin 12 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: 
- `controller/inputs/emergency_stop` (ASSUMED - TO BE CONFIRMED)
- Expected values: 0 (not pressed/HIGH) or 1 (pressed/LOW)? TO BE CONFIRMED

**Test Procedure** (DRAFT - NEEDS YOUR INPUT):

### Initial State Verification
- Read MQTT topic `controller/inputs/emergency_stop`
- Expected value when NOT pressed: ??? (0 or 1?)
- Display to operator: "Emergency Stop is currently: INACTIVE"

### Activation Test
- Prompt operator: "PRESS and HOLD the Emergency Stop button"
- Monitor MQTT topic for state change
- Expected value when PRESSED: ??? (0 or 1?)
- System behavior when pressed:
  - All relays turn OFF? (YES/NO?)
  - Safety state changes? (What MQTT topic shows this?)
  - Any other feedback?
- Operator confirms: "Is E-Stop activated?" (Yes/No)

### Release Test
- Prompt operator: "RELEASE the Emergency Stop button"
- Monitor MQTT topic for return to initial state
- Expected value when RELEASED: ??? (back to initial)
- System behavior:
  - Does system automatically recover?
  - Or does it need manual reset?
  - What command puts it back in operational state?

### Pass/Fail Criteria
- MQTT state changes correctly: Press → Active, Release → Inactive
- System responds appropriately (all relays OFF when pressed)
- Operator confirms physical button operation

**Status**: 🔄 IN PROGRESS - Waiting for behavior specification

**Questions Needed:**
1. What is the actual MQTT topic for Pin 12 input state?
2. What are the MQTT values (0/1, true/false, HIGH/LOW)?
3. What system behaviors occur on E-Stop press?
4. Does release automatically restore operation or need manual reset?
5. What MQTT topics show safety system state?

---

## Test 3A: Extend Limit Switch (Pin 6)

**Type**: Input Test (Switch)
**Hardware**: Pin 6 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: TO BE DETERMINED
**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 3B: Retract Limit Switch (Pin 7)

**Type**: Input Test (Switch)
**Hardware**: Pin 7 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: TO BE DETERMINED
**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 4: Safety Reset/Clear Button (Pin 4)

**Type**: Input Test (Switch)
**Hardware**: Pin 4 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: TO BE DETERMINED
**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 5: Manual Extend Button (Pin 2)

**Type**: Input Test (Switch)
**Hardware**: Pin 2 (INPUT_PULLUP, Active LOW)

**From PINS.md**:
- Press and hold → Activates R1 relay
- Release → Deactivates R1 relay
- Safety cutoff at extend limit (Pin 6)

**MQTT Topics**: TO BE DETERMINED
- Input state topic: `controller/inputs/manual_extend` ???
- Related relay topic: `controller/relays/R1` (but this is output, not input being tested)

**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 6: Manual Retract Button (Pin 3)

**Type**: Input Test (Switch)
**Hardware**: Pin 3 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: TO BE DETERMINED
**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 7: Sequence Start Button (Pin 5)

**Type**: Input Test (Switch)
**Hardware**: Pin 5 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: TO BE DETERMINED
**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 8: Splitter Operator Signal (Pin 8)

**Type**: Input Test (Switch)
**Hardware**: Pin 8 (INPUT_PULLUP, Active LOW)

**From PINS.md**:
- MQTT Topic: `controller/data/splitter_operator`

**Test Procedure**: TO BE DOCUMENTED (will use standard switch pattern)

**Status**: ⏸️ Pending

---

## Test 9: Mill Lamp LED (Pin 9 / Relay R9)

**Type**: Output Test (LED/Relay)
**Hardware**: Pin 9 (OUTPUT), Relay R9

**MQTT Topics**:
- `controller/relays/R9` (0=OFF, 1=ON)

**Test Procedure**:
1. Locate Mill Lamp (yellow indicator)
2. Flash lamp 5 times (ON 500ms, OFF 500ms)
3. Operator confirms visual observation
4. Pass if operator confirms flashing

**Status**: ✅ Complete

---

## Test 10: Status LED (Pin 13)

**Type**: Output Test (LED)
**Hardware**: Pin 13 (OUTPUT, built-in LED)

**Test Procedure**:
1. Locate Status LED on Arduino board (Pin 13, labeled 'L')
2. Flash LED 5 times (ON 500ms, OFF 500ms)
3. Operator confirms visual observation
4. Restore LED to normal system-controlled state
5. Pass if operator confirms flashing

**Status**: ✅ Complete

---

## Test 11: System Hydraulic Pressure Sensor (A1)

**Type**: Sensor Test (Analog Input)
**Hardware**: Analog Pin A1 (4-20mA current loop, 0-5000 PSI)

**MQTT Topics**: TO BE DETERMINED

**Status**: ⏸️ Pending

---

## Test 12: Filter Pressure Sensor (A5)

**Type**: Sensor Test (Analog Input)
**Hardware**: Analog Pin A5 (0-5V voltage, 0-30 PSI)

**MQTT Topics**: TO BE DETERMINED

**Status**: ⏸️ Pending

---

## Test 13: Complete Hydraulic Sequence Test

**Type**: Integration Test
**Hardware**: All hydraulic components

**Status**: ⏸️ Pending (requires all previous tests completed)

---

## Backend Interface Requirements

**To be implemented on controller side:**

### Test Mode API
- Command to enter test mode
- Command to exit test mode
- Disable safety alarms during testing
- Prevent normal operation interference

### Input Monitoring API
- Query all digital input states
- Subscribe to input state change events
- Provide debounced, stable readings

### Output Control API
- Command individual relay ON/OFF
- Command LED flash patterns
- Query relay states

### Sensor Reading API
- Query analog sensor values (raw ADC, voltage, calculated)
- Query pressure readings (PSI)
- Get sensor health status

### Safety State API
- Query current safety system state
- Temporarily override safety for testing
- Reset safety system after test

**Status**: 📋 Requirements gathering phase

---

## Next Steps

1. ✅ Complete MQTT topic updates in PINS.md (controller/* prefix)
2. 🔄 Define standard switch test pattern (starting with Test 2)
3. ⏸️ Document MQTT topics for all input switches
4. ⏸️ Complete test procedures for Tests 3-8
5. ⏸️ Document sensor tests (Tests 11-12)
6. ⏸️ Document integration test (Test 13)
7. ⏸️ Design backend interface on controller
8. ⏸️ Implement testing interface (by another AI)

---

## Current Focus

**Test 2: Emergency Stop Button**
- Defining standard switch test pattern
- Determining MQTT topics and values
- Establishing pass/fail criteria
- This pattern will be applied to all other switch tests

---

**Document Version**: 0.1 (Work in Progress)
**Created**: October 14, 2025
**Status**: Requirements gathering
**Next Session**: Complete Test 2 specification with actual system behavior
