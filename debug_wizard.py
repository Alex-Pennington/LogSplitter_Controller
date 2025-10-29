#!/usr/bin/env python3
"""
Debug script for wizard Meshtastic client
"""

import subprocess
import os
import serial.tools.list_ports

# Test absolute path
meshtastic_cmd = os.path.abspath(".venv/Scripts/meshtastic.exe")
print(f"Meshtastic CLI path: {meshtastic_cmd}")
print(f"File exists: {os.path.exists(meshtastic_cmd)}")

# Test COM port scanning
print("\nAvailable COM ports:")
for port in serial.tools.list_ports.comports():
    print(f"  {port.device}: {port.description}")
    
    # Test direct connection to COM8
    if port.device == "COM8":
        print(f"\nTesting connection to {port.device}...")
        try:
            result = subprocess.run([
                meshtastic_cmd, '--port', port.device, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            print(f"Return code: {result.returncode}")
            print(f"Success: {result.returncode == 0}")
            if result.stdout:
                print(f"Output length: {len(result.stdout)} chars")
                print("Contains 'Owner:':", 'Owner:' in result.stdout)
            if result.stderr:
                print(f"Error: {result.stderr[:200]}...")
                
        except Exception as e:
            print(f"Exception: {e}")