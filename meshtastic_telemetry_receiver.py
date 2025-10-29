#!/usr/bin/env python3
"""
Enhanced Telemetry Test Receiver - Supports Arduino Direct & Meshtastic Bridge Modes

This script validates binary protobuf telemetry from LogSplitter Controller via:
1. Direct Arduino connection (A4/A5 pins @ 115200 baud)
2. Meshtastic mesh bridge (wireless transport over LoRa)

Usage:
    python meshtastic_telemetry_receiver.py [--mode MODE] [--port PORT]
    
Examples:
    python meshtastic_telemetry_receiver.py --mode direct --port COM3
    python meshtastic_telemetry_receiver.py --mode meshtastic --port COM4
    python meshtastic_telemetry_receiver.py  # Auto-detect mode and port

Features:
- Auto-detects connection mode (Arduino vs Meshtastic)
- Decodes binary protobuf messages from both sources
- Validates message integrity and sequencing  
- Simulates LCD display output format
- Range and signal quality monitoring for Meshtastic mode
- Performance comparison between direct and mesh transport
"""

import serial
import serial.tools.list_ports
import sys
import time
import struct
import argparse
import json
from datetime import datetime
from enum import Enum

class ConnectionMode(Enum):
    DIRECT = "direct"
    MESHTASTIC = "meshtastic"
    AUTO = "auto"

class MessageType:
    """Binary telemetry message types (matching controller constants)"""
    DIGITAL_INPUT = 0x10
    DIGITAL_OUTPUT = 0x11
    RELAY_CONTROL = 0x12
    PRESSURE_READING = 0x13
    SYSTEM_ERROR = 0x14
    SAFETY_EVENT = 0x15
    SYSTEM_STATUS = 0x16
    SEQUENCE_EVENT = 0x17

class EnhancedTelemetryReceiver:
    """Enhanced receiver supporting both direct Arduino and Meshtastic bridge modes"""
    
    def __init__(self, mode=ConnectionMode.AUTO, port=None, baud=115200):
        self.mode = mode
        self.port = port
        self.baud = baud
        self.ser = None
        self.messages_received = 0
        self.bytes_received = 0
        self.last_sequence_id = None
        self.connection_mode = None
        self.mesh_stats = {
            'rssi': None,
            'snr': None,
            'hop_count': 0,
            'node_id': None
        }
        
    def auto_detect_connection(self):
        """Auto-detect connection type and port"""
        print("🔍 Auto-detecting LogSplitter telemetry connection...")
        
        # Scan for potential connections
        arduino_ports = []
        meshtastic_ports = []
        
        for port in serial.tools.list_ports.comports():
            description = port.description.upper()
            
            # Look for Arduino/CH340 (direct connection)
            if any(keyword in description for keyword in ['ARDUINO', 'CH340']):
                arduino_ports.append((port.device, 'Arduino Direct', port.description))
            
            # Look for Meshtastic devices (CP210X, FTDI, etc.)
            elif any(keyword in description for keyword in ['CP210', 'FTDI', 'USB SERIAL']):
                # Try to identify as Meshtastic by attempting connection
                if self.test_meshtastic_connection(port.device):
                    meshtastic_ports.append((port.device, 'Meshtastic Bridge', port.description))
        
        # Prioritize direct Arduino connection
        if arduino_ports:
            self.port = arduino_ports[0][0]
            self.connection_mode = ConnectionMode.DIRECT
            print(f"✅ Detected Arduino direct connection: {arduino_ports[0][0]} - {arduino_ports[0][2]}")
            return True
        
        # Use Meshtastic if available
        if meshtastic_ports:
            self.port = meshtastic_ports[0][0]
            self.connection_mode = ConnectionMode.MESHTASTIC
            print(f"✅ Detected Meshtastic bridge: {meshtastic_ports[0][0]} - {meshtastic_ports[0][2]}")
            return True
        
        # No specific devices found, try first available port
        all_ports = list(serial.tools.list_ports.comports())
        if all_ports:
            self.port = all_ports[0].device
            self.connection_mode = ConnectionMode.DIRECT  # Default assumption
            print(f"⚠️  No specific devices found, trying: {self.port}")
            return True
        
        print("❌ No COM ports found")
        return False
    
    def test_meshtastic_connection(self, port):
        """Test if port is a Meshtastic device"""
        try:
            # Try to connect and look for Meshtastic packets
            ser = serial.Serial(port, 115200, timeout=2)
            time.sleep(1)
            
            # Look for Meshtastic packet patterns in initial data
            data = ser.read(100)
            ser.close()
            
            # Simple heuristic: Meshtastic tends to have different packet patterns
            # This is a simplified check - in practice you'd look for specific headers
            if len(data) > 10:
                return True
            
        except:
            pass
        
        return False
    
    def connect(self):
        """Connect to the telemetry source"""
        if self.mode == ConnectionMode.AUTO:
            if not self.auto_detect_connection():
                return False
        else:
            self.connection_mode = self.mode
            if not self.port:
                if not self.auto_detect_port_for_mode():
                    return False
        
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            time.sleep(2)  # Wait for initialization
            
            if self.connection_mode == ConnectionMode.DIRECT:
                print(f"✅ Connected to Arduino direct: {self.port} @ {self.baud} baud")
                print(f"📡 Monitoring binary telemetry from A4/A5 pins...")
            else:
                print(f"✅ Connected to Meshtastic bridge: {self.port} @ {self.baud} baud")
                print(f"📡 Monitoring mesh telemetry transport...")
            
            return True
            
        except Exception as e:
            print(f"❌ Connection failed: {e}")
            return False
    
    def auto_detect_port_for_mode(self):
        """Auto-detect port for specified mode"""
        if self.connection_mode == ConnectionMode.DIRECT:
            for port in serial.tools.list_ports.comports():
                if any(keyword in port.description.upper() for keyword in ['ARDUINO', 'CH340']):
                    self.port = port.device
                    return True
        else:  # MESHTASTIC
            for port in serial.tools.list_ports.comports():
                if self.test_meshtastic_connection(port.device):
                    self.port = port.device
                    return True
        
        return False
    
    def read_message(self):
        """Read message based on connection mode"""
        if self.connection_mode == ConnectionMode.DIRECT:
            return self.read_direct_message()
        else:
            return self.read_meshtastic_message()
    
    def read_direct_message(self):
        """Read direct binary telemetry message from Arduino"""
        try:
            # Read size-prefixed binary message
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
            
            return self.decode_binary_message(message_data)
            
        except Exception as e:
            print(f"⚠️  Direct read error: {e}")
            return None
    
    def read_meshtastic_message(self):
        """Read telemetry from Meshtastic bridge"""
        try:
            # Read line-based Meshtastic output
            line = self.ser.readline().decode('utf-8', errors='ignore').strip()
            if not line:
                return None
            
            # Look for telemetry data in Meshtastic packet
            if 'SERIAL_APP' in line or 'LogSplitter' in line:
                # Extract binary data from Meshtastic packet
                binary_data = self.extract_telemetry_from_mesh_packet(line)
                if binary_data:
                    self.messages_received += 1
                    self.bytes_received += len(binary_data)
                    
                    # Update mesh statistics
                    self.update_mesh_stats(line)
                    
                    return self.decode_binary_message(binary_data)
            
            return None
            
        except Exception as e:
            print(f"⚠️  Meshtastic read error: {e}")
            return None
    
    def extract_telemetry_from_mesh_packet(self, packet_line):
        """Extract binary telemetry data from Meshtastic packet"""
        # This is a simplified implementation
        # In practice, you'd need to parse the actual Meshtastic packet format
        
        # Look for hex data in the packet
        try:
            # Example: extract hex payload from Meshtastic output
            if 'payload:' in packet_line.lower():
                hex_part = packet_line.split('payload:')[1].strip()
                if hex_part:
                    # Convert hex string to binary
                    return bytes.fromhex(hex_part.replace(' ', ''))
        except:
            pass
        
        return None
    
    def update_mesh_stats(self, packet_line):
        """Update mesh network statistics from packet"""
        try:
            # Extract RSSI, SNR, hop count from Meshtastic packet
            if 'rssi:' in packet_line.lower():
                parts = packet_line.split('rssi:')[1].split()[0]
                self.mesh_stats['rssi'] = int(parts)
            
            if 'snr:' in packet_line.lower():
                parts = packet_line.split('snr:')[1].split()[0]
                self.mesh_stats['snr'] = float(parts)
            
            if 'hops:' in packet_line.lower():
                parts = packet_line.split('hops:')[1].split()[0]
                self.mesh_stats['hop_count'] = int(parts)
                
        except:
            pass
    
    def decode_binary_message(self, data):
        """Decode binary telemetry message (same for both modes)"""
        if len(data) < 6:  # Minimum header size
            return None
        
        try:
            # Parse message header
            msg_type = data[0]
            sequence_id = data[1]
            timestamp = struct.unpack('<I', data[2:6])[0]  # Little-endian uint32
            
            # Validate sequence (detect dropped messages)
            if self.last_sequence_id is not None:
                expected = (self.last_sequence_id + 1) & 0xFF
                if sequence_id != expected:
                    print(f"⚠️  Sequence gap: expected {expected}, got {sequence_id}")
            
            self.last_sequence_id = sequence_id
            
            # Decode message based on type
            message = {
                'type': msg_type,
                'sequence': sequence_id,
                'timestamp': timestamp,
                'received_at': datetime.now(),
                'connection_mode': self.connection_mode.value
            }
            
            if self.connection_mode == ConnectionMode.MESHTASTIC:
                message['mesh_stats'] = self.mesh_stats.copy()
            
            # Decode payload based on message type
            if msg_type == MessageType.DIGITAL_INPUT:
                message.update(self.decode_digital_input(data[6:]))
            elif msg_type == MessageType.PRESSURE_READING:
                message.update(self.decode_pressure_reading(data[6:]))
            elif msg_type == MessageType.SYSTEM_STATUS:
                message.update(self.decode_system_status(data[6:]))
            elif msg_type == MessageType.SAFETY_EVENT:
                message.update(self.decode_safety_event(data[6:]))
            else:
                message['payload'] = data[6:].hex()
            
            return message
            
        except Exception as e:
            print(f"⚠️  Decode error: {e}")
            return None
    
    def decode_digital_input(self, payload):
        """Decode digital input message"""
        if len(payload) >= 4:
            pin = payload[0]
            flags = payload[1]
            debounce_time = struct.unpack('<H', payload[2:4])[0]
            
            state = bool(flags & 0x01)
            is_debounced = bool(flags & 0x02)
            input_type = (flags >> 2) & 0x3F
            
            return {
                'pin': pin,
                'state': state,
                'debounced': is_debounced,
                'input_type': input_type,
                'debounce_time_ms': debounce_time
            }
        return {'payload': payload.hex()}
    
    def decode_pressure_reading(self, payload):
        """Decode pressure reading message"""
        if len(payload) >= 8:
            sensor_pin = payload[0]
            flags = payload[1]
            raw_value = struct.unpack('<H', payload[2:4])[0]
            pressure_psi = struct.unpack('<f', payload[4:8])[0]
            
            is_fault = bool(flags & 0x01)
            pressure_type = (flags >> 1) & 0x7F
            
            return {
                'sensor_pin': sensor_pin,
                'raw_adc': raw_value,
                'pressure_psi': round(pressure_psi, 2),
                'fault': is_fault,
                'pressure_type': pressure_type
            }
        return {'payload': payload.hex()}
    
    def decode_system_status(self, payload):
        """Decode system status message"""
        if len(payload) >= 12:
            flags = struct.unpack('<I', payload[0:4])[0]
            uptime = struct.unpack('<I', payload[4:8])[0]
            free_memory = struct.unpack('<H', payload[8:10])[0]
            loop_time = struct.unpack('<H', payload[10:12])[0]
            
            return {
                'system_flags': flags,
                'uptime_ms': uptime,
                'free_memory': free_memory,
                'loop_time_us': loop_time
            }
        return {'payload': payload.hex()}
    
    def decode_safety_event(self, payload):
        """Decode safety event message"""
        if len(payload) >= 3:
            event_type = payload[0]
            flags = payload[1]
            severity = payload[2]
            
            return {
                'event_type': event_type,
                'flags': flags,
                'severity': severity
            }
        return {'payload': payload.hex()}
    
    def display_message(self, message):
        """Display message in formatted output"""
        if not message:
            return
        
        # Connection mode indicator
        mode_icon = "🔌" if message['connection_mode'] == 'direct' else "📡"
        
        timestamp = message['received_at'].strftime('%H:%M:%S.%f')[:-3]
        msg_type = message['type']
        seq = message['sequence']
        
        print(f"{mode_icon} [{timestamp}] Type:0x{msg_type:02X} Seq:{seq:3d} ", end="")
        
        # Add mesh stats for Meshtastic mode
        if message['connection_mode'] == 'meshtastic' and 'mesh_stats' in message:
            stats = message['mesh_stats']
            if stats['rssi']:
                print(f"RSSI:{stats['rssi']}dBm ", end="")
            if stats['hop_count'] > 0:
                print(f"Hops:{stats['hop_count']} ", end="")
        
        # Message-specific content
        if msg_type == MessageType.PRESSURE_READING:
            psi = message.get('pressure_psi', 0)
            pin = message.get('sensor_pin', 0)
            print(f"Pressure A{pin}: {psi:.1f} PSI")
            
        elif msg_type == MessageType.DIGITAL_INPUT:
            pin = message.get('pin', 0)
            state = message.get('state', False)
            print(f"Input Pin {pin}: {'HIGH' if state else 'LOW'}")
            
        elif msg_type == MessageType.SYSTEM_STATUS:
            uptime = message.get('uptime_ms', 0)
            memory = message.get('free_memory', 0)
            print(f"System: Uptime {uptime}ms, Memory {memory} bytes")
            
        elif msg_type == MessageType.SAFETY_EVENT:
            event = message.get('event_type', 0)
            severity = message.get('severity', 0)
            print(f"Safety Event: Type {event}, Severity {severity}")
            
        else:
            payload = message.get('payload', '')
            print(f"Data: {payload[:20]}{'...' if len(payload) > 20 else ''}")
    
    def display_statistics(self):
        """Display connection statistics"""
        if self.connection_mode == ConnectionMode.DIRECT:
            print(f"\n📊 Direct Connection Statistics:")
        else:
            print(f"\n📊 Meshtastic Bridge Statistics:")
        
        print(f"   Messages: {self.messages_received}")
        print(f"   Bytes: {self.bytes_received}")
        
        if self.connection_mode == ConnectionMode.MESHTASTIC:
            stats = self.mesh_stats
            print(f"   Signal Quality:")
            if stats['rssi']:
                print(f"     RSSI: {stats['rssi']} dBm")
            if stats['snr']:
                print(f"     SNR: {stats['snr']} dB")
            print(f"     Hop Count: {stats['hop_count']}")
    
    def run(self):
        """Main monitoring loop"""
        if not self.connect():
            return False
        
        print(f"""
╔══════════════════════════════════════════════════════════════╗
║         LOGSPLITTER ENHANCED TELEMETRY MONITOR               ║
║              Direct Arduino + Meshtastic Bridge              ║
╠══════════════════════════════════════════════════════════════╣
║  Mode: {self.connection_mode.value.upper():<15}  Port: {self.port:<15}  ║
║  Expected: Binary protobuf messages from LogSplitter        ║
║  Format: [SIZE][MSG_TYPE][SEQ][TIMESTAMP][DATA...]          ║
╚══════════════════════════════════════════════════════════════╝

Press Ctrl+C to stop monitoring...
        """)
        
        try:
            while True:
                message = self.read_message()
                if message:
                    self.display_message(message)
                else:
                    time.sleep(0.01)  # Small delay to prevent excessive CPU usage
                    
        except KeyboardInterrupt:
            print("\n\n⏹️  Monitoring stopped by user")
        except Exception as e:
            print(f"\n❌ Monitoring error: {e}")
        finally:
            if self.ser:
                self.ser.close()
            self.display_statistics()

def main():
    """Main application entry point"""
    parser = argparse.ArgumentParser(
        description='LogSplitter Enhanced Telemetry Receiver',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Connection Modes:
  direct     - Direct Arduino connection via A4/A5 pins
  meshtastic - Meshtastic bridge (wireless mesh transport)
  auto       - Auto-detect connection type (default)

Examples:
  python meshtastic_telemetry_receiver.py
  python meshtastic_telemetry_receiver.py --mode direct --port COM3
  python meshtastic_telemetry_receiver.py --mode meshtastic --port COM4
        """
    )
    
    parser.add_argument('--mode', choices=['direct', 'meshtastic', 'auto'], 
                       default='auto', help='Connection mode (default: auto)')
    parser.add_argument('--port', help='Serial port (auto-detect if not specified)')
    parser.add_argument('--baud', type=int, default=115200, help='Baud rate (default: 115200)')
    
    args = parser.parse_args()
    
    print("🚀 LogSplitter Enhanced Telemetry Receiver")
    print("=" * 60)
    
    try:
        mode = ConnectionMode(args.mode)
        receiver = EnhancedTelemetryReceiver(mode, args.port, args.baud)
        receiver.run()
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())