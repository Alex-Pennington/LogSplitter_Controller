#!/usr/bin/env python3
"""
COM8 Diagnostic Node Telemetry Monitor
======================================

CRITICAL UNDERSTANDING:
- COM8 = Diagnostic Node (connects to LCARS system)
- COM13 = Controller Node (connected to Arduino via A2/A3)
- Arduino → A2 → COM13 RX → Mesh Network → COM8 TX → LCARS

This script monitors COM8 for Arduino telemetry that should be
transported through the mesh network from the Controller Node.

Expected Data:
- Binary protobuf messages (7-19 bytes per message)
- Message types: 0x10-0x17 (Digital I/O, Relays, Pressure, etc.)
- Size-prefixed format: [size_byte][message_type][protobuf_data]
"""

import serial
import time
import sys
import struct

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

def monitor_com8_telemetry(port="COM8", baudrate=115200, duration=30):
    """Monitor COM8 Diagnostic Node for Arduino telemetry stream"""
    
    print(f"\n{'='*70}")
    print(f"COM8 DIAGNOSTIC NODE - ARDUINO TELEMETRY MONITOR")
    print(f"{'='*70}")
    print(f"Port: {port}")
    print(f"Baud: {baudrate}")
    print(f"Duration: {duration}s")
    print(f"Purpose: Detect Arduino telemetry transported via mesh network")
    print(f"{'='*70}\n")
    
    print("📡 EXPECTED DATA FLOW:")
    print("   Arduino UNO R4 (COM7)")
    print("        ↓ A2 pin (115200 baud)")
    print("   Controller Node (COM13)")
    print("        ↓ Mesh Network (LoRa)")
    print("   Diagnostic Node (COM8) ← YOU ARE HERE")
    print("        ↓ Serial Output")
    print("   LCARS System\n")
    
    try:
        # Open COM8 serial connection
        ser = serial.Serial(
            port=port,
            baudrate=baudrate,
            timeout=1,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
        
        print(f"✅ Connected to {port}\n")
        
        # Statistics
        stats = {
            'total_bytes': 0,
            'messages_detected': 0,
            'by_type': {},
            'protobuf_candidates': 0,
            'ascii_debug': 0,
            'raw_chunks': []
        }
        
        print("🔍 Monitoring for telemetry data...\n")
        print(f"{'Time':<12} {'Bytes':<8} {'Type':<20} {'Data Preview'}")
        print("-" * 70)
        
        start_time = time.time()
        buffer = bytearray()
        
        while time.time() - start_time < duration:
            if ser.in_waiting:
                chunk = ser.read(ser.in_waiting)
                stats['total_bytes'] += len(chunk)
                buffer.extend(chunk)
                
                elapsed = time.time() - start_time
                
                # Check for protobuf message markers
                for i in range(len(chunk)):
                    byte = chunk[i]
                    # Check if it's a valid message type (0x10-0x17)
                    if 0x10 <= byte <= 0x17:
                        stats['protobuf_candidates'] += 1
                        msg_type = MESSAGE_TYPES.get(byte, f"Unknown (0x{byte:02X})")
                        
                        # Look for size byte before message type
                        context_start = max(0, i-5)
                        context_end = min(len(chunk), i+10)
                        context = chunk[context_start:context_end]
                        
                        print(f"[{elapsed:>8.2f}s] {len(context):<8} {'⚡ ' + msg_type:<20} {context.hex(' ')}")
                        
                        stats['messages_detected'] += 1
                        stats['by_type'][msg_type] = stats['by_type'].get(msg_type, 0) + 1
                
                # Check for ASCII debug messages
                try:
                    ascii_text = chunk.decode('utf-8', errors='strict')
                    if any(keyword in ascii_text for keyword in ['DEBUG', 'INFO', 'Battery', 'Power']):
                        stats['ascii_debug'] += len(chunk)
                        print(f"[{elapsed:>8.2f}s] {len(chunk):<8} {'📝 ASCII Debug':<20} {ascii_text[:50]}")
                except UnicodeDecodeError:
                    pass
                
                # Store raw chunks for analysis
                if len(stats['raw_chunks']) < 10:
                    stats['raw_chunks'].append({
                        'timestamp': elapsed,
                        'data': chunk,
                        'hex': chunk.hex(' ')
                    })
                
            time.sleep(0.1)
        
        print("\n" + "=" * 70)
        print("MONITORING RESULTS")
        print("=" * 70)
        
        print(f"\n📊 STATISTICS:")
        print(f"   Total bytes received: {stats['total_bytes']}")
        print(f"   Protobuf candidates: {stats['protobuf_candidates']}")
        print(f"   Messages detected: {stats['messages_detected']}")
        print(f"   ASCII debug bytes: {stats['ascii_debug']}")
        
        if stats['by_type']:
            print(f"\n📋 MESSAGE TYPES DETECTED:")
            for msg_type, count in sorted(stats['by_type'].items()):
                print(f"   • {msg_type}: {count} messages")
        
        if stats['raw_chunks']:
            print(f"\n🔍 RAW DATA SAMPLES (first {len(stats['raw_chunks'])} chunks):")
            for i, chunk in enumerate(stats['raw_chunks'][:3], 1):
                print(f"\n   Sample {i} at {chunk['timestamp']:.2f}s:")
                print(f"   Hex: {chunk['hex'][:100]}")
                print(f"   Bytes: {len(chunk['data'])}")
        
        # Analysis
        print(f"\n💡 ANALYSIS:")
        if stats['messages_detected'] > 0:
            print(f"   ✅ ARDUINO TELEMETRY DETECTED!")
            print(f"   ✅ Mesh network is transporting data successfully")
            print(f"   ✅ COM8 receiving protobuf messages from Arduino")
            print(f"   ✅ Ready for LCARS integration")
        elif stats['ascii_debug'] > 0:
            print(f"   ⚠️  Only Meshtastic debug output detected")
            print(f"   ⚠️  No Arduino telemetry stream found")
            print(f"   💡 Check Controller Node serial module configuration")
        elif stats['total_bytes'] == 0:
            print(f"   ❌ NO DATA RECEIVED")
            print(f"   💡 Possible causes:")
            print(f"      • Serial module not configured on Controller Node")
            print(f"      • Wrong baud rate (try 38400 instead of 115200)")
            print(f"      • 'serial' channel not configured on mesh")
            print(f"      • Serial module in PROTO mode instead of SIMPLE")
        else:
            print(f"   ⚠️  Data received but no protobuf messages identified")
            print(f"   💡 Check data format and message structure")
        
        print(f"\n🎯 NEXT STEPS:")
        if stats['messages_detected'] > 0:
            print(f"   1. ✅ Telemetry working - proceed with LCARS integration")
            print(f"   2. Implement protobuf decoder for LCARS system")
            print(f"   3. Test with full telemetry suite from Arduino")
        else:
            print(f"   1. Check Controller Node serial module mode (SIMPLE vs PROTO)")
            print(f"   2. Verify 'serial' channel exists on mesh network")
            print(f"   3. Try baud rate 38400 (Meshtastic default)")
            print(f"   4. Send 'burst 10' command to Arduino for test data")
            print(f"   5. Monitor COM13 to confirm Arduino is sending data")
        
        print("=" * 70 + "\n")
        
        ser.close()
        print("✅ Serial connection closed\n")
        
        return stats['messages_detected'] > 0
        
    except serial.SerialException as e:
        print(f"\n❌ Error opening {port}: {e}")
        print("\n💡 Possible causes:")
        print("   • Port already in use (close other programs)")
        print("   • Incorrect COM port number")
        print("   • Device not connected")
        return False
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    """Main entry point"""
    
    port = "COM8"
    baudrate = 115200
    duration = 30
    
    if len(sys.argv) > 1:
        port = sys.argv[1]
    if len(sys.argv) > 2:
        baudrate = int(sys.argv[2])
    if len(sys.argv) > 3:
        duration = int(sys.argv[3])
    
    print("""
╔════════════════════════════════════════════════════════════════╗
║         COM8 DIAGNOSTIC NODE - TELEMETRY MONITOR               ║
║                                                                ║
║  Purpose: Monitor COM8 (Diagnostic Node) for Arduino           ║
║           telemetry transported through mesh network           ║
║                                                                ║
║  Connection Chain:                                             ║
║    Arduino (COM7) → A2 → Controller (COM13) → Mesh →          ║
║    → Diagnostic (COM8) → LCARS System                          ║
║                                                                ║
║  Expected: Binary protobuf messages (0x10-0x17)                ║
║  Message Types: Digital I/O, Relays, Pressure, Sequence, etc. ║
╚════════════════════════════════════════════════════════════════╝
""")
    
    success = monitor_com8_telemetry(port, baudrate, duration)
    
    if success:
        print("🎉 SUCCESS - Arduino telemetry detected on COM8!")
        print("   Ready for LCARS integration\n")
    else:
        print("⚠️  No Arduino telemetry detected")
        print("   See analysis above for troubleshooting steps\n")
    
    return 0 if success else 1

if __name__ == "__main__":
    sys.exit(main())
