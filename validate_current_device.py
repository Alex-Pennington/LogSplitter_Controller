#!/usr/bin/env python3
"""
Quick validation test for existing LogSplitter Meshtastic device
"""

import subprocess
import time
from datetime import datetime

def test_existing_device():
    """Test the currently configured diagnostic device on COM8"""
    print("🧪 LogSplitter Meshtastic Device Validation Test")
    print("=" * 50)
    
    port = "COM8"
    meshtastic_cmd = ".venv\\Scripts\\meshtastic.exe"
    
    tests = []
    
    # Test 1: Basic device info
    print("🔍 Test 1: Device information retrieval...")
    try:
        result = subprocess.run([
            meshtastic_cmd, '--port', port, '--info'
        ], capture_output=True, text=True, timeout=10)
        
        if result.returncode == 0:
            print("   ✅ Device responds to --info command")
            tests.append(True)
            
            # Check for LogSplitter naming
            if 'LogSplitter' in result.stdout:
                print("   ✅ LogSplitter device detected")
                tests.append(True)
                
                # Extract device name
                for line in result.stdout.split('\n'):
                    if 'Owner:' in line:
                        owner = line.split('Owner:', 1)[1].strip()
                        print(f"   📡 Device: {owner}")
                        break
            else:
                print("   ❌ Not a LogSplitter device")
                tests.append(False)
        else:
            print("   ❌ Device not responding")
            tests.append(False)
            return False
    except Exception as e:
        print(f"   ❌ Error: {e}")
        tests.append(False)
        return False
    
    # Test 2: LoRa configuration
    print("\n🔍 Test 2: LoRa configuration check...")
    try:
        lora_settings = {
            'region': 'UA_433',
            'modemPreset': 'SHORT_FAST'
        }
        
        config_ok = True
        for setting, expected in lora_settings.items():
            if f'"{setting}": "{expected}"' in result.stdout:
                print(f"   ✅ {setting}: {expected}")
            else:
                print(f"   ❌ {setting} not configured correctly")
                config_ok = False
        
        tests.append(config_ok)
    except Exception as e:
        print(f"   ❌ LoRa config error: {e}")
        tests.append(False)
    
    # Test 3: Network commands
    print("\n🔍 Test 3: Network commands test...")
    try:
        nodes_result = subprocess.run([
            meshtastic_cmd, '--port', port, '--nodes'
        ], capture_output=True, text=True, timeout=10)
        
        if nodes_result.returncode == 0:
            print("   ✅ --nodes command successful")
            
            # Count nodes in mesh
            lines = nodes_result.stdout.split('\n')
            node_count = 0
            for line in lines:
                if '│' in line and 'LogSplitter' in line and line.count('│') > 3:
                    node_count += 1
            
            print(f"   📊 Mesh nodes visible: {node_count}")
            tests.append(True)
        else:
            print("   ❌ --nodes command failed")
            tests.append(False)
    except Exception as e:
        print(f"   ❌ Network test error: {e}")
        tests.append(False)
    
    # Test 4: Message transmission
    print("\n🔍 Test 4: Message transmission test...")
    try:
        timestamp = datetime.now().strftime("%H:%M:%S")
        test_message = f"Validation test from diagnostic device - {timestamp}"
        
        send_result = subprocess.run([
            meshtastic_cmd, '--port', port, '--sendtext', test_message
        ], capture_output=True, text=True, timeout=10)
        
        if send_result.returncode == 0:
            print(f"   ✅ Test message sent: {test_message}")
            tests.append(True)
        else:
            print("   ❌ Message transmission failed")
            tests.append(False)
    except Exception as e:
        print(f"   ❌ Message test error: {e}")
        tests.append(False)
    
    # Test 5: Channel configuration
    print("\n🔍 Test 5: Channel configuration...")
    try:
        if 'Channels:' in result.stdout:
            print("   ✅ Channels configured")
            
            # Look for channel details
            channel_section = result.stdout.split('Channels:')[1] if 'Channels:' in result.stdout else ""
            if 'PRIMARY' in channel_section:
                print("   ✅ Primary channel detected")
            
            tests.append(True)
        else:
            print("   ❌ No channels configured")
            tests.append(False)
    except Exception as e:
        print(f"   ❌ Channel test error: {e}")
        tests.append(False)
    
    # Test 6: Device-specific settings (Diagnostic)
    print("\n🔍 Test 6: Diagnostic-specific settings...")
    try:
        # Check for telemetry settings
        if '"powerMeasurementEnabled": true' in result.stdout:
            print("   ✅ Power telemetry enabled")
        else:
            print("   ⚠️  Power telemetry not confirmed")
        
        # Check for rebroadcast mode
        if '"rebroadcastMode": "ALL"' in result.stdout:
            print("   ✅ Rebroadcast mode: ALL")
        else:
            print("   ⚠️  Rebroadcast mode not confirmed")
        
        # Check GPS disabled
        if '"gpsEnabled": false' in result.stdout:
            print("   ✅ GPS disabled (as configured)")
        else:
            print("   ⚠️  GPS setting not confirmed")
        
        tests.append(True)
    except Exception as e:
        print(f"   ❌ Diagnostic settings error: {e}")
        tests.append(False)
    
    # Test Summary
    print(f"\n📊 Test Summary:")
    passed = sum(tests)
    total = len(tests)
    success_rate = (passed / total) * 100 if total > 0 else 0
    
    print(f"   Tests passed: {passed}/{total} ({success_rate:.1f}%)")
    
    if success_rate >= 90:
        print("   🎉 EXCELLENT: Device fully validated and ready!")
    elif success_rate >= 80:
        print("   ✅ GOOD: Device validated with minor issues")
    elif success_rate >= 60:
        print("   ⚠️  FAIR: Device partially configured")
    else:
        print("   ❌ POOR: Device needs reconfiguration")
    
    print(f"\n🚀 Next steps:")
    print("   1. Configure Controller T-Beam (for Arduino bridge)")
    print("   2. Configure Operator T-Echo (for field operations)")
    print("   3. Run full mesh network test")
    print("   4. Connect to Arduino via pins A4/A5")
    
    return success_rate >= 80

if __name__ == "__main__":
    test_existing_device()