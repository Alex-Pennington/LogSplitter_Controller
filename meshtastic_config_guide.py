#!/usr/bin/env python3
"""
Direct Meshtastic Configuration via Serial
===========================================

Since meshtastic CLI times out on COM13 (debug console mode),
this script provides manual configuration instructions and
verification steps.

The issue: COM13 appears to be in debug console mode, not API mode.
We need to configure the serial module through a different method.
"""

import sys

def print_configuration_guide():
    """Print comprehensive configuration guide"""
    
    print("""
╔════════════════════════════════════════════════════════════════╗
║     MESHTASTIC SERIAL MODULE CONFIGURATION GUIDE               ║
║                                                                ║
║  Problem: COM13 in debug console mode, not API mode            ║
║  Solution: Configure via Android/iOS app or web interface      ║
╚════════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════
METHOD 1: CONFIGURE VIA MOBILE APP (RECOMMENDED)
═══════════════════════════════════════════════════════════════

1. Download Meshtastic App:
   • Android: https://play.google.com/store/apps/details?id=com.geeksville.mesh
   • iOS: https://apps.apple.com/app/meshtastic/id1586432531

2. Connect to Controller Node via Bluetooth

3. Navigate to: Settings → Module Configuration → Serial

4. Configure the following settings:
   ┌─────────────────────────────────────────────┐
   │ Enabled:      ✓ ON                          │
   │ Mode:         SIMPLE (UART tunnel)          │
   │ Baud Rate:    115200                        │
   │ RX GPIO Pin:  16 (or check your device)     │
   │ TX GPIO Pin:  17 (or check your device)     │
   │ Timeout:      1000 ms                       │
   │ Echo:         OFF                           │
   └─────────────────────────────────────────────┘

5. Create 'serial' channel:
   Navigate to: Channels → Add Channel
   • Name: serial
   • Role: Secondary
   • Encryption: (use default or match primary)

6. REBOOT the device (Settings → Reboot)

7. Repeat steps 3-6 for COM8 (Diagnostic Node)

═══════════════════════════════════════════════════════════════
METHOD 2: CONFIGURE VIA WEB INTERFACE
═══════════════════════════════════════════════════════════════

1. Navigate to: https://client.meshtastic.org/

2. Click "Connect" and select Controller Node

3. Go to: Configuration → Module Config → Serial Module

4. Apply same settings as Method 1

5. Save and reboot

═══════════════════════════════════════════════════════════════
METHOD 3: PHYSICAL WIRING VERIFICATION
═══════════════════════════════════════════════════════════════

CRITICAL: Verify Arduino → Meshtastic connections:

Arduino UNO R4        Meshtastic Controller Node
┌──────────┐          ┌────────────────┐
│ A2 (TX)  │─────────→│ RX (GPIO 16)   │
│ A3 (RX)  │←─────────│ TX (GPIO 17)   │ (optional)
│ GND      │──────────│ GND            │
└──────────┘          └────────────────┘

⚠️  VOLTAGE WARNING:
   • Arduino UNO R4: 5V logic
   • Meshtastic: 3.3V logic
   • Use level shifter or voltage divider on A2 → RX!
   • Risk of damaging Meshtastic if direct 5V connection

Voltage Divider for A2 → RX:
   Arduino A2 ──[1kΩ]──┬── Meshtastic RX (GPIO 16)
                       │
                    [2kΩ]
                       │
                      GND
   (3.3V = 5V × 2kΩ/(1kΩ+2kΩ))

═══════════════════════════════════════════════════════════════
CRITICAL CONFIGURATION NOTES
═══════════════════════════════════════════════════════════════

📌 SIMPLE MODE REQUIREMENTS:
   • Must create channel named 'serial' (lowercase)
   • Both sending (COM13) and receiving (COM8) nodes need serial module enabled
   • Baud rate MUST match Arduino (115200)
   • After any config change, REBOOT the device!

📌 PIN CONFIGURATION:
   • Check YOUR device pinout (varies by model)
   • Common: RX=GPIO16, TX=GPIO17
   • RAK4631: RX=15, TX=16 (TXD1/RXD1)
   • TTGO: RX=21, TX=22

📌 WHY COM13 TIMES OUT:
   • Debug console mode shows battery/power status
   • API mode requires specific configuration
   • Some devices default to debug on USB, API on serial module
   • Configuration must be done via Bluetooth/Web, not USB serial

═══════════════════════════════════════════════════════════════
ALTERNATIVE: DIRECT MESH TEST (WITHOUT SERIAL MODULE)
═══════════════════════════════════════════════════════════════

If serial module configuration fails, alternative approach:

1. Use PROTO mode instead of SIMPLE mode
2. Arduino uses meshtastic-arduino library
3. More complex but more reliable
4. Requires firmware changes on Arduino side

Reference: https://github.com/meshtastic/Meshtastic-arduino

═══════════════════════════════════════════════════════════════
VERIFICATION AFTER CONFIGURATION
═══════════════════════════════════════════════════════════════

Run diagnostic to verify configuration worked:

   python diagnose_mesh_transport.py

Expected results after proper configuration:
   ✅ Arduino (COM7) generating telemetry
   ✅ COM13 receiving from Arduino  ← Should be FIXED
   ✅ COM8 receiving from mesh      ← Should be FIXED

═══════════════════════════════════════════════════════════════
TROUBLESHOOTING
═══════════════════════════════════════════════════════════════

If still not working after configuration:

1. Check physical connections (use multimeter)
2. Verify voltage levels (5V vs 3.3V issue)
3. Test with lower baud rate (38400)
4. Check Meshtastic firmware version (update if old)
5. Verify mesh network connectivity (--nodes command)
6. Try different GPIO pins if defaults don't work
7. Check device-specific pinout documentation

For device-specific help:
   https://meshtastic.org/docs/hardware/

═══════════════════════════════════════════════════════════════

🎯 IMMEDIATE ACTION ITEMS:

1. [ ] Connect to Controller Node via Meshtastic mobile app/web
2. [ ] Enable Serial Module in SIMPLE mode at 115200 baud
3. [ ] Configure RX/TX pins (check device pinout)
4. [ ] Create 'serial' channel on mesh network
5. [ ] REBOOT Controller Node (COM13)
6. [ ] Repeat steps 1-5 for Diagnostic Node (COM8)
7. [ ] Verify physical Arduino A2 → Meshtastic RX connection
8. [ ] Check voltage levels (5V logic → 3.3V logic protection)
9. [ ] Run: python diagnose_mesh_transport.py
10. [ ] If COM13 receiving, proceed to verify COM8

⚠️  CRITICAL: Use voltage divider or level shifter for A2 → RX!
    Direct 5V connection may damage 3.3V Meshtastic device!

═══════════════════════════════════════════════════════════════
""")

def main():
    print_configuration_guide()
    
    print("\n📱 Next Steps:")
    print("   1. Use Meshtastic mobile app or web interface to configure")
    print("   2. Follow the configuration settings above")
    print("   3. Verify physical connections with voltage protection")
    print("   4. Run diagnostic after configuration:")
    print("      python diagnose_mesh_transport.py\n")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
