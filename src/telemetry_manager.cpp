/*
 * Telemetry Manager Implementation
 * 
 * High-performance protobuf-based telemetry system for LogSplitter Controller
 * Optimized for Arduino UNO R4 WiFi with minimal memory footprint and maximum throughput
 */

#include "telemetry_manager.h"
#include "constants.h"

TelemetryManager::TelemetryManager() 
    : telemetrySerial(nullptr)
    , sequenceId(0)
    , lastHeartbeat(0)
    , messagesSent(0)
    , bytesTransmitted(0)
{
}

void TelemetryManager::begin(uint8_t rxPin, uint8_t txPin, unsigned long baud) {
    // Create SoftwareSerial instance
    telemetrySerial = new SoftwareSerial(rxPin, txPin);
    telemetrySerial->begin(baud);
    
    // Don't send startup message yet - system not fully initialized
    // sendSystemStatus() will be called later from main loop
    
    lastHeartbeat = millis();
}

void TelemetryManager::begin(SoftwareSerial* existingSerial) {
    // Use existing SoftwareSerial instance (avoid conflicts)
    telemetrySerial = existingSerial;
    
    // Don't send startup message yet - system not fully initialized
    // sendSystemStatus() will be called later from main loop
    
    lastHeartbeat = millis();
}

void TelemetryManager::update() {
    // Send periodic heartbeat (every 5 seconds)
    unsigned long now = millis();
    if (now - lastHeartbeat >= 5000) {
        sendSystemStatus();
        lastHeartbeat = now;
    }
}

void TelemetryManager::sendDigitalInput(uint8_t pin, bool state, bool isDebounced, uint16_t debounceTime, Telemetry::InputType inputType) {
    if (!telemetrySerial) return;
    
    Telemetry::DigitalInputEvent msg;
    setHeader(msg.header, Telemetry::MSG_DIGITAL_INPUT);
    
    msg.pin = pin;
    msg.flags = (state ? 1 : 0) | 
                (isDebounced ? 2 : 0) | 
                ((uint8_t)inputType << 2);
    msg.debounce_time_ms = debounceTime;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendDigitalOutput(uint8_t pin, bool state, Telemetry::OutputType outputType, const char* pattern) {
    if (!telemetrySerial) return;
    
    Telemetry::DigitalOutputEvent msg;
    setHeader(msg.header, Telemetry::MSG_DIGITAL_OUTPUT);
    
    msg.pin = pin;
    msg.flags = (state ? 1 : 0) | 
                ((uint8_t)outputType << 1) | 
                ((uint8_t)encodeMillLampPattern(pattern) << 4);
    msg.reserved = 0;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendRelayEvent(uint8_t relayNumber, bool state, bool isManual, bool success, Telemetry::RelayType relayType) {
    if (!telemetrySerial) return;
    
    Telemetry::RelayEvent msg;
    setHeader(msg.header, Telemetry::MSG_RELAY_EVENT);
    
    msg.relay_number = relayNumber;
    msg.flags = (state ? 1 : 0) | 
                (isManual ? 2 : 0) | 
                (success ? 4 : 0) | 
                ((uint8_t)relayType << 3);
    msg.reserved = 0;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendPressureReading(uint8_t sensorPin, float pressurePsi, uint16_t rawValue, uint8_t pressureType, bool isFault) {
    if (!telemetrySerial) return;
    
    Telemetry::PressureReading msg;
    setHeader(msg.header, Telemetry::MSG_PRESSURE_READING);
    
    msg.sensor_pin = sensorPin;
    msg.flags = (isFault ? 1 : 0) | (pressureType << 1);
    msg.raw_value = rawValue;
    msg.pressure_psi = pressurePsi;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendSystemError(uint8_t errorCode, const char* description, Telemetry::ErrorSeverity severity, bool acknowledged, bool active) {
    if (!telemetrySerial) return;
    
    Telemetry::SystemError msg;
    setHeader(msg.header, Telemetry::MSG_SYSTEM_ERROR);
    
    msg.error_code = errorCode;
    msg.flags = (acknowledged ? 1 : 0) | 
                (active ? 2 : 0) | 
                ((uint8_t)severity << 2);
    
    // Copy description with length limit
    if (description) {
        strncpy(msg.description, description, sizeof(msg.description) - 1);
        msg.description[sizeof(msg.description) - 1] = '\0';
        msg.desc_length = strlen(msg.description);
    } else {
        msg.description[0] = '\0';
        msg.desc_length = 0;
    }
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendSafetyEvent(uint8_t eventType, bool isActive) {
    if (!telemetrySerial) return;
    
    Telemetry::SafetyEvent msg;
    setHeader(msg.header, Telemetry::MSG_SAFETY_EVENT);
    
    msg.event_type = eventType;
    msg.flags = isActive ? 1 : 0;
    msg.reserved = 0;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendSystemStatus() {
    if (!telemetrySerial) return;
    
    Telemetry::SystemStatus msg;
    setHeader(msg.header, Telemetry::MSG_SYSTEM_STATUS);
    
    msg.uptime_ms = millis();
    msg.loop_frequency_hz = 0; // TODO: Calculate from main loop timing
    msg.free_memory_bytes = 0; // TODO: Get free memory
    msg.active_error_count = 0; // TODO: Get from error manager
    
    // Pack flags: safety_active, estop_active, sequence_state, mill_lamp_pattern
    msg.flags = 0; // TODO: Get actual system state
    msg.reserved = 0;
    
    sendMessage(&msg, sizeof(msg));
}

void TelemetryManager::sendSequenceEvent(uint8_t eventType, uint8_t stepNumber, uint16_t elapsedTime) {
    if (!telemetrySerial) return;
    
    Telemetry::SequenceEvent msg;
    setHeader(msg.header, Telemetry::MSG_SEQUENCE_EVENT);
    
    msg.event_type = eventType;
    msg.step_number = stepNumber;
    msg.elapsed_time_ms = elapsedTime;
    
    sendMessage(&msg, sizeof(msg));
}

// Private helper methods

void TelemetryManager::setHeader(Telemetry::MessageHeader& header, Telemetry::MessageType msgType) {
    header.msg_type = (uint8_t)msgType;
    header.sequence_id = sequenceId++;
    header.timestamp_ms = getTimestamp();
}

void TelemetryManager::sendMessage(const void* message, size_t size) {
    if (!telemetrySerial) return;
    
    // Send size byte first (for message framing)
    telemetrySerial->write((uint8_t)size);
    
    // Send message data
    const uint8_t* data = (const uint8_t*)message;
    for (size_t i = 0; i < size; i++) {
        telemetrySerial->write(data[i]);
    }
    
    messagesSent++;
    bytesTransmitted += size + 1; // +1 for size byte
}

Telemetry::InputType TelemetryManager::getInputType(uint8_t pin) {
    switch (pin) {
        case 2: return Telemetry::INPUT_MANUAL_EXTEND;
        case 3: return Telemetry::INPUT_MANUAL_RETRACT;
        case 4: return Telemetry::INPUT_SAFETY_CLEAR;
        case 5: return Telemetry::INPUT_SEQUENCE_START;
        case 6: return Telemetry::INPUT_LIMIT_EXTEND;
        case 7: return Telemetry::INPUT_LIMIT_RETRACT;
        case 8: return Telemetry::INPUT_SPLITTER_OPERATOR;
        case 12: return Telemetry::INPUT_EMERGENCY_STOP;
        default: return Telemetry::INPUT_UNKNOWN;
    }
}

Telemetry::OutputType TelemetryManager::getOutputType(uint8_t pin) {
    switch (pin) {
        case MILL_LAMP_PIN: return Telemetry::OUTPUT_MILL_LAMP;
        case SAFETY_STATUS_PIN: return Telemetry::OUTPUT_SAFETY_STATUS;
        case LED_BUILTIN: return Telemetry::OUTPUT_STATUS_LED;
        default: return Telemetry::OUTPUT_UNKNOWN;
    }
}

Telemetry::RelayType TelemetryManager::getRelayType(uint8_t relayNumber) {
    switch (relayNumber) {
        case RELAY_EXTEND: return Telemetry::RELAY_HYDRAULIC_EXTEND;
        case RELAY_RETRACT: return Telemetry::RELAY_HYDRAULIC_RETRACT;
        case 3: return Telemetry::RELAY_RESERVED_3;
        case 4: return Telemetry::RELAY_RESERVED_4;
        case 5: return Telemetry::RELAY_RESERVED_5;
        case 6: return Telemetry::RELAY_RESERVED_6;
        case 7: return Telemetry::RELAY_OPERATOR_BUZZER;
        case RELAY_ENGINE_STOP: return Telemetry::RELAY_ENGINE_STOP;
        case RELAY_POWER_CONTROL: return Telemetry::RELAY_POWER_CONTROL;
        default: return Telemetry::RELAY_UNKNOWN;
    }
}

Telemetry::MillLampPattern TelemetryManager::encodeMillLampPattern(const char* pattern) {
    if (!pattern) return Telemetry::LAMP_OFF;
    
    if (strcmp(pattern, "solid") == 0) return Telemetry::LAMP_SOLID;
    if (strcmp(pattern, "slow_blink") == 0) return Telemetry::LAMP_SLOW_BLINK;
    if (strcmp(pattern, "fast_blink") == 0) return Telemetry::LAMP_FAST_BLINK;
    return Telemetry::LAMP_OFF;
}