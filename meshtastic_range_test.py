#!/usr/bin/env python3
"""
Meshtastic Range Testing Tool for LogSplitter Controller Integration

This script performs comprehensive range and reliability testing for Meshtastic
mesh networks used in LogSplitter telemetry transport. It validates signal
quality, latency, packet loss, and network performance across various distances
and environmental conditions.

Usage:
    python meshtastic_range_test.py [OPTIONS]

Examples:
    # Auto-detect nodes and run 5-minute test
    python meshtastic_range_test.py --duration 300
    
    # Specific nodes with position logging
    python meshtastic_range_test.py --bridge COM3 --monitor COM4 --gps --duration 600
    
    # Continuous monitoring mode
    python meshtastic_range_test.py --continuous --interval 30

Features:
- Automated range testing between bridge and monitor nodes
- Real-time signal quality monitoring (RSSI, SNR, packet loss)
- GPS position logging for distance calculation
- Performance statistics and reporting
- Environmental condition logging
- Export results to CSV/JSON for analysis
"""

import sys
import time
import math
import argparse
import threading
import json
import csv
from datetime import datetime, timedelta
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict
import subprocess
import serial
import serial.tools.list_ports

@dataclass
class TestResult:
    """Single test measurement result"""
    timestamp: datetime
    distance_km: Optional[float] = None
    rssi_dbm: Optional[int] = None
    snr_db: Optional[float] = None
    packet_loss_percent: float = 0.0
    latency_ms: float = 0.0
    hop_count: int = 0
    bridge_battery: Optional[float] = None
    monitor_battery: Optional[float] = None
    temperature_c: Optional[float] = None
    humidity_percent: Optional[float] = None
    
    def to_dict(self):
        """Convert to dictionary for serialization"""
        result = asdict(self)
        result['timestamp'] = self.timestamp.isoformat()
        return result

@dataclass 
class Position:
    """GPS position data"""
    latitude: float
    longitude: float
    altitude: float = 0.0
    accuracy: float = 0.0
    timestamp: Optional[datetime] = None
    
    def distance_to(self, other: 'Position') -> float:
        """Calculate distance to another position in kilometers"""
        if not other:
            return 0.0
        
        # Haversine formula for great circle distance
        R = 6371  # Earth radius in km
        
        lat1, lon1 = math.radians(self.latitude), math.radians(self.longitude)
        lat2, lon2 = math.radians(other.latitude), math.radians(other.longitude)
        
        dlat = lat2 - lat1
        dlon = lon2 - lon1
        
        a = (math.sin(dlat/2)**2 + 
             math.cos(lat1) * math.cos(lat2) * math.sin(dlon/2)**2)
        c = 2 * math.asin(math.sqrt(a))
        
        return R * c

class MeshtasticRangeTester:
    """Comprehensive range testing for Meshtastic networks"""
    
    def __init__(self, bridge_port: Optional[str] = None, monitor_port: Optional[str] = None, 
                 enable_gps: bool = False, log_environmental: bool = False):
        self.bridge_port = bridge_port
        self.monitor_port = monitor_port
        self.enable_gps = enable_gps
        self.log_environmental = log_environmental
        
        # Test state
        self.running = False
        self.results: List[TestResult] = []
        self.bridge_position: Optional[Position] = None
        self.monitor_position: Optional[Position] = None
        
        # Statistics
        self.packets_sent = 0
        self.packets_received = 0
        self.total_latency = 0.0
        self.rssi_samples = []
        self.snr_samples = []
        
        # Threading
        self.test_thread = None
        self.monitor_thread = None
        
    def auto_detect_nodes(self) -> bool:
        """Auto-detect Meshtastic nodes"""
        print("🔍 Auto-detecting Meshtastic nodes...")
        
        detected_nodes = []
        
        for port in serial.tools.list_ports.comports():
            if self.test_meshtastic_connection(port.device):
                node_info = self.get_node_info(port.device)
                detected_nodes.append((port.device, node_info))
                print(f"✓ Found Meshtastic node: {port.device} - {node_info.get('name', 'Unknown')}")
        
        if len(detected_nodes) >= 2:
            # Assign bridge and monitor nodes
            self.bridge_port = detected_nodes[0][0]
            self.monitor_port = detected_nodes[1][0]
            
            print(f"📍 Bridge Node: {self.bridge_port}")
            print(f"📍 Monitor Node: {self.monitor_port}")
            return True
        
        elif len(detected_nodes) == 1:
            print("⚠️  Only one Meshtastic node detected")
            print("   Connect a second node for range testing")
            return False
        
        else:
            print("❌ No Meshtastic nodes detected")
            return False
    
    def test_meshtastic_connection(self, port: str) -> bool:
        """Test if port has a Meshtastic device"""
        try:
            result = subprocess.run(['meshtastic', '--port', port, '--info'], 
                                   capture_output=True, text=True, timeout=10)
            return result.returncode == 0 and 'MyNodeInfo' in result.stdout
        except:
            return False
    
    def get_node_info(self, port: str) -> dict:
        """Get node information"""
        try:
            result = subprocess.run(['meshtastic', '--port', port, '--info'], 
                                   capture_output=True, text=True, timeout=10)
            if result.returncode == 0:
                # Parse node info from output (simplified)
                lines = result.stdout.split('\n')
                info = {}
                for line in lines:
                    if 'Owner:' in line:
                        info['name'] = line.split('Owner:')[1].strip()
                    elif 'Model:' in line:
                        info['model'] = line.split('Model:')[1].strip()
                    elif 'Battery:' in line:
                        info['battery'] = line.split('Battery:')[1].strip()
                return info
        except:
            pass
        return {}
    
    def send_test_message(self, sequence: int) -> bool:
        """Send test message from bridge to monitor"""
        if not self.bridge_port:
            return False
            
        try:
            test_message = f"RANGE_TEST_{sequence:04d}_{int(time.time()*1000)}"
            
            result = subprocess.run([
                'meshtastic', '--port', self.bridge_port, 
                '--sendtext', test_message, '--ch-index', '1'
            ], capture_output=True, text=True, timeout=5)
            
            if result.returncode == 0:
                self.packets_sent += 1
                return True
            else:
                print(f"⚠️  Send failed: {result.stderr.strip()}")
                return False
                
        except Exception as e:
            print(f"❌ Send error: {e}")
            return False
    
    def monitor_responses(self, duration: float):
        """Monitor for test message responses"""
        if not self.monitor_port:
            return
            
        start_time = time.time()
        process = None
        
        try:
            # Use meshtastic CLI to monitor messages
            process = subprocess.Popen([
                'meshtastic', '--port', self.monitor_port, 
                '--listen', '--ch-index', '1'
            ], stdout=subprocess.PIPE, stderr=subprocess.PIPE, 
               text=True, bufsize=1, universal_newlines=True)
            
            while self.running and time.time() - start_time < duration + 10:
                if process.stdout:
                    line = process.stdout.readline()
                    if line:
                        self.process_received_message(line.strip())
                
        except Exception as e:
            print(f"⚠️  Monitor error: {e}")
        finally:
            try:
                if process:
                    process.terminate()
            except:
                pass
    
    def process_received_message(self, message: str):
        """Process received test message"""
        if 'RANGE_TEST_' in message:
            self.packets_received += 1
            
            # Extract RSSI and SNR if available
            rssi = self.extract_rssi(message)
            snr = self.extract_snr(message)
            
            if rssi:
                self.rssi_samples.append(rssi)
            if snr:
                self.snr_samples.append(snr)
            
            # Calculate latency from timestamp in message
            try:
                parts = message.split('RANGE_TEST_')[1].split('_')
                if len(parts) >= 2:
                    send_time = int(parts[1])
                    receive_time = int(time.time() * 1000)
                    latency = receive_time - send_time
                    if 0 <= latency <= 30000:  # Sanity check (0-30 seconds)
                        self.total_latency += latency
            except:
                pass
    
    def extract_rssi(self, message: str) -> Optional[int]:
        """Extract RSSI from message"""
        try:
            if 'rssi:' in message.lower():
                rssi_part = message.lower().split('rssi:')[1].split()[0]
                return int(rssi_part.replace('dbm', ''))
        except:
            pass
        return None
    
    def extract_snr(self, message: str) -> Optional[float]:
        """Extract SNR from message"""
        try:
            if 'snr:' in message.lower():
                snr_part = message.lower().split('snr:')[1].split()[0]
                return float(snr_part.replace('db', ''))
        except:
            pass
        return None
    
    def get_position(self, port: str) -> Optional[Position]:
        """Get GPS position from node"""
        if not self.enable_gps:
            return None
        
        try:
            result = subprocess.run([
                'meshtastic', '--port', port, '--get', 'position'
            ], capture_output=True, text=True, timeout=10)
            
            if result.returncode == 0:
                # Parse position from output (simplified)
                lines = result.stdout.split('\n')
                lat, lon, alt = None, None, 0.0
                
                for line in lines:
                    if 'latitude:' in line.lower():
                        lat = float(line.split(':')[1].strip())
                    elif 'longitude:' in line.lower():
                        lon = float(line.split(':')[1].strip())
                    elif 'altitude:' in line.lower():
                        alt = float(line.split(':')[1].strip())
                
                if lat is not None and lon is not None:
                    return Position(lat, lon, alt, timestamp=datetime.now())
        except:
            pass
        
        return None
    
    def update_positions(self):
        """Update GPS positions for both nodes"""
        if self.enable_gps and self.bridge_port and self.monitor_port:
            self.bridge_position = self.get_position(self.bridge_port)
            self.monitor_position = self.get_position(self.monitor_port)
            
            if self.bridge_position and self.monitor_position:
                distance = self.bridge_position.distance_to(self.monitor_position)
                print(f"📍 Current distance: {distance:.2f} km")
    
    def calculate_statistics(self) -> dict:
        """Calculate test statistics"""
        if self.packets_sent == 0:
            return {}
        
        packet_loss = ((self.packets_sent - self.packets_received) / self.packets_sent) * 100
        avg_latency = self.total_latency / max(1, self.packets_received)
        
        stats = {
            'packets_sent': self.packets_sent,
            'packets_received': self.packets_received,
            'packet_loss_percent': round(packet_loss, 2),
            'average_latency_ms': round(avg_latency, 1),
            'success_rate_percent': round(100 - packet_loss, 2)
        }
        
        if self.rssi_samples:
            stats['rssi_avg'] = round(sum(self.rssi_samples) / len(self.rssi_samples), 1)
            stats['rssi_min'] = min(self.rssi_samples)
            stats['rssi_max'] = max(self.rssi_samples)
        
        if self.snr_samples:
            stats['snr_avg'] = round(sum(self.snr_samples) / len(self.snr_samples), 1)
            stats['snr_min'] = min(self.snr_samples)
            stats['snr_max'] = max(self.snr_samples)
        
        if self.bridge_position and self.monitor_position:
            stats['distance_km'] = round(self.bridge_position.distance_to(self.monitor_position), 2)
        
        return stats
    
    def run_test(self, duration: int, interval: float = 1.0):
        """Run range test for specified duration"""
        print(f"🔗 Starting {duration}-second range test...")
        print(f"📡 Bridge: {self.bridge_port}")
        print(f"📱 Monitor: {self.monitor_port}")
        print(f"⏱️  Interval: {interval} seconds")
        
        if self.enable_gps:
            print("📍 GPS position logging enabled")
        
        self.running = True
        self.packets_sent = 0
        self.packets_received = 0
        self.total_latency = 0.0
        self.rssi_samples = []
        self.snr_samples = []
        
        # Start monitor thread
        self.monitor_thread = threading.Thread(
            target=self.monitor_responses, 
            args=(duration,), 
            daemon=True
        )
        self.monitor_thread.start()
        
        # Update positions initially
        self.update_positions()
        
        # Send test messages
        start_time = time.time()
        sequence = 0
        
        try:
            while self.running and time.time() - start_time < duration:
                # Send test message
                self.send_test_message(sequence)
                sequence += 1
                
                # Update positions periodically
                if sequence % 10 == 0:
                    self.update_positions()
                
                # Progress indicator
                elapsed = time.time() - start_time
                progress = (elapsed / duration) * 100
                print(f"\r📊 Progress: {progress:.1f}% | "
                      f"Sent: {self.packets_sent} | "
                      f"Received: {self.packets_received} | "
                      f"Loss: {((self.packets_sent - self.packets_received) / max(1, self.packets_sent)) * 100:.1f}%", 
                      end='', flush=True)
                
                time.sleep(interval)
                
        except KeyboardInterrupt:
            print("\n⏹️  Test stopped by user")
        
        finally:
            self.running = False
            
        # Wait for monitor thread to finish
        if self.monitor_thread and self.monitor_thread.is_alive():
            self.monitor_thread.join(timeout=5)
        
        print(f"\n✅ Range test completed")
        
        # Calculate and display results
        stats = self.calculate_statistics()
        self.display_results(stats)
        
        return stats
    
    def display_results(self, stats: dict):
        """Display test results"""
        print("\n📊 Range Test Results")
        print("=" * 50)
        
        print(f"📤 Packets Sent: {stats.get('packets_sent', 0)}")
        print(f"📥 Packets Received: {stats.get('packets_received', 0)}")
        print(f"📉 Packet Loss: {stats.get('packet_loss_percent', 0):.2f}%")
        print(f"✅ Success Rate: {stats.get('success_rate_percent', 0):.2f}%")
        
        if 'average_latency_ms' in stats:
            print(f"⏱️  Average Latency: {stats['average_latency_ms']:.1f} ms")
        
        if 'distance_km' in stats:
            print(f"📍 Distance: {stats['distance_km']:.2f} km")
        
        if 'rssi_avg' in stats:
            print(f"📶 RSSI: {stats['rssi_avg']:.1f} dBm "
                  f"(min: {stats['rssi_min']}, max: {stats['rssi_max']})")
        
        if 'snr_avg' in stats:
            print(f"📊 SNR: {stats['snr_avg']:.1f} dB "
                  f"(min: {stats['snr_min']:.1f}, max: {stats['snr_max']:.1f})")
        
        # Quality assessment
        print(f"\n🎯 Quality Assessment:")
        
        success_rate = stats.get('success_rate_percent', 0)
        if success_rate >= 95:
            print(f"   📶 Excellent ({success_rate:.1f}% success)")
        elif success_rate >= 85:
            print(f"   📶 Good ({success_rate:.1f}% success)")
        elif success_rate >= 70:
            print(f"   📶 Fair ({success_rate:.1f}% success)")
        else:
            print(f"   📶 Poor ({success_rate:.1f}% success)")
        
        avg_rssi = stats.get('rssi_avg', -999)
        if avg_rssi >= -80:
            print(f"   📡 Strong signal ({avg_rssi:.1f} dBm)")
        elif avg_rssi >= -100:
            print(f"   📡 Moderate signal ({avg_rssi:.1f} dBm)")  
        elif avg_rssi >= -120:
            print(f"   📡 Weak signal ({avg_rssi:.1f} dBm)")
        else:
            print(f"   📡 Very weak signal ({avg_rssi:.1f} dBm)")
    
    def export_results(self, filename: str, format: str = 'csv'):
        """Export results to file"""
        stats = self.calculate_statistics()
        
        if format.lower() == 'json':
            with open(filename, 'w') as f:
                json.dump({
                    'test_info': {
                        'timestamp': datetime.now().isoformat(),
                        'bridge_port': self.bridge_port,
                        'monitor_port': self.monitor_port,
                        'gps_enabled': self.enable_gps
                    },
                    'statistics': stats,
                    'positions': {
                        'bridge': asdict(self.bridge_position) if self.bridge_position else None,
                        'monitor': asdict(self.monitor_position) if self.monitor_position else None
                    }
                }, f, indent=2)
        
        elif format.lower() == 'csv':
            with open(filename, 'w', newline='') as f:
                writer = csv.writer(f)
                writer.writerow(['Metric', 'Value'])
                writer.writerow(['Test Date', datetime.now().isoformat()])
                writer.writerow(['Bridge Port', self.bridge_port])
                writer.writerow(['Monitor Port', self.monitor_port])
                
                for key, value in stats.items():
                    writer.writerow([key.replace('_', ' ').title(), value])
        
        print(f"✅ Results exported to: {filename}")

def main():
    """Main application entry point"""
    parser = argparse.ArgumentParser(
        description='Meshtastic Range Testing Tool for LogSplitter Controller',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Auto-detect nodes and run 5-minute test
  python meshtastic_range_test.py --duration 300
  
  # Specific nodes with GPS logging
  python meshtastic_range_test.py --bridge COM3 --monitor COM4 --gps --duration 600
  
  # Quick connectivity test
  python meshtastic_range_test.py --duration 60 --interval 0.5
        """
    )
    
    parser.add_argument('--bridge', help='Bridge node port (auto-detect if not specified)')
    parser.add_argument('--monitor', help='Monitor node port (auto-detect if not specified)')
    parser.add_argument('--duration', type=int, default=300, 
                       help='Test duration in seconds (default: 300)')
    parser.add_argument('--interval', type=float, default=1.0,
                       help='Message interval in seconds (default: 1.0)')
    parser.add_argument('--gps', action='store_true',
                       help='Enable GPS position logging')
    parser.add_argument('--export', help='Export results to file (CSV or JSON)')
    parser.add_argument('--continuous', action='store_true',
                       help='Continuous monitoring mode')
    
    args = parser.parse_args()
    
    print("🚀 Meshtastic Range Testing Tool")
    print("=" * 50)
    
    # Create tester
    tester = MeshtasticRangeTester(
        bridge_port=args.bridge,
        monitor_port=args.monitor, 
        enable_gps=args.gps
    )
    
    # Auto-detect nodes if not specified
    if not args.bridge or not args.monitor:
        if not tester.auto_detect_nodes():
            print("❌ Failed to detect Meshtastic nodes")
            return 1
    
    try:
        if args.continuous:
            print("🔄 Continuous monitoring mode - press Ctrl+C to stop")
            while True:
                stats = tester.run_test(args.interval * 10, args.interval)
                time.sleep(5)  # Brief pause between test cycles
        else:
            # Single test run
            stats = tester.run_test(args.duration, args.interval)
            
            # Export results if requested
            if args.export:
                format = 'json' if args.export.endswith('.json') else 'csv'
                tester.export_results(args.export, format)
    
    except KeyboardInterrupt:
        print("\n⏹️  Testing stopped by user")
    except Exception as e:
        print(f"❌ Test error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())