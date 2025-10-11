# LogSplitter Deployment Guide

## Overview

This guide provides step-by-step instructions for deploying the complete LogSplitter industrial control system in a production environment.

## Prerequisites

### Hardware Requirements

#### Controller Unit
- **Arduino UNO R4 WiFi** (main control unit)
- **Relay modules** (for hydraulic control)
- **Pressure sensors** (analog inputs A1, A5)
- **Safety input switches** (digital pins)
- **Status LED** (pin 13, built-in)

#### Integrated Controller  
- **Arduino UNO R4 WiFi** (unified control and monitoring)
- **NAU7802 Load Cell Amplifier** (I2C via Qwiic)
- **Load cell** (weight measurement)
- **LCD2004A Display** (20x4 I2C LCD)
- **MAX6656 Temperature Sensor** (I2C)

#### Network Infrastructure
- **WiFi Router** with internet access
- **MQTT Broker** (e.g., Mosquitto)
- **Syslog Server** (e.g., rsyslog on Linux)
- **Network Management Tools** (DHCP, DNS)

### Software Requirements
- **PlatformIO** (Arduino IDE alternative)
- **Git** (version control)
- **Text Editor** (VS Code recommended)
- **Network Tools** (telnet, MQTT client)

## Network Configuration

### 1. WiFi Network Setup

Configure your network infrastructure:

```bash
# Example network configuration
Network SSID: LogSplitter_Network
Security: WPA2/WPA3
IP Range: 192.168.1.0/24
Gateway: 192.168.1.1
DNS: 192.168.1.1, 8.8.8.8
```

### 2. MQTT Broker Setup

Install and configure MQTT broker:

```bash
# Install Mosquitto (Ubuntu/Debian)
sudo apt update
sudo apt install mosquitto mosquitto-clients

# Configure mosquitto
sudo nano /etc/mosquitto/mosquitto.conf
```

Mosquitto configuration:
```conf
# /etc/mosquitto/mosquitto.conf
port 1883
allow_anonymous true
listener 1883
persistence true
persistence_location /var/lib/mosquitto/
log_dest file /var/log/mosquitto/mosquitto.log
```

Start MQTT broker:
```bash
sudo systemctl start mosquitto
sudo systemctl enable mosquitto
```

### 3. Syslog Server Setup

Configure rsyslog for centralized logging:

```bash
# Install rsyslog (usually pre-installed)
sudo apt install rsyslog

# Configure for UDP reception
sudo nano /etc/rsyslog.conf
```

Rsyslog configuration:
```conf
# /etc/rsyslog.conf
# Enable UDP syslog reception
$ModLoad imudp
$UDPServerRun 514
$UDPServerAddress 0.0.0.0

# LogSplitter specific logging
:hostname, isequal, "LogSplitter" /var/log/logsplitter-controller.log
:hostname, isequal, "LogSplitter" /var/log/logsplitter.log
& stop
```

Restart rsyslog:
```bash
sudo systemctl restart rsyslog
sudo systemctl enable rsyslog
```

## Firmware Deployment

### 1. Prepare Source Code

Clone and configure the repository:

```bash
# Clone repository
git clone <repository-url> LogSplitter
cd LogSplitter

# Configure WiFi credentials
cp include/arduino_secrets.h.template include/arduino_secrets.h
```

Edit secrets files:
```cpp
// include/arduino_secrets.h
#define SECRET_SSID "LogSplitter_Network"
#define SECRET_PASS "your_wifi_password"
#define MQTT_BROKER_HOST "192.168.1.155"
#define MQTT_PORT 1883
#define MQTT_USER "logsplitter"
#define MQTT_PASS "mqtt_password"
#define SYSLOG_SERVER_HOST "192.168.1.155"
#define SYSLOG_PORT 514
```

### 2. Build and Upload Controller

```bash
# Controller files are in root directory

# Build firmware
pio run

# Upload to Arduino (connect via USB)
pio run --target upload

# Monitor serial output
pio device monitor
```

Expected controller output:
```
[1234] [INFO] System initializing...
[1456] [INFO] WiFi connecting to LogSplitter_Network
[2345] [INFO] WiFi connected: 192.168.1.100
[2567] [INFO] MQTT connecting to 192.168.1.155:1883
[2789] [INFO] MQTT connected as LogSplitter-12345
[3000] [INFO] Telnet server started on port 23
[3001] [INFO] Logger initialized with syslog server
[3002] [INFO] System ready - entering main loop
```

### 3. Monitor System Output

```bash
# Monitor serial output
pio device monitor
```

Expected system output:
```
[1234] [INFO] System initializing...
[1456] [INFO] WiFi connecting to LogSplitter_Network
[2345] [INFO] WiFi connected: 192.168.1.100
[2567] [INFO] MQTT connecting to 192.168.1.155:1883
[2789] [INFO] MQTT connected as LogSplitter-12345
[3000] [INFO] TCA9548A initialized on Wire1
[3100] [INFO] LCD display initialized on channel 7
[3200] [INFO] Sensors initialized and operational
[3300] [INFO] System ready - entering main loop
```

## System Verification

### 1. Network Connectivity Test

Test WiFi and MQTT connections:

```bash
# Test controller telnet
telnet 192.168.1.100 23
> network
wifi=OK mqtt=OK stable=YES disconnects=0 uptime=123s

# Test system telnet  
telnet 192.168.1.100 23
> network
wifi=OK mqtt=OK stable=YES disconnects=0 uptime=456s
```

### 2. MQTT Communication Test

Verify MQTT pub/sub:

```bash
# Subscribe to all topics
mosquitto_sub -h 192.168.1.155 -t "r4/+/+"

# Should see periodic messages:
r4/controller/heartbeat {"uptime": 123, "status": "OK"}
r4/controller/heartbeat {"uptime": 456, "status": "OK"}
r4/controller/temperature 23.5
r4/controller/weight 0.0
```

### 3. Syslog Verification

Check syslog reception:

```bash
# Monitor syslog files
sudo tail -f /var/log/logsplitter.log

# Should see RFC 3164 formatted messages:
<134>Oct  1 12:34:56 LogSplitter logsplitter: System ready - entering main loop
<134>Oct  1 12:35:01 LogSplitter logsplitter: Temperature reading: 23.5C
```

### 4. System Integration Test

Test complete system functionality:

```bash
# Controller tests
telnet 192.168.1.100 23
> test all
> relay R1 ON
> relay R1 OFF
> loglevel 7        # Enable debug logging
> show

# System tests
telnet 192.168.1.101 23
> test sensors
> weight read
> temp read
> loglevel 6        # Set to INFO level
> show
```

## Production Configuration

### 1. Security Hardening

#### Network Security
```bash
# Configure firewall (UFW example)
sudo ufw enable
sudo ufw allow 1883/tcp    # MQTT
sudo ufw allow 514/udp     # Syslog
sudo ufw allow 23/tcp      # Telnet (restrict to management network)
sudo ufw allow ssh
```

#### MQTT Authentication
```conf
# /etc/mosquitto/mosquitto.conf
allow_anonymous false
password_file /etc/mosquitto/passwd
acl_file /etc/mosquitto/acl
```

Create MQTT users:
```bash
sudo mosquitto_passwd -c /etc/mosquitto/passwd logsplitter
sudo mosquitto_passwd /etc/mosquitto/passwd logmonitor
```

#### Access Control List
```conf
# /etc/mosquitto/acl
user logsplitter
topic write r4/controller/#
topic read r4/controller/#
```

### 2. Monitoring Setup

#### Log Rotation
```conf
# /etc/logrotate.d/logsplitter
/var/log/logsplitter-*.log {
    daily
    missingok
    rotate 30
    compress
    delaycompress
    notifempty
    copytruncate
}
```

#### System Monitoring
```bash
# Monitor MQTT broker
sudo systemctl status mosquitto

# Monitor syslog
sudo systemctl status rsyslog

# Check log files
sudo ls -la /var/log/logsplitter-*
```

### 3. Backup and Recovery

#### Configuration Backup
```bash
# Create backup directory
sudo mkdir -p /backup/logsplitter

# Backup configurations
sudo cp /etc/mosquitto/* /backup/logsplitter/
sudo cp /etc/rsyslog.conf /backup/logsplitter/
sudo cp /etc/rsyslog.d/* /backup/logsplitter/

# Backup firmware
cp .pio/build/uno_r4_wifi/firmware.bin /backup/logsplitter/controller-firmware.bin
cp .pio/build/uno_r4_wifi/firmware.bin /backup/logsplitter/controller-firmware.bin
```

#### Recovery Procedures
```bash
# Restore MQTT configuration
sudo cp /backup/logsplitter/mosquitto.conf /etc/mosquitto/
sudo systemctl restart mosquitto

# Restore syslog configuration
sudo cp /backup/logsplitter/rsyslog.conf /etc/rsyslog.conf
sudo systemctl restart rsyslog

# Reflash firmware (connect Arduino via USB)
cd controller && pio run --target upload
cd monitor && pio run --target upload
```

## Troubleshooting

### Common Issues

#### WiFi Connection Problems
```bash
# Check WiFi credentials in arduino_secrets.h
# Verify network SSID and password
# Check DHCP server functionality
# Verify signal strength at installation location
```

#### MQTT Connection Issues
```bash
# Test MQTT broker connectivity
mosquitto_pub -h 192.168.1.155 -t test -m "hello"
mosquitto_sub -h 192.168.1.155 -t test

# Check firewall settings
sudo ufw status
sudo netstat -ln | grep 1883
```

#### Syslog Not Received
```bash
# Test UDP reception
sudo tcpdump -i any port 514

# Check rsyslog configuration
sudo rsyslogd -N1
sudo systemctl status rsyslog

# Verify log file permissions
sudo ls -la /var/log/logsplitter-*
```

### Debug Commands

#### System Diagnostics
```bash
# Controller diagnostics
telnet 192.168.1.100 23
> network           # Show network status
> syslog status     # Show syslog configuration  
> test all          # Run all system tests
> loglevel 7        # Enable debug output

# Monitor diagnostics
telnet 192.168.1.101 23
> test sensors      # Test sensor readings
> test network      # Test network connectivity
> weight status     # Show weight sensor status
> temp status       # Show temperature sensor status
```

## Maintenance

### Regular Tasks

#### Daily
- Check system status via telnet
- Verify MQTT message flow
- Monitor syslog for errors

#### Weekly  
- Review log files for anomalies
- Test emergency stop procedures
- Backup configuration files

#### Monthly
- Update firmware if needed
- Review network security logs
- Test backup and recovery procedures

### Performance Monitoring

#### Key Metrics
- **Uptime**: Both units should maintain >99% uptime
- **Network Latency**: MQTT messages <100ms
- **Memory Usage**: <80% RAM utilization
- **Log Volume**: Monitor for excessive DEBUG logging

#### Alerting Setup
```bash
# Example log monitoring with logwatch
sudo apt install logwatch
sudo nano /etc/logwatch/conf/logfiles/logsplitter.conf
```

---

This deployment guide ensures a robust, secure, and maintainable LogSplitter installation suitable for industrial environments.