# LogSplitter Controller Serial Telemetry API

## Overview

This document defines the serial telemetry interface for the LogSplitter Controller non-networking version. External monitoring systems can connect to the controller's serial port to receive real-time telemetry data and relay it to MQTT brokers, databases, or other monitoring systems.

## Connection Parameters

| Parameter | Value | Description |
|-----------|--------|-------------|
| **Baud Rate** | 115200 | Serial communication speed |
| **Data Bits** | 8 | Character size |
| **Parity** | None | No parity checking |
| **Stop Bits** | 1 | Single stop bit |
| **Flow Control** | None | No hardware flow control |
| **Port** | USB/COM | Arduino USB serial port |

## Interactive Control

The controller supports interactive telemetry control via keyboard commands:

### Ctrl+K Toggle
- **Command**: Press Ctrl+K (ASCII 11)
- **Function**: Toggle telemetry output on/off
- **Default State**: Telemetry enabled
- **Feedback**: Controller sends status message when toggled

```
*** TELEMETRY/DEBUG OUTPUT DISABLED (Ctrl+K to toggle) ***
Interactive mode: keystrokes will be echoed
> 
```

## Message Format

All telemetry messages follow a pipe-delimited format for easy parsing:

```
timestamp|LEVEL|message_content
```

### Format Components

| Field | Description | Example |
|-------|-------------|---------|
| `timestamp` | Milliseconds since controller boot | `45678` |
| `LEVEL` | Log severity level | `INFO`, `WARN`, `ERROR`, `CRIT`, `DEBUG` |
| `message_content` | Actual telemetry data or log message | `Pressure: A1=245.2psi A5=12.1psi` |

### Log Levels

| Level | Description | Always Visible |
|-------|-------------|----------------|
| `EMERG` | System unusable | ✅ Yes |
| `ALERT` | Immediate action required | ✅ Yes |
| `CRIT` | Critical conditions | ✅ Yes |
| `ERROR` | Error conditions | ✅ Yes |
| `WARN` | Warning conditions | 📡 Telemetry dependent |
| `NOTICE` | Normal but significant | 📡 Telemetry dependent |
| `INFO` | Informational messages | 📡 Telemetry dependent |
| `DEBUG` | Debug-level messages | 📡 Telemetry dependent |

> **Note**: CRITICAL and ERROR messages always appear, even when telemetry is disabled, ensuring safety alerts are never missed.

## Telemetry Data Types

### 1. System Status Messages

#### Startup Sequence
```
1234|INFO|System initializing...
1456|INFO|Pressure sensors initialized
1678|INFO|Safety systems active
1890|INFO|System ready - entering main loop
```

#### Periodic System Health
```
45678|INFO|System: state=READY estop=false pressure_ok=true
```

### 2. Pressure Telemetry

#### Real-time Pressure Data
```
12345|INFO|Pressure: A1=245.2psi A5=12.1psi
```

#### Pressure Events
```
23456|WARN|Pressure A1 high: 285.3psi (limit: 280.0psi)
34567|INFO|Pressure normalized: A1=255.1psi
```

### 3. Sequence Control Events

#### Sequence State Changes
```
45678|INFO|Sequence: IDLE -> EXTEND_START
56789|INFO|Sequence: EXTEND_START -> EXTENDING
67890|INFO|Sequence: EXTENDING -> EXTEND_COMPLETE
```

#### Sequence Status Updates
```
78901|INFO|Sequence: type=AUTO state=EXTENDING elapsed=2340ms
```

### 4. Input State Changes

#### Digital Input Events
```
89012|INFO|Input: pin=6 state=true name=LIMIT_EXTEND
90123|INFO|Input: pin=12 state=true name=ESTOP_ACTIVE
```

#### Input State Summary
```
01234|DEBUG|Inputs: [2]=false [3]=true [4]=false [5]=false [6]=true [7]=false [12]=false
```

### 5. Relay Control Events

#### Relay State Changes
```
12345|INFO|Relay: R1=ON (Extend valve)
23456|INFO|Relay: R2=OFF (Retract valve)
```

#### Relay Status Summary
```
34567|DEBUG|Relays: R1=ON R2=OFF R3=OFF R4=ON R5=OFF R6=OFF R7=OFF R8=OFF
```

### 6. Safety System Events

#### Emergency Stop Events
```
45678|CRIT|Emergency stop activated - all sequences halted
56789|WARN|Emergency stop cleared - manual reset required
```

#### Safety Violations
```
67890|ERROR|Safety violation: pressure exceeded safe limits
78901|WARN|Safety system: auto-recovery attempted
```

### 7. Error and Warning Messages

#### System Errors
```
89012|ERROR|Relay controller communication timeout
90123|WARN|Pressure sensor A5 reading unstable
01234|CRIT|Multiple system failures detected
```

#### Recovery Messages
```
12345|INFO|System recovery: relay communication restored
23456|INFO|Pressure sensor A5 stabilized
```

## Event-Driven Telemetry

The controller uses intelligent event-driven telemetry to minimize data while ensuring important information is captured:

### Trigger Conditions

1. **State Changes**: Sequence states, input pin changes, relay changes
2. **Threshold Events**: Pressure limits, timing violations, safety triggers  
3. **Periodic Fallback**: Every 30 seconds if no events occurred
4. **System Events**: Startup, errors, warnings, recovery

### Sample Event Sequence

```
# System startup
1234|INFO|System initializing...
1456|INFO|Safety systems active
1678|INFO|System ready - entering main loop

# User initiates extend sequence
5678|INFO|Input: pin=2 state=true name=START_EXTEND
5679|INFO|Sequence: IDLE -> EXTEND_START
5680|INFO|Relay: R1=ON (Extend valve)
5681|INFO|Sequence: EXTEND_START -> EXTENDING

# Pressure monitoring during extend
8901|INFO|Pressure: A1=156.7psi A5=8.2psi (rising)
12345|WARN|Pressure A1 approaching limit: 275.8psi

# Limit switch reached
15678|INFO|Input: pin=6 state=true name=LIMIT_EXTEND
15679|INFO|Sequence: EXTENDING -> EXTEND_COMPLETE
15680|INFO|Relay: R1=OFF (Extend valve)

# Periodic status (if no events for 30 seconds)
45678|INFO|System: state=READY estop=false pressure=stable
```

## Building a Serial-to-MQTT Bridge

### Recommended Architecture

```
┌─────────────────┐    Serial    ┌─────────────────┐    MQTT     ┌─────────────────┐
│   Controller    │──────────────▶│  Bridge/Relay   │───────────▶│   MQTT Broker   │
│  (Arduino R4)   │   115200 8N1  │   Application   │   WiFi/Eth  │  (Mosquitto)    │
└─────────────────┘              └─────────────────┘            └─────────────────┘
```

### Sample Python Bridge Implementation

```python
#!/usr/bin/env python3
"""
LogSplitter Serial-to-MQTT Bridge

Reads telemetry from controller serial port and publishes to MQTT broker.
Handles telemetry enable/disable and message parsing.
"""

import serial
import paho.mqtt.client as mqtt
import json
import time
import threading
from datetime import datetime
import logging

class LogSplitterBridge:
    def __init__(self, serial_port='/dev/ttyACM0', baud=115200, 
                 mqtt_host='localhost', mqtt_port=1883):
        self.serial_port = serial_port
        self.baud = baud
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        
        # Initialize connections
        self.serial_conn = None
        self.mqtt_client = None
        self.running = False
        
        # Message parsing
        self.telemetry_enabled = True
        
    def connect_serial(self):
        """Connect to controller serial port"""
        try:
            self.serial_conn = serial.Serial(
                port=self.serial_port,
                baudrate=self.baud,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0
            )
            logging.info(f"Serial connected: {self.serial_port} @ {self.baud}")
            return True
        except Exception as e:
            logging.error(f"Serial connection failed: {e}")
            return False
    
    def connect_mqtt(self):
        """Connect to MQTT broker"""
        try:
            self.mqtt_client = mqtt.Client()
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            logging.info(f"MQTT connected: {self.mqtt_host}:{self.mqtt_port}")
            return True
        except Exception as e:
            logging.error(f"MQTT connection failed: {e}")
            return False
    
    def parse_message(self, line):
        """Parse controller telemetry message"""
        try:
            # Handle telemetry control messages
            if "TELEMETRY/DEBUG OUTPUT" in line:
                if "ENABLED" in line:
                    self.telemetry_enabled = True
                elif "DISABLED" in line:
                    self.telemetry_enabled = False
                return None
            
            # Skip interactive prompts and echo
            if line.startswith("> ") or line.strip() == ">":
                return None
                
            # Parse pipe-delimited format: timestamp|LEVEL|message
            if "|" in line:
                parts = line.strip().split("|", 2)
                if len(parts) >= 3:
                    timestamp_ms = int(parts[0])
                    level = parts[1]
                    message = parts[2]
                    
                    return {
                        'timestamp_ms': timestamp_ms,
                        'timestamp_iso': datetime.now().isoformat(),
                        'level': level,
                        'message': message,
                        'raw_line': line.strip()
                    }
            
            # Handle non-standard format messages as raw
            return {
                'timestamp_ms': int(time.time() * 1000),
                'timestamp_iso': datetime.now().isoformat(),
                'level': 'RAW',
                'message': line.strip(),
                'raw_line': line.strip()
            }
            
        except Exception as e:
            logging.warning(f"Message parse error: {e} - Line: {line}")
            return None
    
    def publish_telemetry(self, data):
        """Publish parsed telemetry to MQTT"""
        if not self.mqtt_client or not data:
            return
            
        try:
            # Main telemetry topic
            topic = "logsplitter/controller/telemetry"
            payload = json.dumps(data)
            self.mqtt_client.publish(topic, payload)
            
            # Level-specific topics for filtering
            level_topic = f"logsplitter/controller/telemetry/{data['level'].lower()}"
            self.mqtt_client.publish(level_topic, payload)
            
            # Parse and publish specific data types
            self.parse_and_publish_specific(data)
            
        except Exception as e:
            logging.error(f"MQTT publish error: {e}")
    
    def parse_and_publish_specific(self, data):
        """Parse and publish specific telemetry types"""
        message = data['message']
        
        # Pressure data
        if message.startswith('Pressure:'):
            # Extract pressure values: "Pressure: A1=245.2psi A5=12.1psi"
            pressure_data = {}
            parts = message.split()
            for part in parts[1:]:  # Skip "Pressure:"
                if '=' in part and 'psi' in part:
                    sensor, value = part.split('=')
                    pressure_data[sensor] = float(value.replace('psi', ''))
            
            if pressure_data:
                topic = "logsplitter/controller/pressure"
                self.mqtt_client.publish(topic, json.dumps(pressure_data))
        
        # Relay state changes
        elif message.startswith('Relay:'):
            # Extract relay data: "Relay: R1=ON (Extend valve)"
            try:
                relay_part = message.split()[1]  # "R1=ON"
                relay_num, state = relay_part.split('=')
                relay_data = {
                    'relay': relay_num,
                    'state': state == 'ON',
                    'description': message
                }
                topic = f"logsplitter/controller/relays/{relay_num.lower()}"
                self.mqtt_client.publish(topic, json.dumps(relay_data))
            except:
                pass
        
        # Sequence state changes
        elif message.startswith('Sequence:'):
            # Extract sequence data: "Sequence: IDLE -> EXTEND_START"
            try:
                seq_data = {'raw_message': message}
                if '->' in message:
                    # State transition
                    parts = message.replace('Sequence:', '').strip().split(' -> ')
                    seq_data['from_state'] = parts[0].strip()
                    seq_data['to_state'] = parts[1].strip()
                    seq_data['type'] = 'state_change'
                else:
                    # Status update
                    seq_data['type'] = 'status'
                
                topic = "logsplitter/controller/sequence"
                self.mqtt_client.publish(topic, json.dumps(seq_data))
            except:
                pass
        
        # Input state changes
        elif message.startswith('Input:'):
            # Extract input data: "Input: pin=6 state=true name=LIMIT_EXTEND"
            try:
                input_data = {}
                parts = message.replace('Input:', '').strip().split()
                for part in parts:
                    if '=' in part:
                        key, value = part.split('=', 1)
                        if value.lower() in ['true', 'false']:
                            input_data[key] = value.lower() == 'true'
                        elif value.isdigit():
                            input_data[key] = int(value)
                        else:
                            input_data[key] = value
                
                if 'pin' in input_data:
                    topic = f"logsplitter/controller/inputs/pin_{input_data['pin']}"
                    self.mqtt_client.publish(topic, json.dumps(input_data))
            except:
                pass
    
    def run(self):
        """Main bridge loop"""
        if not self.connect_serial():
            return False
            
        if not self.connect_mqtt():
            return False
        
        self.running = True
        logging.info("Bridge started - monitoring telemetry")
        
        try:
            while self.running:
                if self.serial_conn and self.serial_conn.in_waiting:
                    line = self.serial_conn.readline().decode('utf-8', errors='ignore')
                    if line.strip():
                        data = self.parse_message(line)
                        if data:
                            self.publish_telemetry(data)
                            logging.debug(f"Published: {data['level']} - {data['message'][:50]}")
                
                time.sleep(0.01)  # Small delay to prevent CPU spinning
                
        except KeyboardInterrupt:
            logging.info("Bridge stopped by user")
        except Exception as e:
            logging.error(f"Bridge error: {e}")
        finally:
            self.stop()
    
    def stop(self):
        """Stop the bridge"""
        self.running = False
        if self.serial_conn:
            self.serial_conn.close()
        if self.mqtt_client:
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()

# Example usage
if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    
    bridge = LogSplitterBridge(
        serial_port='/dev/ttyACM0',  # Adjust for your system
        mqtt_host='192.168.1.155'   # Your MQTT broker IP
    )
    
    bridge.run()
```

### MQTT Topic Structure

The bridge should publish telemetry to structured MQTT topics:

#### Main Telemetry Stream
```
logsplitter/controller/telemetry
└── Raw JSON messages with all telemetry data

logsplitter/controller/telemetry/info
logsplitter/controller/telemetry/warn  
logsplitter/controller/telemetry/error
logsplitter/controller/telemetry/crit
└── Filtered by log level
```

#### Specific Data Topics
```
logsplitter/controller/pressure
├── {"A1": 245.2, "A5": 12.1}

logsplitter/controller/sequence  
├── {"type": "state_change", "from_state": "IDLE", "to_state": "EXTENDING"}

logsplitter/controller/relays/r1
logsplitter/controller/relays/r2
├── {"relay": "R1", "state": true, "description": "Extend valve"}

logsplitter/controller/inputs/pin_6
logsplitter/controller/inputs/pin_12
├── {"pin": 6, "state": true, "name": "LIMIT_EXTEND"}

logsplitter/controller/system/status
├── {"state": "READY", "estop": false, "pressure_ok": true}
```

## Integration Examples

### Home Assistant Integration

```yaml
# configuration.yaml
mqtt:
  sensor:
    - name: "LogSplitter Pressure A1"
      state_topic: "logsplitter/controller/pressure"
      value_template: "{{ value_json.A1 }}"
      unit_of_measurement: "psi"
      
    - name: "LogSplitter Pressure A5"  
      state_topic: "logsplitter/controller/pressure"
      value_template: "{{ value_json.A5 }}"
      unit_of_measurement: "psi"

  binary_sensor:
    - name: "LogSplitter Emergency Stop"
      state_topic: "logsplitter/controller/inputs/pin_12"
      value_template: "{{ value_json.state }}"
      payload_on: true
      payload_off: false
      device_class: safety
```

### Node-RED Flow

```json
[
    {
        "id": "mqtt_in",
        "type": "mqtt in",
        "topic": "logsplitter/controller/telemetry/+",
        "qos": "0",
        "broker": "mqtt_broker"
    },
    {
        "id": "parse_telemetry",
        "type": "function",
        "func": "const data = JSON.parse(msg.payload);\nif (data.level === 'CRIT' || data.level === 'ERROR') {\n    // Send alert\n    node.send([msg, null]);\n} else {\n    // Normal logging\n    node.send([null, msg]);\n}",
        "outputs": 2
    }
]
```

### InfluxDB Logging

```python
from influxdb import InfluxDBClient

def log_to_influxdb(data):
    client = InfluxDBClient('localhost', 8086, database='logsplitter')
    
    point = {
        'measurement': 'controller_telemetry',
        'time': data['timestamp_iso'],
        'tags': {
            'level': data['level'],
            'source': 'controller'
        },
        'fields': {
            'message': data['message'],
            'timestamp_ms': data['timestamp_ms']
        }
    }
    
    # Parse specific measurements
    if 'Pressure:' in data['message']:
        # Extract pressure values and log as separate measurements
        pass
    
    client.write_points([point])
```

## Testing and Validation

### Test Commands

Send these commands to the controller serial port to verify telemetry:

```bash
# Test basic functionality
help
show
status

# Test sequence control
extend
stop
retract

# Test relay control (if authorized)
relay 1 on
relay 1 off
```

### Expected Telemetry Output

```
12345|INFO|Command received: help
12346|DEBUG|Processing help command
12347|INFO|Help text sent to serial

23456|INFO|Command received: extend  
23457|INFO|Sequence: IDLE -> EXTEND_START
23458|INFO|Relay: R1=ON (Extend valve)
23459|INFO|Sequence: EXTEND_START -> EXTENDING

34567|INFO|Pressure: A1=178.5psi A5=9.2psi
45678|INFO|Input: pin=6 state=true name=LIMIT_EXTEND
45679|INFO|Sequence: EXTENDING -> EXTEND_COMPLETE
45680|INFO|Relay: R1=OFF (Extend valve)
```

## Error Handling

### Common Issues and Solutions

| Issue | Symptoms | Solution |
|-------|----------|----------|
| **Serial Connection Lost** | No data received | Reconnect to serial port, check USB cable |
| **Malformed Messages** | Parse errors in logs | Update parser to handle new message formats |
| **MQTT Connection Drops** | Messages not published | Implement reconnection logic with backoff |
| **Telemetry Disabled** | Reduced message volume | Check for Ctrl+K toggle messages |
| **Buffer Overflow** | Garbled messages | Increase serial buffer size, add flow control |

### Robust Connection Handling

```python
def robust_serial_read(self):
    """Read with error recovery"""
    retry_count = 0
    max_retries = 3
    
    while retry_count < max_retries:
        try:
            if not self.serial_conn or not self.serial_conn.is_open:
                self.connect_serial()
            
            line = self.serial_conn.readline().decode('utf-8', errors='ignore')
            return line
            
        except (serial.SerialException, OSError) as e:
            logging.warning(f"Serial error (attempt {retry_count + 1}): {e}")
            retry_count += 1
            time.sleep(1)
            
            if self.serial_conn:
                self.serial_conn.close()
            
    return None
```

## Conclusion

This API provides a complete interface for building robust serial-to-MQTT bridges for the LogSplitter Controller. The event-driven telemetry ensures efficient data transmission while maintaining real-time visibility into system operation. The standardized message format and comprehensive examples should enable quick integration with various monitoring and automation systems.

For questions or additional integration requirements, refer to the main LogSplitter documentation or controller source code.