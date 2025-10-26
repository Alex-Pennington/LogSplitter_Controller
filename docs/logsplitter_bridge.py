#!/usr/bin/env python3
"""
LogSplitter Serial-to-MQTT Bridge - Simple Implementation

This is a minimal, production-ready bridge that connects the LogSplitter 
Controller's serial output to an MQTT broker for remote monitoring.

Usage:
    python3 logsplitter_bridge.py --serial /dev/ttyACM0 --mqtt 192.168.1.155

Requirements:
    pip install pyserial paho-mqtt
"""

import serial
import paho.mqtt.client as mqtt
import json
import time
import argparse
import logging
import threading
from datetime import datetime
import signal
import sys

class LogSplitterBridge:
    def __init__(self, serial_port, baudrate=115200, mqtt_host='localhost', 
                 mqtt_port=1883, mqtt_user=None, mqtt_pass=None):
        self.serial_port = serial_port
        self.baudrate = baudrate
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        self.mqtt_user = mqtt_user
        self.mqtt_pass = mqtt_pass
        
        self.serial_conn = None
        self.mqtt_client = None
        self.running = False
        self.telemetry_enabled = True
        
        # Statistics
        self.messages_received = 0
        self.messages_published = 0
        self.start_time = time.time()
        
    def on_mqtt_connect(self, client, userdata, flags, rc):
        """MQTT connection callback"""
        if rc == 0:
            logging.info("Connected to MQTT broker")
            # Publish bridge online status
            self.mqtt_client.publish("logsplitter/bridge/status", 
                                   json.dumps({"status": "online", "timestamp": datetime.now().isoformat()}))
        else:
            logging.error(f"Failed to connect to MQTT broker: {rc}")
    
    def on_mqtt_disconnect(self, client, userdata, rc):
        """MQTT disconnection callback"""
        logging.warning("Disconnected from MQTT broker")
    
    def connect_serial(self):
        """Connect to the controller's serial port"""
        try:
            self.serial_conn = serial.Serial(
                port=self.serial_port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1.0
            )
            logging.info(f"Connected to serial port: {self.serial_port}")
            return True
        except Exception as e:
            logging.error(f"Failed to connect to serial port: {e}")
            return False
    
    def connect_mqtt(self):
        """Connect to MQTT broker"""
        try:
            self.mqtt_client = mqtt.Client(client_id="logsplitter_bridge")
            self.mqtt_client.on_connect = self.on_mqtt_connect
            self.mqtt_client.on_disconnect = self.on_mqtt_disconnect
            
            if self.mqtt_user and self.mqtt_pass:
                self.mqtt_client.username_pw_set(self.mqtt_user, self.mqtt_pass)
            
            self.mqtt_client.connect(self.mqtt_host, self.mqtt_port, 60)
            self.mqtt_client.loop_start()
            return True
        except Exception as e:
            logging.error(f"Failed to connect to MQTT broker: {e}")
            return False
    
    def parse_telemetry_line(self, line):
        """Parse a line of telemetry from the controller"""
        line = line.strip()
        if not line:
            return None
        
        # Handle telemetry control messages
        if "TELEMETRY/DEBUG OUTPUT" in line:
            if "ENABLED" in line:
                self.telemetry_enabled = True
                logging.info("Controller telemetry enabled")
            elif "DISABLED" in line:
                self.telemetry_enabled = False
                logging.info("Controller telemetry disabled")
            return None
        
        # Skip interactive prompts
        if line.startswith("> ") or line == ">":
            return None
        
        # Parse pipe-delimited format: timestamp|LEVEL|message
        if "|" in line and line.count("|") >= 2:
            try:
                parts = line.split("|", 2)
                timestamp_ms = int(parts[0])
                level = parts[1]
                message = parts[2]
                
                return {
                    'timestamp_ms': timestamp_ms,
                    'timestamp_iso': datetime.now().isoformat(),
                    'level': level,
                    'message': message,
                    'raw': line,
                    'telemetry_enabled': self.telemetry_enabled
                }
            except (ValueError, IndexError):
                # Malformed message, treat as raw
                pass
        
        # Handle non-standard messages
        return {
            'timestamp_ms': int(time.time() * 1000),
            'timestamp_iso': datetime.now().isoformat(),
            'level': 'RAW',
            'message': line,
            'raw': line,
            'telemetry_enabled': self.telemetry_enabled
        }
    
    def extract_structured_data(self, data):
        """Extract structured data from telemetry messages"""
        message = data['message']
        structured = []
        
        # Pressure data: "Pressure: A1=245.2psi A5=12.1psi"
        if message.startswith('Pressure:'):
            pressure_data = {}
            parts = message.split()
            for part in parts[1:]:
                if '=' in part and 'psi' in part:
                    try:
                        sensor, value_str = part.split('=')
                        value = float(value_str.replace('psi', ''))
                        pressure_data[sensor] = value
                    except ValueError:
                        continue
            
            if pressure_data:
                structured.append({
                    'topic': 'logsplitter/controller/pressure',
                    'payload': pressure_data
                })
        
        # Relay state: "Relay: R1=ON (Extend valve)"
        elif message.startswith('Relay:'):
            try:
                # Extract "R1=ON" part
                relay_part = message.split()[1]
                relay_num, state = relay_part.split('=')
                
                relay_data = {
                    'relay': relay_num,
                    'state': state == 'ON',
                    'timestamp': data['timestamp_iso']
                }
                
                structured.append({
                    'topic': f'logsplitter/controller/relays/{relay_num.lower()}',
                    'payload': relay_data
                })
            except (IndexError, ValueError):
                pass
        
        # Sequence state: "Sequence: IDLE -> EXTEND_START"
        elif message.startswith('Sequence:'):
            seq_content = message.replace('Sequence:', '').strip()
            seq_data = {
                'raw_message': message,
                'timestamp': data['timestamp_iso']
            }
            
            if '->' in seq_content:
                try:
                    from_state, to_state = seq_content.split(' -> ')
                    seq_data['type'] = 'transition'
                    seq_data['from_state'] = from_state.strip()
                    seq_data['to_state'] = to_state.strip()
                except ValueError:
                    seq_data['type'] = 'status'
                    seq_data['content'] = seq_content
            else:
                seq_data['type'] = 'status'
                seq_data['content'] = seq_content
            
            structured.append({
                'topic': 'logsplitter/controller/sequence',
                'payload': seq_data
            })
        
        # Input changes: "Input: pin=6 state=true name=LIMIT_EXTEND"
        elif message.startswith('Input:'):
            input_data = {'timestamp': data['timestamp_iso']}
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
                structured.append({
                    'topic': f'logsplitter/controller/inputs/pin_{input_data["pin"]}',
                    'payload': input_data
                })
        
        return structured
    
    def publish_telemetry(self, data):
        """Publish telemetry to MQTT"""
        if not self.mqtt_client:
            return
        
        try:
            # Publish raw telemetry
            topic = "logsplitter/controller/telemetry"
            payload = json.dumps(data)
            self.mqtt_client.publish(topic, payload)
            
            # Publish level-specific topic
            level_topic = f"logsplitter/controller/telemetry/{data['level'].lower()}"
            self.mqtt_client.publish(level_topic, payload)
            
            # Extract and publish structured data
            structured_data = self.extract_structured_data(data)
            for item in structured_data:
                self.mqtt_client.publish(item['topic'], json.dumps(item['payload']))
            
            self.messages_published += 1
            
        except Exception as e:
            logging.error(f"Failed to publish telemetry: {e}")
    
    def run(self):
        """Main bridge loop"""
        logging.info("Starting LogSplitter Bridge...")
        
        if not self.connect_serial():
            return False
        
        if not self.connect_mqtt():
            return False
        
        self.running = True
        logging.info("Bridge running - monitoring telemetry")
        
        # Status reporting thread
        status_thread = threading.Thread(target=self.status_reporter, daemon=True)
        status_thread.start()
        
        try:
            while self.running:
                try:
                    if self.serial_conn and self.serial_conn.in_waiting:
                        line = self.serial_conn.readline().decode('utf-8', errors='ignore')
                        if line.strip():
                            data = self.parse_telemetry_line(line)
                            if data:
                                self.publish_telemetry(data)
                                self.messages_received += 1
                                
                                # Log important messages
                                if data['level'] in ['CRIT', 'ERROR', 'ALERT']:
                                    logging.warning(f"Controller {data['level']}: {data['message']}")
                    
                    time.sleep(0.01)  # Small delay
                    
                except serial.SerialException as e:
                    logging.error(f"Serial communication error: {e}")
                    time.sleep(1)
                    if not self.connect_serial():
                        time.sleep(5)
                
        except KeyboardInterrupt:
            logging.info("Bridge stopped by user")
        except Exception as e:
            logging.error(f"Bridge error: {e}")
        finally:
            self.stop()
    
    def status_reporter(self):
        """Periodically publish bridge status"""
        while self.running:
            try:
                uptime = time.time() - self.start_time
                status = {
                    'status': 'running',
                    'uptime_seconds': uptime,
                    'messages_received': self.messages_received,
                    'messages_published': self.messages_published,
                    'telemetry_enabled': self.telemetry_enabled,
                    'timestamp': datetime.now().isoformat()
                }
                
                if self.mqtt_client:
                    self.mqtt_client.publish("logsplitter/bridge/status", json.dumps(status))
                
                time.sleep(30)  # Status every 30 seconds
                
            except Exception as e:
                logging.error(f"Status reporter error: {e}")
                time.sleep(30)
    
    def stop(self):
        """Stop the bridge gracefully"""
        logging.info("Shutting down bridge...")
        self.running = False
        
        # Publish offline status
        if self.mqtt_client:
            self.mqtt_client.publish("logsplitter/bridge/status", 
                                   json.dumps({"status": "offline", "timestamp": datetime.now().isoformat()}))
            time.sleep(0.5)  # Give time for message to send
            self.mqtt_client.loop_stop()
            self.mqtt_client.disconnect()
        
        if self.serial_conn:
            self.serial_conn.close()
        
        logging.info("Bridge stopped")

def signal_handler(sig, frame):
    """Handle Ctrl+C gracefully"""
    logging.info("Received interrupt signal")
    sys.exit(0)

def main():
    parser = argparse.ArgumentParser(description='LogSplitter Serial-to-MQTT Bridge')
    parser.add_argument('--serial', '-s', required=True, help='Serial port (e.g., /dev/ttyACM0 or COM3)')
    parser.add_argument('--baudrate', '-b', type=int, default=115200, help='Serial baudrate (default: 115200)')
    parser.add_argument('--mqtt-host', '-m', default='localhost', help='MQTT broker host (default: localhost)')
    parser.add_argument('--mqtt-port', '-p', type=int, default=1883, help='MQTT broker port (default: 1883)')
    parser.add_argument('--mqtt-user', '-u', help='MQTT username (optional)')
    parser.add_argument('--mqtt-pass', '-P', help='MQTT password (optional)')
    parser.add_argument('--log-level', '-l', default='INFO', choices=['DEBUG', 'INFO', 'WARNING', 'ERROR'],
                       help='Logging level (default: INFO)')
    parser.add_argument('--log-file', '-f', help='Log to file (optional)')
    
    args = parser.parse_args()
    
    # Setup logging
    log_format = '%(asctime)s - %(levelname)s - %(message)s'
    log_handlers = [logging.StreamHandler()]
    
    if args.log_file:
        log_handlers.append(logging.FileHandler(args.log_file))
    
    logging.basicConfig(
        level=getattr(logging, args.log_level),
        format=log_format,
        handlers=log_handlers
    )
    
    # Setup signal handler
    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)
    
    # Create and run bridge
    bridge = LogSplitterBridge(
        serial_port=args.serial,
        baudrate=args.baudrate,
        mqtt_host=args.mqtt_host,
        mqtt_port=args.mqtt_port,
        mqtt_user=args.mqtt_user,
        mqtt_pass=args.mqtt_pass
    )
    
    bridge.run()

if __name__ == '__main__':
    main()