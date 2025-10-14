# LogSplitter Controller - State Machine Documentation

## Overview

The LogSplitter Controller uses a comprehensive state machine to manage hydraulic cylinder operations. The state machine handles both automatic sequences (full extend/retract cycles) and manual operations (operator-controlled extend or retract), with integrated safety mechanisms and timeout protection.

## State Enumeration

### SequenceState

The system operates in one of the following states:

| State | Description | Type |
|-------|-------------|------|
| `SEQ_IDLE` | System at rest, waiting for input | Common |
| `SEQ_WAIT_START_DEBOUNCE` | Debouncing start button press | Automatic |
| `SEQ_STAGE1_ACTIVE` | Cylinder extending | Automatic |
| `SEQ_STAGE1_WAIT_LIMIT` | Waiting for stable extend limit | Automatic |
| `SEQ_STAGE2_ACTIVE` | Cylinder retracting | Automatic |
| `SEQ_STAGE2_WAIT_LIMIT` | Waiting for stable retract limit | Automatic |
| `SEQ_COMPLETE` | Sequence completed successfully | Automatic |
| `SEQ_ABORT` | Sequence aborted due to error/condition | Common |
| `SEQ_MANUAL_EXTEND_ACTIVE` | Manual extend operation in progress | Manual |
| `SEQ_MANUAL_RETRACT_ACTIVE` | Manual retract operation in progress | Manual |

### SequenceType

The system tracks what type of operation is currently running:

| Type | Description |
|------|-------------|
| `SEQ_TYPE_AUTOMATIC` | Full automatic extend→retract cycle |
| `SEQ_TYPE_MANUAL_EXTEND` | Manual extend with safety monitoring |
| `SEQ_TYPE_MANUAL_RETRACT` | Manual retract with safety monitoring |

## Automatic Sequence State Machine

### State Transition Diagram

```
                    [Start Button Pressed]
                            ↓
                    SEQ_IDLE
                            ↓
                    SEQ_WAIT_START_DEBOUNCE
                      (100ms debounce)
                            ↓
                [Debounce Complete] → Relay R1 ON (Extend)
                            ↓
                    SEQ_STAGE1_ACTIVE
                      (Cylinder extending)
                            ↓
                [Extend Limit Detected]
                            ↓
                    SEQ_STAGE1_WAIT_LIMIT
                      (15ms stability check)
                            ↓
                [Limit Stable] → Relay R1 OFF, R2 ON (Retract)
                            ↓
                    SEQ_STAGE2_ACTIVE
                      (Cylinder retracting)
                            ↓
                [Retract Limit Detected]
                            ↓
                    SEQ_STAGE2_WAIT_LIMIT
                      (15ms stability check)
                            ↓
                [Limit Stable] → Relay R2 OFF
                            ↓
                    SEQ_IDLE
                      (Sequence Complete)
```

### State Descriptions

#### SEQ_IDLE
- **Entry**: System initialization, sequence completion, or after abort
- **Conditions**: No active hydraulic operations
- **Relays**: All hydraulic relays OFF
- **Transitions**:
  - To `SEQ_WAIT_START_DEBOUNCE`: Start button (Pin 5) pressed
  - To `SEQ_MANUAL_EXTEND_ACTIVE`: Manual extend command received
  - To `SEQ_MANUAL_RETRACT_ACTIVE`: Manual retract command received

#### SEQ_WAIT_START_DEBOUNCE
- **Entry**: Start button detected
- **Duration**: 100ms (configurable via `startStableTimeMs`)
- **Purpose**: Debounce button press to prevent false starts
- **Relays**: All hydraulic relays OFF
- **Transitions**:
  - To `SEQ_STAGE1_ACTIVE`: After debounce period, start button still active
  - To `SEQ_IDLE` (abort): Start button released during debounce

#### SEQ_STAGE1_ACTIVE
- **Entry**: Debounce complete, sequence starting
- **Action**: Relay R1 (Extend) turned ON
- **Monitoring**: 
  - Physical limit switch (Pin 6)
  - Pressure threshold (≥2300 PSI)
  - Overall timeout (30 seconds)
- **Transitions**:
  - To `SEQ_STAGE1_WAIT_LIMIT`: Extend limit detected (switch OR pressure)
  - To `SEQ_IDLE` (abort): Timeout, new button press, or start button released

#### SEQ_STAGE1_WAIT_LIMIT
- **Entry**: Extend limit detected
- **Duration**: 15ms stability check (configurable via `stableTimeMs`)
- **Purpose**: Ensure limit is stable before transitioning
- **Monitoring**: Limit must remain active for full stability period
- **Relays**: R1 still ON during stability check
- **Transitions**:
  - To `SEQ_STAGE2_ACTIVE`: Limit stable for required duration
  - To `SEQ_STAGE1_ACTIVE`: Limit lost before stability achieved
  - To `SEQ_IDLE` (abort): Timeout or safety condition

#### SEQ_STAGE2_ACTIVE
- **Entry**: Extend limit confirmed stable
- **Action**: Relay R1 OFF, Relay R2 (Retract) turned ON
- **Monitoring**:
  - Physical limit switch (Pin 7)
  - Pressure threshold (≥2300 PSI)
  - Overall timeout (30 seconds)
- **Transitions**:
  - To `SEQ_STAGE2_WAIT_LIMIT`: Retract limit detected (switch OR pressure)
  - To `SEQ_IDLE` (abort): Timeout, new button press

#### SEQ_STAGE2_WAIT_LIMIT
- **Entry**: Retract limit detected
- **Duration**: 15ms stability check (configurable via `stableTimeMs`)
- **Purpose**: Ensure limit is stable before completing sequence
- **Monitoring**: Limit must remain active for full stability period
- **Relays**: R2 still ON during stability check
- **Transitions**:
  - To `SEQ_IDLE`: Limit stable - sequence complete, R2 turned OFF
  - To `SEQ_STAGE2_ACTIVE`: Limit lost before stability achieved
  - To `SEQ_IDLE` (abort): Timeout or safety condition

## Manual Operation State Machine

### Manual Extend

```
    [Manual Extend Command]
            ↓
    SEQ_IDLE
            ↓
    [Safety Checks Pass]
            ↓
    SEQ_MANUAL_EXTEND_ACTIVE
    → Relay R1 ON
            ↓
    [Continuous Monitoring]
    - Extend limit (Pin 6)
    - Pressure (≥2300 PSI)
    - Timeout (30s)
            ↓
    [Limit Reached OR Stop Command]
            ↓
    → Relay R1 OFF
            ↓
    SEQ_IDLE
```

### Manual Retract

```
    [Manual Retract Command]
            ↓
    SEQ_IDLE
            ↓
    [Safety Checks Pass]
            ↓
    SEQ_MANUAL_RETRACT_ACTIVE
    → Relay R2 ON
            ↓
    [Continuous Monitoring]
    - Retract limit (Pin 7)
    - Pressure (≥2300 PSI)
    - Timeout (30s)
            ↓
    [Limit Reached OR Stop Command]
            ↓
    → Relay R2 OFF
            ↓
    SEQ_IDLE
```

### Manual Operation Features

#### Pre-Start Safety Checks
Before any manual operation begins, the system verifies:
- Sequence controller not already active
- Sequence controller not disabled (timeout lockout)
- Not already at destination limit
- Pressure not already at threshold

#### Continuous Monitoring
During manual operations, the system continuously checks:
- **Physical Limits**: Pin 6 (extend) or Pin 7 (retract)
- **Pressure Limits**: 2300 PSI threshold for both extend and retract
- **Timeout**: 30-second maximum operation time
- **User Commands**: Stop command can interrupt at any time

#### Automatic Stop Conditions
Manual operations automatically stop when:
1. Physical limit switch activated
2. Pressure threshold reached (2300 PSI)
3. Timeout exceeded (30 seconds)
4. User issues stop command

## Safety Mechanisms

### Timeout Protection

**Configuration**:
- Default: 30 seconds (`DEFAULT_SEQUENCE_TIMEOUT_MS`)
- Applies to: All active states (automatic and manual)

**Behavior**:
- Timer starts on state entry
- Checked continuously during `update()` cycle
- On timeout:
  1. All hydraulic relays turned OFF immediately
  2. State transitions to `SEQ_IDLE`
  3. Error logged with CRITICAL level
  4. Sequence controller DISABLED (requires safety clear)
  5. Mill lamp activated via error manager
  6. MQTT events published

**Recovery**:
- Requires manual safety clear (Pin 4)
- Clears timeout lockout and re-enables sequence controller
- Error history preserved for diagnostics

### Abort Conditions

The state machine can abort for the following reasons:

| Reason | Description | States Affected |
|--------|-------------|----------------|
| `timeout` | Operation exceeded 30-second limit | All active states |
| `released_during_debounce` | Start button released too early | `SEQ_WAIT_START_DEBOUNCE` |
| `start_released` | Start button released before allowed | `SEQ_STAGE1_*`, `SEQ_STAGE2_*` |
| `new_press` | New button pressed during operation | `SEQ_STAGE1_*`, `SEQ_STAGE2_*` |
| `manual_reset` | User issued reset command | Any state |
| `manual_abort` | User issued abort command | Any state |

### Limit Detection Methods

The system uses dual detection for limit switches:

#### 1. Physical Limit Switches
- **Extend Limit**: Pin 6 (digital input)
- **Retract Limit**: Pin 7 (digital input)
- **Debouncing**: 15ms stability requirement

#### 2. Pressure-Based Limits
- **Extend Threshold**: ≥2300 PSI
- **Retract Threshold**: ≥2300 PSI
- **Purpose**: Backup detection when physical switches fail

**Logic**: Limit is considered "reached" when EITHER condition is true (physical switch OR pressure threshold).

### Stability Requirements

To prevent false transitions from electrical noise or mechanical bounce:

**Start Debounce** (`startStableTimeMs`):
- Default: 100ms
- Applied to: Start button presses
- Purpose: Confirm intentional start

**Limit Stability** (`stableTimeMs`):
- Default: 15ms
- Applied to: Limit switch detections
- Purpose: Confirm solid mechanical contact
- Applies to: Both physical switches and pressure thresholds

### Button Interlock System

The automatic sequence uses a sophisticated button interlock:

**Phase 1 - Start Lockout**:
- During `SEQ_WAIT_START_DEBOUNCE`: Start buttons must remain active
- Release triggers immediate abort

**Phase 2 - Button Release Allowed**:
- After `SEQ_STAGE1_ACTIVE` begins: `allowButtonRelease = true`
- Operator can release start button
- System captures button states at sequence start

**Phase 3 - New Press Detection**:
- Any NEW button press (not held at start) triggers abort
- Prevents accidental interference during operation
- Checked during both Stage 1 and Stage 2

## Sequence Disable/Enable

### Disable Mechanism

The sequence controller can be disabled to prevent operations:

**Automatic Disable**:
- Triggered by: Sequence timeout
- Effect: Blocks all sequence starts (automatic and manual)
- Indication: Mill lamp activated, error logged

**Manual Disable**:
- Command: Via command processor
- Effect: Same as automatic
- Use case: Maintenance or troubleshooting

### Enable/Recovery

**Requirements**:
1. Safety clear button (Pin 4) pressed
2. Clears E-stop state (if active)
3. Re-enables sequence controller
4. Error history preserved

**Process**:
- E-stop cleared first
- General safety system cleared
- Sequence controller re-enabled
- System restored to operational state
- Ready for new operations

## Timing Configuration

All timing parameters are configurable:

| Parameter | Default | Purpose | Configurable Via |
|-----------|---------|---------|------------------|
| `stableTimeMs` | 15ms | Limit switch debounce | `setStableTime()` |
| `startStableTimeMs` | 100ms | Start button debounce | `setStartStableTime()` |
| `timeoutMs` | 30000ms | Maximum operation time | `setTimeout()` |

## State Transition Events

The state machine publishes events via MQTT for monitoring:

### Event Topics

**State Changes**: `controller/sequence/state`
- Values: `"start"`, `"complete"`, `"abort"`, `"manual_extend"`, `"manual_retract"`, `"stopped"`, `"limit_reached"`

**Sequence Events**: `controller/sequence/event`
- Values: 
  - `"started_R1"` - Automatic extend started
  - `"switched_to_R2_pressure_or_limit"` - Transition to retract
  - `"complete_pressure_or_limit"` - Sequence completed
  - `"aborted_<reason>"` - Abort with reason
  - `"manual_extend_started"`, `"manual_extend_limit_reached"`
  - `"manual_retract_started"`, `"manual_retract_limit_reached"`
  - `"manual_stopped"` - Manual operation stopped by user

### Status Topics

Continuously published during operation:
- `controller/sequence/active` - Boolean (0 or 1)
- `controller/sequence/stage` - Stage number (0, 1, or 2)
- `controller/sequence/elapsed` - Milliseconds since state entry

## Integration Points

### Hardware Interfaces

**Input Pins**:
- Pin 2: Manual extend button
- Pin 3: Reserved (historically auto/manual select)
- Pin 4: Safety clear button
- Pin 5: Automatic sequence start button
- Pin 6: Extend limit switch
- Pin 7: Retract limit switch
- Pin 12: E-stop input

**Analog Inputs**:
- A1: Main hydraulic pressure sensor (0-5000 PSI)
- A5: Hydraulic filter pressure sensor (0-30 PSI)

**Relay Outputs**:
- R1: Hydraulic extend solenoid (24V DC)
- R2: Hydraulic retract solenoid (24V DC)
- R7: Operator signal buzzer
- R8: Engine enable/disable
- R9: Mill lamp (yellow warning light)

### Software Components

**Dependencies**:
- `RelayController`: Controls hydraulic solenoid relays
- `PressureManager`: Provides pressure readings for limit detection
- `InputManager`: Provides debounced pin states
- `SystemErrorManager`: Logs errors and controls mill lamp
- `NetworkManager`: Publishes MQTT telemetry
- `SafetySystem`: Overall safety monitoring and E-stop handling

**Called By**:
- `main.cpp`: Main loop calls `update()` continuously
- `onInputChange()`: Input change callback calls `processInputChange()`
- `CommandProcessor`: Manual operation commands call `startManualExtend()`, etc.

## Diagnostic Information

### Debug Output

When debug is enabled, the state machine logs:
- State transitions with timestamps
- Limit detection events
- Stability timing progress
- Abort reasons
- Pressure readings during limit checks

### Status Query

The `getStatusString()` method provides:
- Current stage (0, 1, 2)
- Active status (0 or 1)
- Elapsed time in current state
- Configured timing parameters
- Disabled status

## Usage Examples

### Starting Automatic Sequence

1. System in `SEQ_IDLE` state
2. Operator presses start button (Pin 5)
3. System enters `SEQ_WAIT_START_DEBOUNCE`
4. After 100ms, if button still held, enters `SEQ_STAGE1_ACTIVE`
5. R1 turns ON, cylinder extends
6. When extend limit reached, waits 15ms for stability
7. R1 OFF, R2 ON, cylinder retracts
8. When retract limit reached, waits 15ms for stability
9. R2 OFF, returns to `SEQ_IDLE`

### Manual Extend Operation

1. System in `SEQ_IDLE` state
2. Command `manual extend` received
3. Safety checks pass
4. Enter `SEQ_MANUAL_EXTEND_ACTIVE`, R1 ON
5. Cylinder extends until:
   - Extend limit switch (Pin 6) activated, OR
   - Pressure reaches 2300 PSI, OR
   - 30-second timeout, OR
   - `manual stop` command received
6. R1 OFF, return to `SEQ_IDLE`

### Timeout Recovery

1. Operation times out (>30 seconds)
2. All relays immediately OFF
3. Mill lamp activated
4. Sequence controller disabled
5. Error logged to MQTT and syslog
6. Operator presses safety clear button (Pin 4)
7. E-stop and safety system cleared
8. Sequence controller re-enabled
9. System ready for new operations

## State Machine Architecture

### Design Principles

1. **Safety First**: All transitions prioritize safety
2. **Fail-Safe**: Timeouts and limits stop operations
3. **Predictable**: State transitions are deterministic
4. **Observable**: All state changes are logged and published
5. **Recoverable**: Clear recovery procedures for all error conditions

### Key Features

- **Dual Limit Detection**: Physical switches backed by pressure monitoring
- **Stability Checking**: Prevents bounce-induced false transitions
- **Timeout Protection**: Automatic abort on excessive duration
- **Button Interlock**: Prevents accidental interference
- **Lockout System**: Disables operations after critical errors
- **MQTT Telemetry**: Real-time state and event broadcasting
- **Manual Override**: Operator-controlled movements with safety

## References

### Source Files
- `include/sequence_controller.h` - State machine definitions
- `src/sequence_controller.cpp` - State machine implementation
- `src/main.cpp` - Integration and callbacks
- `include/constants.h` - Configuration constants

### Related Documentation
- `docs/PINS.md` - Pin assignments and hardware interfaces
- `docs/Controller_Commands.md` - Command reference including manual operations
- `MQTT_DETAILS.md` - MQTT topic documentation
- `docs/COMPREHENSIVE_REVIEW.md` - Overall system architecture

### Configuration Constants
- `DEFAULT_SEQUENCE_STABLE_MS` - Limit stability time (15ms)
- `DEFAULT_SEQUENCE_START_STABLE_MS` - Start debounce time (100ms)
- `DEFAULT_SEQUENCE_TIMEOUT_MS` - Maximum operation time (30000ms)
- `EXTEND_PRESSURE_LIMIT_PSI` - Extend pressure threshold (2300 PSI)
- `RETRACT_PRESSURE_LIMIT_PSI` - Retract pressure threshold (2300 PSI)
