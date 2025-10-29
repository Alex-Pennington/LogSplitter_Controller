#!/usr/bin/env python3
"""
Comprehensive Test Suite for Mesh → LCARS Integration
Tests all telemetry API endpoints and data flow from Arduino → Meshtastic → LCARS
"""

import requests
import json
import time
import sys
from datetime import datetime
from collections import Counter

class Colors:
    """ANSI color codes for terminal output"""
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

class LCARSMeshTester:
    def __init__(self, base_url='http://localhost:3000'):
        self.base_url = base_url
        self.tests_passed = 0
        self.tests_failed = 0
        self.test_results = []
        
    def print_header(self, text):
        """Print test section header"""
        print(f"\n{Colors.HEADER}{Colors.BOLD}{'='*80}{Colors.ENDC}")
        print(f"{Colors.HEADER}{Colors.BOLD}{text:^80}{Colors.ENDC}")
        print(f"{Colors.HEADER}{Colors.BOLD}{'='*80}{Colors.ENDC}\n")
    
    def print_test(self, name, passed, details=""):
        """Print individual test result"""
        if passed:
            icon = "✅"
            color = Colors.OKGREEN
            self.tests_passed += 1
        else:
            icon = "❌"
            color = Colors.FAIL
            self.tests_failed += 1
        
        print(f"{color}{icon} {name}{Colors.ENDC}")
        if details:
            print(f"   {details}")
        
        self.test_results.append({
            'name': name,
            'passed': passed,
            'details': details
        })
    
    def test_server_reachable(self):
        """Test 1: LCARS server is reachable"""
        self.print_header("TEST 1: Server Connectivity")
        
        try:
            response = requests.get(self.base_url, timeout=5)
            self.print_test(
                "LCARS server reachable", 
                response.status_code == 200,
                f"Status: {response.status_code}"
            )
            return response.status_code == 200
        except requests.exceptions.RequestException as e:
            self.print_test("LCARS server reachable", False, f"Error: {e}")
            return False
    
    def test_telemetry_status_endpoint(self):
        """Test 2: /api/telemetry/status endpoint"""
        self.print_header("TEST 2: Telemetry Status API")
        
        try:
            response = requests.get(f"{self.base_url}/api/telemetry/status", timeout=5)
            self.print_test(
                "Status endpoint reachable",
                response.status_code == 200,
                f"Status: {response.status_code}"
            )
            
            if response.status_code != 200:
                return False
            
            data = response.json()
            
            # Check required fields
            required_fields = ['connected', 'session_id', 'total_messages', 'by_type', 'port', 'baud']
            for field in required_fields:
                has_field = field in data
                self.print_test(
                    f"Status has '{field}' field",
                    has_field,
                    f"Value: {data.get(field, 'MISSING')}"
                )
            
            # Check connection status
            is_connected = data.get('connected', False)
            self.print_test(
                "Telemetry decoder connected to COM8",
                is_connected,
                f"Connected: {is_connected}"
            )
            
            # Check baud rate
            correct_baud = data.get('baud') == 38400
            self.print_test(
                "Baud rate is 38400",
                correct_baud,
                f"Baud: {data.get('baud')}"
            )
            
            # Check port
            correct_port = data.get('port') == 'COM8'
            self.print_test(
                "Monitoring COM8",
                correct_port,
                f"Port: {data.get('port')}"
            )
            
            return is_connected
            
        except Exception as e:
            self.print_test("Status endpoint test", False, f"Exception: {e}")
            return False
    
    def test_telemetry_latest_endpoint(self):
        """Test 3: /api/telemetry/latest endpoint"""
        self.print_header("TEST 3: Latest Telemetry Messages API")
        
        try:
            # Test default limit
            response = requests.get(f"{self.base_url}/api/telemetry/latest", timeout=5)
            self.print_test(
                "Latest endpoint reachable",
                response.status_code == 200,
                f"Status: {response.status_code}"
            )
            
            if response.status_code != 200:
                return False
            
            data = response.json()
            
            # Check structure
            has_messages = 'messages' in data
            has_count = 'count' in data
            self.print_test("Response has 'messages' field", has_messages)
            self.print_test("Response has 'count' field", has_count)
            
            if not (has_messages and has_count):
                return False
            
            messages = data['messages']
            count = data['count']
            
            self.print_test(
                "Count matches message array length",
                count == len(messages),
                f"Count: {count}, Array length: {len(messages)}"
            )
            
            # Check if we're receiving messages
            has_data = count > 0
            self.print_test(
                "Receiving telemetry messages",
                has_data,
                f"Total messages: {count}"
            )
            
            if has_data and messages:
                # Check message structure
                msg = messages[0]
                required_msg_fields = ['timestamp', 'type', 'type_name', 'raw_hex']
                
                for field in required_msg_fields:
                    has_field = field in msg
                    self.print_test(
                        f"Message has '{field}' field",
                        has_field,
                        f"Sample: {str(msg.get(field, 'MISSING'))[:50]}"
                    )
                
                # Print message type distribution
                type_dist = Counter(m.get('type_name', 'Unknown') for m in messages)
                print(f"\n   📊 Message Type Distribution:")
                for msg_type, count in type_dist.most_common():
                    print(f"      • {msg_type}: {count}")
            
            # Test custom limit parameter
            response_limit = requests.get(f"{self.base_url}/api/telemetry/latest?limit=5", timeout=5)
            if response_limit.status_code == 200:
                data_limit = response_limit.json()
                limited_correctly = len(data_limit['messages']) <= 5
                self.print_test(
                    "Limit parameter works",
                    limited_correctly,
                    f"Requested 5, got {len(data_limit['messages'])}"
                )
            
            return has_data
            
        except Exception as e:
            self.print_test("Latest endpoint test", False, f"Exception: {e}")
            return False
    
    def test_telemetry_stream_endpoint(self):
        """Test 4: /api/telemetry/stream SSE endpoint"""
        self.print_header("TEST 4: Real-Time Telemetry Stream (SSE)")
        
        try:
            # Test if endpoint is reachable (don't actually stream, just check)
            response = requests.get(
                f"{self.base_url}/api/telemetry/stream",
                stream=True,
                timeout=3
            )
            
            self.print_test(
                "Stream endpoint reachable",
                response.status_code == 200,
                f"Status: {response.status_code}"
            )
            
            if response.status_code != 200:
                return False
            
            # Check content type
            content_type = response.headers.get('Content-Type', '')
            is_sse = 'text/event-stream' in content_type
            self.print_test(
                "Content-Type is text/event-stream",
                is_sse,
                f"Content-Type: {content_type}"
            )
            
            # Try to read first few events (with timeout)
            print(f"\n   📡 Attempting to read SSE stream (5 second timeout)...")
            events_received = 0
            start_time = time.time()
            
            try:
                for line in response.iter_lines(decode_unicode=True):
                    if time.time() - start_time > 5:
                        break
                    if line and line.startswith('data:'):
                        events_received += 1
                        if events_received <= 3:
                            print(f"      Event {events_received}: {line[:80]}...")
            except:
                pass
            
            self.print_test(
                "SSE events received",
                events_received > 0,
                f"Received {events_received} events in 5 seconds"
            )
            
            return events_received > 0
            
        except requests.exceptions.Timeout:
            self.print_test("Stream endpoint test", False, "Timeout (this may be normal for SSE)")
            return False
        except Exception as e:
            self.print_test("Stream endpoint test", False, f"Exception: {e}")
            return False
    
    def test_message_types_coverage(self):
        """Test 5: Message type coverage"""
        self.print_header("TEST 5: Message Type Coverage")
        
        try:
            # Get status to see message type breakdown
            response = requests.get(f"{self.base_url}/api/telemetry/status", timeout=5)
            if response.status_code != 200:
                self.print_test("Get message types", False, "Status endpoint failed")
                return False
            
            data = response.json()
            by_type = data.get('by_type', {})
            
            # Expected message types from Arduino
            expected_types = {
                'Digital I/O': 0x10,
                'Relays': 0x11,
                'Pressure': 0x12,
                'Sequence': 0x13,
                'Error': 0x14,
                'Safety': 0x15,
                'System Status': 0x16,
                'Heartbeat': 0x17
            }
            
            print(f"   📊 Message Type Coverage:")
            for type_name, type_code in expected_types.items():
                count = by_type.get(type_name, 0)
                has_messages = count > 0
                self.print_test(
                    f"{type_name} (0x{type_code:02X})",
                    has_messages,
                    f"Count: {count}"
                )
            
            # Overall coverage
            types_received = len([t for t in expected_types.keys() if by_type.get(t, 0) > 0])
            coverage_pct = (types_received / len(expected_types)) * 100
            
            good_coverage = coverage_pct >= 75
            self.print_test(
                "Overall type coverage",
                good_coverage,
                f"{types_received}/{len(expected_types)} types ({coverage_pct:.1f}%)"
            )
            
            return good_coverage
            
        except Exception as e:
            self.print_test("Message types test", False, f"Exception: {e}")
            return False
    
    def test_data_freshness(self):
        """Test 6: Data freshness (messages arriving in real-time)"""
        self.print_header("TEST 6: Data Freshness Test")
        
        try:
            # Get initial message count
            response1 = requests.get(f"{self.base_url}/api/telemetry/status", timeout=5)
            if response1.status_code != 200:
                self.print_test("Get initial count", False, "Status endpoint failed")
                return False
            
            count1 = response1.json().get('total_messages', 0)
            print(f"   Initial message count: {count1}")
            
            # Wait a few seconds
            print(f"   ⏳ Waiting 5 seconds for new messages...")
            time.sleep(5)
            
            # Get new message count
            response2 = requests.get(f"{self.base_url}/api/telemetry/status", timeout=5)
            if response2.status_code != 200:
                self.print_test("Get new count", False, "Status endpoint failed")
                return False
            
            count2 = response2.json().get('total_messages', 0)
            print(f"   New message count: {count2}")
            
            messages_received = count2 - count1
            receiving_data = messages_received > 0
            
            self.print_test(
                "New messages arrived",
                receiving_data,
                f"Received {messages_received} messages in 5 seconds"
            )
            
            # Expected rate: Arduino sends ~1 message/sec, so we should get ~5 messages
            if receiving_data:
                rate = messages_received / 5.0
                good_rate = rate >= 0.5  # At least 0.5 messages/sec
                self.print_test(
                    "Message rate acceptable",
                    good_rate,
                    f"Rate: {rate:.2f} messages/sec"
                )
            
            return receiving_data
            
        except Exception as e:
            self.print_test("Data freshness test", False, f"Exception: {e}")
            return False
    
    def test_database_logging(self):
        """Test 7: Database logging functionality"""
        self.print_header("TEST 7: Database Logging")
        
        import os
        import sqlite3
        
        db_path = 'telemetry_logs.db'
        
        try:
            # Check if database exists
            db_exists = os.path.exists(db_path)
            self.print_test(
                "Database file exists",
                db_exists,
                f"Path: {db_path}"
            )
            
            if not db_exists:
                return False
            
            # Connect and check tables
            conn = sqlite3.connect(db_path)
            cursor = conn.cursor()
            
            # Check sessions table
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='sessions'")
            has_sessions = cursor.fetchone() is not None
            self.print_test("Sessions table exists", has_sessions)
            
            # Check raw_messages table
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='raw_messages'")
            has_messages = cursor.fetchone() is not None
            self.print_test("Raw messages table exists", has_messages)
            
            # Check statistics table
            cursor.execute("SELECT name FROM sqlite_master WHERE type='table' AND name='statistics'")
            has_stats = cursor.fetchone() is not None
            self.print_test("Statistics table exists", has_stats)
            
            if has_sessions:
                # Check for active session
                cursor.execute("SELECT COUNT(*) FROM sessions")
                session_count = cursor.fetchone()[0]
                self.print_test(
                    "Session records exist",
                    session_count > 0,
                    f"Count: {session_count}"
                )
            
            if has_messages:
                # Check message count
                cursor.execute("SELECT COUNT(*) FROM raw_messages")
                msg_count = cursor.fetchone()[0]
                self.print_test(
                    "Messages logged to database",
                    msg_count > 0,
                    f"Count: {msg_count}"
                )
                
                # Check recent messages
                cursor.execute("""
                    SELECT timestamp, message_type 
                    FROM raw_messages 
                    ORDER BY timestamp DESC 
                    LIMIT 5
                """)
                recent = cursor.fetchall()
                if recent:
                    print(f"\n   📝 Recent database entries:")
                    for ts, msg_type in recent:
                        print(f"      • {ts}: Type 0x{msg_type:02X}")
            
            conn.close()
            return has_sessions and has_messages
            
        except Exception as e:
            self.print_test("Database logging test", False, f"Exception: {e}")
            return False
    
    def test_arduino_commands(self):
        """Test 8: Arduino command interface via Emergency Diagnostic"""
        self.print_header("TEST 8: Arduino Command Interface")
        
        try:
            # Test if the emergency diagnostic API exists
            # Note: We won't actually send commands without user confirmation
            response = requests.get(f"{self.base_url}/emergency_diagnostic", timeout=5)
            
            endpoint_exists = response.status_code == 200
            self.print_test(
                "Emergency diagnostic page exists",
                endpoint_exists,
                f"Status: {response.status_code}"
            )
            
            # Check if run_command API exists
            # We'll just check the endpoint exists, not execute commands
            print(f"\n   ⚠️  Command execution not tested (requires manual confirmation)")
            print(f"      Available commands: ping, show, status, help, mesh")
            
            return endpoint_exists
            
        except Exception as e:
            self.print_test("Arduino command test", False, f"Exception: {e}")
            return False
    
    def print_summary(self):
        """Print test summary"""
        self.print_header("TEST SUMMARY")
        
        total_tests = self.tests_passed + self.tests_failed
        pass_rate = (self.tests_passed / total_tests * 100) if total_tests > 0 else 0
        
        print(f"   Total Tests: {total_tests}")
        print(f"   {Colors.OKGREEN}✅ Passed: {self.tests_passed}{Colors.ENDC}")
        print(f"   {Colors.FAIL}❌ Failed: {self.tests_failed}{Colors.ENDC}")
        print(f"   Pass Rate: {pass_rate:.1f}%\n")
        
        if pass_rate >= 90:
            status_color = Colors.OKGREEN
            status_icon = "✅"
            status_text = "EXCELLENT"
        elif pass_rate >= 75:
            status_color = Colors.WARNING
            status_icon = "⚠️"
            status_text = "GOOD"
        else:
            status_color = Colors.FAIL
            status_icon = "❌"
            status_text = "NEEDS ATTENTION"
        
        print(f"   {status_color}{status_icon} Overall Status: {status_text}{Colors.ENDC}\n")
        
        # Failed tests
        if self.tests_failed > 0:
            print(f"   {Colors.FAIL}Failed Tests:{Colors.ENDC}")
            for result in self.test_results:
                if not result['passed']:
                    print(f"      • {result['name']}")
                    if result['details']:
                        print(f"        {result['details']}")
            print()
    
    def run_all_tests(self):
        """Run complete test suite"""
        print(f"\n{Colors.BOLD}{Colors.OKCYAN}")
        print("╔══════════════════════════════════════════════════════════════════════════════╗")
        print("║          LOGSPLITTER MESH → LCARS INTEGRATION TEST SUITE                    ║")
        print("║                     Arduino → Meshtastic → LCARS                             ║")
        print("╚══════════════════════════════════════════════════════════════════════════════╝")
        print(f"{Colors.ENDC}")
        
        # Run test sequence
        server_ok = self.test_server_reachable()
        if not server_ok:
            print(f"\n{Colors.FAIL}❌ Server not reachable. Cannot continue tests.{Colors.ENDC}")
            return False
        
        self.test_telemetry_status_endpoint()
        self.test_telemetry_latest_endpoint()
        self.test_telemetry_stream_endpoint()
        self.test_message_types_coverage()
        self.test_data_freshness()
        self.test_database_logging()
        self.test_arduino_commands()
        
        self.print_summary()
        
        return self.tests_failed == 0

def main():
    """Main test runner"""
    import argparse
    
    parser = argparse.ArgumentParser(description='Test LCARS mesh integration')
    parser.add_argument('--url', default='http://localhost:3000',
                       help='LCARS server URL (default: http://localhost:3000)')
    args = parser.parse_args()
    
    tester = LCARSMeshTester(base_url=args.url)
    
    try:
        success = tester.run_all_tests()
        sys.exit(0 if success else 1)
    except KeyboardInterrupt:
        print(f"\n\n{Colors.WARNING}⚠️  Tests interrupted by user{Colors.ENDC}")
        sys.exit(2)
    except Exception as e:
        print(f"\n\n{Colors.FAIL}❌ Test suite error: {e}{Colors.ENDC}")
        sys.exit(3)

if __name__ == '__main__':
    main()
