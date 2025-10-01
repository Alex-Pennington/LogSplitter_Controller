#!/usr/bin/env python3
"""
Simple telnet client for testing Arduino commands
Usage: python telnet_test.py <host> <port> <command>
Example: python telnet_test.py 192.168.1.170 23 "temp debug on"
"""

import socket
import sys
import time

def send_telnet_command(host, port, command):
    try:
        # Create socket connection
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            sock.settimeout(5)  # 5 second timeout
            
            print(f"Connecting to {host}:{port}...")
            sock.connect((host, int(port)))
            
            # Wait a moment for connection to establish
            time.sleep(0.1)
            
            # Send command followed by newline
            command_bytes = (command + '\r\n').encode('utf-8')
            print(f"Sending: {command}")
            sock.send(command_bytes)
            
            # Wait for response
            time.sleep(0.5)
            
            # Receive response
            response = sock.recv(1024).decode('utf-8', errors='ignore')
            print(f"Response: {response.strip()}")
            
            return True
            
    except socket.timeout:
        print("Connection timed out")
        return False
    except ConnectionRefusedError:
        print("Connection refused - is the device online?")
        return False
    except Exception as e:
        print(f"Error: {e}")
        return False

if __name__ == "__main__":
    if len(sys.argv) != 4:
        print("Usage: python telnet_test.py <host> <port> <command>")
        print('Example: python telnet_test.py 192.168.1.170 23 "temp debug on"')
        sys.exit(1)
    
    host = sys.argv[1]
    port = sys.argv[2]
    command = sys.argv[3]
    
    send_telnet_command(host, port, command)