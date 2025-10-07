#pragma once

#include <WiFiS3.h>
#include <ArduinoMqttClient.h>
#include <WiFiUdp.h>
#include "constants.h"

// Network connection states
enum class WiFiState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

enum class MQTTState {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

class NetworkManager {
public:
    NetworkManager();
    
    void begin();
    void update();
    
    // Connection status
    bool isWiFiConnected() const;
    bool isMQTTConnected() const;
    bool isConnected() const;
    bool isStable() const;
    
    // Publishing
    bool publish(const char* topic, const char* payload);
    bool publishWithRetain(const char* topic, const char* payload);
    
    // Syslog functionality
    bool sendSyslog(const char* message, int level = 6);  // Default to INFO level
    void setSyslogServer(const char* server, int port = 514);  // Standard syslog port
    
    // Live reconfiguration methods
    bool reconfigureMQTT(const char* brokerHost, int brokerPort, const char* username, const char* password);
    bool reconfigureSyslog(const char* server, int port);
    bool reconfigureWiFi(const char* ssid, const char* password);
    
    // Configuration getters
    const char* getSyslogServer() const { return syslogServer; }
    int getSyslogPort() const { return syslogPort; }
    
    // Status reporting
    void getHealthString(char* buffer, size_t bufferSize);
    unsigned long getConnectionUptime() const;
    uint16_t getDisconnectCount() const;
    uint16_t getFailedPublishCount() const;
    
    // Hostname configuration
    void setHostname(const char* hostname);
    const char* getHostname() const;

private:
    // WiFi and MQTT clients
    WiFiClient wifiClient;
    MqttClient mqttClient;
    WiFiUDP udpClient;
    
    // Connection state
    WiFiState wifiState;
    MQTTState mqttState;
    
    // Connection management
    void startWiFiConnection();
    void startMQTTConnection();
    void updateConnectionHealth();
    void onMqttMessage(int messageSize);
    
    // Retry logic
    unsigned long lastConnectAttempt;
    uint8_t wifiRetries;
    uint8_t mqttRetries;
    
    // Health tracking
    unsigned long connectionUptime;
    uint16_t disconnectCount;
    uint16_t failedPublishCount;
    bool connectionStable;
    
    // Syslog configuration
    char syslogServer[64];  // Increased size for longer hostnames
    int syslogPort;
    
    // Hostname
    char hostname[32];
    
    // Allow access to onMqttMessage from static callback
    friend void onMqttMessageStatic(int messageSize);
};

// Static callback function declaration
void onMqttMessageStatic(int messageSize);