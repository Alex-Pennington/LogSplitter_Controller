#!/usr/bin/env python3
"""
LogSplitter Meshtastic Configuration Wizard
Automated setup for 3-node industrial mesh network
"""

import subprocess
import time
import serial.tools.list_ports
from typing import Dict, List, Optional, Tuple
from dataclasses import dataclass
from datetime import datetime

@dataclass
class NodeConfig:
    """Configuration for a Meshtastic node"""
    name: str
    short_name: str
    role: str
    device_type: str  # "tbeam" or "techo"
    port: Optional[str] = None
    channels: List[int] = None
    gps_enabled: bool = True
    power_saving: bool = False
    serial_enabled: bool = False
    
    def __post_init__(self):
        if self.channels is None:
            self.channels = [0]  # Default to channel 0

class MeshtasticConfigWizard:
    """Automated configuration wizard for LogSplitter Meshtastic network"""
    
    def __init__(self):
        self.meshtastic_cmd = self._find_meshtastic_cli()
        self.nodes = {
            'controller': NodeConfig(
                name="LogSplitter-Controller",
                short_name="LS-CTRL", 
                role="CLIENT",
                device_type="tbeam",
                channels=[0, 1],
                gps_enabled=False,  # GPS disabled, time sync only
                serial_enabled=True
            ),
            'diagnostic': NodeConfig(
                name="LogSplitter-Diagnostics", 
                short_name="LS-DIAG",
                role="CLIENT",  # CLIENT role, not router
                device_type="tbeam", 
                channels=[0, 1],
                gps_enabled=False  # GPS disabled, time sync only
            ),
            'operator': NodeConfig(
                name="LogSplitter-Operator",
                short_name="LS-OP",
                role="CLIENT", 
                device_type="techo",
                channels=[0],  # Receive-only log traffic
                gps_enabled=False,  # GPS disabled
                power_saving=True
            )
        }
        self.config_log = []
        
    def _find_meshtastic_cli(self) -> str:
        """Find Meshtastic CLI executable"""
        import os
        # Try local venv first
        local_path = os.path.abspath(".venv/Scripts/meshtastic.exe")
        if os.path.exists(local_path):
            return local_path
        # Fall back to system path
        return "meshtastic"
    
    def _log_action(self, action: str, success: bool, details: str = ""):
        """Log configuration actions for debugging"""
        self.config_log.append({
            'timestamp': datetime.now().isoformat(),
            'action': action,
            'success': success,
            'details': details
        })
        status = "✅" if success else "❌"
        print(f"{status} {action}: {details}")
    
    def discover_nodes(self) -> Dict[str, List[str]]:
        """Discover connected Meshtastic devices"""
        print("🔍 Discovering Meshtastic devices...")
        
        available_ports = []
        for port in serial.tools.list_ports.comports():
            print(f"   Testing {port.device}: {port.description}")
            if self._test_meshtastic_connection(port.device):
                available_ports.append(port.device)
                print(f"   ✅ Found Meshtastic device on {port.device}")
        
        return {'available': available_ports, 'configured': []}
    
    def _test_meshtastic_connection(self, port: str) -> bool:
        """Test if port has responding Meshtastic device"""
        try:
            result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--info'
            ], capture_output=True, text=True, timeout=8)
            return result.returncode == 0 and ('Owner:' in result.stdout or 'MyNodeInfo' in result.stdout)
        except Exception:
            return False
    
    def assign_ports_interactive(self, available_ports: List[str]) -> bool:
        """Interactively assign ports to nodes"""
        if len(available_ports) < 3:
            print(f"❌ Need 3 devices, found {len(available_ports)}")
            return False
            
        print(f"\n📡 Found {len(available_ports)} Meshtastic devices")
        print("Please assign each device to its role:\n")
        
        for i, port in enumerate(available_ports):
            print(f"{i+1}. {port}")
            
        try:
            controller_idx = int(input("Controller T-Beam (1-3): ")) - 1
            diagnostic_idx = int(input("Diagnostic PC T-Beam (1-3): ")) - 1  
            operator_idx = int(input("Operator T-Echo (1-3): ")) - 1
            
            self.nodes['controller'].port = available_ports[controller_idx]
            self.nodes['diagnostic'].port = available_ports[diagnostic_idx]
            self.nodes['operator'].port = available_ports[operator_idx]
            
            return True
            
        except (ValueError, IndexError):
            print("❌ Invalid selection")
            return False
    
    def factory_reset_all(self) -> bool:
        """Factory reset all assigned nodes"""
        print("\n🔄 Factory resetting all devices...")
        success = True
        
        for node_name, node in self.nodes.items():
            if node.port:
                print(f"   Resetting {node.name} on {node.port}...")
                result = self._run_meshtastic_cmd(node.port, ['--factory-reset'])
                if result:
                    self._log_action(f"Factory reset {node_name}", True)
                    time.sleep(5)  # Wait for reboot
                else:
                    self._log_action(f"Factory reset {node_name}", False, "Command failed")
                    success = False
                    
        return success
    
    def configure_base_settings(self) -> bool:
        """Configure basic settings for all nodes"""
        print("\n⚙️  Configuring base settings...")
        success = True
        
        for node_name, node in self.nodes.items():
            if not node.port:
                continue
                
            print(f"   Configuring {node.name}...")
            
            # Set LoRa region (CRITICAL FIRST STEP)
            if not self._run_meshtastic_cmd(node.port, ['--set', 'lora.region', 'UA_433']):
                success = False
                continue
            
            # Set device role and owner
            commands = [
                ['--set', 'device.role', node.role],
                ['--set-owner', node.name, '--set-owner-short', node.short_name]
            ]
            
            for cmd in commands:
                if not self._run_meshtastic_cmd(node.port, cmd):
                    success = False
                    
            self._log_action(f"Base config {node_name}", success)
            
        return success
    
    def configure_channels(self) -> bool:
        """Configure mesh network channels"""
        print("\n📡 Configuring network channels...")
        success = True
        
        # Set global modem preset (applies to all channels)
        for node_name, node in self.nodes.items():
            if not node.port:
                continue
            if not self._run_meshtastic_cmd(node.port, ['--set', 'lora.modem_preset', 'SHORT_FAST']):
                success = False
        
        # Channel 0: Log Traffic (all nodes, but operator is receive-only)
        for node_name, node in self.nodes.items():
            if not node.port or 0 not in node.channels:
                continue
                
            commands = [
                ['--ch-set', 'name', 'LogSplitter-LogTraffic', '--ch-index', '0'],
                ['--ch-set', 'psk', 'CUSTOM_LOG_PSK_HERE', '--ch-index', '0']  # Replace with actual PSK
            ]
            
            for cmd in commands:
                if not self._run_meshtastic_cmd(node.port, cmd):
                    success = False
        
        # Channel 1: Binary Telemetry (controller and diagnostic only)
        telemetry_nodes = ['controller', 'diagnostic']
        for node_name in telemetry_nodes:
            node = self.nodes[node_name]
            if not node.port or 1 not in node.channels:
                continue
                
            commands = [
                ['--ch-add', 'Telemetry-Binary', '--ch-index', '1'],
                ['--ch-set', 'psk', 'CUSTOM_TELEMETRY_PSK_HERE', '--ch-index', '1']  # Replace with actual PSK
            ]
            
            for cmd in commands:
                if not self._run_meshtastic_cmd(node.port, cmd):
                    success = False
        
        self._log_action("Channel configuration", success)
        return success
    
    def configure_node_specific(self) -> bool:
        """Configure node-specific settings"""
        print("\n🔧 Configuring device-specific settings...")
        
        # Controller T-Beam (SoftwareSerial bridge to Arduino)
        controller = self.nodes['controller']
        if controller.port:
            commands = [
                # Serial bridge for Arduino telemetry
                ['--set', 'serial.enabled', 'true'],
                ['--set', 'serial.baud', 'BAUD_115200'],
                ['--set', 'serial.mode', 'TEXTMSG'],
                # GPS disabled, time sync only
                ['--set', 'position.gps_enabled', 'false'],
                # Device telemetry enabled
                ['--set', 'telemetry.device_update_interval', '300'],
                ['--set', 'telemetry.power_measurement_enabled', 'true'],
                # Node announcements every 300 seconds
                ['--set', 'device.node_info_broadcast_secs', '300'],
                # Max TX power for base station
                ['--set', 'lora.tx_power', '30']
            ]
            
            for cmd in commands:
                self._run_meshtastic_cmd(controller.port, cmd)
        
        # Diagnostic PC T-Beam (CLIENT role, not router)
        diagnostic = self.nodes['diagnostic']
        if diagnostic.port:
            commands = [
                # CLIENT role, no store & forward
                ['--set', 'device.rebroadcast_mode', 'ALL'],
                # GPS disabled, time sync only
                ['--set', 'position.gps_enabled', 'false'],
                # Device telemetry enabled
                ['--set', 'telemetry.device_update_interval', '300'],
                ['--set', 'telemetry.power_measurement_enabled', 'true'],
                # Node announcements every 300 seconds
                ['--set', 'device.node_info_broadcast_secs', '300'],
                # Max TX power for base station
                ['--set', 'lora.tx_power', '30']
            ]
            
            for cmd in commands:
                self._run_meshtastic_cmd(diagnostic.port, cmd)
        
        # Operator T-Echo (Receive-only handheld)
        operator = self.nodes['operator']
        if operator.port:
            commands = [
                # Aggressive power saving
                ['--set', 'power.is_power_saving', 'true'],
                ['--set', 'power.ls_secs', '300'],
                # GPS disabled
                ['--set', 'position.gps_enabled', 'false'],
                # Reduced TX power for battery savings
                ['--set', 'lora.tx_power', '10'],
                # Display optimization
                ['--set', 'display.screen_on_secs', '30'],
                # Device telemetry enabled but less frequent
                ['--set', 'telemetry.device_update_interval', '600'],
                ['--set', 'telemetry.power_measurement_enabled', 'true'],
                # Node announcements every 300 seconds
                ['--set', 'device.node_info_broadcast_secs', '300']
            ]
            
            for cmd in commands:
                self._run_meshtastic_cmd(operator.port, cmd)
        
        self._log_action("Node-specific configuration", True)
        return True
    
    def validate_network(self) -> Dict[str, bool]:
        """Validate network configuration and connectivity"""
        print("\n✅ Validating network configuration...")
        
        results = {}
        for node_name, node in self.nodes.items():
            if not node.port:
                results[node_name] = False
                continue
                
            # Test basic connectivity
            info_result = self._run_meshtastic_cmd(node.port, ['--info'], capture=True)
            if info_result and node.name in info_result.get('stdout', ''):
                results[node_name] = True
                print(f"   ✅ {node.name}: Configuration valid")
            else:
                results[node_name] = False
                print(f"   ❌ {node.name}: Configuration failed")
        
        return results
    
    def _run_meshtastic_cmd(self, port: str, args: List[str], capture: bool = False) -> bool:
        """Run Meshtastic CLI command"""
        try:
            cmd = [self.meshtastic_cmd, '--port', port] + args
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
            
            if capture:
                return {
                    'success': result.returncode == 0,
                    'stdout': result.stdout,
                    'stderr': result.stderr
                }
            
            return result.returncode == 0
            
        except Exception as e:
            if capture:
                return {'success': False, 'error': str(e)}
            return False
    
    def run_full_setup(self) -> bool:
        """Run complete network setup wizard"""
        print("🚀 LogSplitter Meshtastic Network Setup Wizard")
        print("=" * 50)
        
        # Step 1: Discover devices
        discovered = self.discover_nodes()
        if not discovered['available']:
            print("❌ No Meshtastic devices found")
            return False
        
        # Step 2: Assign ports
        if not self.assign_ports_interactive(discovered['available']):
            print("❌ Port assignment failed")
            return False
        
        # Step 3: Factory reset
        if not self.factory_reset_all():
            print("❌ Factory reset failed")
            return False
        
        # Wait for devices to reboot
        print("⏳ Waiting for devices to reboot...")
        time.sleep(10)
        
        # Step 4: Base configuration
        if not self.configure_base_settings():
            print("❌ Base configuration failed")  
            return False
        
        # Step 5: Channel setup
        if not self.configure_channels():
            print("❌ Channel configuration failed")
            return False
        
        # Step 6: Node-specific settings
        if not self.configure_node_specific():
            print("❌ Node configuration failed")
            return False
            
        # Step 7: Validate setup
        validation_results = self.validate_network()
        
        all_success = all(validation_results.values())
        if all_success:
            print("\n🎉 Network setup completed successfully!")
            self._print_network_summary()
        else:
            print("\n⚠️  Setup completed with errors")
            
        return all_success
    
    def _print_network_summary(self):
        """Print network configuration summary"""
        print("\n📊 Network Configuration Summary:")
        print("-" * 40)
        
        for node_name, node in self.nodes.items():
            if node.port:
                print(f"🔸 {node.name}")
                print(f"   Port: {node.port}")
                print(f"   Role: {node.role}")
                print(f"   Channels: {', '.join(map(str, node.channels))}")
                print()
        
        print("🔗 Network Channels:")
        print("   Channel 0: LogSplitter-Ops (SHORT_FAST)")
        print("   Channel 1: Telemetry-Binary (LONG_FAST)")
        print()
        print("📖 See MESHTASTIC_MAINTENANCE_PLAN.md for detailed procedures")

def main():
    """Main configuration wizard entry point"""
    wizard = MeshtasticConfigWizard()
    
    try:
        success = wizard.run_full_setup()
        exit_code = 0 if success else 1
        
        # Save configuration log
        import json
        with open('meshtastic_config_log.json', 'w') as f:
            json.dump(wizard.config_log, f, indent=2)
            
        print(f"\n📝 Configuration log saved to: meshtastic_config_log.json")
        return exit_code
        
    except KeyboardInterrupt:
        print("\n⏹️  Setup cancelled by user")
        return 1
    except Exception as e:
        print(f"\n💥 Setup failed with error: {e}")
        return 1

if __name__ == "__main__":
    exit(main())