# LogSplitter Controller - Operator Testing Interface Specification

## Document Purpose

This document specifies requirements for another AI to build an operator testing interface. The interface will communicate with the LogSplitter Controller to test all inputs, outputs, and sensors during pre-shift safety verification.

**Important Notes:**
- This is a specification document for AI implementation
- Python program will communicate via MQTT
- All commands use the same format as Serial interface
- Commands sent to `controller/control` topic
- Responses received on `controller/control/resp` topic

---

## MQTT Communication Interface

### Command Topic (Publish)

**Topic**: `controller/control`  
**QoS**: 0  
**Retained**: No  
**Format**: Plain text commands (same as serial console)  
**Direction**: Testing program → Controller

**Examples**:

```python
# Relay commands
mqtt_publish("controller/control", "relay R1 ON")
mqtt_publish("controller/control", "relay R8 OFF")

# Show system status
mqtt_publish("controller/control", "show")

# Set configuration
mqtt_publish("controller/control", "set loglevel 6")
```

### Response Topic (Subscribe)

**Topic**: `controller/control/resp`  
**QoS**: 0  
**Retained**: No  
**Format**: Plain text response messages  
**Direction**: Controller → Testing program

**Examples**:

```text
"relay R1 ON"
"relay R8 OFF"
"SAFETY: Extend operation stopped - limit switch reached"
"manual extend started (safety-monitored)"
```

### State Monitoring Topics (Subscribe)

All state topics publish automatically when values change. Testing program should subscribe to these topics.

**Input Topics** (Active LOW, debounced):

- `controller/inputs/{PIN}/state` - Numeric: `0` (not pressed/inactive) or `1` (pressed/active)
- `controller/inputs/{PIN}/active` - Boolean: `false` or `true`
- Valid PIN values: 2, 3, 4, 5, 6, 7, 8, 12

**Relay Topics**:

- `controller/relays/{NUM}/state` - Numeric: `0` (OFF) or `1` (ON)
- `controller/relays/{NUM}/active` - Boolean: `false` or `true`
- Valid NUM values: 1-9

**Pressure Topics**:

- `controller/pressure/system/psi` - Float: `0.0` to `5000.0` PSI
- `controller/pressure/system/raw` - Integer: `0` to `1023` (ADC)
- `controller/pressure/system/voltage` - Float: `0.0` to `5.0` V
- `controller/pressure/system/ma` - Float: `4.0` to `20.0` mA
- `controller/pressure/filter/psi` - Float: `0.0` to `30.0` PSI
- `controller/pressure/filter/raw` - Integer: `0` to `1023` (ADC)
- `controller/pressure/filter/voltage` - Float: `0.0` to `5.0` V

**Sequence Topics**:

- `controller/sequence/state` - String: `idle`, `start`, `stage1`, `stage2`, `complete`, `abort`
- `controller/sequence/event` - String: event descriptions (e.g., `started_R1`, `switched_to_R2`)
- `controller/sequence/active` - Numeric: `0` or `1`
- `controller/sequence/stage` - Numeric: `0`, `1`, or `2`
- `controller/sequence/elapsed` - Integer: milliseconds since sequence start

**Safety Topics**:

- `controller/safety/active` - Numeric: `0` (OK) or `1` (ACTIVE)
- `controller/safety/estop` - Numeric: `0` (OK) or `1` (ACTIVE)
- `controller/safety/engine` - String: `RUNNING` or `STOPPED`

### Python MQTT Implementation Example

```python
import paho.mqtt.client as mqtt
import time

class LogSplitterTester:
    def __init__(self, broker_host="192.168.1.100", broker_port=1883):
        self.client = mqtt.Client()
        self.client.on_message = self.on_message
        self.received_messages = {}
        
    def connect(self):
        self.client.connect(broker_host, broker_port, 60)
        
        # Subscribe to all monitoring topics
        self.client.subscribe("controller/control/resp")
        self.client.subscribe("controller/inputs/+/state")
        self.client.subscribe("controller/inputs/+/active")
        self.client.subscribe("controller/relays/+/state")
        self.client.subscribe("controller/relays/+/active")
        self.client.subscribe("controller/pressure/#")
        self.client.subscribe("controller/sequence/#")
        self.client.subscribe("controller/safety/#")
        
        self.client.loop_start()
        
    def on_message(self, client, userdata, message):
        topic = message.topic
        payload = message.payload.decode()
        self.received_messages[topic] = payload
        print(f"Received: {topic} = {payload}")
        
    def send_command(self, command):
        """Send command to controller"""
        self.client.publish("controller/control", command)
        
    def relay_on(self, relay_num):
        """Turn relay ON (relay_num: 1-9)"""
        self.send_command(f"relay R{relay_num} ON")
        time.sleep(0.1)  # Allow time for state update
        
    def relay_off(self, relay_num):
        """Turn relay OFF (relay_num: 1-9)"""
        self.send_command(f"relay R{relay_num} OFF")
        time.sleep(0.1)
        
    def get_input_state(self, pin):
        """Get input state for pin (2,3,4,5,6,7,8,12)"""
        state_topic = f"controller/inputs/{pin}/state"
        return self.received_messages.get(state_topic, "0")
        
    def get_relay_state(self, num):
        """Get relay state (1-9)"""
        state_topic = f"controller/relays/{num}/state"
        return self.received_messages.get(state_topic, "0")
        
    def get_pressure_psi(self, sensor="system"):
        """Get pressure reading (sensor: 'system' or 'filter')"""
        psi_topic = f"controller/pressure/{sensor}/psi"
        return float(self.received_messages.get(psi_topic, "0.0"))
        
    def wait_for_input_change(self, pin, expected_state, timeout=5.0):
        """Wait for input to reach expected state"""
        start = time.time()
        while (time.time() - start) < timeout:
            if self.get_input_state(pin) == str(expected_state):
                return True
            time.sleep(0.05)
        return False
```

### Data Type Reference

**String Values**:

- Boolean text: `"true"` or `"false"` (lowercase)
- Numeric text: `"0"`, `"1"`, `"2.5"`, `"1234.56"`, etc.
- State text: `"idle"`, `"RUNNING"`, `"STOPPED"`, etc.

**Numeric Conversions**:

```python
# Convert MQTT payload to appropriate Python type
state = int(payload)  # For state topics (0 or 1)
active = payload.lower() == "true"  # For active topics
psi = float(payload)  # For pressure topics
elapsed = int(payload)  # For elapsed time topics
```

**Active LOW Logic**:

- Physical: Button NOT pressed = Pin HIGH = 5V
- MQTT: state = `0`, active = `false`
- Physical: Button pressed = Pin LOW = 0V  
- MQTT: state = `1`, active = `true`

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
   - Operator confirms relay clicks and engine control signal is active (DO NOT start engine yet)
3. **Disable Test**:
   - Send command: `relay R8 OFF`
   - Verify MQTT `controller/relays/R8` changes to 0
   - Operator confirms relay clicks and engine control signal is inactive
4. **Result**: Pass if relay clicks and MQTT changes confirmed

**Status**: ✅ COMPLETE - Relay electrical operation verified

**Notes**: 
- This test verifies the relay operates electrically (clicks/activates)
- Engine is NOT started during this test - only relay function is verified
- Actual engine start occurs before Test 2 (E-Stop test)

---

## Engine Start Procedure (Before E-Stop Test)

**Instruction to Operator**:
1. Turn relay R8 ON using command: `relay R8 ON`
2. Verify MQTT `controller/relays/8/state` shows `1` (relay active)
3. **Operator: Turn the key and start the engine**
4. **Operator: Confirm the engine is running**
5. Wait for operator confirmation before proceeding to Test 2

**Purpose**: Engine must be running to verify E-Stop actually stops the engine in Test 2

**Expected MQTT**:
- `controller/relays/8/active` should be `true` (R8 energized)
- Engine should be audibly running

---

## Test 2: Emergency Stop Button (Pin 12)

**Type**: Input Test (Switch)
**Purpose**: Verify emergency stop functionality
**Hardware**: Pin 12 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: 
- `controller/inputs/12/state` - Numeric state (0 or 1)
- `controller/inputs/12/active` - Boolean state (true or false)
- `controller/safety/estop` - Safety system E-Stop status (0 or 1)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- E-Stop activation → All relays turn OFF immediately
- Safety system activates
- Engine stops (Relay R8 OFF)
- System enters EMERGENCY_STOP state
- Manual reset required after E-Stop release

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/12/state`
- Expected value when NOT pressed: `0` (inactive, button not pressed, pin HIGH)
- Read MQTT topic `controller/inputs/12/active`
- Expected value: `false` (not active)
- Read MQTT topic `controller/safety/estop`
- Expected value: `0` (E-Stop not active)
- Display to operator: "Emergency Stop is currently: INACTIVE"

### Activation Test
- Prompt operator: "PRESS and HOLD the Emergency Stop button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/12/state` should change to `1` (active, button pressed, pin LOW)
  - `controller/inputs/12/active` should change to `true`
  - `controller/safety/estop` should change to `1`
  - `controller/control/resp` may publish: `"E-STOP: ACTIVATED - System shutdown"`
- Monitor relay topics (should all turn OFF):
  - `controller/relays/1/state` → `0` (Extend OFF)
  - `controller/relays/2/state` → `0` (Retract OFF)
  - `controller/relays/8/state` → `0` (Engine OFF)
- Monitor safety topics:
  - `controller/safety/active` → `1` (Safety system active)
  - `controller/safety/engine` → `STOPPED`
- **CRITICAL**: Operator must confirm: "Has the ENGINE STOPPED running?"
- Display: "EMERGENCY STOP ACTIVE - All hydraulic operations halted - Engine stopped"
- Operator confirms: "Is E-Stop button pressed, all operations stopped, and engine stopped?" (Yes/No)

### Release Test
- Prompt operator: "RELEASE the Emergency Stop button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/12/state` should return to `0` (inactive, pin HIGH)
  - `controller/inputs/12/active` should return to `false`
  - `controller/safety/estop` should return to `0`
- Note: Safety system remains active until manual reset
- Display: "Emergency Stop released - Safety system still active, manual reset required"
- Operator confirms: "Is E-Stop button released?" (Yes/No)

### Reset Verification
- Note: System requires manual reset after E-Stop
- Options to reset:
  - Press Safety Reset/Clear button (Pin 4)
  - Send command: `safety reset` via `controller/control` topic
  - Or use testing interface reset command
- After reset, monitor:
  - `controller/safety/active` → `0` (Safety cleared)
  - `controller/control/resp` may publish: `"E-STOP: Cleared - System restored"`
- Display: "Safety system reset - System ready for operation"

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/12/state` changes: 0 → 1 (press) → 0 (release)
  - MQTT `controller/inputs/12/active` changes: false → true → false
  - MQTT `controller/safety/estop` changes: 0 → 1 → 0
  - All relays turn OFF when E-Stop pressed (monitored via relay topics)
  - Safety system activates (monitored via safety topics)
  - System requires manual reset after E-Stop release
  - Operator confirms physical button operation at each step
- ❌ FAIL if:
  - MQTT topics don't change or show incorrect values
  - Relays don't turn OFF when E-Stop pressed
  - System doesn't enter safety state
  - E-Stop button not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- This is a **critical safety test** - failures must be addressed immediately
- E-Stop has highest priority - overrides all other inputs
- Debounce time: 5ms (default for E-Stop pin)
- System response should be immediate (within 10ms)

---

## Test 3A: Extend Limit Switch (Pin 6)

**Type**: Input Test (Switch)
**Purpose**: Verify extend limit switch detection and safety cutoff
**Hardware**: Pin 6 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: 
- `controller/inputs/6/state` - Numeric state (0 or 1)
- `controller/inputs/6/active` - Boolean state (true or false)
- `controller/relays/1/state` - Extend relay status (should turn OFF at limit)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Limit activation → Pin 6 goes LOW (active)
- If Relay R1 (extend) is active, it automatically turns OFF
- Safety cutoff prevents over-travel
- System logs safety message

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/6/state`
- Expected value when NOT at limit: `0` (inactive, limit not reached, pin HIGH)
- Read MQTT topic `controller/inputs/6/active`
- Expected value: `false` (not active)
- Display to operator: "Extend Limit Switch is currently: NOT ACTIVE"

### Limit Activation Test
- Prompt operator: "Manually ACTIVATE the extend limit switch (press/move to limit)"
- Monitor MQTT topics for state changes:
  - `controller/inputs/6/state` should change to `1` (active, limit reached, pin LOW)
  - `controller/inputs/6/active` should change to `true`
- If Relay R1 was active, monitor:
  - `controller/relays/1/state` should change to `0` (safety cutoff)
  - `controller/control/resp` may publish: `"SAFETY: Extend operation stopped - limit switch reached"`
- Display: "EXTEND LIMIT REACHED - Relay R1 safety cutoff active"
- Operator confirms: "Is limit switch activated?" (Yes/No)

### Limit Release Test
- Prompt operator: "RELEASE the extend limit switch (move away from limit)"
- Monitor MQTT topics for state changes:
  - `controller/inputs/6/state` should return to `0` (inactive, pin HIGH)
  - `controller/inputs/6/active` should return to `false`
- Display: "Extend limit switch released - Normal operation restored"
- Operator confirms: "Is limit switch released?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/6/state` changes: 0 → 1 (activate) → 0 (release)
  - MQTT `controller/inputs/6/active` changes: false → true → false
  - If R1 was active, it automatically turns OFF when limit reached
  - Operator confirms physical switch operation at each step
- ❌ FAIL if:
  - MQTT topics don't change or show incorrect values
  - R1 doesn't turn OFF when limit reached (if it was active)
  - Limit switch not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- Debounce time: 10ms (configurable for limit switches)
- Safety-critical for preventing cylinder over-travel
- Automatic R1 cutoff works even during manual operation

---

## Test 3B: Retract Limit Switch (Pin 7)

**Type**: Input Test (Switch)
**Purpose**: Verify retract limit switch detection and safety cutoff
**Hardware**: Pin 7 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: 
- `controller/inputs/7/state` - Numeric state (0 or 1)
- `controller/inputs/7/active` - Boolean state (true or false)
- `controller/relays/2/state` - Retract relay status (should turn OFF at limit)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Limit activation → Pin 7 goes LOW (active)
- If Relay R2 (retract) is active, it automatically turns OFF
- Safety cutoff prevents over-travel
- System logs safety message

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/7/state`
- Expected value when NOT at limit: `0` (inactive, limit not reached, pin HIGH)
- Read MQTT topic `controller/inputs/7/active`
- Expected value: `false` (not active)
- Display to operator: "Retract Limit Switch is currently: NOT ACTIVE"

### Limit Activation Test
- Prompt operator: "Manually ACTIVATE the retract limit switch (press/move to limit)"
- Monitor MQTT topics for state changes:
  - `controller/inputs/7/state` should change to `1` (active, limit reached, pin LOW)
  - `controller/inputs/7/active` should change to `true`
- If Relay R2 was active, monitor:
  - `controller/relays/2/state` should change to `0` (safety cutoff)
  - `controller/control/resp` may publish: `"SAFETY: Retract operation stopped - limit switch reached"`
- Display: "RETRACT LIMIT REACHED - Relay R2 safety cutoff active"
- Operator confirms: "Is limit switch activated?" (Yes/No)

### Limit Release Test
- Prompt operator: "RELEASE the retract limit switch (move away from limit)"
- Monitor MQTT topics for state changes:
  - `controller/inputs/7/state` should return to `0` (inactive, pin HIGH)
  - `controller/inputs/7/active` should return to `false`
- Display: "Retract limit switch released - Normal operation restored"
- Operator confirms: "Is limit switch released?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/7/state` changes: 0 → 1 (activate) → 0 (release)
  - MQTT `controller/inputs/7/active` changes: false → true → false
  - If R2 was active, it automatically turns OFF when limit reached
  - Operator confirms physical switch operation at each step
- ❌ FAIL if:
  - MQTT topics don't change or show incorrect values
  - R2 doesn't turn OFF when limit reached (if it was active)
  - Limit switch not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- Debounce time: 10ms (configurable for limit switches)
- Safety-critical for preventing cylinder over-travel
- Automatic R2 cutoff works even during manual operation

---

## Test 4: Safety Reset/Clear Button (Pin 4)

**Type**: Input Test (Switch)
**Purpose**: Verify safety system reset/clear functionality
**Hardware**: Pin 4 (INPUT_PULLUP, Active LOW)

**MQTT Topics**: 
- `controller/inputs/4/state` - Numeric state (0 or 1)
- `controller/inputs/4/active` - Boolean state (true or false)
- `controller/safety/active` - Safety system status (0 or 1)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Button press clears safety lockouts
- Preserves error history (requires separate `error clear` command)
- Mill light errors remain active
- Management override function
- Restores normal operation after safety events

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/4/state`
- Expected value when NOT pressed: `0` (inactive, button not pressed, pin HIGH)
- Read MQTT topic `controller/inputs/4/active`
- Expected value: `false` (not active)
- Display to operator: "Safety Reset/Clear Button is currently: NOT PRESSED"

### Button Press Test
- Prompt operator: "PRESS the Safety Reset/Clear button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/4/state` should change to `1` (active, button pressed, pin LOW)
  - `controller/inputs/4/active` should change to `true`
- Monitor safety system response:
  - `controller/safety/active` should change to `0` (safety cleared)
  - `controller/control/resp` may publish: `"SAFETY: deactivated"` or `"SAFETY: E-Stop cleared"`
- Display: "Safety Reset activated - Clearing safety lockouts"
- Note: System should clear safety lockouts but preserve error history
- Operator confirms: "Is safety reset button pressed?" (Yes/No)

### Button Release Test
- Prompt operator: "RELEASE the Safety Reset/Clear button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/4/state` should return to `0` (inactive, pin HIGH)
  - `controller/inputs/4/active` should return to `false`
- Display: "Safety reset button released"
- Operator confirms: "Is button released?" (Yes/No)

### Functionality Verification
- Verify safety system state cleared:
  - `controller/safety/active` should remain `0` (cleared)
  - `controller/safety/estop` should be `0` (if E-Stop was cleared)
- Display: "Safety system cleared - Normal operation restored"
- Note: Error history requires separate `error clear` command

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/4/state` changes: 0 → 1 (press) → 0 (release)
  - MQTT `controller/inputs/4/active` changes: false → true → false
  - Safety system responds: `controller/safety/active` changes to 0
  - System response message published to `controller/control/resp`
  - Operator confirms physical button operation at each step
- ❌ FAIL if:
  - MQTT topics don't change or show incorrect values
  - Safety system doesn't clear when button pressed
  - Button not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- Debounce time: 15ms (button debounce)
- Management override - clears safety system but preserves error history
- Mill light errors remain active (separate reset required)
- Error history cleared separately via `error clear` command
- Purpose: Allows managing operator to restore operation after reviewing/addressing fault causes

---

## Engine Restart Procedure (Before Hydraulic Tests)

**Instruction to Operator**:
1. Verify safety system is cleared (Test 4 completed successfully)
2. Turn relay R8 ON using command: `relay R8 ON`
3. Verify MQTT `controller/relays/8/state` shows `1` (relay active)
4. **Operator: Turn the key and start the engine**
5. **Operator: Confirm the engine is running**
6. Wait for operator confirmation before proceeding to Test 5

**Purpose**: Engine must be running for Tests 5-8 (hydraulic operation tests)

**Expected MQTT**:
- `controller/relays/8/active` should be `true` (R8 energized)
- `controller/safety/active` should be `0` (safety cleared)
- Engine should be audibly running

---

## Test 5: Manual Extend Button (Pin 2)

**Type**: Input Test (Switch) + Hydraulic Operation + Limit Detection
**Purpose**: Verify manual extend button operation, relay control, and limit switch safety cutoff
**Hardware**: Pin 2 (INPUT_PULLUP, Active LOW)

**MQTT Topics**:
- `controller/inputs/2/state` - Button state (0 or 1)
- `controller/inputs/2/active` - Button active state (true or false)
- `controller/relays/1/state` - Extend relay R1 status (0 or 1)
- `controller/relays/1/active` - Extend relay R1 active state (true or false)
- `controller/inputs/6/state` - Extend limit switch state (0 or 1)
- `controller/inputs/6/active` - Extend limit switch active (true or false)
- `controller/pressure/system/psi` - System pressure (0-5000 PSI)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Press and hold → Activates R1 relay (hydraulic extend)
- Cylinder moves in extend direction
- Pressure varies during movement (monitored but not critical for this test)
- When extend limit (Pin 6) reached → R1 automatically turns OFF (safety cutoff)
- Release button → System returns to idle
- Comprehensive test of button → relay → hydraulic → limit → safety chain

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/2/state`
- Expected value when NOT pressed: `0` (inactive, button not pressed, pin HIGH)
- Read MQTT topic `controller/inputs/2/active`
- Expected value: `false` (not active)
- Read MQTT topic `controller/relays/1/state`
- Expected value: `0` (R1 relay OFF)
- Read MQTT topic `controller/inputs/6/state`
- Expected value: `0` (extend limit not reached)
- Display to operator: "Manual Extend Button is currently: NOT PRESSED, Relay R1: OFF, Extend Limit: NOT REACHED"

### Manual Extend Operation Test
- Prompt operator: "PRESS and HOLD the Manual Extend button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/2/state` should change to `1` (button pressed, pin LOW)
  - `controller/inputs/2/active` should change to `true`
  - `controller/relays/1/state` should change to `1` (R1 relay ON)
  - `controller/relays/1/active` should change to `true`
- Display: "Manual Extend activated - Relay R1 ON - Cylinder extending"
- Operator confirms: "Is button held and cylinder moving in extend direction?" (Yes/No)
- Monitor pressure: `controller/pressure/system/psi` (should show variation during movement)
- Display pressure readings to operator (informational - not pass/fail criteria)

### Limit Switch Activation Test
- Operator continues holding button
- Display: "Continue holding button until cylinder reaches extend limit"
- Monitor for limit switch activation:
  - `controller/inputs/6/state` should change to `1` (limit reached)
  - `controller/inputs/6/active` should change to `true`
  - `controller/relays/1/state` should change to `0` (R1 safety cutoff)
  - `controller/relays/1/active` should change to `false`
  - `controller/control/resp` may publish: `"SAFETY: Extend operation stopped - limit switch reached"`
- Display: "EXTEND LIMIT REACHED - Relay R1 automatically turned OFF (safety cutoff)"
- Operator confirms: "Has cylinder stopped at extend limit?" (Yes/No)

### Button Release Test
- Prompt operator: "RELEASE the Manual Extend button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/2/state` should return to `0` (button released, pin HIGH)
  - `controller/inputs/2/active` should return to `false`
  - `controller/relays/1/state` should remain `0` (R1 still OFF)
  - `controller/inputs/6/state` should remain `1` (cylinder still at limit)
- Display: "Manual Extend button released - System at extend limit"
- Operator confirms: "Is button released?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/2/state` changes: 0 → 1 (press) → 0 (release)
  - MQTT `controller/inputs/2/active` changes: false → true → false
  - Relay R1 activates when button pressed: `controller/relays/1/state` 0 → 1
  - Cylinder moves in extend direction while button held
  - Extend limit switch (Pin 6) activates when limit reached: `controller/inputs/6/state` 0 → 1
  - Relay R1 automatically turns OFF when limit reached (safety cutoff)
  - Operator confirms physical operation at each step
  - Pressure variations observed during movement (not critical)
- ❌ FAIL if:
  - Button MQTT topics don't change or show incorrect values
  - Relay R1 doesn't activate when button pressed
  - Cylinder doesn't move when relay active
  - Limit switch doesn't activate at limit
  - Relay R1 doesn't turn OFF when limit reached (CRITICAL SAFETY FAILURE)
  - Button not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- Debounce time: 15ms (button debounce)
- This is a hold-to-run test - button must be held for continuous operation
- Limit switch provides safety cutoff - relay MUST turn OFF when limit reached
- Pressure monitoring is informational - detailed pressure testing in Tests 11-12
- Safety cutoff works even if button is still held - operator doesn't need to release button for relay to turn off at limit
- Comprehensive test of entire extend operation chain: input → relay → hydraulic → limit → safety

---

## Test 6: Manual Retract Button (Pin 3)

**Type**: Input Test (Switch) + Hydraulic Operation + Limit Detection
**Purpose**: Verify manual retract button operation, relay control, and limit switch safety cutoff
**Hardware**: Pin 3 (INPUT_PULLUP, Active LOW)

**MQTT Topics**:
- `controller/inputs/3/state` - Button state (0 or 1)
- `controller/inputs/3/active` - Button active state (true or false)
- `controller/relays/2/state` - Retract relay R2 status (0 or 1)
- `controller/relays/2/active` - Retract relay R2 active state (true or false)
- `controller/inputs/7/state` - Retract limit switch state (0 or 1)
- `controller/inputs/7/active` - Retract limit switch active (true or false)
- `controller/pressure/system/psi` - System pressure (0-5000 PSI)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Press and hold → Activates R2 relay (hydraulic retract)
- Cylinder moves in retract direction
- Pressure varies during movement (monitored but not critical for this test)
- When retract limit (Pin 7) reached → R2 automatically turns OFF (safety cutoff)
- Release button → System returns to idle
- Comprehensive test of button → relay → hydraulic → limit → safety chain

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/3/state`
- Expected value when NOT pressed: `0` (inactive, button not pressed, pin HIGH)
- Read MQTT topic `controller/inputs/3/active`
- Expected value: `false` (not active)
- Read MQTT topic `controller/relays/2/state`
- Expected value: `0` (R2 relay OFF)
- Read MQTT topic `controller/inputs/7/state`
- Expected value: `0` (retract limit not reached - cylinder is at extend limit from Test 5)
- Display to operator: "Manual Retract Button is currently: NOT PRESSED, Relay R2: OFF, Retract Limit: NOT REACHED"

### Manual Retract Operation Test
- Prompt operator: "PRESS and HOLD the Manual Retract button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/3/state` should change to `1` (button pressed, pin LOW)
  - `controller/inputs/3/active` should change to `true`
  - `controller/relays/2/state` should change to `1` (R2 relay ON)
  - `controller/relays/2/active` should change to `true`
- As cylinder moves away from extend limit:
  - `controller/inputs/6/state` should change to `0` (extend limit released)
  - `controller/inputs/6/active` should change to `false`
- Display: "Manual Retract activated - Relay R2 ON - Cylinder retracting"
- Operator confirms: "Is button held and cylinder moving in retract direction?" (Yes/No)
- Monitor pressure: `controller/pressure/system/psi` (should show variation during movement)
- Display pressure readings to operator (informational - not pass/fail criteria)

### Limit Switch Activation Test
- Operator continues holding button
- Display: "Continue holding button until cylinder reaches retract limit"
- Monitor for limit switch activation:
  - `controller/inputs/7/state` should change to `1` (limit reached)
  - `controller/inputs/7/active` should change to `true`
  - `controller/relays/2/state` should change to `0` (R2 safety cutoff)
  - `controller/relays/2/active` should change to `false`
  - `controller/control/resp` may publish: `"SAFETY: Retract operation stopped - limit switch reached"`
- Display: "RETRACT LIMIT REACHED - Relay R2 automatically turned OFF (safety cutoff)"
- Operator confirms: "Has cylinder stopped at retract limit?" (Yes/No)

### Button Release Test
- Prompt operator: "RELEASE the Manual Retract button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/3/state` should return to `0` (button released, pin HIGH)
  - `controller/inputs/3/active` should return to `false`
  - `controller/relays/2/state` should remain `0` (R2 still OFF)
  - `controller/inputs/7/state` should remain `1` (cylinder still at limit)
- Display: "Manual Retract button released - System at retract limit"
- Operator confirms: "Is button released?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/3/state` changes: 0 → 1 (press) → 0 (release)
  - MQTT `controller/inputs/3/active` changes: false → true → false
  - Relay R2 activates when button pressed: `controller/relays/2/state` 0 → 1
  - Cylinder moves in retract direction while button held
  - Extend limit (Pin 6) releases as cylinder moves away
  - Retract limit switch (Pin 7) activates when limit reached: `controller/inputs/7/state` 0 → 1
  - Relay R2 automatically turns OFF when limit reached (safety cutoff)
  - Operator confirms physical operation at each step
  - Pressure variations observed during movement (not critical)
- ❌ FAIL if:
  - Button MQTT topics don't change or show incorrect values
  - Relay R2 doesn't activate when button pressed
  - Cylinder doesn't move when relay active
  - Limit switch doesn't activate at limit
  - Relay R2 doesn't turn OFF when limit reached (CRITICAL SAFETY FAILURE)
  - Button not responding physically

**Status**: ✅ COMPLETE - MQTT topics and behavior documented

**Notes**:
- Debounce time: 15ms (button debounce)
- This is a hold-to-run test - button must be held for continuous operation
- Limit switch provides safety cutoff - relay MUST turn OFF when limit reached
- Pressure monitoring is informational - detailed pressure testing in Tests 11-12
- Safety cutoff works even if button is still held - operator doesn't need to release button for relay to turn off at limit
- Comprehensive test of entire retract operation chain: input → relay → hydraulic → limit → safety
- Cylinder position after this test: At retract limit (fully retracted)

---

## Test 7: System Pressure Sensor (A1) - Load Test

**Type**: Sensor Test + Pressure Monitoring + Automated Relay Control
**Purpose**: Verify system pressure sensor reads accurately under load and testing program can control relays via MQTT
**Hardware**: Analog Pin A1 (4-20mA current loop, 0-5000 PSI range)

**MQTT Topics**:
- `controller/pressure/system/psi` - System pressure reading (0-5000 PSI)
- `controller/pressure/system/raw` - Raw ADC value (0-1023)
- `controller/pressure/system/voltage` - Calculated voltage
- `controller/pressure/system/ma` - Calculated current (4-20mA)
- `controller/relays/1/state` - Extend relay R1 status (0 or 1)
- `controller/relays/1/active` - Extend relay R1 active state (true or false)
- `controller/control/resp` - System response messages

**MQTT Commands Used**:
- Publish to `controller/control/relay/1/set` with payload `1` (turn ON relay R1)
- Publish to `controller/control/relay/1/set` with payload `0` (turn OFF relay R1)

**Expected System Behavior**:
- Operator places test object in front of anvil
- Testing program sends MQTT command to activate R1 (extend)
- Cylinder extends and contacts test object
- Pressure rises as cylinder presses against object
- Testing program monitors `controller/pressure/system/psi`
- When pressure reaches 1000 PSI → Testing program sends MQTT command to deactivate R1
- Cylinder stops, pressure stabilizes or drops
- Verifies: pressure sensor accuracy, MQTT relay control, threshold monitoring

---

### Test Procedure

### Initial State Verification
- Verify cylinder is at retract limit (from Test 6)
- Read MQTT topic `controller/pressure/system/psi`
- Expected value: Low pressure (< 100 PSI, system at rest)
- Read MQTT topic `controller/relays/1/state`
- Expected value: `0` (R1 relay OFF)
- Display to operator: "System Pressure: [X] PSI, Relay R1: OFF"

### Test Object Placement
- Prompt operator: "Place a test object (log/block) in front of the anvil"
- Prompt operator: "Position object so cylinder will contact it during extend operation"
- Wait for operator confirmation: "Is test object in position?" (Yes/No)
- Display: "Test object positioned - Ready for pressure test"

### Automated Extend Operation
- Display: "Testing program will now control extend operation"
- Testing program publishes MQTT command:
  - Topic: `controller/control/relay/1/set`
  - Payload: `1` (turn ON relay R1)
- Monitor MQTT topics:
  - `controller/relays/1/state` should change to `1` (R1 ON)
  - `controller/relays/1/active` should change to `true`
- Display: "Relay R1 activated via MQTT - Cylinder extending"
- Operator confirms: "Is cylinder extending toward test object?" (Yes/No)

### Pressure Monitoring and Threshold Detection
- Testing program continuously monitors `controller/pressure/system/psi`
- Display real-time pressure readings to operator: "Current Pressure: [X] PSI"
- As cylinder contacts object, pressure will rise
- Testing program watches for pressure >= 1000 PSI
- Display pressure trend: "Pressure rising: [X] PSI → [Y] PSI → [Z] PSI"
- Optional: Log pressure samples with timestamps for analysis

### Automated Pressure Cutoff
- When `controller/pressure/system/psi` >= 1000 PSI:
  - Testing program immediately publishes MQTT command:
    - Topic: `controller/control/relay/1/set`
    - Payload: `0` (turn OFF relay R1)
  - Monitor MQTT topics:
    - `controller/relays/1/state` should change to `0` (R1 OFF)
    - `controller/relays/1/active` should change to `false`
  - Display: "PRESSURE THRESHOLD REACHED (1000 PSI) - Relay R1 deactivated via MQTT"
  - Record final pressure reading when cutoff occurred
- Monitor pressure after cutoff:
  - Pressure should stabilize or begin to drop
  - Display: "Pressure after cutoff: [X] PSI"
- Operator confirms: "Has cylinder stopped and pressure stabilized?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - Pressure sensor shows readings increasing as load applied
  - Pressure reaches approximately 1000 PSI (±50 PSI acceptable)
  - Testing program successfully activates R1 via MQTT command
  - Testing program successfully deactivates R1 via MQTT command when threshold reached
  - Relay responds to MQTT commands (state changes observed)
  - Pressure readings are reasonable (0-5000 PSI range, no negative values)
  - Operator confirms cylinder operation matches MQTT state
  - System responds to external MQTT control correctly
- ❌ FAIL if:
  - Pressure sensor shows no reading or unrealistic readings
  - Pressure doesn't increase when cylinder contacts object
  - Testing program can't control relay via MQTT
  - Relay doesn't respond to MQTT commands
  - Pressure exceeds safe limits before cutoff
  - Pressure readings are erratic or out of range
  - System doesn't respond to MQTT control commands

**Status**: ✅ COMPLETE - Pressure sensor test with automated control documented

**Notes**:
- This test verifies both the pressure sensor AND the testing program's ability to control the system via MQTT
- Pressure threshold: 1000 PSI (safe test value, well below 5000 PSI maximum)
- Testing program acts as automated controller - monitors sensor and controls relay
- Operator role: Physical setup and confirmation, not manual control
- Sensor specs: A1 pin, 4-20mA current loop, 0-5000 PSI range
- MQTT command format must match controller's expected topic/payload structure
- This demonstrates the testing program can safely control hydraulic operations
- After test: Cylinder remains extended against test object at ~1000 PSI

---

## Test 8: Automatic Pressure Safety Cutoff (2500 PSI)

**Type**: Safety System Test + Manual Operation + Automatic Cutoff
**Purpose**: Verify safety system automatically shuts off extend relay when pressure reaches 2500 PSI threshold
**Hardware**: Safety System logic, Pressure Sensor A1, Manual Extend Button (Pin 2), Relay R1

**MQTT Topics**:
- `controller/inputs/2/state` - Manual extend button state (0 or 1)
- `controller/inputs/2/active` - Manual extend button active (true or false)
- `controller/relays/1/state` - Extend relay R1 status (0 or 1)
- `controller/relays/1/active` - Extend relay R1 active state (true or false)
- `controller/pressure/system/psi` - System pressure reading (0-5000 PSI)
- `controller/safety/active` - Safety system status (0 or 1)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Operator holds manual extend button → R1 activates
- Cylinder extends into test object, pressure rises
- When pressure reaches **2500 PSI** → Safety system automatically activates
- Safety system immediately turns OFF R1 relay (automatic cutoff)
- Relay stays OFF even if operator continues holding button
- Safety system requires manual reset via safety clear button (Test 4)
- Verifies: Pressure monitoring, automatic safety threshold, relay cutoff, safety lockout

---

### Test Procedure

### Initial State Verification and Setup
- Read MQTT topic `controller/relays/1/state`
- Expected value: `0` (R1 relay OFF from previous test)
- Read MQTT topic `controller/pressure/system/psi`
- Expected value: ~1000 PSI (from Test 7, cylinder still extended against test object)
- Prompt operator: "Verify test object is still in position and functional"
- Wait for operator confirmation: "Is test object ready?" (Yes/No)

### Cylinder Retract to Starting Position
- Operator can manually retract cylinder OR testing program can command:
  - **Manual Option**: Prompt operator "Press and hold Manual Retract button until cylinder fully retracts"
  - **Automated Option**: Testing program publishes to `controller/control/relay/2/set` payload `1` (R2 ON)
- Monitor `controller/inputs/7/state` for retract limit activation (changes to `1`)
- When retract limit reached, turn off R2
- Verify cylinder at retract limit and ready for test
- Display: "Cylinder retracted - Ready for automatic safety cutoff test"

### Manual Extend Operation (Operator Hold Button)
- Prompt operator: "PRESS and HOLD the Manual Extend button (Pin 2)"
- Prompt operator: "Continue holding until system automatically stops (DO NOT RELEASE)"
- Monitor MQTT topics:
  - `controller/inputs/2/state` should change to `1` (button pressed)
  - `controller/inputs/2/active` should change to `true`
  - `controller/relays/1/state` should change to `1` (R1 ON)
  - `controller/relays/1/active` should change to `true`
- Display: "Manual Extend button pressed - Relay R1 ON - Cylinder extending"
- Operator confirms: "Are you holding the button and is cylinder extending?" (Yes/No)

### Pressure Rise Monitoring
- Testing program continuously monitors `controller/pressure/system/psi`
- Display real-time pressure to operator: "Current Pressure: [X] PSI"
- As cylinder contacts test object, pressure will rise: "Pressure rising: [X] → [Y] → [Z] PSI"
- Display warning as threshold approaches: "Approaching safety threshold (2500 PSI)"
- Operator continues holding button throughout pressure rise

### Automatic Safety Cutoff at 2500 PSI
- When pressure reaches **2500 PSI**:
  - Safety system automatically activates
  - Monitor MQTT topics for automatic changes:
    - `controller/safety/active` should change to `1` (safety activated)
    - `controller/relays/1/state` should change to `0` (R1 automatically turned OFF)
    - `controller/relays/1/active` should change to `false`
    - `controller/control/resp` may publish: `"SAFETY ACTIVATED: pressure_threshold (pressure=2500.0 PSI)"`
  - Display: "AUTOMATIC SAFETY CUTOFF - Relay R1 turned OFF at 2500 PSI threshold"
  - Record exact pressure when cutoff occurred
- Operator should notice cylinder stops even though they're still holding button
- Prompt operator: "Are you still holding button but cylinder has stopped?" (Yes/No)
- Display: "Safety system active - Relay locked OFF despite button being held"

### Button Release and Safety Lockout Verification
- Prompt operator: "RELEASE the Manual Extend button"
- Monitor MQTT topics:
  - `controller/inputs/2/state` should return to `0` (button released)
  - `controller/inputs/2/active` should return to `false`
  - `controller/relays/1/state` should remain `0` (relay stays OFF - safety locked)
  - `controller/safety/active` should remain `1` (safety still active)
- Display: "Button released - Relay remains OFF due to safety lockout"
- Attempt to activate relay via MQTT command:
  - Testing program publishes to `controller/control/relay/1/set` payload `1`
  - Verify relay does NOT turn on (safety blocks automatic operations)
  - Display: "MQTT relay command blocked by safety system (expected behavior)"

### Safety System Reset
- Prompt operator: "Press the Safety Reset/Clear button (Pin 4) to clear safety lockout"
- Monitor MQTT topics:
  - `controller/inputs/4/state` should change to `1` (button pressed)
  - `controller/safety/active` should change to `0` (safety cleared)
  - `controller/control/resp` may publish: `"SAFETY: deactivated"`
- Display: "Safety system cleared - Normal operation restored"
- Operator releases safety reset button
- Display: "System ready for normal operation"

### Pass/Fail Criteria
- ✅ PASS if:
  - Operator holds manual extend button successfully
  - Relay R1 activates when button pressed (before pressure reaches threshold)
  - Cylinder extends and pressure rises to ~2500 PSI
  - Safety system automatically activates at 2500 PSI threshold
  - Relay R1 automatically turns OFF when safety activates
  - Relay stays OFF even though operator still holding button (critical safety feature)
  - Safety system blocks MQTT relay commands while active
  - Safety can be cleared with safety reset button (Pin 4)
  - `controller/safety/active` changes: 0 → 1 (automatic) → 0 (manual reset)
  - Operator confirms cylinder stopped automatically at pressure threshold
- ❌ FAIL if:
  - Safety system doesn't activate at 2500 PSI
  - Relay doesn't turn OFF when safety activates (CRITICAL SAFETY FAILURE)
  - Relay can be reactivated while safety is active
  - Safety system doesn't block automatic operations
  - Safety can't be cleared with reset button
  - Pressure readings unrealistic or sensor not working

**Status**: ✅ COMPLETE - Automatic pressure safety cutoff test documented

**Notes**:
- **SAFETY_THRESHOLD_PSI = 2500 PSI** (from safety_system.cpp)
- This is a CRITICAL SAFETY TEST - verifies automatic protection against over-pressure
- Safety system provides pressure relief time for operator (operator doesn't need to react quickly)
- Operator continues holding button to prove relay cutoff is automatic, not operator action
- Safety does NOT auto-clear - requires manual reset via safety clear button (Test 4)
- Safety system blocks all automatic relay operations when active (manual override still works if needed)
- This test also verifies Manual Extend button (Pin 2) operation under load
- After test: Cylinder extended at ~2500 PSI, safety system cleared
- **HIGH_PRESSURE_ESTOP** = 2300 PSI sustained for 10 seconds would trigger full E-Stop (different from this test)

---

## Test 9: Sequence Start Button (Pin 5) + Automatic Hydraulic Sequence

**Type**: Input Test (Switch) + Complete Automatic Sequence Test
**Purpose**: Verify sequence start button operation and complete automatic hydraulic cycle
**Hardware**: Pin 5 (INPUT_PULLUP, Active LOW), Full hydraulic system

**MQTT Topics**:
- `controller/inputs/5/state` - Sequence start button state (0 or 1)
- `controller/inputs/5/active` - Sequence start button active (true or false)
- `controller/sequence/state` - Sequence state (idle, start, complete, abort, etc.)
- `controller/sequence/event` - Sequence events (started_R1, switched_to_R2, complete, etc.)
- `controller/sequence/active` - Sequence running (0 or 1)
- `controller/sequence/stage` - Current stage (0, 1, 2)
- `controller/sequence/elapsed` - Elapsed time in milliseconds
- `controller/relays/1/state` - Extend relay R1 status (0 or 1)
- `controller/relays/1/active` - Extend relay R1 active (true or false)
- `controller/relays/2/state` - Retract relay R2 status (0 or 1)
- `controller/relays/2/active` - Retract relay R2 active (true or false)
- `controller/inputs/6/state` - Extend limit switch (0 or 1)
- `controller/inputs/7/state` - Retract limit switch (0 or 1)
- `controller/pressure/system/psi` - System pressure (0-5000 PSI)
- `controller/control/resp` - System response messages

**Expected System Behavior**:
- Momentary press of sequence start button (Pin 5)
- System debounces for 100ms (seq_start_stable_ms)
- Sequence starts: R1 activates (extend)
- Cylinder extends until limit (Pin 6) OR pressure reaches 2300 PSI
- Limit stabilizes for 15ms (seq_stable_ms)
- Sequence switches: R1 OFF, R2 ON (retract)
- Cylinder retracts until limit (Pin 7) OR pressure reaches 500 PSI
- Limit stabilizes for 15ms
- Sequence completes: R2 OFF, return to idle
- Comprehensive test of automatic hydraulic cycle

---

### Test Procedure

### Initial State Verification and Setup
- Read MQTT topics for initial state:
  - `controller/sequence/state` should be `idle` or similar
  - `controller/sequence/active` should be `0`
  - `controller/relays/1/state` should be `0` (R1 OFF)
  - `controller/relays/2/state` should be `0` (R2 OFF)
- Verify cylinder position (should be at retract limit from Test 8)
- Verify test object is still in position in front of anvil
- Verify engine is running
- Display: "System ready for automatic sequence test"

### Sequence Start Button Test
- Prompt operator: "PRESS and RELEASE the Sequence Start button (Pin 5) - Momentary press only"
- Monitor MQTT topics during button press:
  - `controller/inputs/5/state` should change to `1` (button pressed)
  - `controller/inputs/5/active` should change to `true`
- Display: "Sequence Start button pressed - System debouncing (100ms)"
- Operator can release button immediately (momentary press)
- Monitor button release:
  - `controller/inputs/5/state` should return to `0` (button released)
  - `controller/inputs/5/active` should return to `false`

### Stage 0: Start Debounce (100ms)
- System waits 100ms after button press to confirm stable start condition
- Display: "Debouncing start button... (100ms)"
- Monitor `controller/sequence/state` - may show transitional state
- No relay activation yet during debounce

### Stage 1: Extend Phase
- After 100ms debounce, sequence starts automatically
- Monitor MQTT topics for sequence start:
  - `controller/sequence/state` should change to `start` or `stage1`
  - `controller/sequence/event` should publish: `started_R1`
  - `controller/sequence/active` should change to `1`
  - `controller/sequence/stage` should be `1`
  - `controller/relays/1/state` should change to `1` (R1 ON - extend)
  - `controller/relays/1/active` should change to `true`
- Display: "STAGE 1: Extend relay R1 activated - Cylinder extending"
- Monitor pressure rise: `controller/pressure/system/psi`
- Display real-time: "Pressure: [X] PSI, Elapsed: [Y] ms"
- Cylinder extends toward test object

### Stage 1 Completion: Extend Limit Detection
- Monitor for extend limit condition (whichever occurs first):
  - **Physical limit**: `controller/inputs/6/state` changes to `1` (extend limit switch)
  - **Pressure limit**: `controller/pressure/system/psi` >= 2300 PSI (EXTEND_PRESSURE_LIMIT_PSI)
- When limit detected, system starts 15ms stability timer
- Display: "Extend limit detected - Stabilizing (15ms)..."
- After 15ms stable, sequence transitions to Stage 2

### Stage 1 to Stage 2 Transition
- Monitor MQTT topics for automatic transition:
  - `controller/sequence/event` should publish: `switched_to_R2_pressure_or_limit`
  - `controller/sequence/stage` should change to `2`
  - `controller/relays/1/state` should change to `0` (R1 OFF)
  - `controller/relays/1/active` should change to `false`
  - `controller/relays/2/state` should change to `1` (R2 ON - retract)
  - `controller/relays/2/active` should change to `true`
- Display: "STAGE 2: Retract relay R2 activated - Cylinder retracting"
- This transition happens automatically without operator intervention

### Stage 2: Retract Phase
- Monitor MQTT topics during retract:
  - `controller/sequence/stage` remains `2`
  - `controller/sequence/active` remains `1`
  - Relay R2 remains active
- Monitor pressure drop as cylinder moves away from test object
- Display real-time: "Pressure: [X] PSI, Elapsed: [Y] ms"
- Monitor extend limit release: `controller/inputs/6/state` returns to `0`
- Cylinder retracts back to starting position

### Stage 2 Completion: Retract Limit Detection
- Monitor for retract limit condition (whichever occurs first):
  - **Physical limit**: `controller/inputs/7/state` changes to `1` (retract limit switch)
  - **Pressure limit**: `controller/pressure/system/psi` >= 500 PSI (RETRACT_PRESSURE_LIMIT_PSI)
- When limit detected, system starts 15ms stability timer
- Display: "Retract limit detected - Stabilizing (15ms)..."
- After 15ms stable, sequence completes

### Sequence Completion
- Monitor MQTT topics for sequence completion:
  - `controller/sequence/event` should publish: `complete_pressure_or_limit`
  - `controller/sequence/state` should change to `complete` then `idle`
  - `controller/sequence/active` should change to `0`
  - `controller/sequence/stage` should change to `0`
  - `controller/relays/2/state` should change to `0` (R2 OFF)
  - `controller/relays/2/active` should change to `false`
- Record total elapsed time from `controller/sequence/elapsed`
- Display: "SEQUENCE COMPLETE - Total time: [X] ms"
- Verify all relays OFF and system returned to idle
- Operator confirms: "Did cylinder complete full extend and retract cycle?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - Sequence start button (Pin 5) responds: `controller/inputs/5/state` 0 → 1 → 0
  - 100ms start debounce completes successfully
  - Stage 1 activates: R1 turns ON automatically
  - Cylinder extends to limit (physical switch OR 2300 PSI pressure)
  - 15ms limit stabilization completes
  - Automatic transition: R1 OFF → R2 ON (no operator action needed)
  - Stage 2 activates: R2 turns ON automatically
  - Cylinder retracts to limit (physical switch OR 500 PSI pressure)
  - 15ms limit stabilization completes
  - Sequence completes: R2 OFF, return to idle
  - All MQTT sequence topics publish correct values throughout cycle
  - Total elapsed time < 30 seconds (sequence timeout not triggered)
  - Operator confirms physical operation matches MQTT state
  - System returns to idle state ready for next operation
- ❌ FAIL if:
  - Sequence start button doesn't respond
  - Debounce doesn't complete or sequence doesn't start
  - Stage 1 doesn't activate R1
  - Cylinder doesn't extend or limit not detected
  - Transition doesn't occur automatically (stuck in Stage 1)
  - Stage 2 doesn't activate R2
  - Cylinder doesn't retract or limit not detected
  - Sequence doesn't complete (stuck in Stage 2)
  - Sequence times out (> 30 seconds)
  - Relays don't turn OFF after completion
  - MQTT topics show incorrect values
  - System doesn't return to idle

**Status**: ✅ COMPLETE - Automatic sequence test with full MQTT monitoring documented

**Notes**:
- **Debounce times**: Start = 100ms (seq_start_stable_ms), Limits = 15ms (seq_stable_ms)
- **Pressure limits**: Extend = 2300 PSI, Retract = 500 PSI (parallel with physical switches)
- **Timeout**: 30 seconds (seq_timeout_ms) - if exceeded, sequence aborts and disables controller
- This is a comprehensive test of the entire automatic hydraulic system
- Operator only needs to press start button - rest is automatic
- Sequence can abort if new button pressed during operation (safety feature)
- Test validates: button input, sequence logic, relay control, limit detection, pressure monitoring, MQTT reporting
- After test: Cylinder at retract limit, ready for next operation
- Typical cycle time: 5-15 seconds depending on load and cylinder speed
- All relay transitions are automatic based on limit/pressure conditions

---

## Test 10: Splitter Operator Signal (Pin 8)

**Type**: Input Test (Switch)
**Purpose**: Verify splitter operator communication signal for basket exchange notification
**Hardware**: Pin 8 (INPUT_PULLUP, Active LOW)

**MQTT Topics**:
- `controller/inputs/8/state` - Splitter operator button state (0 or 1)
- `controller/inputs/8/active` - Splitter operator button active (true or false)
- `controller/control/resp` - System response messages (notification)

**Expected System Behavior**:
- Splitter operator presses button when basket is full and needs exchange
- System publishes MQTT notification to `controller/inputs/8/active` = `true`
- Loader operator monitoring system sees notification
- Optional: Could trigger buzzer (R7) for audio notification (if implemented)
- Communication between operators for coordinated basket exchange

---

### Test Procedure

### Initial State Verification
- Read MQTT topic `controller/inputs/8/state`
- Expected value when NOT pressed: `0` (inactive, button not pressed, pin HIGH)
- Read MQTT topic `controller/inputs/8/active`
- Expected value: `false` (not active)
- Display to operator: "Splitter Operator Signal is currently: INACTIVE"

### Button Press Test
- Prompt operator: "PRESS the Splitter Operator Signal button (Pin 8)"
- Monitor MQTT topics for state changes:
  - `controller/inputs/8/state` should change to `1` (button pressed, pin LOW)
  - `controller/inputs/8/active` should change to `true`
  - `controller/control/resp` may publish notification message
- Display: "SPLITTER OPERATOR SIGNAL ACTIVE - Basket exchange notification sent"
- Display: "Notification published to MQTT - Loader operator should see alert"
- Operator confirms: "Is button pressed?" (Yes/No)

### Button Release Test
- Prompt operator: "RELEASE the Splitter Operator Signal button"
- Monitor MQTT topics for state changes:
  - `controller/inputs/8/state` should return to `0` (button released, pin HIGH)
  - `controller/inputs/8/active` should return to `false`
- Display: "Splitter operator signal released - Notification cleared"
- Operator confirms: "Is button released?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT `controller/inputs/8/state` changes: 0 → 1 (press) → 0 (release)
  - MQTT `controller/inputs/8/active` changes: false → true → false
  - Notification appears in testing program interface when button pressed
  - Operator confirms physical button operation at each step
- ❌ FAIL if:
  - MQTT topics don't change or show incorrect values
  - Notification not received when button pressed
  - Button not responding physically

**Status**: ✅ COMPLETE - Splitter operator communication signal documented

**Notes**:
- Debounce time: 15ms (button debounce)
- This is a communication/coordination signal, not a safety or control input
- Purpose: Splitter operator notifies loader operator that basket is full
- Loader operator monitors MQTT topic or testing program display for notification
- Future enhancement: Could trigger buzzer relay (R7) for audio notification
- Simple switch test - no hydraulic or sequence interaction
- Does not affect hydraulic operations or system state

---

## Test 11: Filter Pressure Sensor (A5)

**Type**: Sensor Test (Analog Input)
**Purpose**: Verify filter pressure sensor reading is reasonable
**Hardware**: Analog Pin A5 (0-5V voltage output, 0-30 PSI range)

**MQTT Topics**:
- `controller/pressure/filter/psi` - Filter pressure reading (0-30 PSI)
- `controller/pressure/filter/raw` - Raw ADC value (0-1023)
- `controller/pressure/filter/voltage` - Calculated voltage (0-5V)

**Expected System Behavior**:
- Filter pressure sensor reads current hydraulic filter pressure
- Range: 0-30 PSI (diagnostic/maintenance monitoring)
- Reading should be reasonable for current system state
- Low priority - diagnostic only, not safety-critical

---

### Test Procedure

### Sensor Reading Verification
- Read MQTT topics for filter pressure:
  - `controller/pressure/filter/psi` - Current PSI reading
  - `controller/pressure/filter/voltage` - Voltage reading
  - `controller/pressure/filter/raw` - Raw ADC value
- Display to operator: "Filter Pressure: [X] PSI ([Y]V, ADC:[Z])"
- Prompt operator: "Does this filter pressure reading seem reasonable?" (Yes/No)
- Expected range: 0-30 PSI (exact value depends on system state)
- Note: Value should be stable and within sensor range

### Pass/Fail Criteria
- ✅ PASS if:
  - Sensor provides readings (not stuck at 0 or max)
  - Voltage reading is within 0-5V range
  - PSI reading is within 0-30 PSI range
  - Operator confirms reading seems reasonable
  - Reading is stable (not wildly fluctuating)
- ❌ FAIL if:
  - No reading available (sensor disconnected)
  - Reading is out of range (negative, > 30 PSI)
  - Voltage reading is invalid (< 0V or > 5V)
  - Reading fluctuates wildly
  - Operator believes reading is unreasonable

**Status**: ✅ COMPLETE - Filter pressure sensor reading verification documented

**Notes**:
- This is a diagnostic sensor, not safety-critical
- Used for maintenance scheduling and filter condition monitoring
- Exact reading depends on current system state and filter condition
- Clean filter: Low pressure (< 5 PSI typical)
- Dirty filter: Higher pressure (> 10 PSI indicates filter needs service)
- Purpose: Operator visual confirmation that sensor is working

---

## Test 12: Mill Lamp (R9)

**Type**: Output Test (Visual Indicator)
**Purpose**: Verify mill lamp visual indicator operation
**Hardware**: Pin 9 (OUTPUT), Relay R9, Mill Lamp (Yellow)

**MQTT Topics**:
- `controller/relays/9/state` - Relay R9 status (0 or 1)
- `controller/relays/9/active` - Relay R9 active (true or false)

**Expected System Behavior**:
- Mill lamp controlled by Relay R9
- Used for visual error/status indication
- Testing program will flash lamp several times
- Operator confirms visual observation

---

### Test Procedure

### Mill Lamp Flash Test
- Display: "Testing mill lamp - Watch for yellow indicator"
- Testing program flashes mill lamp 5 times:
  - Cycle: ON 500ms → OFF 500ms (repeat 5 times)
  - Send MQTT command to `controller/control/relay/9/set`:
    - Payload `1` (ON) - wait 500ms
    - Payload `0` (OFF) - wait 500ms
    - Repeat 5 times
- Monitor MQTT topics during test:
  - `controller/relays/9/state` should toggle: 0 → 1 → 0 → 1 → 0...
  - `controller/relays/9/active` should toggle: false → true → false → true...
- Prompt operator: "Did you see the yellow mill lamp flash 5 times?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT commands successfully control relay R9
  - MQTT topics show relay state changes (0 ↔ 1)
  - Operator confirms seeing mill lamp flash 5 times
  - Lamp turns OFF after test completes
- ❌ FAIL if:
  - MQTT commands don't control relay
  - Relay state doesn't change on MQTT topics
  - Operator doesn't see lamp flashing
  - Lamp doesn't respond to commands

**Status**: ✅ COMPLETE - Mill lamp visual indicator test documented

**Notes**:
- Mill lamp used for error indication during normal operation
- Different blink patterns indicate different error types
- This test only verifies lamp can be controlled and is visible
- After test: Lamp should be OFF (normal state)

---

## Test 13: Buzzer (R7) - Splitter Operator Signal

**Type**: Output Test (Audio Indicator)
**Purpose**: Verify buzzer audio indicator operation
**Hardware**: Relay R7, Buzzer/Alarm (12V DC)

**MQTT Topics**:
- `controller/relays/7/state` - Relay R7 status (0 or 1)
- `controller/relays/7/active` - Relay R7 active (true or false)

**Expected System Behavior**:
- Buzzer controlled by Relay R7
- Used for audio notifications (pressure warnings, operator signals)
- Testing program will pulse buzzer with 50% duty cycle
- Operator confirms audio is heard

---

### Test Procedure

### Buzzer Audio Test
- Display: "Testing buzzer - Listen for audio signal"
- Testing program pulses buzzer for 10 seconds:
  - Pattern: 50% duty cycle, 0.5 second intervals (ON 500ms, OFF 500ms)
  - Total duration: 10 seconds (10 ON/OFF cycles)
  - Send MQTT command to `controller/control/relay/7/set`:
    - Payload `1` (ON) - wait 500ms
    - Payload `0` (OFF) - wait 500ms
    - Repeat for 10 seconds total
- Monitor MQTT topics during test:
  - `controller/relays/7/state` should toggle: 0 → 1 → 0 → 1 → 0...
  - `controller/relays/7/active` should toggle: false → true → false → true...
- Display: "Buzzer pulsing: [X] seconds elapsed / 10 seconds"
- Prompt operator: "Did you hear the buzzer pulsing for 10 seconds?" (Yes/No)

### Pass/Fail Criteria
- ✅ PASS if:
  - MQTT commands successfully control relay R7
  - MQTT topics show relay state changes (0 ↔ 1)
  - Operator confirms hearing buzzer pulsing for 10 seconds
  - Buzzer turns OFF after test completes
  - Pulsing pattern was clear (50% duty cycle)
- ❌ FAIL if:
  - MQTT commands don't control relay
  - Relay state doesn't change on MQTT topics
  - Operator doesn't hear buzzer
  - Buzzer doesn't turn OFF after test
  - Buzzer doesn't respond to commands

**Status**: ✅ COMPLETE - Buzzer audio indicator test documented

**Notes**:
- Buzzer used for pressure warning notifications during normal operation
- 10-second warning period before automatic system shutdown at high pressure
- This test verifies buzzer is audible and controllable
- After test: Buzzer should be OFF (silent)
- Typical use: Warns splitter operator of high pressure conditions

---

## Summary

**Tests Completed**: 13 tests
**Status**: Complete - Comprehensive pre-shift operator testing suite

### Completed Tests:
1. ✅ Test 1: Engine Run/Stop Relay (R8) - Relay electrical operation
2. ✅ Engine Start Procedure (between Tests 1 and 2)
3. ✅ Test 2: Emergency Stop (E-Stop) Switch (Pin 12) - with engine stop verification
4. ✅ Test 3A: Extend Limit Switch (Pin 6)
5. ✅ Test 3B: Retract Limit Switch (Pin 7)
6. ✅ Test 4: Safety Reset/Clear Button (Pin 4)
7. ✅ Engine Restart Procedure (between Tests 4 and 5)
8. ✅ Test 5: Manual Extend Button (Pin 2) - Full hydraulic operation to limit
9. ✅ Test 6: Manual Retract Button (Pin 3) - Full hydraulic operation to limit
10. ✅ Test 7: System Pressure Sensor (A1) - Load test with automated control (1000 PSI threshold)
11. ✅ Test 8: Automatic Pressure Safety Cutoff (2500 PSI threshold test)
12. ✅ Test 9: Sequence Start Button (Pin 5) + Complete Automatic Hydraulic Sequence
13. ✅ Test 10: Splitter Operator Signal (Pin 8) - Communication signal
14. ✅ Test 11: Filter Pressure Sensor (A5) - Diagnostic reading verification
15. ✅ Test 12: Mill Lamp (R9) - Visual indicator flash test
16. ✅ Test 13: Buzzer (R7) - Audio indicator pulse test (50% duty, 10s)

### Hardware Coverage:

**Digital Inputs (8/8 tested):**
- ✅ Pin 2 - Manual Extend Button
- ✅ Pin 3 - Manual Retract Button
- ✅ Pin 4 - Safety Reset/Clear Button
- ✅ Pin 5 - Sequence Start Button
- ✅ Pin 6 - Extend Limit Switch
- ✅ Pin 7 - Retract Limit Switch
- ✅ Pin 8 - Splitter Operator Signal
- ✅ Pin 12 - Emergency Stop

**Analog Inputs (2/2 tested):**
- ✅ A1 - System Pressure Sensor (4-20mA, 0-5000 PSI)
- ✅ A5 - Filter Pressure Sensor (0-5V, 0-30 PSI)

**Relays (5/9 tested, 4 reserved):**
- ✅ R1 - Hydraulic Extend
- ✅ R2 - Hydraulic Retract
- ✅ R7 - Buzzer (audio notification)
- ✅ R8 - Engine Control
- ✅ R9 - Mill Lamp (visual indicator)
- ⬜ R3-R6 - Reserved for future use (not tested)

**System Functions:**
- ✅ Manual hydraulic operations
- ✅ Automatic sequence
- ✅ Emergency stop functionality
- ✅ Safety system and reset
- ✅ Limit switch safety cutoffs
- ✅ Pressure monitoring and automated control
- ✅ Automatic pressure safety cutoff
- ✅ Visual indicators (mill lamp)
- ✅ Audio indicators (buzzer)
- ✅ Inter-operator communication

### Next Steps:
- Additional tests to be defined as needed
- Backend interface design for testing program interaction
- Testing program implementation (separate AI project)

---

## Notes for Testing Program Implementation

### Key Patterns Established:
- Standard switch test pattern: Initial state → Activate → Monitor MQTT → Release → Verify
- Dual MQTT publishing: `/state` (0/1 numeric) and `/active` (true/false boolean)
- Active LOW configuration: Pressed/activated = LOW = state:1 = active:true
- Debounce timing: 15ms (buttons), 10ms (limit switches), 5ms (E-Stop)
- All topics use `controller/` prefix

### MQTT Command Format:
- Relay control: Publish to `controller/control/relay/{N}/set` with payload `0` (OFF) or `1` (ON)
- Topics documented in MQTT.md for reference

### Safety Features Tested:
- E-Stop immediate shutdown (Test 2)
- Limit switch safety cutoff (Tests 3A/3B, 5, 6)
- Automatic pressure cutoff at 2500 PSI (Test 8)
- Manual safety reset required (Test 4)
- Complete automatic sequence with dual limit detection (Test 9)

---

## Backend Interface Requirements

**To be implemented on controller side:**

This section will be expanded as testing program requirements are defined during implementation.
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
