# Setting up Arduino Secrets

This project requires a secrets file containing your WiFi and MQTT credentials.

## Setup Instructions

1. Copy the template file:
   ```
   cp include/arduino_secrets.h.template include/arduino_secrets.h
   ```

2. Edit `include/arduino_secrets.h` with your actual credentials:
   - Replace `your_wifi_ssid` with your WiFi network name
   - Replace `your_wifi_password` with your WiFi password
   - Update `MQTT_BROKER_HOST` with your MQTT broker address (default: 192.168.1.155)
   - Replace `your_mqtt_username` with your MQTT username
   - Replace `your_mqtt_password` with your MQTT password
   - Update `SYSLOG_SERVER_HOST` with your syslog server address (default: 192.168.1.155)

## Security Note

The `arduino_secrets.h` file is excluded from git commits to protect your credentials. Never commit this file to version control.

## Example Configuration

```cpp
// WiFi Configuration
#define SECRET_SSID "MyHomeWiFi"
#define SECRET_PASS "MySecurePassword123"

// MQTT Configuration
#define MQTT_BROKER_HOST "192.168.1.155"
#define MQTT_BROKER_PORT 1883
#define MQTT_USER "myusername"
#define MQTT_PASS "mypassword"

// Syslog Configuration
#define SYSLOG_SERVER_HOST "192.168.1.155"
#define SYSLOG_SERVER_PORT 514
```