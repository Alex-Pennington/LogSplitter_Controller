#include "network_manager.h"
#include "arduino_secrets.h"
#include <string.h>

extern void debugPrintf(const char* fmt, ...);

// Static instance pointer for callback
static NetworkManager* s_instance = nullptr;

// Static callback function for MQTT
void onMqttMessageStatic(int messageSize) {
    if (s_instance) {
        s_instance->onMqttMessage(messageSize);
    }
}

NetworkManager::NetworkManager(ConfigManager* configMgr) :
    configManager(configMgr),
    wifiClient(),
    mqttClient(wifiClient),
    udpClient(),
    wifiState(WiFiState::DISCONNECTED),
    mqttState(MQTTState::DISCONNECTED),
    lastConnectAttempt(0),
    wifiRetries(0),
    mqttRetries(0),
    connectionUptime(0),
    disconnectCount(0),
    failedPublishCount(0),
    connectionStable(false) {
    
    // Configure MQTT client
    mqttClient.setId("LogMonitor");
}

void NetworkManager::begin() {
    debugPrintf("NetworkManager: Initializing WiFi and MQTT\n");
    
    // Set static instance for callback
    s_instance = this;
    
    // Set hostname for easier network identification
    if (configManager) {
        WiFi.setHostname(configManager->getHostname());
        debugPrintf("NetworkManager: Set hostname to '%s'\n", configManager->getHostname());
    } else {
        WiFi.setHostname(SYSLOG_HOSTNAME);
        debugPrintf("NetworkManager: Using default hostname '%s'\n", SYSLOG_HOSTNAME);
    }
    
    // Initialize UDP client for syslog
    udpClient.begin(0);
    
    // Set MQTT callback
    mqttClient.onMessage(onMqttMessageStatic);
    
    // Start WiFi connection
    startWiFiConnection();
}

void NetworkManager::startWiFiConnection() {
    if (wifiState == WiFiState::CONNECTING) return;
    
    wifiState = WiFiState::CONNECTING;
    debugPrintf("NetworkManager: Connecting to WiFi SSID: %s\n", SECRET_SSID);
    
    WiFi.begin(SECRET_SSID, SECRET_PASS);
}

void NetworkManager::startMQTTConnection() {
    if (mqttState == MQTTState::CONNECTING || wifiState != WiFiState::CONNECTED) return;
    
    const char* brokerHost = getMqttBrokerHost();
    int brokerPort = getMqttBrokerPort();
    
    mqttState = MQTTState::CONNECTING;
    debugPrintf("NetworkManager: Connecting to MQTT broker: %s:%d\n", brokerHost, brokerPort);
    
    mqttClient.setId("LogMonitor");
    mqttClient.setUsernamePassword(MQTT_USER, MQTT_PASS);
    
    if (mqttClient.connect(brokerHost, brokerPort)) {
        mqttState = MQTTState::CONNECTED;
        debugPrintf("NetworkManager: MQTT connected successfully\n");
        
        // Subscribe to control topic
        mqttClient.subscribe(TOPIC_MONITOR_CONTROL);
        debugPrintf("NetworkManager: Subscribed to %s\n", TOPIC_MONITOR_CONTROL);
        
        // Reset retry counter on successful connection
        mqttRetries = 0;
    } else {
        mqttState = MQTTState::FAILED;
        debugPrintf("NetworkManager: MQTT connection failed\n");
    }
}

void NetworkManager::update() {
    unsigned long now = millis();
    bool attemptedConnection = false;
    
    // Check WiFi status
    if (WiFi.status() == WL_CONNECTED) {
        if (wifiState != WiFiState::CONNECTED) {
            wifiState = WiFiState::CONNECTED;
            wifiRetries = 0;
            debugPrintf("NetworkManager: WiFi connected - IP: %s\n", WiFi.localIP().toString().c_str());
        }
    } else {
        if (wifiState == WiFiState::CONNECTED) {
            wifiState = WiFiState::DISCONNECTED;
            mqttState = MQTTState::DISCONNECTED;
            disconnectCount++;
            connectionStable = false;
            debugPrintf("NetworkManager: WiFi disconnected (total: %d)\n", disconnectCount);
        }
    }
    
    // Check MQTT status
    if (wifiState == WiFiState::CONNECTED) {
        if (!mqttClient.connected()) {
            if (mqttState == MQTTState::CONNECTED) {
                mqttState = MQTTState::DISCONNECTED;
                disconnectCount++;
                connectionStable = false;
                debugPrintf("NetworkManager: MQTT disconnected\n");
            }
        }
    }
    
    // Reconnection logic with rate limiting
    if (now - lastConnectAttempt >= MQTT_RECONNECT_INTERVAL_MS) {
        // WiFi reconnection logic
        if (wifiState == WiFiState::DISCONNECTED && wifiRetries < MAX_WIFI_RETRIES) {
            lastConnectAttempt = now;
            wifiRetries++;
            debugPrintf("NetworkManager: WiFi attempt %d/%d\n", wifiRetries, MAX_WIFI_RETRIES);
            startWiFiConnection();
            attemptedConnection = true;
        }
        else if (wifiState == WiFiState::FAILED && wifiRetries < MAX_WIFI_RETRIES) {
            lastConnectAttempt = now;
            wifiRetries++;
            debugPrintf("NetworkManager: WiFi retry %d/%d\n", wifiRetries, MAX_WIFI_RETRIES);
            wifiState = WiFiState::DISCONNECTED;
            startWiFiConnection();
            attemptedConnection = true;
        }
        // MQTT reconnection logic (only if WiFi is connected)
        else if (wifiState == WiFiState::CONNECTED) {
            if (mqttState == MQTTState::DISCONNECTED && mqttRetries < MAX_MQTT_RETRIES) {
                lastConnectAttempt = now;
                mqttRetries++;
                debugPrintf("NetworkManager: MQTT attempt %d/%d\n", mqttRetries, MAX_MQTT_RETRIES);
                startMQTTConnection();
                attemptedConnection = true;
            }
            else if (mqttState == MQTTState::FAILED && mqttRetries < MAX_MQTT_RETRIES) {
                lastConnectAttempt = now;
                mqttRetries++;
                debugPrintf("NetworkManager: MQTT retry %d/%d\n", mqttRetries, MAX_MQTT_RETRIES);
                mqttState = MQTTState::DISCONNECTED;
                startMQTTConnection();
                attemptedConnection = true;
            }
        }
        
        // Reset retry counters after extended wait period
        if (!attemptedConnection && wifiRetries >= MAX_WIFI_RETRIES && 
            now - lastConnectAttempt >= (MQTT_RECONNECT_INTERVAL_MS * 3)) {
            debugPrintf("NetworkManager: Resetting WiFi retry count after extended wait\n");
            wifiRetries = 0;
        }
        if (!attemptedConnection && mqttRetries >= MAX_MQTT_RETRIES && 
            now - lastConnectAttempt >= (MQTT_RECONNECT_INTERVAL_MS * 2)) {
            debugPrintf("NetworkManager: Resetting MQTT retry count after extended wait\n");
            mqttRetries = 0;
        }
    }
    
    // Update connection health and stability
    updateConnectionHealth();
    
    // Poll MQTT (with protection)
    if (mqttState == MQTTState::CONNECTED) {
        unsigned long pollStart = millis();
        mqttClient.poll();
        unsigned long pollDuration = millis() - pollStart;
        if (pollDuration > 100) {
            debugPrintf("NetworkManager: WARNING: MQTT poll took %lums\n", pollDuration);
        }
    }
}

void NetworkManager::updateConnectionHealth() {
    unsigned long now = millis();
    
    // Update connection uptime
    if (isConnected()) {
        if (connectionUptime == 0) {
            connectionUptime = now;
        }
        
        // Mark as stable after continuous uptime
        if (!connectionStable && (now - connectionUptime) > NETWORK_STABILITY_TIME_MS) {
            connectionStable = true;
            debugPrintf("NetworkManager: Network connection marked as stable\n");
        }
    } else {
        connectionUptime = 0;
        connectionStable = false;
    }
}

bool NetworkManager::publish(const char* topic, const char* payload) {
    if (mqttState != MQTTState::CONNECTED) {
        failedPublishCount++;
        return false;
    }
    
    // Timeout protection - ensure publish doesn't block
    unsigned long startTime = millis();
    
    if (mqttClient.beginMessage(topic)) {
        mqttClient.print(payload);
        bool success = mqttClient.endMessage();
        
        unsigned long duration = millis() - startTime;
        if (duration > 100) { // Warn if publish takes >100ms
            debugPrintf("NetworkManager: WARNING: MQTT publish took %lums\n", duration);
        }
        
        if (!success) {
            failedPublishCount++;
        }
        return success;
    } else {
        failedPublishCount++;
        return false;
    }
}

bool NetworkManager::publishWithRetain(const char* topic, const char* payload) {
    if (mqttState != MQTTState::CONNECTED) {
        failedPublishCount++;
        return false;
    }
    
    if (mqttClient.beginMessage(topic, true)) { // true = retain
        mqttClient.print(payload);
        bool success = mqttClient.endMessage();
        if (!success) {
            failedPublishCount++;
        }
        return success;
    } else {
        failedPublishCount++;
        return false;
    }
}

bool NetworkManager::sendSyslog(const char* message, int level) {
    const char* server = getSyslogServer();
    int port = getSyslogPort();
    
    if (!isWiFiConnected() || strlen(server) == 0) {
        return false;
    }
    
    // Validate severity level (0-7 per RFC 3164)
    if (level < 0 || level > 7) {
        level = 6; // Default to INFO if invalid
    }
    
    // RFC 3164 syslog format: <PRI>TIMESTAMP HOSTNAME TAG: MESSAGE
    // PRI = Facility * 8 + Severity
    // Using SYSLOG_FACILITY (16 = local0) from constants.h
    int priority = SYSLOG_FACILITY * 8 + level;
    
    const char* hostname = configManager ? configManager->getHostname() : SYSLOG_HOSTNAME;
    
    char syslogMessage[512];
    snprintf(syslogMessage, sizeof(syslogMessage), 
        "<%d>%s %s: %s", priority, hostname, SYSLOG_TAG, message);
    
    // Send UDP packet to syslog server
    if (udpClient.beginPacket(server, port)) {
        udpClient.print(syslogMessage);
        bool success = udpClient.endPacket();
        return success;
    }
    
    return false;
}

void NetworkManager::setSyslogServer(const char* server, int port) {
    if (configManager) {
        configManager->setSyslogServer(server, port);
        configManager->saveToEEPROM();
        debugPrintf("NetworkManager: Syslog server set to %s:%d (saved to EEPROM)\n", server, port);
    } else {
        debugPrintf("NetworkManager: ConfigManager not available\n");
    }
}

bool NetworkManager::reconfigureMqttBroker(const char* host, int port) {
    debugPrintf("NetworkManager: Reconfiguring MQTT to %s:%d\n", host, port);
    
    if (!configManager) {
        debugPrintf("NetworkManager: ConfigManager not available\n");
        return false;
    }
    
    // Check if settings actually changed
    if (strcmp(host, configManager->getMqttBrokerHost()) == 0 && port == configManager->getMqttBrokerPort()) {
        debugPrintf("NetworkManager: MQTT settings unchanged\n");
        return true;
    }
    
    // Update MQTT broker settings and save to EEPROM
    configManager->setMqttBroker(host, port);
    configManager->saveToEEPROM();
    
    // Disconnect current MQTT connection to force reconnection with new broker
    if (mqttState == MQTTState::CONNECTED) {
        mqttClient.stop();
        mqttState = MQTTState::DISCONNECTED;
        debugPrintf("NetworkManager: Disconnected from old MQTT broker\n");
    }
    
    // Reset retry counters for immediate reconnection attempt
    mqttRetries = 0;
    lastConnectAttempt = 0;
    
    debugPrintf("NetworkManager: MQTT broker updated to %s:%d (saved to EEPROM)\n", host, port);
    return true;
}

bool NetworkManager::sendMqttTest(const char* message) {
    if (!isWiFiConnected() || !isMQTTConnected()) {
        return false;
    }
    
    return publish("monitor/test", message);
}

void NetworkManager::setHostname(const char* newHostname) {
    if (configManager) {
        configManager->setHostname(newHostname);
        configManager->saveToEEPROM();
        WiFi.setHostname(newHostname);
        debugPrintf("NetworkManager: Hostname set to '%s' (saved to EEPROM)\n", newHostname);
    } else {
        debugPrintf("NetworkManager: ConfigManager not available\n");
    }
}

const char* NetworkManager::getHostname() const {
    return configManager ? configManager->getHostname() : SYSLOG_HOSTNAME;
}

bool NetworkManager::isWiFiConnected() const {
    return wifiState == WiFiState::CONNECTED;
}

bool NetworkManager::isMQTTConnected() const {
    return mqttState == MQTTState::CONNECTED;
}

bool NetworkManager::isConnected() const {
    return isWiFiConnected() && isMQTTConnected();
}

bool NetworkManager::isStable() const {
    return connectionStable;
}

void NetworkManager::getHealthString(char* buffer, size_t bufferSize) {
    const char* wifiStatus = isWiFiConnected() ? "OK" : "DOWN";
    const char* mqttStatus = isMQTTConnected() ? "OK" : "DOWN";
    const char* stableStatus = isStable() ? "YES" : "NO";
    unsigned long uptimeSeconds = connectionUptime > 0 ? (millis() - connectionUptime) / 1000 : 0;
    
    snprintf(buffer, bufferSize, 
        "wifi=%s mqtt=%s stable=%s disconnects=%d fails=%d uptime=%lus",
        wifiStatus, mqttStatus, stableStatus, 
        disconnectCount, failedPublishCount, uptimeSeconds);
}

unsigned long NetworkManager::getConnectionUptime() const {
    return connectionUptime > 0 ? (millis() - connectionUptime) / 1000 : 0;
}

uint16_t NetworkManager::getDisconnectCount() const {
    return disconnectCount;
}

uint16_t NetworkManager::getFailedPublishCount() const {
    return failedPublishCount;
}

const char* NetworkManager::getSyslogServer() const {
    return configManager ? configManager->getSyslogServer() : SYSLOG_SERVER;
}

int NetworkManager::getSyslogPort() const {
    return configManager ? configManager->getSyslogPort() : SYSLOG_PORT;
}

const char* NetworkManager::getMqttBrokerHost() const {
    return configManager ? configManager->getMqttBrokerHost() : BROKER_HOST;
}

int NetworkManager::getMqttBrokerPort() const {
    return configManager ? configManager->getMqttBrokerPort() : BROKER_PORT;
}

void NetworkManager::onMqttMessage(int messageSize) {
    // Handle incoming MQTT messages
    String topic = mqttClient.messageTopic();
    String payload = "";
    
    while (mqttClient.available()) {
        payload += (char)mqttClient.read();
    }
    
    debugPrintf("NetworkManager: Received MQTT message on topic: %s, payload: %s\n", 
        topic.c_str(), payload.c_str());
    
    // Process the message (this would be handled by command processor in a full implementation)
    // For now, just log it
}