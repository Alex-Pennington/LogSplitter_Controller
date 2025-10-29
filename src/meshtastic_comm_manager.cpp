/**
 * Meshtastic Communication Manager Implementation
 * 
 * Complete implementation of Meshtastic mesh networking communication
 * for LogSplitter Controller. Handles all telemetry, commands, and
 * emergency alerts via binary protobuf messaging.
 */

#include "meshtastic_comm_manager.h"
#include "logger.h"
#include "config_manager.h"

// Function declarations
int freeMemory();

// Global instance
MeshtasticCommManager meshtasticComm;

MeshtasticCommManager::MeshtasticCommManager() {
    meshtasticSerial = nullptr;
    errorManager = nullptr;
    commandProcessor = nullptr;
    sequenceCounter = 0;
    lastHeartbeat = 0;
    messagesSent = 0;
    messagesReceived = 0;
    meshConnected = false;
    rxBufferPos = 0;
    
    // Initialize message handlers to null
    for (int i = 0; i < 256; i++) {
        messageHandlers[i] = nullptr;
    }
}

MeshtasticCommManager::~MeshtasticCommManager() {
    if (meshtasticSerial) {
        delete meshtasticSerial;
    }
}

void MeshtasticCommManager::begin() {
    LOG_INFO("Starting Meshtastic Communication Manager");
    
    // Initialize SoftwareSerial on A4/A5 pins
    // Pin A4 (18) = TX to Meshtastic RX
    // Pin A5 (19) = RX from Meshtastic TX
    meshtasticSerial = new SoftwareSerial(19, 18); // RX, TX
    meshtasticSerial->begin(115200);
    
    // Initialize protocol
    initializeProtocol();
    
    // Register message handlers
    messageHandlers[MSG_COMMAND_REQUEST] = &MeshtasticCommManager::handleCommandRequest;
    messageHandlers[MSG_DIAGNOSTIC_REQUEST] = &MeshtasticCommManager::handleDiagnosticRequest;
    messageHandlers[MSG_CONFIG_REQUEST] = &MeshtasticCommManager::handleConfigRequest;
    messageHandlers[MSG_HEARTBEAT] = &MeshtasticCommManager::handleHeartbeatRequest;
    
    LOG_INFO("Meshtastic communication initialized on A4/A5");
    LOG_INFO("Node ID: %s", nodeId);
}

void MeshtasticCommManager::initializeProtocol() {
    // Generate unique node ID
    generateNodeId();
    
    // Clear buffers
    memset(txBuffer, 0, sizeof(txBuffer));
    memset(rxBuffer, 0, sizeof(rxBuffer));
    rxBufferPos = 0;
    
    // Reset counters
    sequenceCounter = 0;
    messagesSent = 0;
    messagesReceived = 0;
    meshConnected = false;
    lastHeartbeat = getCurrentTimestamp();
    
    LOG_DEBUG("Protocol initialized with node ID: %s", nodeId);
}

void MeshtasticCommManager::update() {
    uint32_t currentTime = getCurrentTimestamp();
    
    // Process incoming data
    processIncomingData();
    
    // Send periodic heartbeat
    if (currentTime - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
        sendHeartbeat();
        lastHeartbeat = currentTime;
    }
    
    // Update mesh connection status based on recent activity
    // Consider connected if we've received messages in last 60 seconds
    meshConnected = (currentTime - lastHeartbeat < 60000);
}

void MeshtasticCommManager::processIncomingData() {
    if (!meshtasticSerial) return;
    
    while (meshtasticSerial->available()) {
        uint8_t byte = meshtasticSerial->read();
        
        // Add to buffer
        if (rxBufferPos < sizeof(rxBuffer)) {
            rxBuffer[rxBufferPos++] = byte;
            
            // Try to parse complete messages
            if (parseMessage()) {
                // Message parsed successfully, reset for next message
                rxBufferPos = 0;
            }
        } else {
            // Buffer overflow, reset
            LOG_WARN("RX buffer overflow, resetting");
            rxBufferPos = 0;
        }
    }
}

bool MeshtasticCommManager::parseMessage() {
    // Need at least header size to parse
    if (rxBufferPos < HEADER_SIZE) {
        return false;
    }
    
    // Parse header
    ProtocolHeader* header = (ProtocolHeader*)rxBuffer;
    
    // Validate header
    if (!validateHeader(*header)) {
        LOG_WARN("Invalid message header received");
        return true; // Consume invalid message
    }
    
    // Check if we have complete message (header + payload)
    uint16_t totalSize = HEADER_SIZE + header->payloadSize;
    if (rxBufferPos < totalSize) {
        return false; // Wait for more data
    }
    
    // Extract payload
    const uint8_t* payload = rxBuffer + HEADER_SIZE;
    
    // Handle the message
    handleMessage(*header, payload);
    
    // If there's remaining data, move it to beginning of buffer
    if (rxBufferPos > totalSize) {
        memmove(rxBuffer, rxBuffer + totalSize, rxBufferPos - totalSize);
        rxBufferPos -= totalSize;
        return false; // Continue processing remaining data
    }
    
    return true; // Message consumed completely
}

void MeshtasticCommManager::handleMessage(const ProtocolHeader& header, const uint8_t* payload) {
    messagesReceived++;
    
    LOG_DEBUG("Received message type 0x%02X from %s", header.messageType, header.sourceId);
    
    // Call appropriate handler
    MessageHandler handler = messageHandlers[header.messageType];
    if (handler) {
        (this->*handler)(header, payload);
    } else {
        LOG_WARN("No handler for message type 0x%02X", header.messageType);
    }
    
    // Update connection status
    meshConnected = true;
    lastHeartbeat = getCurrentTimestamp();
}

bool MeshtasticCommManager::validateHeader(const ProtocolHeader& header) {
    // Check protocol version
    if (header.version != PROTOCOL_VERSION) {
        return false;
    }
    
    // Check payload size is reasonable
    if (header.payloadSize > MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Check sequence number is incrementing (basic validation)
    // Note: This is simplified - full implementation would track per-source
    
    return true;
}

void MeshtasticCommManager::sendTelemetryMessage(LogSplitterMessageType type, const uint8_t* data, uint16_t length) {
    if (createMessage(type, data, length)) {
        LOG_DEBUG("Sent telemetry message type 0x%02X (%d bytes)", type, length);
    } else {
        LOG_ERROR("Failed to send telemetry message type 0x%02X", type);
    }
}

bool MeshtasticCommManager::createMessage(LogSplitterMessageType type, const uint8_t* payload, uint16_t payloadSize) {
    if (!meshtasticSerial || payloadSize > MAX_PAYLOAD_SIZE) {
        return false;
    }
    
    // Create header
    ProtocolHeader header;
    writeHeader(header, type, payloadSize);
    
    // Copy header to tx buffer
    memcpy(txBuffer, &header, HEADER_SIZE);
    
    // Copy payload to tx buffer
    if (payload && payloadSize > 0) {
        memcpy(txBuffer + HEADER_SIZE, payload, payloadSize);
    }
    
    // Send complete message
    uint16_t totalSize = HEADER_SIZE + payloadSize;
    size_t bytesSent = meshtasticSerial->write(txBuffer, totalSize);
    
    if (bytesSent == totalSize) {
        messagesSent++;
        sequenceCounter++;
        return true;
    } else {
        logProtocolError("Incomplete message transmission");
        return false;
    }
}

void MeshtasticCommManager::writeHeader(ProtocolHeader& header, LogSplitterMessageType type, uint16_t payloadSize) {
    header.version = PROTOCOL_VERSION;
    header.messageType = (uint8_t)type;
    header.sequence = sequenceCounter;
    header.timestamp = getCurrentTimestamp();
    header.reserved = 0;
    strncpy(header.sourceId, nodeId, 8);
    header.payloadSize = payloadSize;
}

// Telemetry message implementations
void MeshtasticCommManager::sendDigitalIOTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_DIGITAL_IO, data, length);
}

void MeshtasticCommManager::sendRelaysTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_RELAYS, data, length);
}

void MeshtasticCommManager::sendPressureTelemetry(const uint8_t* data, uint16_t length, bool isAux) {
    LogSplitterMessageType type = isAux ? MSG_TELEMETRY_PRESSURE_AUX : MSG_TELEMETRY_PRESSURE_MAIN;
    sendTelemetryMessage(type, data, length);
}

void MeshtasticCommManager::sendErrorsTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_ERRORS, data, length);
}

void MeshtasticCommManager::sendSafetyTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_SAFETY, data, length);
}

void MeshtasticCommManager::sendSystemStatusTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_SYSTEM_STATUS, data, length);
}

void MeshtasticCommManager::sendTimingTelemetry(const uint8_t* data, uint16_t length) {
    sendTelemetryMessage(MSG_TELEMETRY_TIMING, data, length);
}

// Emergency alert implementations
void MeshtasticCommManager::sendEmergencyAlert(const char* severity, const char* message) {
    // Create plain language emergency message for Channel 0 broadcast
    char alertMessage[128];
    uint32_t timestamp = getCurrentTimestamp();
    
    // Format: [SEVERITY] HH:MM:SS NodeID: message (AUTO)
    uint32_t seconds = (timestamp / 1000) % 86400; // Seconds in day
    uint16_t hours = seconds / 3600;
    uint16_t minutes = (seconds % 3600) / 60;
    uint16_t secs = seconds % 60;
    
    snprintf(alertMessage, sizeof(alertMessage), 
             "[%s] %02d:%02d:%02d %s: %s (AUTO)", 
             severity, hours, minutes, secs, nodeId, message);
    
    // Send as emergency broadcast (Channel 0)
    broadcastEmergency(alertMessage);
    
    // Also send as binary message for logging
    sendTelemetryMessage(MSG_EMERGENCY_ALERT, (const uint8_t*)alertMessage, strlen(alertMessage));
}

void MeshtasticCommManager::sendSafetyAlert(const char* message) {
    sendEmergencyAlert("SAFETY", message);
}

void MeshtasticCommManager::sendErrorAlert(const char* message) {
    sendEmergencyAlert("ERROR", message);
}

void MeshtasticCommManager::broadcastEmergency(const char* message) {
    // For now, we send emergency messages as regular telemetry
    // In full implementation, this would use Channel 0 specifically
    LOG_CRITICAL("EMERGENCY: %s", message);
    
    // Send emergency message with special marker
    char emergencyPacket[200];
    snprintf(emergencyPacket, sizeof(emergencyPacket), "EMERGENCY:%s", message);
    sendTelemetryMessage(MSG_EMERGENCY_ALERT, (const uint8_t*)emergencyPacket, strlen(emergencyPacket));
}

void MeshtasticCommManager::sendHeartbeat() {
    // Create heartbeat payload
    struct {
        char nodeId[8];
        uint32_t uptime;
        uint8_t status;
        uint8_t meshConnected;
        uint16_t messagesSent;
        uint16_t messagesReceived;
    } heartbeatData;
    
    strncpy(heartbeatData.nodeId, nodeId, 8);
    heartbeatData.uptime = millis();
    heartbeatData.status = 1; // Online
    heartbeatData.meshConnected = meshConnected ? 1 : 0;
    heartbeatData.messagesSent = (uint16_t)messagesSent;
    heartbeatData.messagesReceived = (uint16_t)messagesReceived;
    
    sendTelemetryMessage(MSG_HEARTBEAT, (const uint8_t*)&heartbeatData, sizeof(heartbeatData));
}

void MeshtasticCommManager::sendNodeStatus() {
    // Create detailed node status
    struct {
        char nodeId[8];
        uint32_t uptime;
        uint16_t freeRAM;
        uint8_t errorCount;
        uint8_t safetyStatus;
        uint32_t lastErrorTime;
    } statusData;
    
    strncpy(statusData.nodeId, nodeId, 8);
    statusData.uptime = millis();
    statusData.freeRAM = (uint16_t)freeMemory(); // If available
    statusData.errorCount = errorManager ? errorManager->getActiveErrorCount() : 0;
    statusData.safetyStatus = 1; // TODO: Get from safety system
    statusData.lastErrorTime = 0; // TODO: Get from error manager
    
    sendTelemetryMessage(MSG_NODE_STATUS, (const uint8_t*)&statusData, sizeof(statusData));
}

// Message handlers
void MeshtasticCommManager::handleCommandRequest(const ProtocolHeader& header, const uint8_t* payload) {
    if (!commandProcessor) {
        LOG_WARN("No command processor available");
        return;
    }
    
    // Parse command message
    if (header.payloadSize < sizeof(CommandMessage)) {
        LOG_WARN("Invalid command message size");
        return;
    }
    
    const CommandMessage* cmdMsg = (const CommandMessage*)payload;
    
    LOG_INFO("Received command: %s", cmdMsg->command);
    
    // Execute command
    char response[256];
    uint32_t startTime = millis();
    bool success = commandProcessor->processCommand(
        (char*)cmdMsg->command, 
        true, // fromMqtt (indicates remote command)
        response, 
        sizeof(response)
    );
    uint32_t executionTime = millis() - startTime;
    
    // Create response
    CommandResponse resp;
    strncpy(resp.command, cmdMsg->command, sizeof(resp.command));
    resp.success = success ? 1 : 0;
    resp.executionTimeMs = (uint16_t)executionTime;
    resp.outputLength = strlen(response);
    strncpy(resp.error, success ? "" : "Command failed", sizeof(resp.error));
    
    // Send response header
    createMessage(MSG_COMMAND_RESPONSE, (const uint8_t*)&resp, sizeof(resp));
    
    // Send output data if present (in separate message or extended payload)
    if (resp.outputLength > 0) {
        // For simplicity, truncate output to fit in single message
        uint16_t outputSize = min(resp.outputLength, (uint16_t)(MAX_PAYLOAD_SIZE - sizeof(resp)));
        
        // Create extended response with output
        uint8_t extendedResp[sizeof(resp) + outputSize];
        memcpy(extendedResp, &resp, sizeof(resp));
        memcpy(extendedResp + sizeof(resp), response, outputSize);
        
        createMessage(MSG_COMMAND_RESPONSE, extendedResp, sizeof(resp) + outputSize);
    }
}

void MeshtasticCommManager::handleDiagnosticRequest(const ProtocolHeader& header, const uint8_t* payload) {
    LOG_DEBUG("Handling diagnostic request");
    
    // Create diagnostic response with system information
    struct {
        char nodeId[8];
        uint32_t uptime;
        uint16_t freeRAM;
        uint8_t errorCount;
        uint8_t meshStatus;
        uint32_t messagesSent;
        uint32_t messagesReceived;
        uint16_t sequenceCounter;
    } diagnosticData;
    
    strncpy(diagnosticData.nodeId, nodeId, 8);
    diagnosticData.uptime = millis();
    diagnosticData.freeRAM = 0; // TODO: Implement free RAM check
    diagnosticData.errorCount = errorManager ? errorManager->getActiveErrorCount() : 0;
    diagnosticData.meshStatus = meshConnected ? 1 : 0;
    diagnosticData.messagesSent = messagesSent;
    diagnosticData.messagesReceived = messagesReceived;
    diagnosticData.sequenceCounter = sequenceCounter;
    
    createMessage(MSG_DIAGNOSTIC_RESPONSE, (const uint8_t*)&diagnosticData, sizeof(diagnosticData));
}

void MeshtasticCommManager::handleConfigRequest(const ProtocolHeader& header, const uint8_t* payload) {
    LOG_DEBUG("Handling config request");
    
    // TODO: Implement configuration request handling
    // This would allow remote configuration of the controller
    
    // For now, send basic acknowledgment
    uint8_t ack = 1;
    createMessage(MSG_CONFIG_RESPONSE, &ack, 1);
}

void MeshtasticCommManager::handleHeartbeatRequest(const ProtocolHeader& header, const uint8_t* payload) {
    LOG_DEBUG("Received heartbeat from %s", header.sourceId);
    
    // Respond with our own heartbeat
    sendHeartbeat();
}

// Utility functions
uint32_t MeshtasticCommManager::getCurrentTimestamp() {
    return millis();
}

void MeshtasticCommManager::generateNodeId() {
    // Generate unique node ID based on Arduino's unique characteristics
    // Using a combination of analog readings for pseudo-randomness
    
    uint32_t seed = 0;
    for (int i = 0; i < 4; i++) {
        seed ^= analogRead(A0 + i) << (i * 8);
    }
    
    // Create 8-character ID
    snprintf(nodeId, sizeof(nodeId), "CTRL%04X", (uint16_t)(seed & 0xFFFF));
}

void MeshtasticCommManager::logProtocolError(const char* error, uint8_t errorCode) {
    LOG_ERROR("Protocol error: %s (code: %d)", error, errorCode);
    
    if (errorManager) {
        errorManager->setError(ERROR_HARDWARE_FAULT);
    }
}

// Memory check function (if available)
#ifdef __AVR__
extern int __heap_start, *__brkval;
int freeMemory() {
    int v;
    return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}
#else
int freeMemory() {
    return -1; // Not available on this platform
}
#endif