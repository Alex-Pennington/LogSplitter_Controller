/*
 * Protobuf Telemetry Manager for LogSplitter Controller
 * 
 * This file contains the generated protobuf structures and telemetry manager
 * optimized for Arduino UNO R4 WiFi with nanopb-lite implementation.
 * 
 * All telemetry data is sent via SoftwareSerial (pins A4/A5) as binary protobuf messages
 * for maximum throughput and minimal overhead.
 */

#ifndef TELEMETRY_MANAGER_H
#define TELEMETRY_MANAGER_H

#include <Arduino.h>
#include <SoftwareSerial.h>

// Simplified protobuf structures for embedded use
// Based on telemetry.proto but hand-optimized for Arduino

namespace Telemetry {

// Message type identifiers (1 byte)
enum MessageType : uint8_t {
    MSG_DIGITAL_INPUT = 0x10,
    MSG_DIGITAL_OUTPUT = 0x11,
    MSG_RELAY_EVENT = 0x12,
    MSG_PRESSURE_READING = 0x13,
    MSG_SYSTEM_ERROR = 0x14,
    MSG_SAFETY_EVENT = 0x15,
    MSG_SYSTEM_STATUS = 0x16,
    MSG_SEQUENCE_EVENT = 0x17
};

// Input types
enum InputType : uint8_t {
    INPUT_UNKNOWN = 0,
    INPUT_MANUAL_EXTEND = 1,     // Pin 2
    INPUT_MANUAL_RETRACT = 2,    // Pin 3
    INPUT_SAFETY_CLEAR = 3,      // Pin 4
    INPUT_SEQUENCE_START = 4,    // Pin 5
    INPUT_LIMIT_EXTEND = 5,      // Pin 6
    INPUT_LIMIT_RETRACT = 6,     // Pin 7
    INPUT_SPLITTER_OPERATOR = 7, // Pin 8
    INPUT_EMERGENCY_STOP = 8     // Pin 12
};

// Output types
enum OutputType : uint8_t {
    OUTPUT_UNKNOWN = 0,
    OUTPUT_MILL_LAMP = 1,        // Pin 9
    OUTPUT_STATUS_LED = 2        // Pin 13
};

// Relay types
enum RelayType : uint8_t {
    RELAY_UNKNOWN = 0,
    RELAY_HYDRAULIC_EXTEND = 1,   // R1
    RELAY_HYDRAULIC_RETRACT = 2,  // R2
    RELAY_RESERVED_3 = 3,         // R3
    RELAY_RESERVED_4 = 4,         // R4
    RELAY_RESERVED_5 = 5,         // R5
    RELAY_RESERVED_6 = 6,         // R6
    RELAY_OPERATOR_BUZZER = 7,    // R7
    RELAY_ENGINE_STOP = 8,        // R8
    RELAY_POWER_CONTROL = 9       // R9
};

// Compact message structures (byte-packed for efficiency)

#pragma pack(push, 1)  // Ensure no padding

// Base message header (6 bytes)
struct MessageHeader {
    uint8_t msg_type;           // MessageType
    uint8_t sequence_id;        // Rolling sequence number (0-255)
    uint32_t timestamp_ms;      // 32-bit timestamp (49 days max)
};

// Digital Input Event (4 bytes payload)
struct DigitalInputEvent {
    MessageHeader header;
    uint8_t pin;                // Pin number
    uint8_t flags;              // Bit 0: state, Bit 1: is_debounced, Bits 2-7: input_type
    uint16_t debounce_time_ms;  // Debounce time
};

// Digital Output Event (3 bytes payload)
struct DigitalOutputEvent {
    MessageHeader header;
    uint8_t pin;                // Pin number
    uint8_t flags;              // Bit 0: state, Bits 1-3: output_type, Bits 4-7: pattern
    uint8_t reserved;           // For alignment
};

// Relay Event (3 bytes payload)
struct RelayEvent {
    MessageHeader header;
    uint8_t relay_number;       // 1-9
    uint8_t flags;              // Bit 0: state, Bit 1: is_manual, Bit 2: success, Bits 3-7: relay_type
    uint8_t reserved;           // For alignment
};

// Pressure Reading (8 bytes payload)
struct PressureReading {
    MessageHeader header;
    uint8_t sensor_pin;         // Analog pin (A0-A3)
    uint8_t flags;              // Bit 0: is_fault, Bits 1-7: pressure_type
    uint16_t raw_value;         // Raw ADC value
    float pressure_psi;         // 4-byte float
};

// System Error (variable length, max 32 bytes payload)
struct SystemError {
    MessageHeader header;
    uint8_t error_code;         // Error code (0x01-0x80)
    uint8_t flags;              // Bit 0: acknowledged, Bit 1: active, Bits 2-3: severity
    uint8_t desc_length;        // Description string length (0-24)
    char description[24];       // Truncated description
};

// Safety Event (3 bytes payload)
struct SafetyEvent {
    MessageHeader header;
    uint8_t event_type;         // SafetyEventType
    uint8_t flags;              // Bit 0: is_active
    uint8_t reserved;           // For alignment
};

// System Status (12 bytes payload)
struct SystemStatus {
    MessageHeader header;
    uint32_t uptime_ms;         // System uptime
    uint16_t loop_frequency_hz; // Main loop frequency
    uint16_t free_memory_bytes; // Available memory
    uint8_t active_error_count; // Number of active errors
    uint8_t flags;              // Bit 0: safety_active, Bit 1: estop_active, Bits 2-5: sequence_state, Bits 6-7: mill_lamp_pattern
    uint16_t reserved;          // For alignment
};

// Sequence Event (4 bytes payload)
struct SequenceEvent {
    MessageHeader header;
    uint8_t event_type;         // SequenceEventType
    uint8_t step_number;        // Current step
    uint16_t elapsed_time_ms;   // Time in step (max 65 seconds)
};

#pragma pack(pop)

// Mill lamp patterns for encoding
enum MillLampPattern : uint8_t {
    LAMP_OFF = 0,
    LAMP_SOLID = 1,
    LAMP_SLOW_BLINK = 2,
    LAMP_FAST_BLINK = 3
};

// Sequence states for encoding
enum SequenceState : uint8_t {
    SEQ_IDLE = 0,
    SEQ_EXTENDING = 1,
    SEQ_EXTENDED = 2,
    SEQ_RETRACTING = 3,
    SEQ_RETRACTED = 4,
    SEQ_PAUSED = 5,
    SEQ_ERROR_STATE = 6
};

// Error severity levels
enum ErrorSeverity : uint8_t {
    SEVERITY_INFO = 0,
    SEVERITY_WARNING = 1,
    SEVERITY_ERROR = 2,
    SEVERITY_CRITICAL = 3
};

} // namespace Telemetry

// Telemetry Manager Class
class TelemetryManager {
public:
    TelemetryManager();
    
    // Initialize with SoftwareSerial pins
    void begin(uint8_t rxPin, uint8_t txPin, unsigned long baud = 57600);
    
    // Initialize with existing SoftwareSerial instance
    void begin(SoftwareSerial* existingSerial);
    
    // Send telemetry messages
    void sendDigitalInput(uint8_t pin, bool state, bool isDebounced, uint16_t debounceTime, Telemetry::InputType inputType);
    void sendDigitalOutput(uint8_t pin, bool state, Telemetry::OutputType outputType, const char* pattern = nullptr);
    void sendRelayEvent(uint8_t relayNumber, bool state, bool isManual, bool success, Telemetry::RelayType relayType);
    void sendPressureReading(uint8_t sensorPin, float pressurePsi, uint16_t rawValue, uint8_t pressureType, bool isFault = false);
    void sendSystemError(uint8_t errorCode, const char* description, Telemetry::ErrorSeverity severity, bool acknowledged, bool active);
    void sendSafetyEvent(uint8_t eventType, bool isActive);
    void sendSystemStatus();
    void sendSequenceEvent(uint8_t eventType, uint8_t stepNumber, uint16_t elapsedTime);
    
    // Periodic update (call from main loop)
    void update();
    
    // Statistics
    uint32_t getMessagesSent() const { return messagesSent; }
    uint32_t getBytesTransmitted() const { return bytesTransmitted; }
    
private:
    SoftwareSerial* telemetrySerial;
    uint8_t sequenceId;
    uint32_t lastHeartbeat;
    uint32_t messagesSent;
    uint32_t bytesTransmitted;
    
    // Internal helpers
    uint32_t getTimestamp() const { return millis(); }
    void sendMessage(const void* message, size_t size);
    Telemetry::InputType getInputType(uint8_t pin);
    Telemetry::OutputType getOutputType(uint8_t pin);
    Telemetry::RelayType getRelayType(uint8_t relayNumber);
    Telemetry::MillLampPattern encodeMillLampPattern(const char* pattern);
    
    // Message assembly helpers
    void setHeader(Telemetry::MessageHeader& header, Telemetry::MessageType msgType);
};

// Global telemetry manager instance
extern TelemetryManager telemetryManager;

// Convenience macros for easy integration
#define TELEM_DIGITAL_INPUT(pin, state, debounced, time, type) \
    telemetryManager.sendDigitalInput(pin, state, debounced, time, type)

#define TELEM_DIGITAL_OUTPUT(pin, state, type, pattern) \
    telemetryManager.sendDigitalOutput(pin, state, type, pattern)

#define TELEM_RELAY(num, state, manual, success, type) \
    telemetryManager.sendRelayEvent(num, state, manual, success, type)

#define TELEM_PRESSURE(pin, psi, raw, type, fault) \
    telemetryManager.sendPressureReading(pin, psi, raw, type, fault)

#define TELEM_ERROR(code, desc, severity, acked, active) \
    telemetryManager.sendSystemError(code, desc, severity, acked, active)

#define TELEM_SAFETY(event, active) \
    telemetryManager.sendSafetyEvent(event, active)

#define TELEM_SEQUENCE(event, step, elapsed) \
    telemetryManager.sendSequenceEvent(event, step, elapsed)

#endif // TELEMETRY_MANAGER_H