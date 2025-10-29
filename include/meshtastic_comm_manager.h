/**
 * Meshtastic Communication Manager for LogSplitter Controller
 * 
 * Complete redesign of Arduino communication to use Meshtastic mesh networking
 * as the primary/sole communication path. Handles binary protobuf messaging
 * via A4/A5 pins connected to Meshtastic Serial Module.
 * 
 * Features:
 * - Binary protobuf message encoding/decoding
 * - Unified telemetry, command, and diagnostic protocols
 * - Emergency alert broadcasting (Channel 0)
 * - Automatic message sequencing and validation
 * - No USB dependencies (fully wireless)
 * 
 * Hardware Connection:
 * - Pin A4 (SDA/TX) → Meshtastic Serial Module RX
 * - Pin A5 (SCL/RX) → Meshtastic Serial Module TX
 * - 115200 baud, protobuf mode
 * 
 * Protocol:
 * - Channel 0: Emergency broadcasts (plain text)
 * - Channel 1: LogSplitter protobuf messages
 * - Header: 18 bytes + variable payload
 */

#ifndef MESHTASTIC_COMM_MANAGER_H
#define MESHTASTIC_COMM_MANAGER_H

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "system_error_manager.h"
#include "command_processor.h"
#include "constants.h"

// Protocol constants
#define PROTOCOL_VERSION 1
#define HEADER_SIZE 18
#define MAX_PAYLOAD_SIZE 200
#define MESSAGE_TIMEOUT_MS 5000
#define HEARTBEAT_INTERVAL_MS 30000
#define MAX_RETRY_COUNT 3

// Message types (matching Python protocol)
enum LogSplitterMessageType {
    // Telemetry Messages (0x10-0x17)
    MSG_TELEMETRY_DIGITAL_IO = 0x10,
    MSG_TELEMETRY_RELAYS = 0x11,
    MSG_TELEMETRY_PRESSURE_MAIN = 0x12,
    MSG_TELEMETRY_PRESSURE_AUX = 0x13,
    MSG_TELEMETRY_ERRORS = 0x14,
    MSG_TELEMETRY_SAFETY = 0x15,
    MSG_TELEMETRY_SYSTEM_STATUS = 0x16,
    MSG_TELEMETRY_TIMING = 0x17,
    
    // Command Messages (0x20-0x2F)
    MSG_COMMAND_REQUEST = 0x20,
    MSG_COMMAND_RESPONSE = 0x21,
    MSG_COMMAND_STATUS = 0x22,
    
    // Diagnostic Messages (0x30-0x3F)
    MSG_DIAGNOSTIC_REQUEST = 0x30,
    MSG_DIAGNOSTIC_RESPONSE = 0x31,
    MSG_DIAGNOSTIC_STREAM = 0x32,
    
    // System Messages (0x40-0x4F)
    MSG_HEARTBEAT = 0x40,
    MSG_NODE_STATUS = 0x41,
    MSG_MESH_HEALTH = 0x42,
    MSG_CONFIG_REQUEST = 0x43,
    MSG_CONFIG_RESPONSE = 0x44,
    
    // Emergency Messages (0x50-0x5F)
    MSG_EMERGENCY_ALERT = 0x50,
    MSG_SAFETY_ALERT = 0x51,
    MSG_ERROR_ALERT = 0x52
};

// Message header structure (18 bytes)
struct ProtocolHeader {
    uint8_t version;           // Protocol version (1 byte)
    uint8_t messageType;       // Message type (1 byte)  
    uint16_t sequence;         // Message sequence (2 bytes)
    uint32_t timestamp;        // Timestamp in ms (4 bytes)
    uint32_t reserved;         // Reserved for future use (4 bytes)
    char sourceId[8];          // Source node ID (8 bytes)
    uint16_t payloadSize;      // Payload size (2 bytes)
    
    // Total: 18 bytes
} __attribute__((packed));

// Command message structure
struct CommandMessage {
    char command[64];          // Command string
    uint16_t timeoutMs;        // Command timeout
    uint8_t requireAck;        // Require acknowledgment flag
    uint8_t reserved;          // Padding
} __attribute__((packed));

// Command response structure  
struct CommandResponse {
    char command[64];          // Original command
    uint8_t success;           // Success flag
    uint16_t executionTimeMs;  // Execution time
    uint16_t outputLength;     // Output string length
    char error[32];            // Error message if failed
    // Variable-length output follows
} __attribute__((packed));

class MeshtasticCommManager {
private:
    SoftwareSerial* meshtasticSerial;
    SystemErrorManager* errorManager;
    CommandProcessor* commandProcessor;
    
    // Protocol state
    char nodeId[9];            // 8 chars + null terminator
    uint16_t sequenceCounter;
    uint32_t lastHeartbeat;
    uint32_t messagesSent;
    uint32_t messagesReceived;
    bool meshConnected;
    
    // Buffers
    uint8_t txBuffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
    uint8_t rxBuffer[HEADER_SIZE + MAX_PAYLOAD_SIZE];
    uint16_t rxBufferPos;
    
    // Message handlers
    typedef void (MeshtasticCommManager::*MessageHandler)(const ProtocolHeader& header, const uint8_t* payload);
    MessageHandler messageHandlers[256];  // Indexed by message type
    
public:
    MeshtasticCommManager();
    ~MeshtasticCommManager();
    
    // Initialization
    void begin();
    void setErrorManager(SystemErrorManager* em) { errorManager = em; }
    void setCommandProcessor(CommandProcessor* cp) { commandProcessor = cp; }
    
    // Main processing
    void update();
    
    // Telemetry transmission (existing format compatibility)
    void sendTelemetryMessage(LogSplitterMessageType type, const uint8_t* data, uint16_t length);
    void sendDigitalIOTelemetry(const uint8_t* data, uint16_t length);
    void sendRelaysTelemetry(const uint8_t* data, uint16_t length);
    void sendPressureTelemetry(const uint8_t* data, uint16_t length, bool isAux = false);
    void sendErrorsTelemetry(const uint8_t* data, uint16_t length);
    void sendSafetyTelemetry(const uint8_t* data, uint16_t length);
    void sendSystemStatusTelemetry(const uint8_t* data, uint16_t length);
    void sendTimingTelemetry(const uint8_t* data, uint16_t length);
    
    // Emergency alerts (Channel 0 - plain language)
    void sendEmergencyAlert(const char* severity, const char* message);
    void sendSafetyAlert(const char* message);
    void sendErrorAlert(const char* message);
    
    // System messages
    void sendHeartbeat();
    void sendNodeStatus();
    
    // Status and diagnostics
    bool isMeshConnected() const { return meshConnected; }
    uint32_t getMessagesSent() const { return messagesSent; }
    uint32_t getMessagesReceived() const { return messagesReceived; }
    const char* getNodeId() const { return nodeId; }
    
private:
    // Protocol handling
    void initializeProtocol();
    void processIncomingData();
    bool parseMessage();
    void handleMessage(const ProtocolHeader& header, const uint8_t* payload);
    
    // Message creation
    bool createMessage(LogSplitterMessageType type, const uint8_t* payload, uint16_t payloadSize);
    void writeHeader(ProtocolHeader& header, LogSplitterMessageType type, uint16_t payloadSize);
    
    // Message handlers
    void handleCommandRequest(const ProtocolHeader& header, const uint8_t* payload);
    void handleDiagnosticRequest(const ProtocolHeader& header, const uint8_t* payload);
    void handleConfigRequest(const ProtocolHeader& header, const uint8_t* payload);
    void handleHeartbeatRequest(const ProtocolHeader& header, const uint8_t* payload);
    
    // Utility functions
    uint32_t getCurrentTimestamp();
    void generateNodeId();
    bool validateHeader(const ProtocolHeader& header);
    void logProtocolError(const char* error, uint8_t errorCode = 0);
    
    // Emergency broadcast (Channel 0 - plain text)
    void broadcastEmergency(const char* message);
};

// Global instance
extern MeshtasticCommManager meshtasticComm;

// Compatibility macros for existing telemetry code
#define SEND_DIGITAL_IO_TELEMETRY(data, len) meshtasticComm.sendDigitalIOTelemetry(data, len)
#define SEND_RELAYS_TELEMETRY(data, len) meshtasticComm.sendRelaysTelemetry(data, len)
#define SEND_PRESSURE_TELEMETRY(data, len, aux) meshtasticComm.sendPressureTelemetry(data, len, aux)
#define SEND_ERRORS_TELEMETRY(data, len) meshtasticComm.sendErrorsTelemetry(data, len)
#define SEND_SAFETY_TELEMETRY(data, len) meshtasticComm.sendSafetyTelemetry(data, len)
#define SEND_SYSTEM_STATUS_TELEMETRY(data, len) meshtasticComm.sendSystemStatusTelemetry(data, len)
#define SEND_TIMING_TELEMETRY(data, len) meshtasticComm.sendTimingTelemetry(data, len)

#endif // MESHTASTIC_COMM_MANAGER_H