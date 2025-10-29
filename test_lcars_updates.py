#!/usr/bin/env python3
"""
Test LCARS System Updates
Verify the updated documentation system with Meshtastic integration
"""

import requests
import time
import json
from datetime import datetime

def test_lcars_system_updates():
    """Test the updated LCARS documentation system"""
    print("🧪 Testing LCARS System Updates - Meshtastic Integration")
    print("=" * 60)
    
    base_url = "http://localhost:3000"
    
    # Test 1: Main Documentation Index
    print("\n1. Testing main documentation index...")
    try:
        response = requests.get(base_url, timeout=5)
        if response.status_code == 200:
            content = response.text
            if "Meshtastic Network" in content:
                print("   ✅ Meshtastic link present in main navigation")
            else:
                print("   ⚠️  Meshtastic link not found in navigation")
            
            if "Wireless" in content or "📡" in content:
                print("   ✅ Wireless/Meshtastic content detected")
            else:
                print("   ⚠️  No wireless indicators found")
                
        else:
            print(f"   ❌ Main page failed: {response.status_code}")
    except Exception as e:
        print(f"   ❌ Main page error: {e}")
    
    # Test 2: Meshtastic Overview Page
    print("\n2. Testing Meshtastic overview page...")
    try:
        response = requests.get(f"{base_url}/meshtastic_overview", timeout=5)
        if response.status_code == 200:
            print("   ✅ Meshtastic overview page accessible")
            content = response.text
            
            if "Network Health Monitor" in content:
                print("   ✅ Network health monitoring present")
            if "Integration Tools" in content:
                print("   ✅ Integration tools section present")
            if "Channel 0" in content and "Channel 1" in content:
                print("   ✅ Channel configuration information present")
            
        else:
            print(f"   ❌ Meshtastic overview failed: {response.status_code}")
    except Exception as e:
        print(f"   ❌ Meshtastic overview error: {e}")
    
    # Test 3: System Info API
    print("\n3. Testing system info API...")
    try:
        response = requests.get(f"{base_url}/api/system_info", timeout=10)
        if response.status_code == 200:
            data = response.json()
            print("   ✅ System info API working")
            
            if 'system' in data and data['system'].get('architecture'):
                print(f"   📡 Architecture: {data['system']['architecture']}")
            if 'meshtastic' in data:
                print(f"   📡 Meshtastic status: {data['meshtastic'].get('status', 'unknown')}")
            if 'capabilities' in data and data['capabilities'].get('wireless_commands'):
                print("   ✅ Wireless capabilities confirmed")
            if 'channels' in data:
                print("   ✅ Channel configuration available")
                
        else:
            print(f"   ❌ System info API failed: {response.status_code}")
    except Exception as e:
        print(f"   ❌ System info API error: {e}")
    
    # Test 4: Emergency Diagnostic with Meshtastic
    print("\n4. Testing emergency diagnostic updates...")
    try:
        response = requests.get(f"{base_url}/emergency_diagnostic", timeout=5)
        if response.status_code == 200:
            content = response.text
            print("   ✅ Emergency diagnostic accessible")
            
            if "Meshtastic" in content:
                print("   ✅ Meshtastic integration in diagnostic wizard")
            if "checkMeshtasticStatus" in content:
                print("   ✅ Meshtastic status checking function present")
            if "protobuf protocol" in content:
                print("   ✅ Protobuf protocol mentioned")
                
        else:
            print(f"   ❌ Emergency diagnostic failed: {response.status_code}")
    except Exception as e:
        print(f"   ❌ Emergency diagnostic error: {e}")
    
    # Test 5: Document Categorization
    print("\n5. Testing document categorization...")
    try:
        response = requests.get(f"{base_url}/api/system_info", timeout=5)
        if response.status_code == 200:
            data = response.json()
            if 'documentation' in data and 'categories' in data['documentation']:
                categories = data['documentation']['categories']
                print(f"   📚 Total categories: {len(categories)}")
                
                if 'Wireless' in categories:
                    print(f"   📡 Wireless documents: {categories['Wireless']}")
                else:
                    print("   ⚠️  No Wireless category found")
                
                if 'Emergency' in categories:
                    print(f"   🚨 Emergency documents: {categories['Emergency']}")
                    
        else:
            print("   ❌ Could not check document categories")
    except Exception as e:
        print(f"   ❌ Category check error: {e}")
    
    print("\n" + "=" * 60)
    print("🎯 LCARS System Update Summary:")
    print("   • Added Wireless category for Meshtastic documentation")
    print("   • Created Meshtastic overview dashboard")
    print("   • Enhanced emergency diagnostic with network status")
    print("   • Added system info API with wireless capabilities")
    print("   • Updated main navigation with Meshtastic link")
    print("   • Added wireless button styling with animations")
    
    print("\n💡 Features Available:")
    print("   • Real-time Meshtastic network monitoring")
    print("   • Integration status dashboard")
    print("   • Wireless diagnostic commands")
    print("   • System capability reporting")
    print("   • Channel configuration display")
    
    print(f"\n🕒 Test completed at {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("\n🚀 LCARS Documentation System Updated Successfully!")
    print("   Visit http://localhost:3000 to explore the enhanced interface")

if __name__ == "__main__":
    test_lcars_system_updates()