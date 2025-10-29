#!/usr/bin/env python3
"""
LogSplitter Meshtastic Node Provisioning System

Complete automated system for configuring fresh Meshtastic nodes for LogSplitter
Controller integration. Handles everything from factory reset to full deployment
configuration with validation and hardware debugging.

Features:
- Fresh node detection and factory reset
- Complete LogSplitter configuration deployment  
- Serial Module protobuf configuration
- Channel setup with encryption
- Hardware validation and debugging
- Persistent configuration backup/restore
- Deployment verification and testing

Usage:
    python logsplitter_node_provisioning.py --provision-controller COM3
    python logsplitter_node_provisioning.py --provision-base COM4
    python logsplitter_node_provisioning.py --factory-reset COM3
    python logsplitter_node_provisioning.py --validate-all
"""

import sys
import time
import json
import hashlib
import argparse
import subprocess
from datetime import datetime
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import serial
import serial.tools.list_ports

from logsplitter_meshtastic_protocol import (
    MeshtasticChannel, 
    CHANNEL_CONFIG, 
    SERIAL_MODULE_CONFIG,
    LogSplitterProtocol
)

class NodeType:
    """Node configuration types"""
    CONTROLLER = "controller"  # Arduino-attached node at log splitter
    BASE = "base"             # PC-attached node at monitoring station
    RELAY = "relay"           # Mesh relay nodes for range extension

class LogSplitterNodeProvisioner:
    """Complete Meshtastic node provisioning system"""
    
    def __init__(self):
        self.config_dir = Path("meshtastic_configs")
        self.config_dir.mkdir(exist_ok=True)
        
        # Standard LogSplitter configuration
        self.logsplitter_config = {
            "network": {
                "region": "US",  # Default, can be overridden
                "channel_num": 20,  # Default LoRa channel
                "tx_power": 30,     # Max power for industrial range
                "bandwidth": 125,   # Standard bandwidth
                "spread_factor": 12, # Max range configuration
                "coding_rate": 8,   # Error correction
            },
            "channels": {
                0: {  # Emergency channel
                    "name": "Emergency",
                    "psk": None,  # Open channel
                    "downlink_enabled": True,
                    "uplink_enabled": True,
                },
                1: {  # LogSplitter secure channel
                    "name": "LogSplitter",
                    "psk": "AQIDBAUGBwgJCgsMDQ4PEBESExQVFhcYGRobHB0eHw==",  # Industrial PSK
                    "downlink_enabled": True,
                    "uplink_enabled": True,
                }
            },
            "modules": {
                "serial": SERIAL_MODULE_CONFIG,
                "telemetry": {
                    "device_update_interval": 30,
                    "environment_update_interval": 60,
                },
                "range_test": {
                    "enabled": True,
                    "sender": 30,  # Send interval for controller nodes
                    "save": True   # Save results
                },
                "detection_sensor": {
                    "enabled": False  # Disable to save power
                },
                "paxcounter": {
                    "enabled": False  # Disable to save power  
                }
            },
            "power": {
                "wait_bluetooth_secs": 60,    # Reduced for faster boot
                "sds_secs": 31536000,         # Long sleep when not used
                "ls_secs": 300,               # Light sleep interval
                "min_wake_secs": 10,          # Minimum wake time
            },
            "device": {
                "role": "ROUTER_CLIENT",      # Default role
                "serial_enabled": True,       # Enable serial interface
                "debug_log_enabled": False,   # Disable for production
                "factory_reset": False,       # Don't auto-reset
            }
        }
    
    def detect_nodes(self) -> List[Tuple[str, Dict]]:
        """Detect all connected Meshtastic nodes"""
        print("🔍 Detecting Meshtastic nodes...")
        nodes = []
        
        for port in serial.tools.list_ports.comports():
            # Test if port has Meshtastic device
            if self.test_meshtastic_port(port.device):
                node_info = self.get_node_info(port.device)
                nodes.append((port.device, node_info))
                print(f"✓ Found node: {port.device} - {node_info.get('name', 'Unknown')}")
        
        print(f"📊 Total nodes detected: {len(nodes)}")
        return nodes
    
    def test_meshtastic_port(self, port: str) -> bool:
        """Test if port has responding Meshtastic device"""
        try:
            result = subprocess.run([
                'meshtastic', '--port', port, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            return (result.returncode == 0 and 
                   ('MyNodeInfo' in result.stdout or 'Node' in result.stdout))
        except:
            return False
    
    def get_node_info(self, port: str) -> Dict:
        """Get comprehensive node information"""
        info: Dict = {'port': port}
        
        try:
            # Get basic info
            result = subprocess.run([
                'meshtastic', '--port', port, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                lines = result.stdout.split('\n')
                for line in lines:
                    if 'Owner:' in line:
                        info['owner'] = line.split('Owner:')[1].strip()
                    elif 'Model:' in line:
                        info['model'] = line.split('Model:')[1].strip()
                    elif 'Firmware:' in line:
                        info['firmware'] = line.split('Firmware:')[1].strip()
                    elif 'Battery:' in line:
                        info['battery'] = line.split('Battery:')[1].strip()
                    elif 'MyNodeInfo:' in line:
                        info['node_id'] = line.split('MyNodeInfo:')[1].strip()
            
            # Get configuration status
            config_result = subprocess.run([
                'meshtastic', '--port', port, '--get', 'lora'
            ], capture_output=True, text=True, timeout=10)
            
            if config_result.returncode == 0:
                info['configured'] = 'LogSplitter' in config_result.stdout
            else:
                info['configured'] = False
                
        except Exception as e:
            info['error'] = str(e)
        
        return info
    
    def factory_reset_node(self, port: str) -> bool:
        """Factory reset Meshtastic node"""
        print(f"🔄 Factory resetting node on {port}...")
        
        try:
            # Factory reset
            result = subprocess.run([
                'meshtastic', '--port', port, '--factory-reset'
            ], capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                print(f"❌ Factory reset failed: {result.stderr}")
                return False
            
            print("✓ Factory reset complete, waiting for reboot...")
            time.sleep(10)  # Wait for reboot
            
            # Verify reset worked
            info_result = subprocess.run([
                'meshtastic', '--port', port, '--info'
            ], capture_output=True, text=True, timeout=15)
            
            if info_result.returncode == 0:
                print("✓ Node is responsive after reset")
                return True
            else:
                print("⚠️  Node may still be rebooting...")
                return False
                
        except Exception as e:
            print(f"❌ Factory reset error: {e}")
            return False
    
    def configure_base_settings(self, port: str, node_type: str) -> bool:
        """Configure base Meshtastic settings"""
        print(f"⚙️  Configuring base settings for {node_type} node...")
        
        config = self.logsplitter_config
        
        try:
            # Set region and basic LoRa settings
            commands = [
                ['--set', 'lora.region', config['network']['region']],
                ['--set', 'lora.tx_power', str(config['network']['tx_power'])],
                ['--set', 'lora.bandwidth', str(config['network']['bandwidth'])],
                ['--set', 'lora.spread_factor', str(config['network']['spread_factor'])],
                ['--set', 'lora.coding_rate', str(config['network']['coding_rate'])],
                
                # Device settings based on node type
                ['--set', 'device.role', self.get_node_role(node_type)],
                ['--set', 'device.serial_enabled', 'true'],
                ['--set', 'device.debug_log_enabled', 'false'],
                
                # Power management
                ['--set', 'power.wait_bluetooth_secs', str(config['power']['wait_bluetooth_secs'])],
                ['--set', 'power.ls_secs', str(config['power']['ls_secs'])],
                ['--set', 'power.min_wake_secs', str(config['power']['min_wake_secs'])],
                
                # Set owner name based on node type
                ['--set-owner', f'LogSplitter-{node_type.upper()}']
            ]
            
            for cmd in commands:
                result = subprocess.run(
                    ['meshtastic', '--port', port] + cmd,
                    capture_output=True, text=True, timeout=10
                )
                
                if result.returncode != 0:
                    print(f"❌ Command failed: {' '.join(cmd)}")
                    print(f"   Error: {result.stderr}")
                    return False
                
                time.sleep(0.5)  # Small delay between commands
            
            print("✓ Base settings configured successfully")
            return True
            
        except Exception as e:
            print(f"❌ Base configuration error: {e}")
            return False
    
    def configure_channels(self, port: str) -> bool:
        """Configure LogSplitter channels"""
        print("🔐 Configuring LogSplitter channels...")
        
        try:
            # Configure Channel 0 (Emergency - open)
            emergency_commands = [
                ['--ch-set', 'name', 'Emergency', '--ch-index', '0'],
                ['--ch-set', 'psk', 'none', '--ch-index', '0'],  # Open channel
                ['--ch-enable', '--ch-index', '0']
            ]
            
            # Configure Channel 1 (LogSplitter - encrypted)
            logsplitter_commands = [
                ['--ch-add', 'LogSplitter'],  # Add new channel
                ['--ch-set', 'name', 'LogSplitter', '--ch-index', '1'],
                ['--ch-set', 'psk', self.logsplitter_config['channels'][1]['psk'], '--ch-index', '1'],
                ['--ch-enable', '--ch-index', '1']
            ]
            
            all_commands = emergency_commands + logsplitter_commands
            
            for cmd in all_commands:
                result = subprocess.run(
                    ['meshtastic', '--port', port] + cmd,
                    capture_output=True, text=True, timeout=10
                )
                
                if result.returncode != 0:
                    print(f"⚠️  Channel command warning: {' '.join(cmd)}")
                    print(f"   Output: {result.stderr}")
                    # Continue anyway - some commands may fail if channels exist
                
                time.sleep(0.5)
            
            print("✓ Channels configured successfully")
            return True
            
        except Exception as e:
            print(f"❌ Channel configuration error: {e}")
            return False
    
    def configure_serial_module(self, port: str, node_type: str) -> bool:
        """Configure Serial Module for protobuf communication"""
        print("📡 Configuring Serial Module for protobuf...")
        
        # Only enable serial module for controller nodes (Arduino-connected)
        if node_type != NodeType.CONTROLLER:
            print("ℹ️  Skipping Serial Module for non-controller node")
            return True
        
        try:
            serial_config = SERIAL_MODULE_CONFIG
            
            commands = [
                ['--set', 'serial.enabled', 'true'],
                ['--set', 'serial.baud', str(serial_config['baud'])],
                ['--set', 'serial.timeout', str(serial_config['timeout'])],
                ['--set', 'serial.mode', 'PROTO'],  # Key setting!
                ['--set', 'serial.echo', 'false'],
            ]
            
            for cmd in commands:
                result = subprocess.run(
                    ['meshtastic', '--port', port] + cmd,
                    capture_output=True, text=True, timeout=10
                )
                
                if result.returncode != 0:
                    print(f"❌ Serial command failed: {' '.join(cmd)}")
                    print(f"   Error: {result.stderr}")
                    return False
                
                time.sleep(0.5)
            
            print("✓ Serial Module configured for protobuf communication")
            return True
            
        except Exception as e:
            print(f"❌ Serial Module configuration error: {e}")
            return False
    
    def configure_modules(self, port: str, node_type: str) -> bool:
        """Configure additional modules"""
        print("🔧 Configuring additional modules...")
        
        try:
            # Telemetry module
            telemetry_commands = [
                ['--set', 'telemetry.device_update_interval', '30'],
                ['--set', 'telemetry.environment_update_interval', '60'],
            ]
            
            # Range test module (enable for testing)
            range_test_commands = [
                ['--set', 'range_test.enabled', 'true'],
                ['--set', 'range_test.sender', '30'],
                ['--set', 'range_test.save', 'true'],
            ]
            
            # Disable unused modules to save power
            disable_commands = [
                ['--set', 'detection_sensor.enabled', 'false'],
                ['--set', 'paxcounter.enabled', 'false'],
            ]
            
            all_commands = telemetry_commands + range_test_commands + disable_commands
            
            for cmd in all_commands:
                result = subprocess.run(
                    ['meshtastic', '--port', port] + cmd,
                    capture_output=True, text=True, timeout=10
                )
                
                if result.returncode != 0:
                    print(f"⚠️  Module command warning: {' '.join(cmd)}")
                    # Continue - some modules may not be available on all devices
                
                time.sleep(0.5)
            
            print("✓ Additional modules configured")
            return True
            
        except Exception as e:
            print(f"❌ Module configuration error: {e}")
            return False
    
    def get_node_role(self, node_type: str) -> str:
        """Get appropriate node role for type"""
        role_map = {
            NodeType.CONTROLLER: "ROUTER_CLIENT",  # Arduino-connected
            NodeType.BASE: "CLIENT",               # PC-connected
            NodeType.RELAY: "ROUTER"               # Mesh relay
        }
        return role_map.get(node_type, "ROUTER_CLIENT")
    
    def validate_configuration(self, port: str, node_type: str) -> bool:
        """Validate complete node configuration"""
        print(f"✅ Validating {node_type} node configuration...")
        
        validation_tests = [
            ("Basic Info", self.validate_basic_info),
            ("LoRa Settings", self.validate_lora_settings),
            ("Channels", self.validate_channels),
            ("Modules", lambda p, t: self.validate_modules(p, t)),
        ]
        
        if node_type == NodeType.CONTROLLER:
            validation_tests.append(("Serial Module", self.validate_serial_module))
        
        all_passed = True
        
        for test_name, test_func in validation_tests:
            print(f"  🔍 {test_name}...", end=" ")
            try:
                if test_func(port, node_type):
                    print("✓ PASS")
                else:
                    print("❌ FAIL")
                    all_passed = False
            except Exception as e:
                print(f"❌ ERROR: {e}")
                all_passed = False
        
        if all_passed:
            print(f"🎉 {node_type} node validation PASSED")
        else:
            print(f"⚠️  {node_type} node validation FAILED")
        
        return all_passed
    
    def validate_basic_info(self, port: str, node_type: str) -> bool:
        """Validate basic node information"""
        result = subprocess.run([
            'meshtastic', '--port', port, '--info'
        ], capture_output=True, text=True, timeout=10)
        
        return (result.returncode == 0 and 
               'LogSplitter' in result.stdout and
               node_type.upper() in result.stdout)
    
    def validate_lora_settings(self, port: str, node_type: str) -> bool:
        """Validate LoRa configuration"""
        result = subprocess.run([
            'meshtastic', '--port', port, '--get', 'lora'
        ], capture_output=True, text=True, timeout=10)
        
        if result.returncode != 0:
            return False
        
        config = self.logsplitter_config['network']
        required_settings = [
            f"region: {config['region']}",
            f"tx_power: {config['tx_power']}",
        ]
        
        for setting in required_settings:
            if setting not in result.stdout:
                return False
        
        return True
    
    def validate_channels(self, port: str, node_type: str) -> bool:
        """Validate channel configuration"""
        result = subprocess.run([
            'meshtastic', '--port', port, '--ch-index', '1'
        ], capture_output=True, text=True, timeout=10)
        
        return (result.returncode == 0 and 
               'LogSplitter' in result.stdout)
    
    def validate_modules(self, port: str, node_type: str) -> bool:
        """Validate module configuration"""
        result = subprocess.run([
            'meshtastic', '--port', port, '--get', 'telemetry'
        ], capture_output=True, text=True, timeout=10)
        
        return result.returncode == 0  # Basic check that modules respond
    
    def validate_serial_module(self, port: str, node_type: str) -> bool:
        """Validate Serial Module protobuf configuration"""
        result = subprocess.run([
            'meshtastic', '--port', port, '--get', 'serial'
        ], capture_output=True, text=True, timeout=10)
        
        if result.returncode != 0:
            return False
        
        required_settings = [
            "enabled: true",
            "mode: PROTO",
            "baud: 115200"
        ]
        
        for setting in required_settings:
            if setting not in result.stdout:
                return False
        
        return True
    
    def save_configuration_backup(self, port: str, node_type: str) -> bool:
        """Save complete node configuration backup"""
        print(f"💾 Saving configuration backup for {node_type} node...")
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_file = self.config_dir / f"{node_type}_config_{timestamp}.json"
        
        try:
            # Get complete configuration
            config_data = {
                'timestamp': timestamp,
                'node_type': node_type,
                'port': port,
                'configurations': {}
            }
            
            # Get various configuration sections
            config_sections = ['lora', 'device', 'channels', 'serial', 'telemetry']
            
            for section in config_sections:
                result = subprocess.run([
                    'meshtastic', '--port', port, '--get', section
                ], capture_output=True, text=True, timeout=10)
                
                if result.returncode == 0:
                    config_data['configurations'][section] = result.stdout
            
            # Save to file
            with open(backup_file, 'w') as f:
                json.dump(config_data, f, indent=2)
            
            print(f"✓ Configuration backup saved: {backup_file}")
            return True
            
        except Exception as e:
            print(f"❌ Backup error: {e}")
            return False
    
    def provision_node(self, port: str, node_type: str, factory_reset: bool = True) -> bool:
        """Complete node provisioning workflow"""
        print(f"\n🚀 Starting {node_type} node provisioning on {port}")
        print("=" * 60)
        
        steps = [
            ("Factory Reset", lambda: self.factory_reset_node(port) if factory_reset else True),
            ("Base Settings", lambda: self.configure_base_settings(port, node_type)),
            ("Channels", lambda: self.configure_channels(port)),
            ("Serial Module", lambda: self.configure_serial_module(port, node_type)),
            ("Additional Modules", lambda: self.configure_modules(port, node_type)),
            ("Validation", lambda: self.validate_configuration(port, node_type)),
            ("Backup", lambda: self.save_configuration_backup(port, node_type)),
        ]
        
        for step_name, step_func in steps:
            print(f"\n📋 Step: {step_name}")
            if not step_func():
                print(f"❌ Provisioning FAILED at step: {step_name}")
                return False
            time.sleep(1)  # Brief pause between steps
        
        print(f"\n🎉 {node_type} node provisioning COMPLETED successfully!")
        print("✓ Node is ready for LogSplitter deployment")
        return True

def main():
    """Main application entry point"""
    parser = argparse.ArgumentParser(
        description='LogSplitter Meshtastic Node Provisioning System',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Provision controller node (Arduino-connected)
  python logsplitter_node_provisioning.py --provision-controller COM3
  
  # Provision base station node (PC-connected)
  python logsplitter_node_provisioning.py --provision-base COM4
  
  # Factory reset a node
  python logsplitter_node_provisioning.py --factory-reset COM3
  
  # Detect and validate all nodes
  python logsplitter_node_provisioning.py --validate-all
        """
    )
    
    parser.add_argument('--provision-controller', metavar='PORT',
                       help='Provision controller node on specified port')
    parser.add_argument('--provision-base', metavar='PORT',
                       help='Provision base station node on specified port')
    parser.add_argument('--factory-reset', metavar='PORT',
                       help='Factory reset node on specified port')
    parser.add_argument('--validate-all', action='store_true',
                       help='Detect and validate all connected nodes')
    parser.add_argument('--no-factory-reset', action='store_true',
                       help='Skip factory reset step')
    
    args = parser.parse_args()
    
    provisioner = LogSplitterNodeProvisioner()
    
    try:
        if args.provision_controller:
            success = provisioner.provision_node(
                args.provision_controller, 
                NodeType.CONTROLLER,
                not args.no_factory_reset
            )
            return 0 if success else 1
        
        elif args.provision_base:
            success = provisioner.provision_node(
                args.provision_base, 
                NodeType.BASE,
                not args.no_factory_reset
            )
            return 0 if success else 1
        
        elif args.factory_reset:
            success = provisioner.factory_reset_node(args.factory_reset)
            return 0 if success else 1
        
        elif args.validate_all:
            nodes = provisioner.detect_nodes()
            if not nodes:
                print("❌ No Meshtastic nodes detected")
                return 1
            
            all_valid = True
            for port, info in nodes:
                node_type = NodeType.BASE  # Default for validation
                if not provisioner.validate_configuration(port, node_type):
                    all_valid = False
            
            if all_valid:
                print("\n🎉 All nodes validated successfully!")
                return 0
            else:
                print("\n⚠️  Some nodes failed validation")
                return 1
        
        else:
            parser.print_help()
            return 1
    
    except KeyboardInterrupt:
        print("\n⏹️  Provisioning interrupted by user")
        return 1
    except Exception as e:
        print(f"\n❌ Provisioning error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())