#!/usr/bin/env python3
"""
Mesh Transport Failure Diagnosis
=================================

PROBLEM: No protobuf data on COM8 (Diagnostic Node)
EXPECTED: Arduino → A2 → COM13 → Mesh → COM8 → LCARS

This script runs comprehensive diagnostics to identify why
the mesh transport isn't working.

Potential Issues:
1. Serial module not enabled on Controller Node (COM13)
2. Wrong serial module mode (PROTO vs SIMPLE)
3. Missing 'serial' channel on mesh network
4. Baud rate mismatch (115200 vs 38400)
5. RX/TX pins not configured correctly
6. Arduino not actually sending data to A2
7. Mesh network connectivity issues
"""

import serial
import time
import sys
import subprocess

def test_arduino_output(port="COM7", duration=5):
    """Test if Arduino is actually sending telemetry"""
    print("\n" + "="*70)
    print("TEST 1: ARDUINO OUTPUT VERIFICATION (COM7)")
    print("="*70)
    
    try:
        ser = serial.Serial(port, 115200, timeout=1)
        print(f"✅ Connected to Arduino on {port}")
        print("🔍 Monitoring debug output for telemetry confirmation...\n")
        
        start_time = time.time()
        telemetry_confirmed = False
        
        while time.time() - start_time < duration:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"   {line}")
                    # Check for telemetry markers
                    if any(marker in line for marker in ['📡', '→ A2', 'TELEMETRY', 'Sending']):
                        telemetry_confirmed = True
        
        ser.close()
        
        if telemetry_confirmed:
            print("\n✅ PASS: Arduino is generating and sending telemetry to A2")
            return True
        else:
            print("\n⚠️  WARNING: No telemetry markers detected in Arduino output")
            print("💡 Send 'burst 10' command to Arduino to generate test data")
            return False
            
    except Exception as e:
        print(f"❌ FAIL: Cannot connect to Arduino - {e}")
        return False

def test_com13_reception(port="COM13", duration=5):
    """Test if COM13 Controller Node is receiving data from Arduino"""
    print("\n" + "="*70)
    print("TEST 2: COM13 CONTROLLER NODE RECEPTION")
    print("="*70)
    
    try:
        # Try both 115200 and 38400 baud
        for baud in [115200, 38400]:
            print(f"\n🔍 Trying {baud} baud...")
            ser = serial.Serial(port, baud, timeout=1)
            
            start_time = time.time()
            received_bytes = 0
            protobuf_detected = False
            
            while time.time() - start_time < duration:
                if ser.in_waiting:
                    chunk = ser.read(ser.in_waiting)
                    received_bytes += len(chunk)
                    
                    # Check for protobuf message types (0x10-0x17)
                    for byte in chunk:
                        if 0x10 <= byte <= 0x17:
                            protobuf_detected = True
                            print(f"   ⚡ Protobuf marker detected: 0x{byte:02X}")
                            break
                    
                    if protobuf_detected:
                        break
                time.sleep(0.1)
            
            ser.close()
            
            if protobuf_detected:
                print(f"\n✅ PASS: COM13 receiving protobuf data at {baud} baud")
                return True, baud
            elif received_bytes > 0:
                print(f"   Received {received_bytes} bytes (debug only, no protobuf)")
        
        print("\n❌ FAIL: COM13 not receiving protobuf data from Arduino")
        print("💡 Possible causes:")
        print("   • Arduino A2 pin not connected to Meshtastic RX")
        print("   • Wrong baud rate on serial module")
        print("   • Serial module not enabled")
        return False, None
            
    except Exception as e:
        print(f"❌ FAIL: Cannot connect to COM13 - {e}")
        return False, None

def test_com8_reception(port="COM8", duration=5):
    """Test if COM8 Diagnostic Node is receiving any data"""
    print("\n" + "="*70)
    print("TEST 3: COM8 DIAGNOSTIC NODE RECEPTION")
    print("="*70)
    
    try:
        for baud in [115200, 38400]:
            print(f"\n🔍 Trying {baud} baud...")
            ser = serial.Serial(port, baud, timeout=1)
            
            start_time = time.time()
            received_bytes = 0
            protobuf_detected = False
            
            while time.time() - start_time < duration:
                if ser.in_waiting:
                    chunk = ser.read(ser.in_waiting)
                    received_bytes += len(chunk)
                    
                    # Check for protobuf message types
                    for byte in chunk:
                        if 0x10 <= byte <= 0x17:
                            protobuf_detected = True
                            print(f"   ⚡ Protobuf marker detected: 0x{byte:02X}")
                            break
                    
                    if protobuf_detected:
                        break
                time.sleep(0.1)
            
            ser.close()
            
            if protobuf_detected:
                print(f"\n✅ PASS: COM8 receiving protobuf data at {baud} baud")
                return True, baud
            elif received_bytes > 0:
                print(f"   Received {received_bytes} bytes (likely debug only)")
        
        print("\n❌ FAIL: COM8 not receiving protobuf data from mesh")
        return False, None
            
    except Exception as e:
        print(f"❌ FAIL: Cannot connect to COM8 - {e}")
        return False, None

def print_diagnosis_summary(results):
    """Print comprehensive diagnosis summary"""
    print("\n" + "="*70)
    print("DIAGNOSIS SUMMARY")
    print("="*70)
    
    arduino_ok = results.get('arduino', False)
    com13_ok, com13_baud = results.get('com13', (False, None))
    com8_ok, com8_baud = results.get('com8', (False, None))
    
    print(f"\n📊 TEST RESULTS:")
    print(f"   Arduino (COM7) generating telemetry: {'✅' if arduino_ok else '❌'}")
    print(f"   COM13 receiving from Arduino: {'✅' if com13_ok else '❌'} {f'({com13_baud} baud)' if com13_baud else ''}")
    print(f"   COM8 receiving from mesh: {'✅' if com8_ok else '❌'} {f'({com8_baud} baud)' if com8_baud else ''}")
    
    print(f"\n🔍 ROOT CAUSE ANALYSIS:")
    
    if arduino_ok and com13_ok and com8_ok:
        print("   ✅ SYSTEM WORKING - All stages operational!")
        print("   ✅ Arduino → COM13 → Mesh → COM8 chain is functional")
        
    elif not arduino_ok:
        print("   ❌ PROBLEM: Arduino not generating telemetry")
        print("   💡 SOLUTION:")
        print("      1. Connect to Arduino on COM7 (115200 baud)")
        print("      2. Send 'burst 10' command to generate test data")
        print("      3. Verify A2 pin output in debug messages")
        
    elif arduino_ok and not com13_ok:
        print("   ❌ PROBLEM: Arduino → COM13 link broken")
        print("   💡 SOLUTION:")
        print("      1. Verify physical connection: Arduino A2 → Meshtastic RX")
        print("      2. Check serial module enabled on Controller Node:")
        print("         meshtastic --port COM13 --get serial")
        print("      3. Verify baud rate matches Arduino (115200):")
        print("         meshtastic --port COM13 --set serial.baud 115200")
        print("      4. Set serial module to SIMPLE mode:")
        print("         meshtastic --port COM13 --set serial.mode SIMPLE")
        print("      5. Configure RX/TX pins:")
        print("         meshtastic --port COM13 --set serial.rxd 16")
        print("         meshtastic --port COM13 --set serial.txd 17")
        
    elif arduino_ok and com13_ok and not com8_ok:
        print("   ❌ PROBLEM: Mesh transport COM13 → COM8 not working")
        print("   💡 SOLUTION:")
        print("      1. Verify 'serial' channel exists on mesh:")
        print("         meshtastic --port COM13 --info")
        print("         meshtastic --port COM13 --ch-index 1 --ch-set name serial")
        print("      2. Check mesh connectivity:")
        print("         meshtastic --port COM13 --nodes")
        print("      3. Verify COM8 node is online and in range")
        print("      4. Test with ping:")
        print("         meshtastic --port COM13 --ping <COM8_NODE_ID>")
        print("      5. Enable serial module on COM8 as well:")
        print("         meshtastic --port COM8 --set serial.enabled true")
        print("         meshtastic --port COM8 --set serial.mode SIMPLE")
    
    print(f"\n🎯 RECOMMENDED NEXT STEPS:")
    print("   1. Check Meshtastic serial module configuration:")
    print("      meshtastic --port COM13 --get serial")
    print("   2. Verify 'serial' channel configuration:")
    print("      meshtastic --port COM13 --ch-long")
    print("   3. Test mesh network connectivity:")
    print("      meshtastic --port COM13 --nodes")
    print("   4. Enable debug logging:")
    print("      meshtastic --port COM13 --set device.debug_log_enabled true")
    
    print("\n📖 CRITICAL MESHTASTIC CONFIGURATION:")
    print("   For SIMPLE mode to work, you need:")
    print("   • serial.enabled = true")
    print("   • serial.mode = SIMPLE (not PROTO)")
    print("   • serial.baud = 115200 (match Arduino)")
    print("   • Channel named 'serial' must exist")
    print("   • Both COM13 and COM8 nodes need serial module enabled")
    
    print("="*70 + "\n")

def main():
    """Main diagnostic workflow"""
    
    print("""
╔════════════════════════════════════════════════════════════════╗
║              MESH TRANSPORT FAILURE DIAGNOSIS                  ║
║                                                                ║
║  Problem: No protobuf data on COM8 (Diagnostic Node)           ║
║  Expected: Arduino → A2 → COM13 → Mesh → COM8 → LCARS          ║
║                                                                ║
║  This tool will test each stage to identify the failure point  ║
╚════════════════════════════════════════════════════════════════╝
""")
    
    results = {}
    
    # Test 1: Arduino telemetry generation
    print("Starting diagnostics...\n")
    time.sleep(1)
    
    results['arduino'] = test_arduino_output()
    time.sleep(1)
    
    # Test 2: COM13 reception from Arduino
    results['com13'] = test_com13_reception()
    time.sleep(1)
    
    # Test 3: COM8 reception from mesh
    results['com8'] = test_com8_reception()
    
    # Print comprehensive diagnosis
    print_diagnosis_summary(results)
    
    # Return success if all stages working
    all_ok = (results['arduino'] and 
              results['com13'][0] and 
              results['com8'][0])
    
    return 0 if all_ok else 1

if __name__ == "__main__":
    sys.exit(main())
