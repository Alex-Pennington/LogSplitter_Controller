#!/usr/bin/env python3
"""
Telemetry Test Receiver - Validates binary protobuf data from LogSplitter Controller

This script simulates the monitor system by receiving and decoding protobuf telemetry
data transmitted via SoftwareSerial on pins A4/A5 at 115200 baud.

Usage:
    python telemetry_test_receiver.py [COM_PORT]
    
Example:
    python telemetry_test_receiver.py COM3

Features:
- Auto-detects Arduino controller COM ports
- Decodes binary protobuf messages  
- Displays system status, inputs, outputs, and events
- Validates message integrity and sequencing
- Simulates LCD display output format
"""

import serial
import serial.tools.list_ports
import sys
import time
import struct
from datetime import datetime

# Protobuf message type definitions (matching telemetry.proto)
class MessageType:
    SYSTEM_STATUS = 0x01
    DIGITAL_INPUT = 0x02
    DIGITAL_OUTPUT = 0x03
    RELAY_CONTROL = 0x04
    PRESSURE_READING = 0x05
    SEQUENCE_EVENT = 0x06
    ERROR_EVENT = 0x07

class TelemetryReceiver:
    def __init__(self, port=None, baud=115200):
        self.port = port or self.auto_detect_arduino()
        self.baud = baud
        self.ser = None
        self.messages_received = 0
        self.bytes_received = 0
        self.last_sequence_id = None
        
    def auto_detect_arduino(self):
        """Auto-detect Arduino controller COM port"""
        for port in serial.tools.list_ports.comports():
            if any(keyword in port.description.upper() for keyword in ['ARDUINO', 'CH340', 'USB']):
                print(f"🔍 Found Arduino-like device: {port.device} - {port.description}")
                return port.device
        
        # Fallback to first available COM port
        ports = list(serial.tools.list_ports.comports())
        if ports:
            print(f"⚠️  No Arduino detected, using first available: {ports[0].device}")
            return ports[0].device
        
        raise Exception("❌ No COM ports found")
    
    def connect(self):
        """Connect to the telemetry serial port"""
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            time.sleep(2)  # Wait for Arduino initialization
            print(f"✅ Connected to {self.port} at {self.baud} baud")
            print(f"📡 Waiting for telemetry data from pins A4/A5...")
            return True
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    def read_message(self):
        """Read and decode a single telemetry message"""
        try:
            # Read message size byte
            size_byte = self.ser.read(1)
            if not size_byte:
                return None
            
            message_size = size_byte[0]
            if message_size == 0 or message_size > 50:  # Sanity check
                return None
            
            # Read message data
            message_data = self.ser.read(message_size)
            if len(message_data) != message_size:
                return None
            
            self.messages_received += 1
            self.bytes_received += message_size + 1
            
            return self.decode_message(message_data)
            
        except Exception as e:
            print(f"⚠️  Read error: {e}")
            return None
    
    def decode_message(self, data):
        """Decode protobuf message (simplified decoder)"""
        if len(data) < 4:
            return None
        
        # Basic protobuf field extraction (simplified)
        msg_type = data[0]
        sequence_id = data[1] if len(data) > 1 else 0
        timestamp = struct.unpack('<H', data[2:4])[0] if len(data) >= 4 else 0
        
        # Validate sequence
        if self.last_sequence_id is not None:
            expected = (self.last_sequence_id + 1) % 256
            if sequence_id != expected:
                print(f"⚠️  Sequence gap: expected {expected}, got {sequence_id}")
        self.last_sequence_id = sequence_id
        
        message = {
            'type': msg_type,
            'sequence': sequence_id,
            'timestamp': timestamp,
            'size': len(data),
            'raw_data': data.hex()
        }
        
        # Decode specific message types
        if msg_type == MessageType.SYSTEM_STATUS:
            message['decoded'] = self.decode_system_status(data)
        elif msg_type == MessageType.DIGITAL_INPUT:
            message['decoded'] = self.decode_digital_input(data)
        elif msg_type == MessageType.DIGITAL_OUTPUT:
            message['decoded'] = self.decode_digital_output(data)
        elif msg_type == MessageType.RELAY_CONTROL:
            message['decoded'] = self.decode_relay_control(data)
        elif msg_type == MessageType.PRESSURE_READING:
            message['decoded'] = self.decode_pressure_reading(data)
        else:
            message['decoded'] = f"Unknown message type: 0x{msg_type:02X}"
        
        return message
    
    def decode_system_status(self, data):
        """Decode system status message"""
        if len(data) >= 8:
            system_state = data[4] if len(data) > 4 else 0
            mill_lamp_state = data[5] if len(data) > 5 else 0
            error_count = data[6] if len(data) > 6 else 0
            uptime_s = struct.unpack('<H', data[6:8])[0] if len(data) >= 8 else 0
            
            return f"System: {system_state}, Mill Lamp: {mill_lamp_state}, Errors: {error_count}, Uptime: {uptime_s}s"
        return "System Status (partial)"
    
    def decode_digital_input(self, data):
        """Decode digital input message"""
        if len(data) >= 6:
            pin = data[4] if len(data) > 4 else 0
            state = data[5] if len(data) > 5 else 0
            return f"Input Pin {pin}: {'HIGH' if state else 'LOW'}"
        return "Digital Input (partial)"
    
    def decode_digital_output(self, data):
        """Decode digital output message"""
        if len(data) >= 6:
            pin = data[4] if len(data) > 4 else 0
            state = data[5] if len(data) > 5 else 0
            return f"Output Pin {pin}: {'HIGH' if state else 'LOW'}"
        return "Digital Output (partial)"
    
    def decode_relay_control(self, data):
        """Decode relay control message"""
        if len(data) >= 6:
            relay = data[4] if len(data) > 4 else 0
            state = data[5] if len(data) > 5 else 0
            return f"Relay {relay}: {'ON' if state else 'OFF'}"
        return "Relay Control (partial)"
    
    def decode_pressure_reading(self, data):
        """Decode pressure reading message"""
        if len(data) >= 8:
            pressure_raw = struct.unpack('<H', data[4:6])[0] if len(data) >= 6 else 0
            pressure_psi = struct.unpack('<H', data[6:8])[0] if len(data) >= 8 else 0
            return f"Pressure: {pressure_psi} PSI (raw: {pressure_raw})"
        return "Pressure Reading (partial)"
    
    def display_lcd_format(self, message):
        """Display message in simulated LCD format"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        msg_type_names = {
            0x01: "STATUS",
            0x02: "INPUT ",
            0x03: "OUTPUT",
            0x04: "RELAY ",
            0x05: "PRESS ",
            0x06: "EVENT ",
            0x07: "ERROR "
        }
        
        type_name = msg_type_names.get(message['type'], f"0x{message['type']:02X}")
        
        # Simulate 20x4 LCD display
        print(f"┌────────────────────┐")
        print(f"│ LogSplitter Monitor│")
        print(f"│ {timestamp}   #{message['sequence']:03d} │")
        print(f"│ {type_name}: {message['decoded'][:13]:<13} │")
        print(f"│ Size: {message['size']:2d}B  Total:{self.messages_received:4d}│")
        print(f"└────────────────────┘")
        print()
    
    def run(self):
        """Main monitoring loop"""
        if not self.connect():
            return False
        
        print(f"""
╔══════════════════════════════════════════════════════════════╗
║              LOGSPLITTER TELEMETRY MONITOR                   ║
║                    Binary Protobuf Receiver                  ║
╠══════════════════════════════════════════════════════════════╣
║  Port: {self.port:<10}  Baud: {self.baud:<10}  Protocol: Binary  ║
║  Expected: A4/A5 SoftwareSerial from Arduino Controller     ║
║  Format: [SIZE][MSG_TYPE][SEQ][TIMESTAMP][DATA...]          ║
╚══════════════════════════════════════════════════════════════╝

Press Ctrl+C to stop monitoring...
        """)
        
        try:
            while True:
                message = self.read_message()
                if message:
                    self.display_lcd_format(message)
                    
                    # Debug output
                    print(f"🔍 Debug: Type=0x{message['type']:02X}, Seq={message['sequence']}, "
                          f"Size={message['size']}, Raw={message['raw_data']}")
                    print("-" * 50)
                else:
                    # No data received, show heartbeat
                    print(f"⏱️  {datetime.now().strftime('%H:%M:%S')} - Waiting for data... "
                          f"(Received: {self.messages_received} msgs, {self.bytes_received} bytes)")
                    time.sleep(1)
                    
        except KeyboardInterrupt:
            print(f"\n\n📊 Session Summary:")
            print(f"   Messages Received: {self.messages_received}")
            print(f"   Bytes Received: {self.bytes_received}")
            print(f"   Average Message Size: {self.bytes_received/max(1, self.messages_received):.1f} bytes")
            print(f"✅ Telemetry receiver stopped.")
        
        finally:
            if self.ser:
                self.ser.close()

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else None
    
    print("🚀 LogSplitter Telemetry Test Receiver")
    print("=" * 50)
    
    try:
        receiver = TelemetryReceiver(port)
        receiver.run()
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())