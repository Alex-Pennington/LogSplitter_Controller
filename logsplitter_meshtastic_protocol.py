#!/usr/bin/env python3
"""
Unified LogSplitter Meshtastic Communication Protocol

This module defines the complete communication protocol for LogSplitter Controller
systems using Meshtastic mesh networking as the primary/sole communication path.

Architecture:
- Channel 0: Emergency broadcasts (plain language) for critical alerts
- Channel 1 ("LogSplitter"): All normal communication via protobuf
- A4/A5 Serial: Meshtastic Serial Module in protobuf mode (115200 baud)
- No USB dependencies: Fully wireless mesh communication

Protocol Design:
- Telemetry: Existing 8 message types (0x10-0x17) encapsulated in Meshtastic
- Commands: Bidirectional command/response via protobuf
- Diagnostics: Real-time system status and debugging
- Emergency: Immediate plain language broadcasts to all mesh nodes

Message Flow:
Arduino ←A4/A5→ Meshtastic(Proto) ←LoRa→ Mesh ←LoRa→ Base(Proto) ←USB→ PC Wizard
"""

import struct
import json
import time
from datetime import datetime
from enum import Enum
from dataclasses import dataclass, asdict
from typing import Optional, Dict, List, Any, Union, Callable

class MeshtasticChannel(Enum):
    """Meshtastic channel assignments for LogSplitter system"""
    EMERGENCY = 0      # Channel 0: Plain language emergency broadcasts
    LOGSPLITTER = 1    # Channel 1: Binary protobuf communication

class LogSplitterMessageType(Enum):
    """Unified message types for LogSplitter Meshtastic communication"""
    
    # Telemetry Messages (0x10-0x17) - Existing Arduino telemetry
    TELEMETRY_DIGITAL_IO = 0x10
    TELEMETRY_RELAYS = 0x11  
    TELEMETRY_PRESSURE_MAIN = 0x12
    TELEMETRY_PRESSURE_AUX = 0x13
    TELEMETRY_ERRORS = 0x14
    TELEMETRY_SAFETY = 0x15
    TELEMETRY_SYSTEM_STATUS = 0x16
    TELEMETRY_TIMING = 0x17
    
    # Command Messages (0x20-0x2F) - Wizard to Controller
    COMMAND_REQUEST = 0x20      # Execute controller command
    COMMAND_RESPONSE = 0x21     # Controller response
    COMMAND_STATUS = 0x22       # Command execution status
    
    # Diagnostic Messages (0x30-0x3F) - System diagnostics  
    DIAGNOSTIC_REQUEST = 0x30   # Request diagnostic info
    DIAGNOSTIC_RESPONSE = 0x31  # Diagnostic data response
    DIAGNOSTIC_STREAM = 0x32    # Real-time diagnostic stream
    
    # System Messages (0x40-0x4F) - Network and system management
    HEARTBEAT = 0x40           # Network health check
    NODE_STATUS = 0x41         # Node status report
    MESH_HEALTH = 0x42         # Mesh network health
    CONFIG_REQUEST = 0x43      # Node configuration request
    CONFIG_RESPONSE = 0x44     # Configuration data
    
    # Emergency Messages (0x50-0x5F) - Critical alerts
    EMERGENCY_ALERT = 0x50     # Critical system alert
    SAFETY_ALERT = 0x51        # Safety system activation
    ERROR_ALERT = 0x52         # System error condition

@dataclass
class LogSplitterProtocolHeader:
    """Standard header for all LogSplitter Meshtastic messages"""
    version: int = 1                    # Protocol version
    message_type: LogSplitterMessageType = LogSplitterMessageType.HEARTBEAT
    sequence: int = 0                   # Message sequence number
    timestamp: int = 0                  # Unix timestamp (milliseconds)
    source_id: str = "UNKNOWN"          # Source node identifier
    payload_size: int = 0               # Payload size in bytes
    
    def to_bytes(self) -> bytes:
        """Convert header to binary format"""
        # Pack: version(1), type(1), sequence(2), timestamp(4), source_id(8), payload_size(2)
        source_bytes = self.source_id.encode('utf-8')[:8].ljust(8, b'\0')
        return struct.pack('<BBHIQ8sH', 
                          self.version,
                          self.message_type.value,
                          self.sequence,
                          self.timestamp,
                          0,  # Reserved for future use
                          source_bytes,
                          self.payload_size)
    
    @classmethod  
    def from_bytes(cls, data: bytes) -> 'LogSplitterProtocolHeader':
        """Parse header from binary data"""
        if len(data) < 18:
            raise ValueError("Header data too short")
        
        version, msg_type, sequence, timestamp, _, source_bytes, payload_size = \
            struct.unpack('<BBHIQ8sH', data[:18])
        
        source_id = source_bytes.rstrip(b'\0').decode('utf-8', errors='ignore')
        
        return cls(
            version=version,
            message_type=LogSplitterMessageType(msg_type),
            sequence=sequence, 
            timestamp=timestamp,
            source_id=source_id,
            payload_size=payload_size
        )

@dataclass
class CommandMessage:
    """Command message for controller operations"""
    command: str                        # Command string (e.g., "show", "R1 ON")
    parameters: Dict[str, Any]          # Additional parameters
    timeout_ms: int = 5000             # Command timeout
    require_ack: bool = True           # Require acknowledgment
    
    def to_json(self) -> str:
        """Convert to JSON for payload"""
        return json.dumps({
            'command': self.command,
            'parameters': self.parameters,
            'timeout_ms': self.timeout_ms,
            'require_ack': self.require_ack,
            'timestamp': int(time.time() * 1000)
        })
    
    @classmethod
    def from_json(cls, json_data: str) -> 'CommandMessage':
        """Create from JSON payload"""
        data = json.loads(json_data)
        return cls(
            command=data['command'],
            parameters=data.get('parameters', {}),
            timeout_ms=data.get('timeout_ms', 5000),
            require_ack=data.get('require_ack', True)
        )

@dataclass 
class CommandResponse:
    """Response to command execution"""
    command: str                        # Original command
    success: bool                      # Execution success
    output: str                        # Command output/result
    error: Optional[str] = None        # Error message if failed
    execution_time_ms: int = 0         # Execution duration
    
    def to_json(self) -> str:
        """Convert to JSON for payload"""
        return json.dumps({
            'command': self.command,
            'success': self.success,
            'output': self.output,
            'error': self.error,
            'execution_time_ms': self.execution_time_ms,
            'timestamp': int(time.time() * 1000)
        })
    
    @classmethod
    def from_json(cls, json_data: str) -> 'CommandResponse':
        """Create from JSON payload"""
        data = json.loads(json_data)
        return cls(
            command=data['command'],
            success=data['success'],
            output=data['output'],
            error=data.get('error'),
            execution_time_ms=data.get('execution_time_ms', 0)
        )

@dataclass
class EmergencyAlert:
    """Emergency alert for Channel 0 broadcast"""
    severity: str                      # CRITICAL, SEVERE, ERROR
    message: str                       # Plain language message
    location: str                      # Controller location/ID
    timestamp: datetime                # Alert timestamp
    auto_generated: bool = True        # System vs manual alert
    
    def to_plaintext(self) -> str:
        """Convert to plain language for Channel 0"""
        time_str = self.timestamp.strftime("%H:%M:%S")
        source = "AUTO" if self.auto_generated else "MANUAL"
        return f"[{self.severity}] {time_str} {self.location}: {self.message} ({source})"
    
    @classmethod
    def from_plaintext(cls, text: str) -> Optional['EmergencyAlert']:
        """Parse emergency alert from plain text"""
        # Basic parsing for received alerts
        import re
        pattern = r'\[(\w+)\] (\d{2}:\d{2}:\d{2}) ([^:]+): ([^(]+) \((\w+)\)'
        match = re.match(pattern, text.strip())
        
        if match:
            severity, time_str, location, message, source = match.groups()
            # Parse time (assumes today)
            hour, minute, second = map(int, time_str.split(':'))
            timestamp = datetime.now().replace(hour=hour, minute=minute, second=second, microsecond=0)
            
            return cls(
                severity=severity,
                message=message.strip(),
                location=location,
                timestamp=timestamp,
                auto_generated=(source == "AUTO")
            )
        return None

class LogSplitterProtocol:
    """Unified protocol handler for LogSplitter Meshtastic communication"""
    
    def __init__(self, node_id: str = "CONTROLLER"):
        self.node_id = node_id
        self.sequence_counter = 0
        self.message_handlers: Dict[LogSplitterMessageType, Callable] = {}
        
    def create_message(self, message_type: LogSplitterMessageType, 
                      payload: Union[str, bytes, Dict]) -> bytes:
        """Create complete protocol message"""
        
        # Convert payload to bytes
        if isinstance(payload, str):
            payload_bytes = payload.encode('utf-8')
        elif isinstance(payload, dict):
            payload_bytes = json.dumps(payload).encode('utf-8')
        elif isinstance(payload, bytes):
            payload_bytes = payload
        else:
            payload_bytes = str(payload).encode('utf-8')
        
        # Create header
        header = LogSplitterProtocolHeader(
            version=1,
            message_type=message_type,
            sequence=self.sequence_counter,
            timestamp=int(time.time() * 1000),
            source_id=self.node_id,
            payload_size=len(payload_bytes)
        )
        
        self.sequence_counter = (self.sequence_counter + 1) % 65536
        
        # Combine header + payload
        return header.to_bytes() + payload_bytes
    
    def parse_message(self, data: bytes) -> tuple[LogSplitterProtocolHeader, bytes]:
        """Parse received protocol message"""
        if len(data) < 18:
            raise ValueError("Message too short for header")
        
        header = LogSplitterProtocolHeader.from_bytes(data[:18])
        payload = data[18:18+header.payload_size]
        
        if len(payload) != header.payload_size:
            raise ValueError("Payload size mismatch")
        
        return header, payload
    
    def register_handler(self, message_type: LogSplitterMessageType, handler: Callable):
        """Register handler for specific message type"""
        self.message_handlers[message_type] = handler
    
    def handle_message(self, header: LogSplitterProtocolHeader, payload: bytes) -> Optional[bytes]:
        """Process received message and return response if needed"""
        handler = self.message_handlers.get(header.message_type)
        if handler:
            return handler(header, payload)
        return None
    
    def create_command_message(self, command: str, parameters: Optional[Dict] = None) -> bytes:
        """Create command message for controller"""
        cmd_msg = CommandMessage(
            command=command,
            parameters=parameters or {}
        )
        return self.create_message(LogSplitterMessageType.COMMAND_REQUEST, cmd_msg.to_json())
    
    def create_command_response(self, command: str, success: bool, 
                              output: str, error: Optional[str] = None, exec_time: int = 0) -> bytes:
        """Create command response from controller"""
        resp_msg = CommandResponse(
            command=command,
            success=success,
            output=output,
            error=error,
            execution_time_ms=exec_time
        )
        return self.create_message(LogSplitterMessageType.COMMAND_RESPONSE, resp_msg.to_json())
    
    def create_telemetry_message(self, telemetry_type: LogSplitterMessageType, 
                                telemetry_data: bytes) -> bytes:
        """Create telemetry message (existing Arduino format)"""
        return self.create_message(telemetry_type, telemetry_data)
    
    def create_emergency_alert(self, severity: str, message: str, location: Optional[str] = None) -> str:
        """Create plain language emergency alert for Channel 0"""
        alert = EmergencyAlert(
            severity=severity,
            message=message,
            location=location or self.node_id,
            timestamp=datetime.now()
        )
        return alert.to_plaintext()
    
    def create_heartbeat(self) -> bytes:
        """Create heartbeat message"""
        heartbeat_data = {
            'node_id': self.node_id,
            'timestamp': int(time.time() * 1000),
            'uptime_ms': int(time.time() * 1000),  # Simplified
            'status': 'online'
        }
        return self.create_message(LogSplitterMessageType.HEARTBEAT, heartbeat_data)

# Protocol constants
PROTOCOL_VERSION = 1
HEADER_SIZE = 18
MAX_PAYLOAD_SIZE = 200  # Meshtastic limit minus overhead
DEFAULT_TIMEOUT_MS = 5000

# Channel configuration
CHANNEL_CONFIG = {
    MeshtasticChannel.EMERGENCY: {
        'name': 'Emergency',
        'index': 0,
        'purpose': 'Plain language emergency broadcasts',
        'encryption': False,  # Open for all mesh nodes
        'priority': 'HIGH'
    },
    MeshtasticChannel.LOGSPLITTER: {
        'name': 'LogSplitter', 
        'index': 1,
        'purpose': 'Binary protobuf communication',
        'encryption': True,   # Industrial PSK
        'priority': 'NORMAL'
    }
}

# Serial Module configuration for Arduino-side Meshtastic node
SERIAL_MODULE_CONFIG = {
    'enabled': True,
    'baud': 115200,
    'timeout': 1000,
    'mode': 'PROTO',      # Key setting: Protobuf mode
    'echo': False,
    'rxd': 255,           # Use default pins (will be A5/RX)
    'txd': 255,           # Use default pins (will be A4/TX)  
}

def example_usage():
    """Example usage of the protocol"""
    
    # Create protocol handler
    protocol = LogSplitterProtocol("CONTROLLER_01")
    
    # Create a command message
    cmd_msg = protocol.create_command_message("show", {"verbose": True})
    print(f"Command message: {len(cmd_msg)} bytes")
    
    # Parse it back
    header, payload = protocol.parse_message(cmd_msg)
    print(f"Parsed header: {header}")
    
    cmd_obj = CommandMessage.from_json(payload.decode('utf-8'))
    print(f"Parsed command: {cmd_obj}")
    
    # Create emergency alert
    emergency = protocol.create_emergency_alert("CRITICAL", "E-Stop activated - hydraulic system shutdown")
    print(f"Emergency alert: {emergency}")
    
    # Create telemetry message (existing format)
    telemetry_data = b'\x10\x05\x00\x00\x01\x00'  # Example digital I/O
    telem_msg = protocol.create_telemetry_message(LogSplitterMessageType.TELEMETRY_DIGITAL_IO, telemetry_data)
    print(f"Telemetry message: {len(telem_msg)} bytes")

if __name__ == "__main__":
    example_usage()