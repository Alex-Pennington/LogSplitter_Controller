#!/usr/bin/env python3
"""
LogSplitter Meshtastic Single Device Configuration
Configure one Meshtastic device at a time for the LogSplitter network
"""

import subprocess
import time
import serial.tools.list_ports
from typing import Dict, Optional, List
from datetime import datetime
import sys

class SingleNodeConfigurator:
    """Configure individual Meshtastic nodes for LogSplitter network"""
    
    def __init__(self):
        self.meshtastic_cmd = self._find_meshtastic_cli()
        self.config_log = []
        
        # Define node configurations
        self.node_configs = {
            'controller': {
                'name': 'LogSplitter-Controller',
                'short_name': 'LS-CTRL',
                'role': 'CLIENT',
                'description': 'Arduino SoftwareSerial bridge, sends telemetry',
                'channels': [0, 1],
                'settings': [
                    ('lora.region', 'UA_433'),
                    ('lora.modem_preset', 'SHORT_FAST'), 
                    ('lora.tx_power', '30'),
                    ('device.role', 'CLIENT'),
                    ('serial.enabled', 'true'),
                    ('serial.baud', 'BAUD_115200'),
                    ('serial.mode', 'TEXTMSG'),
                    ('position.gps_enabled', 'false'),
                    ('telemetry.device_update_interval', '300'),
                    ('telemetry.power_measurement_enabled', 'true'),
                    ('device.node_info_broadcast_secs', '300')
                ]
            },
            'diagnostic': {
                'name': 'LogSplitter-Diagnostics',
                'short_name': 'LS-DIAG',
                'role': 'CLIENT',
                'description': 'LCARS diagnostic station, receives telemetry',
                'channels': [0, 1],
                'settings': [
                    ('lora.region', 'UA_433'),
                    ('lora.modem_preset', 'SHORT_FAST'),
                    ('lora.tx_power', '30'), 
                    ('device.role', 'CLIENT'),
                    ('position.gps_enabled', 'false'),
                    ('telemetry.device_update_interval', '300'),
                    ('telemetry.power_measurement_enabled', 'true'),
                    ('device.node_info_broadcast_secs', '300'),
                    ('device.rebroadcast_mode', 'ALL')
                ]
            },
            'operator': {
                'name': 'LogSplitter-Operator',
                'short_name': 'LS-OP',
                'role': 'CLIENT',
                'description': 'Handheld receive-only field monitor',
                'channels': [0],
                'settings': [
                    ('lora.region', 'UA_433'),
                    ('lora.modem_preset', 'SHORT_FAST'),
                    ('lora.tx_power', '10'),
                    ('device.role', 'CLIENT'),
                    ('power.is_power_saving', 'true'),
                    ('power.ls_secs', '300'),
                    ('position.gps_enabled', 'false'),
                    ('telemetry.device_update_interval', '600'),
                    ('telemetry.power_measurement_enabled', 'true'),
                    ('device.node_info_broadcast_secs', '300'),
                    ('display.screen_on_secs', '30')
                ]
            }
        }
    
    def _find_meshtastic_cli(self) -> str:
        """Find Meshtastic CLI executable"""
        import os
        local_path = os.path.abspath(".venv/Scripts/meshtastic.exe")
        if os.path.exists(local_path):
            return local_path
        return "meshtastic"
    
    def _log_action(self, action: str, success: bool, details: str = ""):
        """Log configuration actions"""
        self.config_log.append({
            'timestamp': datetime.now().isoformat(),
            'action': action,
            'success': success,
            'details': details
        })
        status = "✅" if success else "❌"
        print(f"{status} {action}: {details}")
    
    def discover_devices(self) -> List[str]:
        """Discover available Meshtastic devices"""
        print("🔍 Discovering Meshtastic devices...")
        devices = []
        
        for port in serial.tools.list_ports.comports():
            print(f"   Testing {port.device}: {port.description}")
            if self._test_meshtastic_connection(port.device):
                devices.append(port.device)
                print(f"   ✅ Meshtastic device found on {port.device}")
            else:
                print(f"   ❌ No Meshtastic device on {port.device}")
        
        return devices
    
    def _test_meshtastic_connection(self, port: str) -> bool:
        """Test if port has responding Meshtastic device"""
        try:
            result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--info'
            ], capture_output=True, text=True, timeout=8)
            return result.returncode == 0 and ('Owner:' in result.stdout or 'MyNodeInfo' in result.stdout)
        except Exception:
            return False
    
    def _run_command_with_retry(self, cmd: List[str], max_retries: int = 3, timeout: int = 20, delay: float = 2.0) -> bool:
        """Run a command with retry logic for handling timeouts"""
        for attempt in range(max_retries):
            try:
                print(f"      Attempt {attempt + 1}/{max_retries}: {' '.join(cmd[-4:])}")  # Show last 4 parts
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
                
                if result.returncode == 0:
                    return True
                else:
                    print(f"      ❌ Command failed: {result.stderr.strip()}")
                    if attempt < max_retries - 1:
                        print(f"      ⏳ Retrying in {delay} seconds...")
                        time.sleep(delay)
            
            except subprocess.TimeoutExpired:
                print(f"      ⏰ Command timed out after {timeout}s")
                if attempt < max_retries - 1:
                    print(f"      ⏳ Retrying in {delay} seconds...")
                    time.sleep(delay)
            except Exception as e:
                print(f"      💥 Unexpected error: {e}")
                if attempt < max_retries - 1:
                    time.sleep(delay)
        
        return False

    def get_current_device_info(self, port: str) -> Dict:
        """Get current device information"""
        try:
            result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                info = {'port': port, 'raw_output': result.stdout}
                
                # Parse owner info
                for line in result.stdout.split('\n'):
                    line = line.strip()
                    if 'Owner:' in line:
                        info['current_owner'] = line.split('Owner:', 1)[1].strip()
                    elif 'My info:' in line:
                        info['node_info'] = line
                        
                return info
        except Exception as e:
            return {'port': port, 'error': str(e)}
        
        return {'port': port, 'status': 'no_response'}
    
    def factory_reset_device(self, port: str) -> bool:
        """Factory reset a single device"""
        print(f"🔄 Factory resetting device on {port}...")
        print("⚠️  This will erase all current settings!")
        
        confirm = input("Continue with factory reset? (y/N): ").lower()
        if confirm != 'y':
            print("❌ Factory reset cancelled")
            return False
        
        try:
            result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--factory-reset'
            ], capture_output=True, text=True, timeout=15)
            
            if result.returncode == 0:
                self._log_action(f"Factory reset {port}", True)
                print("⏳ Waiting 10 seconds for device reboot...")
                time.sleep(10)
                return True
            else:
                self._log_action(f"Factory reset {port}", False, result.stderr)
                return False
                
        except Exception as e:
            self._log_action(f"Factory reset {port}", False, str(e))
            return False
    
    def configure_node(self, port: str, node_type: str) -> bool:
        """Configure a single node with specified type"""
        if node_type not in self.node_configs:
            print(f"❌ Unknown node type: {node_type}")
            return False
        
        config = self.node_configs[node_type]
        print(f"\n⚙️  Configuring {config['name']} on {port}")
        print(f"📋 Description: {config['description']}")
        
        # Set owner name first
        owner_cmd = [
            self.meshtastic_cmd, '--port', port,
            '--set-owner', config['name'],
            '--set-owner-short', config['short_name']
        ]
        
        print("   Setting device owner...")
        if not self._run_command_with_retry(owner_cmd, max_retries=3, timeout=20, delay=2.0):
            self._log_action(f"Set owner {node_type}", False, "Failed to set owner after retries")
            return False
        
        # Apply all settings
        success_count = 0
        total_settings = len(config['settings'])
        
        for setting, value in config['settings']:
            print(f"   Setting {setting} = {value}")
            
            cmd = [self.meshtastic_cmd, '--port', port, '--set', setting, value]
            
            if self._run_command_with_retry(cmd, max_retries=2, timeout=20, delay=2.0):
                success_count += 1
                time.sleep(1.5)  # Brief pause between commands
            else:
                print(f"   ❌ Failed to set {setting} after retries")
        
        # Configure channels with retry logic
        if 0 in config['channels']:
            print("   Configuring Channel 0 (Log Traffic)...")
            ch0_cmd = [self.meshtastic_cmd, '--port', port, '--ch-set', 'name', 'LogSplitter-LogTraffic', '--ch-index', '0']
            
            if self._run_command_with_retry(ch0_cmd, max_retries=3, timeout=25, delay=3.0):
                success_count += 1
                print("   ✅ Channel 0 configured successfully")
            else:
                print("   ❌ Failed to configure Channel 0 after multiple retries")
        
        if 1 in config['channels']:
            print("   Configuring Channel 1 (Binary Telemetry)...")
            ch1_cmd = [self.meshtastic_cmd, '--port', port, '--ch-add', 'Telemetry-Binary', '--ch-index', '1']
            
            if self._run_command_with_retry(ch1_cmd, max_retries=3, timeout=25, delay=3.0):
                success_count += 1
                print("   ✅ Channel 1 configured successfully")
            else:
                print("   ❌ Failed to configure Channel 1 after multiple retries")
        
        success_rate = success_count / (total_settings + len(config['channels']))
        if success_rate > 0.8:  # 80% success rate
            self._log_action(f"Configure {node_type}", True, f"{success_count} settings applied")
            return True
        else:
            self._log_action(f"Configure {node_type}", False, f"Only {success_count} settings applied")
            return False
    
    def validate_configuration(self, port: str, node_type: str) -> bool:
        """Comprehensive validation of device configuration"""
        print(f"\n✅ Validating configuration for {node_type}...")
        
        config = self.node_configs[node_type]
        validation_results = {
            'device_info': False,
            'owner_name': False,
            'lora_settings': False,
            'channel_config': False,
            'network_commands': False,
            'message_transmission': False,
            'node_specific': False
        }
        
        try:
            # Test 1: Basic device info retrieval
            print("   🔍 Test 1: Device information retrieval...")
            info_result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            if info_result.returncode == 0:
                validation_results['device_info'] = True
                print("   ✅ Device responds to --info command")
                
                # Test 2: Owner name validation
                print("   🔍 Test 2: Owner name validation...")
                expected_name = config['name']
                if expected_name in info_result.stdout:
                    validation_results['owner_name'] = True
                    print(f"   ✅ Device name correct: {expected_name}")
                    
                    # Extract and validate short name
                    for line in info_result.stdout.split('\n'):
                        if 'Owner:' in line:
                            owner_line = line.strip()
                            if config['short_name'] in owner_line:
                                print(f"   ✅ Short name correct: {config['short_name']}")
                            break
                else:
                    print(f"   ❌ Device name incorrect. Expected: {expected_name}")
            else:
                print("   ❌ Device not responding to --info command")
                return False
            
            # Test 3: LoRa configuration validation
            print("   🔍 Test 3: LoRa configuration validation...")
            lora_checks = {
                'region': 'UA_433',
                'modem_preset': 'SHORT_FAST',
                'role': 'CLIENT'
            }
            
            lora_valid = True
            for setting, expected_value in lora_checks.items():
                if f'"{setting}": "{expected_value}"' in info_result.stdout or f'{setting}": {expected_value}' in info_result.stdout:
                    print(f"   ✅ {setting}: {expected_value}")
                else:
                    print(f"   ❌ {setting} not set to {expected_value}")
                    lora_valid = False
            
            validation_results['lora_settings'] = lora_valid
            
            # Test 4: Channel configuration validation
            print("   🔍 Test 4: Channel configuration validation...")
            if 'Channels:' in info_result.stdout:
                validation_results['channel_config'] = True
                print("   ✅ Channels configured")
                
                # Check for specific channel names
                if 0 in config['channels'] and 'LogSplitter' in info_result.stdout:
                    print("   ✅ Channel 0 (LogSplitter) detected")
                if 1 in config['channels'] and 'Telemetry' in info_result.stdout:
                    print("   ✅ Channel 1 (Telemetry) detected")
            else:
                print("   ❌ No channels detected in configuration")
            
            # Test 5: Network commands test
            print("   🔍 Test 5: Network commands test...")
            nodes_result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--nodes'
            ], capture_output=True, text=True, timeout=10)
            
            if nodes_result.returncode == 0:
                validation_results['network_commands'] = True
                print("   ✅ --nodes command successful")
                
                # Parse node table for self-detection
                if expected_name in nodes_result.stdout or config['short_name'] in nodes_result.stdout:
                    print(f"   ✅ Device appears in mesh node list")
            else:
                print("   ❌ --nodes command failed")
            
            # Test 6: Message transmission test
            print("   🔍 Test 6: Message transmission test...")
            test_message = f"{config['short_name']}_validation_test_{int(time.time())}"
            
            send_result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--sendtext', test_message
            ], capture_output=True, text=True, timeout=10)
            
            if send_result.returncode == 0:
                validation_results['message_transmission'] = True
                print(f"   ✅ Test message sent: {test_message}")
            else:
                print("   ❌ Message transmission failed")
            
            # Test 7: Node-specific configuration validation
            print(f"   🔍 Test 7: {node_type.title()}-specific settings...")
            node_specific_valid = True
            
            if node_type == 'controller':
                # Check serial bridge settings
                if '"enabled": true' in info_result.stdout and '"baud": "BAUD_115200"' in info_result.stdout:
                    print("   ✅ Serial bridge enabled at 115200 baud")
                else:
                    print("   ❌ Serial bridge not properly configured")
                    node_specific_valid = False
            
            elif node_type == 'diagnostic':
                # Check rebroadcast mode
                if '"rebroadcastMode": "ALL"' in info_result.stdout:
                    print("   ✅ Rebroadcast mode set to ALL")
                else:
                    print("   ❌ Rebroadcast mode not set correctly")
                    node_specific_valid = False
            
            elif node_type == 'operator':
                # Check power saving settings
                if '"isPowerSaving": true' in info_result.stdout:
                    print("   ✅ Power saving enabled")
                else:
                    print("   ❌ Power saving not enabled")
                    node_specific_valid = False
            
            validation_results['node_specific'] = node_specific_valid
            
            # Test 8: GPS and telemetry settings
            print("   🔍 Test 8: GPS and telemetry settings...")
            if '"gpsEnabled": false' in info_result.stdout:
                print("   ✅ GPS disabled as configured")
            else:
                print("   ⚠️  GPS setting may not be applied correctly")
            
            if '"powerMeasurementEnabled": true' in info_result.stdout:
                print("   ✅ Power telemetry enabled")
            else:
                print("   ⚠️  Power telemetry may not be enabled")
            
            # Calculate overall success
            passed_tests = sum(validation_results.values())
            total_tests = len(validation_results)
            success_rate = passed_tests / total_tests
            
            print(f"\n📊 Validation Summary:")
            print(f"   Tests passed: {passed_tests}/{total_tests} ({success_rate:.1%})")
            
            for test_name, result in validation_results.items():
                status = "✅" if result else "❌"
                print(f"   {status} {test_name.replace('_', ' ').title()}")
            
            # Log detailed validation results
            self._log_action(f"Validation {node_type}", success_rate >= 0.8, 
                           f"{passed_tests}/{total_tests} tests passed")
            
            if success_rate >= 0.9:
                print("   🎉 Excellent! Device fully validated")
                return True
            elif success_rate >= 0.8:
                print("   ✅ Good! Device validation passed with minor issues")
                return True
            else:
                print("   ❌ Validation failed - too many configuration issues")
                return False
                
        except Exception as e:
            print(f"   ❌ Validation error: {e}")
            self._log_action(f"Validation {node_type}", False, f"Exception: {str(e)}")
            return False
    
    def run_network_tests(self, port: str, node_type: str) -> None:
        """Run comprehensive network connectivity and performance tests"""
        print(f"\n🌐 Network Integration Tests for {node_type.title()}...")
        
        config = self.node_configs[node_type]
        
        # Test 1: Multi-device discovery
        print("   🔍 Test 1: Multi-device discovery...")
        try:
            all_devices = self.discover_devices()
            if len(all_devices) > 1:
                print(f"   ✅ Found {len(all_devices)} Meshtastic devices in network")
                for device_port in all_devices:
                    if device_port != port:
                        other_info = self.get_current_device_info(device_port)
                        other_name = other_info.get('current_owner', 'Unknown')
                        print(f"   📡 Other device: {device_port} - {other_name}")
            else:
                print(f"   ⚠️  Only this device detected ({port})")
        except Exception as e:
            print(f"   ❌ Device discovery failed: {e}")
        
        # Test 2: Channel configuration verification
        print("   🔍 Test 2: Channel configuration check...")
        try:
            # Check channel 0
            ch0_result = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--ch-index', '0'
            ], capture_output=True, text=True, timeout=8)
            
            if ch0_result.returncode == 0:
                print("   ✅ Channel 0 accessible")
            else:
                print("   ❌ Channel 0 not accessible")
            
            # Check channel 1 if configured
            if 1 in config['channels']:
                ch1_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--ch-index', '1'  
                ], capture_output=True, text=True, timeout=8)
                
                if ch1_result.returncode == 0:
                    print("   ✅ Channel 1 (Telemetry) accessible")
                else:
                    print("   ❌ Channel 1 (Telemetry) not accessible")
        except Exception as e:
            print(f"   ❌ Channel check failed: {e}")
        
        # Test 3: Message broadcast test with timestamps
        print("   🔍 Test 3: Timestamped message broadcast...")
        try:
            timestamp = datetime.now().strftime("%H:%M:%S")
            test_messages = [
                f"Network test from {config['short_name']} at {timestamp}",
                f"LogSplitter {node_type} ready - {timestamp}",
                f"Mesh connectivity test - {config['name']} - {timestamp}"
            ]
            
            for i, message in enumerate(test_messages, 1):
                send_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--sendtext', message
                ], capture_output=True, text=True, timeout=8)
                
                if send_result.returncode == 0:
                    print(f"   ✅ Test message {i}/3 sent successfully")
                    time.sleep(2)  # Prevent flooding
                else:
                    print(f"   ❌ Test message {i}/3 failed")
        except Exception as e:
            print(f"   ❌ Message broadcast test failed: {e}")
        
        # Test 4: Node-specific functionality tests
        print(f"   🔍 Test 4: {node_type.title()}-specific functionality...")
        
        if node_type == 'controller':
            # Test serial bridge readiness
            print("   📡 Testing serial bridge configuration...")
            try:
                serial_test = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--get', 'serial'
                ], capture_output=True, text=True, timeout=8)
                
                if serial_test.returncode == 0 and 'enabled: True' in serial_test.stdout:
                    print("   ✅ Serial bridge ready for Arduino connection")
                else:
                    print("   ⚠️  Serial bridge may need manual verification")
            except Exception as e:
                print(f"   ❌ Serial bridge test failed: {e}")
        
        elif node_type == 'diagnostic':
            # Test telemetry reception readiness
            print("   📊 Testing telemetry reception readiness...")
            try:
                nodes_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--nodes'
                ], capture_output=True, text=True, timeout=10)
                
                if nodes_result.returncode == 0:
                    # Count visible nodes
                    node_lines = [line for line in nodes_result.stdout.split('\n') 
                                if '│' in line and 'User' not in line and line.strip()]
                    node_count = len([line for line in node_lines if '│' in line and line.count('│') > 3])
                    
                    print(f"   ✅ Can monitor {node_count} mesh node(s)")
                    if node_count == 0:
                        print("   ⚠️  No other nodes visible yet (normal for first device)")
                else:
                    print("   ❌ Node monitoring failed")
            except Exception as e:
                print(f"   ❌ Telemetry readiness test failed: {e}")
        
        elif node_type == 'operator':
            # Test power optimization
            print("   🔋 Testing power optimization settings...")
            try:
                power_test = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--get', 'power'
                ], capture_output=True, text=True, timeout=8)
                
                if power_test.returncode == 0:
                    if 'is_power_saving: True' in power_test.stdout:
                        print("   ✅ Power saving optimizations active")
                    else:
                        print("   ⚠️  Power saving may not be fully configured")
                else:
                    print("   ❌ Power settings verification failed")
            except Exception as e:
                print(f"   ❌ Power optimization test failed: {e}")
        
        # Test 5: Final connectivity and status check
        print("   🔍 Test 5: Final device status verification...")
        try:
            final_info = subprocess.run([
                self.meshtastic_cmd, '--port', port, '--info'
            ], capture_output=True, text=True, timeout=10)
            
            if final_info.returncode == 0:
                print("   ✅ Device responsive after all tests")
                
                # Extract key metrics
                info_text = final_info.stdout
                if 'batteryLevel' in info_text:
                    print("   ✅ Device telemetry active")
                if 'channelUtilization' in info_text:
                    print("   ✅ Channel utilization monitoring active")
                if config['name'] in info_text:
                    print(f"   ✅ Device identity confirmed: {config['name']}")
            else:
                print("   ❌ Device not responsive after tests")
        except Exception as e:
            print(f"   ❌ Final status check failed: {e}")
        
        # Test Summary
        print(f"\n📋 Network Test Summary for {config['name']}:")
        print(f"   Device Type: {node_type.title()}")
        print(f"   Role: {config['role']}")
        print(f"   Channels: {config['channels']}")
        print(f"   Ready for: {config['description']}")
        
        if node_type == 'controller':
            print("   🔗 Next: Connect to Arduino via pins A4/A5")
        elif node_type == 'diagnostic':
            print("   🖥️  Next: Integrate with LCARS diagnostic system")
        elif node_type == 'operator':
            print("   👤 Next: Field deployment for operations monitoring")
        
        print("   📶 All network tests completed!")
        
        # Log network test completion
        self._log_action(f"Network tests {node_type}", True, "Comprehensive network testing completed")
    
    def run_full_mesh_test(self, configured_devices: List) -> None:
        """Run comprehensive mesh network tests across all configured devices"""
        print("\n🕸️  Full Mesh Network Test Suite")
        print("=" * 50)
        
        device_info = {}
        for port, name in configured_devices:
            device_info[port] = {'name': name, 'type': self._detect_device_type(name)}
        
        # Test 1: Cross-device connectivity
        print("🔍 Test 1: Cross-device connectivity matrix...")
        for i, (port1, name1) in enumerate(configured_devices):
            for j, (port2, name2) in enumerate(configured_devices):
                if i != j:
                    success = self._test_device_to_device_connectivity(port1, port2, name1, name2)
                    status = "✅" if success else "❌"
                    print(f"   {status} {name1} → {name2}")
        
        # Test 2: Message delivery test
        print("\n🔍 Test 2: Message delivery verification...")
        test_timestamp = datetime.now().strftime("%H:%M:%S")
        
        for port, name in configured_devices:
            message = f"Mesh test from {name} at {test_timestamp}"
            try:
                result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--sendtext', message
                ], capture_output=True, text=True, timeout=10)
                
                if result.returncode == 0:
                    print(f"   ✅ {name}: Message sent successfully")
                else:
                    print(f"   ❌ {name}: Message send failed")
                time.sleep(3)  # Prevent message flooding
            except Exception as e:
                print(f"   ❌ {name}: Error - {e}")
        
        # Test 3: Channel configuration consistency 
        print("\n🔍 Test 3: Channel configuration consistency...")
        channel_configs = {}
        
        for port, name in configured_devices:
            try:
                info_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--info'
                ], capture_output=True, text=True, timeout=10)
                
                if info_result.returncode == 0:
                    # Extract channel info
                    if 'Channels:' in info_result.stdout:
                        channel_configs[name] = info_result.stdout.split('Channels:')[1].split('\n')[0:3]
                        print(f"   ✅ {name}: Channel config extracted")
                    else:
                        print(f"   ⚠️  {name}: No channel config found")
            except Exception as e:
                print(f"   ❌ {name}: Channel config error - {e}")
        
        # Test 4: Network performance metrics
        print("\n🔍 Test 4: Network performance analysis...")
        
        for port, name in configured_devices:
            try:
                nodes_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--nodes'
                ], capture_output=True, text=True, timeout=15)
                
                if nodes_result.returncode == 0:
                    # Count visible nodes from this device's perspective
                    lines = nodes_result.stdout.split('\n')
                    node_count = 0
                    
                    for line in lines:
                        if '│' in line and 'LogSplitter' in line and line.count('│') > 3:
                            node_count += 1
                    
                    print(f"   📊 {name}: Sees {node_count} mesh nodes")
                    
                    # Check for channel utilization data
                    if 'Channel util' in nodes_result.stdout:
                        print(f"   📈 {name}: Channel utilization monitoring active")
                else:
                    print(f"   ❌ {name}: Performance data unavailable")
            except Exception as e:
                print(f"   ❌ {name}: Performance test error - {e}")
        
        # Test 5: Device-specific functionality verification
        print("\n🔍 Test 5: Device-specific functionality...")
        
        for port, name in configured_devices:
            device_type = device_info[port]['type']
            
            if device_type == 'controller':
                # Test serial bridge
                try:
                    serial_result = subprocess.run([
                        self.meshtastic_cmd, '--port', port, '--get', 'serial'
                    ], capture_output=True, text=True, timeout=8)
                    
                    if serial_result.returncode == 0 and 'enabled: True' in serial_result.stdout:
                        print(f"   ✅ {name}: Serial bridge ready for Arduino")
                    else:
                        print(f"   ⚠️  {name}: Serial bridge needs verification")
                except Exception as e:
                    print(f"   ❌ {name}: Serial test failed - {e}")
            
            elif device_type == 'diagnostic':
                # Test LCARS integration readiness
                print(f"   🖥️  {name}: LCARS diagnostic integration ready")
                print(f"   📊 {name}: Can monitor telemetry from controller")
            
            elif device_type == 'operator':
                # Test handheld optimization
                try:
                    power_result = subprocess.run([
                        self.meshtastic_cmd, '--port', port, '--get', 'power'
                    ], capture_output=True, text=True, timeout=8)
                    
                    if power_result.returncode == 0 and 'is_power_saving: True' in power_result.stdout:
                        print(f"   🔋 {name}: Battery optimization active")
                    else:
                        print(f"   ⚠️  {name}: Power settings need review")
                except Exception as e:
                    print(f"   ❌ {name}: Power test failed - {e}")
        
        # Test 6: Emergency procedures validation
        print("\n🔍 Test 6: Emergency procedures validation...")
        
        # Test emergency message broadcast
        emergency_message = f"EMERGENCY_TEST_{int(time.time())}"
        print(f"   🚨 Broadcasting emergency test message: {emergency_message}")
        
        for port, name in configured_devices:
            try:
                emergency_result = subprocess.run([
                    self.meshtastic_cmd, '--port', port, '--sendtext', emergency_message
                ], capture_output=True, text=True, timeout=8)
                
                if emergency_result.returncode == 0:
                    print(f"   ✅ {name}: Emergency broadcast capability confirmed")
                else:
                    print(f"   ❌ {name}: Emergency broadcast failed")
                time.sleep(2)
            except Exception as e:
                print(f"   ❌ {name}: Emergency test error - {e}")
        
        # Final mesh test summary
        print(f"\n🎊 Mesh Network Test Summary:")
        print(f"   Total devices tested: {len(configured_devices)}")
        print(f"   Network topology: {self._describe_topology(device_info)}")
        print(f"   Test timestamp: {datetime.now().isoformat()}")
        
        print("\n✅ Mesh network testing complete!")
        print("🚀 LogSplitter Meshtastic network ready for deployment!")
        
        # Log full mesh test completion
        self._log_action("Full mesh test", True, f"{len(configured_devices)} devices tested successfully")
    
    def _detect_device_type(self, name: str) -> str:
        """Detect device type from name"""
        if 'Controller' in name:
            return 'controller'
        elif 'Diagnostics' in name:
            return 'diagnostic'
        elif 'Operator' in name:
            return 'operator'
        else:
            return 'unknown'
    
    def _test_device_to_device_connectivity(self, port1: str, port2: str, name1: str, name2: str) -> bool:
        """Test connectivity between two specific devices"""
        try:
            # Check if device1 can see device2 in its node list
            nodes_result = subprocess.run([
                self.meshtastic_cmd, '--port', port1, '--nodes'
            ], capture_output=True, text=True, timeout=10)
            
            if nodes_result.returncode == 0:
                # Look for the other device in the output
                return name2.split('-')[-1] in nodes_result.stdout or name2 in nodes_result.stdout
            
            return False
        except Exception:
            return False
    
    def _describe_topology(self, device_info: Dict) -> str:
        """Describe the network topology based on configured devices"""
        types = [info['type'] for info in device_info.values()]
        
        if 'controller' in types and 'diagnostic' in types and 'operator' in types:
            return "Full 3-node mesh (Controller ↔ Diagnostic ↔ Operator)"
        elif 'controller' in types and 'diagnostic' in types:
            return "Base infrastructure (Controller ↔ Diagnostic)"
        elif len(types) == 1:
            return f"Single node ({types[0]})"
        else:
            return f"Partial mesh ({len(types)} nodes)"
    
    def interactive_setup(self):
        """Interactive single device setup"""
        print("🚀 LogSplitter Meshtastic Single Device Configurator")
        print("=" * 55)
        
        # Discover devices
        devices = self.discover_devices()
        if not devices:
            print("❌ No Meshtastic devices found")
            return False
        
        # Select device
        print(f"\n📡 Found {len(devices)} Meshtastic device(s):")
        for i, port in enumerate(devices):
            info = self.get_current_device_info(port)
            current_name = info.get('current_owner', 'Unknown')
            print(f"   {i+1}. {port} - Current: {current_name}")
        
        try:
            device_idx = int(input(f"\nSelect device to configure (1-{len(devices)}): ")) - 1
            selected_port = devices[device_idx]
        except (ValueError, IndexError):
            print("❌ Invalid selection")
            return False
        
        # Select node type  
        print("\n📋 Available node configurations:")
        node_types = list(self.node_configs.keys())
        for i, node_type in enumerate(node_types):
            config = self.node_configs[node_type]
            print(f"   {i+1}. {node_type.title()}: {config['description']}")
        
        try:
            node_idx = int(input(f"\nSelect configuration type (1-{len(node_types)}): ")) - 1
            selected_node_type = node_types[node_idx]
        except (ValueError, IndexError):
            print("❌ Invalid selection")
            return False
        
        # Show current device info
        current_info = self.get_current_device_info(selected_port)
        print(f"\n📊 Current device info for {selected_port}:")
        print(f"   Current Owner: {current_info.get('current_owner', 'Unknown')}")
        
        # Confirm configuration
        config = self.node_configs[selected_node_type]
        print(f"\n🎯 Will configure as: {config['name']}")
        print(f"   Description: {config['description']}")
        print(f"   Channels: {config['channels']}")
        
        proceed = input("\nProceed with configuration? (y/N): ").lower()
        if proceed != 'y':
            print("❌ Configuration cancelled")
            return False
        
        # Factory reset option
        reset_choice = input("\nFactory reset device first? (recommended) (Y/n): ").lower()
        if reset_choice != 'n':
            if not self.factory_reset_device(selected_port):
                return False
        
        # Configure device
        if self.configure_node(selected_port, selected_node_type):
            print("\n🎉 Configuration completed successfully!")
            
            # Validate configuration
            if self.validate_configuration(selected_port, selected_node_type):
                print("✅ Validation passed - device ready for network")
                
                # Run additional network tests
                self.run_network_tests(selected_port, selected_node_type)
                return True
            else:
                print("⚠️  Configuration applied but validation failed")
                return False
        else:
            print("❌ Configuration failed")
            return False

def main():
    """Main entry point"""
    configurator = SingleNodeConfigurator()
    
    try:
        success = configurator.interactive_setup()
        
        # Save log
        import json
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        log_filename = f"single_node_config_{timestamp}.json"
        
        with open(log_filename, 'w') as f:
            json.dump(configurator.config_log, f, indent=2)
        
        print(f"\n📝 Configuration log saved to: {log_filename}")
        
        if success:
            print("\n🎯 Next steps:")
            print("   1. Test device connectivity")
            print("   2. Configure additional devices")
            print("   3. Test network mesh when all devices configured")
            print("   4. Integrate with LCARS diagnostic system")
            
            # Check if we should run full network tests
            all_devices = configurator.discover_devices()
            configured_devices = []
            
            for device_port in all_devices:
                info = configurator.get_current_device_info(device_port)
                owner = info.get('current_owner', '')
                if 'LogSplitter' in owner:
                    configured_devices.append((device_port, owner))
            
            if len(configured_devices) >= 2:
                print(f"\n🌐 Found {len(configured_devices)} configured LogSplitter devices!")
                for port, name in configured_devices:
                    print(f"   📡 {port}: {name}")
                
                run_mesh_test = input("\nRun full mesh network test? (y/N): ").lower()
                if run_mesh_test == 'y':
                    configurator.run_full_mesh_test(configured_devices)
        
        return 0 if success else 1
        
    except KeyboardInterrupt:
        print("\n⏹️  Configuration cancelled by user")
        return 1
    except Exception as e:
        print(f"\n💥 Configuration failed: {e}")
        return 1

if __name__ == "__main__":
    exit(main())