#!/usr/bin/env python3
"""
LCARS Protobuf Decoder with Database Logger
============================================

Receives binary protobuf telemetry from COM8 Meshtastic Diagnostic Node
and logs all raw messages to SQLite database for analysis and playback.

Message Types:
- 0x10: Digital I/O (limit switches, buttons, e-stop)
- 0x11: Relays (extend, retract, engine stop)
- 0x12: Pressure (main and auxiliary hydraulic pressure)
- 0x13: Sequence (hydraulic cycle state machine)
- 0x14: Error (system error conditions)
- 0x15: Safety (safety system status, mill lamp)
- 0x16: System Status (uptime, cycle count, metrics)
- 0x17: Heartbeat (periodic alive signal)
"""

import serial
import sqlite3
import time
import sys
import struct
from datetime import datetime
from pathlib import Path

# Message type definitions
MESSAGE_TYPES = {
    0x10: "Digital I/O",
    0x11: "Relays",
    0x12: "Pressure",
    0x13: "Sequence",
    0x14: "Error",
    0x15: "Safety",
    0x16: "System Status",
    0x17: "Heartbeat"
}

class ProtobufDatabase:
    """SQLite database for raw protobuf message logging"""
    
    def __init__(self, db_path="telemetry_logs.db"):
        self.db_path = db_path
        self.conn = None
        self.cursor = None
        self.init_database()
    
    def init_database(self):
        """Initialize SQLite database with schema"""
        self.conn = sqlite3.connect(self.db_path)
        self.cursor = self.conn.cursor()
        
        # Create raw messages table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS raw_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                timestamp REAL NOT NULL,
                timestamp_iso TEXT NOT NULL,
                message_type INTEGER NOT NULL,
                message_type_name TEXT NOT NULL,
                size INTEGER NOT NULL,
                raw_data BLOB NOT NULL,
                hex_data TEXT NOT NULL,
                session_id TEXT NOT NULL
            )
        """)
        
        # Create decoded messages table (for future parsing)
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS decoded_messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                raw_message_id INTEGER NOT NULL,
                timestamp REAL NOT NULL,
                message_type TEXT NOT NULL,
                decoded_data TEXT NOT NULL,
                FOREIGN KEY (raw_message_id) REFERENCES raw_messages(id)
            )
        """)
        
        # Create sessions table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS sessions (
                session_id TEXT PRIMARY KEY,
                start_time REAL NOT NULL,
                start_time_iso TEXT NOT NULL,
                end_time REAL,
                end_time_iso TEXT,
                message_count INTEGER DEFAULT 0,
                port TEXT NOT NULL,
                baud_rate INTEGER NOT NULL
            )
        """)
        
        # Create statistics table
        self.cursor.execute("""
            CREATE TABLE IF NOT EXISTS statistics (
                session_id TEXT NOT NULL,
                message_type TEXT NOT NULL,
                count INTEGER DEFAULT 0,
                PRIMARY KEY (session_id, message_type),
                FOREIGN KEY (session_id) REFERENCES sessions(session_id)
            )
        """)
        
        self.conn.commit()
        print(f"✅ Database initialized: {self.db_path}")
    
    def start_session(self, session_id, port, baud_rate):
        """Start new logging session"""
        now = time.time()
        self.cursor.execute("""
            INSERT INTO sessions (session_id, start_time, start_time_iso, port, baud_rate)
            VALUES (?, ?, ?, ?, ?)
        """, (session_id, now, datetime.fromtimestamp(now).isoformat(), port, baud_rate))
        self.conn.commit()
    
    def log_message(self, session_id, msg_type, raw_data):
        """Log raw protobuf message"""
        now = time.time()
        msg_type_name = MESSAGE_TYPES.get(msg_type, f"Unknown (0x{msg_type:02X})")
        hex_data = raw_data.hex(' ')
        
        self.cursor.execute("""
            INSERT INTO raw_messages 
            (timestamp, timestamp_iso, message_type, message_type_name, size, raw_data, hex_data, session_id)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
        """, (now, datetime.fromtimestamp(now).isoformat(), msg_type, msg_type_name, 
              len(raw_data), raw_data, hex_data, session_id))
        
        # Update session message count
        self.cursor.execute("""
            UPDATE sessions SET message_count = message_count + 1 
            WHERE session_id = ?
        """, (session_id,))
        
        # Update statistics
        self.cursor.execute("""
            INSERT INTO statistics (session_id, message_type, count)
            VALUES (?, ?, 1)
            ON CONFLICT(session_id, message_type) 
            DO UPDATE SET count = count + 1
        """, (session_id, msg_type_name))
        
        self.conn.commit()
        return self.cursor.lastrowid
    
    def end_session(self, session_id):
        """End logging session"""
        now = time.time()
        self.cursor.execute("""
            UPDATE sessions 
            SET end_time = ?, end_time_iso = ?
            WHERE session_id = ?
        """, (now, datetime.fromtimestamp(now).isoformat(), session_id))
        self.conn.commit()
    
    def get_session_stats(self, session_id):
        """Get statistics for current session"""
        self.cursor.execute("""
            SELECT message_type, count 
            FROM statistics 
            WHERE session_id = ?
            ORDER BY count DESC
        """, (session_id,))
        return self.cursor.fetchall()
    
    def close(self):
        """Close database connection"""
        if self.conn:
            self.conn.close()

class LCARSProtobufDecoder:
    """LCARS protobuf decoder with database logging"""
    
    def __init__(self, port="COM8", baud_rate=38400, db_path="telemetry_logs.db"):
        self.port = port
        self.baud_rate = baud_rate
        self.serial_conn = None
        self.database = ProtobufDatabase(db_path)
        self.session_id = f"session_{int(time.time())}"
        
        self.stats = {
            'total_bytes': 0,
            'total_messages': 0,
            'by_type': {},
            'errors': 0
        }
    
    def connect(self):
        """Connect to COM8 serial port"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baud_rate,
                timeout=1,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            print(f"✅ Connected to {self.port} at {self.baud_rate} baud")
            
            # Start database session
            self.database.start_session(self.session_id, self.port, self.baud_rate)
            print(f"📊 Database session started: {self.session_id}")
            
            return True
        except Exception as e:
            print(f"❌ Connection error: {e}")
            return False
    
    def parse_message(self, raw_data):
        """Parse protobuf message (basic extraction)"""
        if len(raw_data) < 2:
            return None
        
        # Simple message detection - look for type bytes
        decoded = {
            'raw': raw_data,
            'hex': raw_data.hex(' '),
            'size': len(raw_data),
            'fields': {}
        }
        
        # Look for message type markers
        for i, byte in enumerate(raw_data):
            if 0x10 <= byte <= 0x17:
                decoded['likely_type'] = MESSAGE_TYPES.get(byte, f"Unknown (0x{byte:02X})")
                decoded['type_offset'] = i
                break
        
        return decoded
    
    def process_stream(self, duration=None):
        """Process incoming protobuf stream"""
        print(f"\n🔍 Monitoring protobuf stream...")
        print(f"{'Time':<12} {'Type':<20} {'Size':<6} {'Hex Preview'}")
        print("-" * 80)
        
        start_time = time.time()
        buffer = bytearray()
        
        try:
            while True:
                # Check duration limit
                if duration and (time.time() - start_time) > duration:
                    break
                
                # Read available data
                if self.serial_conn.in_waiting:
                    chunk = self.serial_conn.read(self.serial_conn.in_waiting)
                    self.stats['total_bytes'] += len(chunk)
                    buffer.extend(chunk)
                    
                    # Look for message type markers in chunk
                    for i in range(len(chunk)):
                        byte = chunk[i]
                        if 0x10 <= byte <= 0x17:
                            # Found potential message type
                            msg_type = byte
                            msg_type_name = MESSAGE_TYPES.get(msg_type, f"Unknown (0x{msg_type:02X})")
                            
                            # Extract surrounding context (up to 20 bytes)
                            context_start = max(0, i - 5)
                            context_end = min(len(chunk), i + 15)
                            context = chunk[context_start:context_end]
                            
                            # Log to database
                            msg_id = self.database.log_message(self.session_id, msg_type, context)
                            
                            # Update stats
                            self.stats['total_messages'] += 1
                            self.stats['by_type'][msg_type_name] = self.stats['by_type'].get(msg_type_name, 0) + 1
                            
                            # Display
                            elapsed = time.time() - start_time
                            hex_preview = context.hex(' ')[:60]
                            print(f"[{elapsed:>8.2f}s] {msg_type_name:<20} {len(context):<6} {hex_preview}")
                
                time.sleep(0.1)
                
        except KeyboardInterrupt:
            print("\n\n⚠️  Interrupted by user")
        
        # End session
        self.database.end_session(self.session_id)
        
        return self.stats
    
    def print_summary(self):
        """Print session summary"""
        print("\n" + "=" * 80)
        print("SESSION SUMMARY")
        print("=" * 80)
        
        print(f"\n📊 STATISTICS:")
        print(f"   Session ID: {self.session_id}")
        print(f"   Total bytes received: {self.stats['total_bytes']:,}")
        print(f"   Total messages logged: {self.stats['total_messages']:,}")
        print(f"   Errors: {self.stats['errors']}")
        
        if self.stats['by_type']:
            print(f"\n📋 MESSAGE TYPES:")
            for msg_type, count in sorted(self.stats['by_type'].items(), key=lambda x: x[1], reverse=True):
                percentage = (count / self.stats['total_messages'] * 100) if self.stats['total_messages'] > 0 else 0
                print(f"   • {msg_type:<20} {count:>6} messages ({percentage:>5.1f}%)")
        
        # Get database stats
        db_stats = self.database.get_session_stats(self.session_id)
        print(f"\n💾 DATABASE:")
        print(f"   File: {self.database.db_path}")
        print(f"   Session: {self.session_id}")
        print(f"   Messages stored: {sum(count for _, count in db_stats)}")
        
        print("\n🔍 QUERY DATABASE:")
        print(f"   sqlite3 {self.database.db_path}")
        print(f"   SELECT * FROM raw_messages WHERE session_id='{self.session_id}';")
        print(f"   SELECT message_type_name, COUNT(*) FROM raw_messages WHERE session_id='{self.session_id}' GROUP BY message_type_name;")
        
        print("=" * 80)
    
    def close(self):
        """Close connections"""
        if self.serial_conn:
            self.serial_conn.close()
            print("\n✅ Serial connection closed")
        
        self.database.close()
        print("✅ Database connection closed")

def main():
    """Main entry point"""
    
    port = "COM8"
    baud_rate = 38400
    duration = 60  # Default 60 seconds
    db_path = "telemetry_logs.db"
    
    if len(sys.argv) > 1:
        port = sys.argv[1]
    if len(sys.argv) > 2:
        baud_rate = int(sys.argv[2])
    if len(sys.argv) > 3:
        duration = int(sys.argv[3])
    if len(sys.argv) > 4:
        db_path = sys.argv[4]
    
    print("""
╔════════════════════════════════════════════════════════════════╗
║         LCARS PROTOBUF DECODER WITH DATABASE LOGGER            ║
║                                                                ║
║  Purpose: Decode binary protobuf telemetry from COM8 and       ║
║           log all raw messages to SQLite database              ║
║                                                                ║
║  Features:                                                     ║
║  • Real-time protobuf message detection                        ║
║  • SQLite database logging for analysis                        ║
║  • Session management and statistics                           ║
║  • Message type classification                                 ║
╚════════════════════════════════════════════════════════════════╝
""")
    
    print(f"⚙️  Configuration:")
    print(f"   Port: {port}")
    print(f"   Baud: {baud_rate}")
    print(f"   Duration: {duration}s")
    print(f"   Database: {db_path}")
    print()
    
    # Create decoder
    decoder = LCARSProtobufDecoder(port, baud_rate, db_path)
    
    # Connect
    if not decoder.connect():
        return 1
    
    # Process stream
    try:
        decoder.process_stream(duration)
    except Exception as e:
        print(f"\n❌ Error during processing: {e}")
        import traceback
        traceback.print_exc()
        return 1
    finally:
        # Print summary
        decoder.print_summary()
        
        # Close connections
        decoder.close()
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
