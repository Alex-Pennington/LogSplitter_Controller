#!/usr/bin/env python3
"""
Quick test to verify telemetry dashboard is working
"""

import requests
import sys

BASE_URL = 'http://localhost:3000'

def test_telemetry_dashboard():
    """Test that telemetry dashboard page loads"""
    try:
        print("Testing telemetry dashboard page...")
        response = requests.get(f"{BASE_URL}/telemetry", timeout=5)
        
        if response.status_code == 200:
            # Check for key elements
            content = response.text
            checks = [
                ('REAL-TIME TELEMETRY MONITOR' in content, 'Title present'),
                ('MESSAGE RATE' in content, 'Message rate panel'),
                ('MESSAGE TYPES' in content, 'Type distribution panel'),
                ('LIVE MESSAGE FEED' in content, 'Message feed panel'),
                ('connection-indicator' in content, 'Connection indicator'),
                ('/api/telemetry/stream' in content, 'SSE stream endpoint'),
            ]
            
            all_passed = True
            for check, description in checks:
                status = "✅" if check else "❌"
                print(f"  {status} {description}")
                if not check:
                    all_passed = False
            
            if all_passed:
                print("\n✅ Telemetry dashboard is fully operational!")
                print(f"   Visit: {BASE_URL}/telemetry")
                return True
            else:
                print("\n⚠️  Dashboard loaded but some elements missing")
                return False
        else:
            print(f"❌ Dashboard returned status {response.status_code}")
            return False
            
    except Exception as e:
        print(f"❌ Error: {e}")
        return False

def test_index_page():
    """Test that index page has telemetry link"""
    try:
        print("\nTesting index page telemetry link...")
        response = requests.get(BASE_URL, timeout=5)
        
        if response.status_code == 200:
            content = response.text
            if 'REAL-TIME TELEMETRY' in content or '/telemetry' in content:
                print("  ✅ Telemetry link present on index page")
                return True
            else:
                print("  ⚠️  Telemetry link not found on index page")
                return False
        else:
            print(f"  ❌ Index page returned status {response.status_code}")
            return False
            
    except Exception as e:
        print(f"  ❌ Error: {e}")
        return False

if __name__ == '__main__':
    print("=" * 70)
    print(" TELEMETRY DASHBOARD VERIFICATION TEST")
    print("=" * 70)
    
    test1 = test_telemetry_dashboard()
    test2 = test_index_page()
    
    print("\n" + "=" * 70)
    if test1 and test2:
        print("✅ ALL TESTS PASSED - Telemetry dashboard is ready!")
        print(f"\n   🌐 Dashboard URL: {BASE_URL}/telemetry")
        print("   📡 Real-time Arduino data streaming via Meshtastic mesh")
        sys.exit(0)
    else:
        print("⚠️  SOME TESTS FAILED - Check output above")
        sys.exit(1)
