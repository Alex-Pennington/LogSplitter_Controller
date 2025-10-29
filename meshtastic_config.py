#!/usr/bin/env python3
"""
Meshtastic Configuration Tool for LogSplitter Controller Integration

This script configures Meshtastic nodes to act as serial bridges for the LogSplitter
Controller's binary telemetry system, enabling wireless mesh networking for industrial
hydraulic log splitter operations.

Usage:
    python meshtastic_config.py --setup-bridge    # Configure node as telemetry bridge
    python meshtastic_config.py --setup-receiver  # Configure node as monitoring receiver
    python meshtastic_config.py --status          # Show current configuration
    python meshtastic_config.py --test            # Test connectivity

Features:
- Automatic node detection via Meshtastic CLI
- LogSplitter-specific channel setup with industrial PSK
- Serial bridge configuration for 115200 baud telemetry
- Range optimization for industrial environments
- Network topology validation

Requirements:
    pip install meshtastic
"""

import sys
import argparse
import subprocess
import time
from datetime import datetime

class LogSplitterMeshtasticConfig:
    """Configuration manager for LogSplitter Meshtastic integration using CLI"""
    
    # LogSplitter configuration constants
    LOGSPLITTER_PSK = "LogSplitterIndustrial2025!"
    LOGSPLITTER_CHANNEL_NAME = "LogSplitter"
    
    def __init__(self):
        """Initialize configuration manager"""
        if not self.check_meshtastic_cli():
            sys.exit(1)
    
    def check_meshtastic_cli(self):
        """Check if Meshtastic CLI is available"""
        try:
            result = subprocess.run(['meshtastic', '--version'], 
                                   capture_output=True, text=True, timeout=10)
            if result.returncode == 0:
                print(f"✓ Meshtastic CLI found: {result.stdout.strip()}")
                return True
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass
        
        print("❌ Meshtastic CLI not found")
        print("   Install with: pip install meshtastic")
        print("   Or download from: https://meshtastic.org/docs/software/python/cli/")
        return False
    
    def run_cmd(self, args, timeout=30):
        """Run a meshtastic CLI command"""
        try:
            cmd = ['meshtastic'] + args
            print(f"🔧 Running: {' '.join(cmd)}")
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
            
            if result.returncode == 0:
                if result.stdout.strip():
                    print(f"✓ {result.stdout.strip()}")
                return True
            else:
                print(f"❌ Command failed: {result.stderr.strip()}")
                return False
                
        except subprocess.TimeoutExpired:
            print(f"❌ Command timed out after {timeout} seconds")
            return False
        except Exception as e:
            print(f"❌ Command error: {e}")
            return False
    
    def __init__(self, interface=None):
        """Initialize configuration manager"""
        self.interface = interface
        self.node_info = None
        
    def configure_bridge_node(self):
        """Configure node as LogSplitter telemetry bridge"""
        print("⚙️  Configuring Meshtastic node as LogSplitter telemetry bridge...")
        print("   This node will connect to Arduino A4/A5 pins")
        
        # Set device name
        print("\n📝 Setting device name...")
        if not self.run_cmd(['--set-owner', 'LogSplitter Bridge']):
            return False
        
        # Configure LoRa for long range (industrial environment)
        print("\n📡 Configuring LoRa for industrial range...")
        configs = [
            ['--set', 'lora.use_preset', 'LONG_FAST'],
            ['--set', 'lora.tx_power', '22'],
            ['--set', 'lora.hop_limit', '7'],
        ]
        
        for config in configs:
            if not self.run_cmd(config):
                print(f"⚠️  Warning: Failed to set {config[2]}")
        
        # Set up LogSplitter channel
        print("\n🔐 Configuring LogSplitter secure channel...")
        if not self.run_cmd(['--ch-set', 'psk', self.LOGSPLITTER_PSK, '--ch-index', '1']):
            return False
        
        if not self.run_cmd(['--ch-set', 'name', self.LOGSPLITTER_CHANNEL_NAME, '--ch-index', '1']):
            return False
        
        # Enable serial module for Arduino communication
        print("\n� Enabling serial bridge (115200 baud for Arduino)...")
        serial_configs = [
            ['--set', 'serial.enabled', 'true'],
            ['--set', 'serial.baud', '115200'],
            ['--set', 'serial.timeout', '1000'],
            ['--set', 'serial.echo', 'false'],
        ]
        
        for config in serial_configs:
            if not self.run_cmd(config):
                print(f"⚠️  Warning: Failed to set serial config")
        
        # Configure position reporting for range monitoring
        print("\n� Enabling position reporting for range monitoring...")
        position_configs = [
            ['--set', 'position.position_broadcast_secs', '900'],  # 15 minutes
            ['--set', 'position.gps_enabled', 'true'],
        ]
        
        for config in position_configs:
            if not self.run_cmd(config):
                print(f"⚠️  Warning: Failed to set position config")
        
        print("\n✅ Bridge node configuration complete!")
        print("   📋 Next steps:")
        print("   1. Connect Arduino A4 (TX) → Meshtastic RX pin")
        print("   2. Connect Arduino A5 (RX) → Meshtastic TX pin") 
        print("   3. Connect power and ground")
        print("   4. Configure receiver node on monitoring computer")
        
        return True
    
    def configure_receiver_node(self):
        """Configure node as monitoring station receiver"""
        print("⚙️  Configuring Meshtastic node as LogSplitter monitoring receiver...")
        print("   This node connects to monitoring computer via USB")
        
        # Set device name
        print("\n📝 Setting device name...")
        if not self.run_cmd(['--set-owner', 'LogSplitter Monitor']):
            return False
        
        # Configure LoRa (same settings as bridge for compatibility)
        print("\n� Configuring LoRa for industrial range...")
        configs = [
            ['--set', 'lora.use_preset', 'LONG_FAST'],
            ['--set', 'lora.tx_power', '22'],
            ['--set', 'lora.hop_limit', '7'],
        ]
        
        for config in configs:
            if not self.run_cmd(config):
                print(f"⚠️  Warning: Failed to set {config[2]}")
        
        # Join LogSplitter channel (same PSK as bridge)
        print("\n🔐 Joining LogSplitter secure channel...")
        if not self.run_cmd(['--ch-set', 'psk', self.LOGSPLITTER_PSK, '--ch-index', '1']):
            return False
        
        if not self.run_cmd(['--ch-set', 'name', self.LOGSPLITTER_CHANNEL_NAME, '--ch-index', '1']):
            return False
        
        # Disable serial module (USB only for this node)
        print("\n💻 Configuring for USB monitoring...")
        if not self.run_cmd(['--set', 'serial.enabled', 'false']):
            print("⚠️  Warning: Could not disable serial module")
        
        # Configure position reporting
        print("\n📍 Enabling position reporting...")
        position_configs = [
            ['--set', 'position.position_broadcast_secs', '900'],
            ['--set', 'position.gps_enabled', 'true'],
        ]
        
        for config in position_configs:
            if not self.run_cmd(config):
                print(f"⚠️  Warning: Failed to set position config")
        
        print("\n✅ Receiver node configuration complete!")
        print("   📋 Next steps:")
        print("   1. Keep this node connected via USB")
        print("   2. Use meshtastic_telemetry_receiver.py to decode telemetry")
        print("   3. Test connectivity with bridge node")
        
        return True
    
    def show_status(self):
        """Display current node configuration"""
        print("📊 LogSplitter Meshtastic Node Status")
        print("=" * 50)
        
        # Get node info
        print("\n📱 Node Information:")
        self.run_cmd(['--info'])
        
        print("\n📡 LoRa Configuration:")
        self.run_cmd(['--get', 'lora'])
        
        print("\n🔐 Channel Configuration:")
        self.run_cmd(['--ch-index', '0'])  # Primary channel
        self.run_cmd(['--ch-index', '1'])  # LogSplitter channel
        
        print("\n🔌 Serial Configuration:")
        self.run_cmd(['--get', 'serial'])
        
        print("\n📍 Position Configuration:")
        self.run_cmd(['--get', 'position'])
        
        return True
    
    def test_connectivity(self):
        """Test mesh connectivity"""
        print("🔗 Testing LogSplitter mesh connectivity...")
        
        # Send test message on LogSplitter channel
        test_message = f"LogSplitter connectivity test - {datetime.now().strftime('%H:%M:%S')}"
        print(f"📤 Sending test message: {test_message}")
        
        if self.run_cmd(['--sendtext', test_message, '--ch-index', '1']):
            print("✅ Test message sent successfully")
            print("   Monitor other nodes for message receipt")
            print("   Check with: meshtastic --nodes")
        else:
            print("❌ Failed to send test message")
            return False
        
        # Show recent nodes
        print("\n🌐 Recent mesh nodes:")
        self.run_cmd(['--nodes'])
        
        return True
    
    def reset_node(self):
        """Reset node to factory defaults"""
        print("⚠️  Resetting Meshtastic node to factory defaults...")
        
        response = input("Are you sure you want to reset this node? (yes/no): ")
        if response.lower() != 'yes':
            print("Reset cancelled")
            return False
        
        if self.run_cmd(['--factory-reset']):
            print("✅ Node reset to factory defaults")
            print("   Reconfigure as needed for LogSplitter integration")
            return True
        else:
            print("❌ Reset failed")
            return False

def main():
    """Main configuration utility"""
    parser = argparse.ArgumentParser(
        description='LogSplitter Meshtastic Configuration Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Configure bridge node (connects to Arduino)
  python meshtastic_config.py --setup-bridge
  
  # Configure receiver node (connects to computer)
  python meshtastic_config.py --setup-receiver
  
  # Check current configuration
  python meshtastic_config.py --status
  
  # Test mesh connectivity
  python meshtastic_config.py --test

Hardware Setup:
  Bridge Node:  Arduino A4→Node RX, A5→Node TX
  Monitor Node: USB connection to computer
        """
    )
    
    parser.add_argument('--setup-bridge', action='store_true', 
                       help='Configure node as telemetry bridge (connects to Arduino)')
    parser.add_argument('--setup-receiver', action='store_true',
                       help='Configure node as monitoring receiver (connects to computer)')
    parser.add_argument('--status', action='store_true',
                       help='Show current node configuration')
    parser.add_argument('--test', action='store_true',
                       help='Test mesh connectivity')
    parser.add_argument('--reset', action='store_true',
                       help='Reset node to factory defaults')
    
    args = parser.parse_args()
    
    print("🚀 LogSplitter Meshtastic Configuration Tool")
    print("=" * 50)
    
    config = LogSplitterMeshtasticConfig()
    
    if args.setup_bridge:
        return 0 if config.configure_bridge_node() else 1
    elif args.setup_receiver:
        return 0 if config.configure_receiver_node() else 1
    elif args.status:
        return 0 if config.show_status() else 1
    elif args.test:
        return 0 if config.test_connectivity() else 1
    elif args.reset:
        return 0 if config.reset_node() else 1
    else:
        parser.print_help()
        return 0

if __name__ == "__main__":
    sys.exit(main())