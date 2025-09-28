#pragma once

#include <Arduino.h>
#include "arduino_secrets.h"

// System Constants
const unsigned long WATCHDOG_TIMEOUT_MS = 8000;
const unsigned long MAIN_LOOP_TIMEOUT_MS = 5000;

// Network Constants
const char* const BROKER_HOST = MQTT_BROKER_HOST;
const int BROKER_PORT = MQTT_BROKER_PORT;
// Non-blocking network timeouts (much shorter for safety)
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // Max time to spend trying WiFi
const unsigned long WIFI_CONNECT_CHECK_INTERVAL_MS = 500;  // Check WiFi status every 500ms
const unsigned long MQTT_CONNECT_TIMEOUT_MS = 5000;   // Max time to spend trying MQTT
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 10000; // Wait 10s between attempts
const unsigned long NETWORK_STABILITY_TIME_MS = 30000; // 30s uptime = stable
const unsigned long NETWORK_HEALTH_CHECK_INTERVAL_MS = 1000; // Check health every 1s
const uint8_t MAX_WIFI_RETRIES = 3;
const uint8_t MAX_MQTT_RETRIES = 5;

// MQTT Topics
const char TOPIC_PUBLISH[] PROGMEM = "r4/example/pub";
const char TOPIC_SUBSCRIBE[] PROGMEM = "r4/example/sub";
const char TOPIC_CONTROL[] PROGMEM = "r4/control";
const char TOPIC_CONTROL_RESP[] PROGMEM = "r4/control/resp";
const char TOPIC_PRESSURE[] PROGMEM = "r4/pressure";
const char TOPIC_HYDRAULIC_SYSTEM_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_system";
const char TOPIC_HYDRAULIC_FILTER_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_filter";
// Added voltage topics for diagnostic / calibration insight
const char TOPIC_HYDRAULIC_SYSTEM_VOLTAGE[] PROGMEM = "r4/pressure/hydraulic_system_voltage";
const char TOPIC_HYDRAULIC_FILTER_VOLTAGE[] PROGMEM = "r4/pressure/hydraulic_filter_voltage";
const char TOPIC_PRESSURE_STATUS[] PROGMEM = "r4/pressure/status";
const char TOPIC_SEQUENCE_STATUS[] PROGMEM = "r4/sequence/status";
const char TOPIC_SEQUENCE_EVENT[] PROGMEM = "r4/sequence/event";
const char TOPIC_SEQUENCE_STATE[] PROGMEM = "r4/sequence/state";
// Individual sequence data topics for database integration
const char TOPIC_SEQUENCE_STAGE[] PROGMEM = "r4/data/sequence_stage";
const char TOPIC_SEQUENCE_ACTIVE[] PROGMEM = "r4/data/sequence_active";
const char TOPIC_SEQUENCE_ELAPSED[] PROGMEM = "r4/data/sequence_elapsed";
const char TOPIC_SYSTEM_UPTIME[] PROGMEM = "r4/data/system_uptime";
const char TOPIC_SAFETY_ACTIVE[] PROGMEM = "r4/data/safety_active";
const char TOPIC_ESTOP_ACTIVE[] PROGMEM = "r4/data/estop_active";
const char TOPIC_ESTOP_LATCHED[] PROGMEM = "r4/data/estop_latched";
const char TOPIC_LIMIT_EXTEND[] PROGMEM = "r4/data/limit_extend";
const char TOPIC_LIMIT_RETRACT[] PROGMEM = "r4/data/limit_retract";
const char TOPIC_RELAY_R1[] PROGMEM = "r4/data/relay_r1";
const char TOPIC_RELAY_R2[] PROGMEM = "r4/data/relay_r2";
const char TOPIC_SPLITTER_OPERATOR[] PROGMEM = "r4/data/splitter_operator";

// Pin Configuration
const uint8_t WATCH_PINS[] = {2, 3, 4, 5, 6, 7, 8, 12};
const uint8_t WATCH_PIN_COUNT = 8;
const unsigned long DEBOUNCE_DELAY_MS = 20;

// Limit Switch Configuration
const uint8_t LIMIT_EXTEND_PIN = 6;   // Cylinder fully extended limit switch
const uint8_t LIMIT_RETRACT_PIN = 7;  // Cylinder fully retracted limit switch
const uint8_t SPLITTER_OPERATOR_PIN = 8; // Splitter operator input signal
const uint8_t EMERGENCY_STOP_PIN = 12; // Emergency stop button input

// Relay Configuration Labels
const uint8_t RELAY_EXTEND = 1;       // Relay 1 - Cylinder extend (hydraulic valve)
const uint8_t RELAY_RETRACT = 2;      // Relay 2 - Cylinder retract (hydraulic valve)
const uint8_t RELAY_ENGINE_STOP = 8;  // Relay 8 - Engine stop relay (safety)
// Note: RELAY_POWER_PIN = 9 (relay board power control)

// Relay Configuration
const unsigned long RELAY_BAUD = 115200;
const uint8_t RELAY_POWER_PIN = 9;
const uint8_t MAX_RELAYS = 9;

// Pressure Sensor Configuration
const unsigned long SAMPLE_INTERVAL_MS = 100;
const unsigned long SAMPLE_WINDOW_MS = 1000;
const size_t SAMPLE_WINDOW_COUNT = SAMPLE_WINDOW_MS / SAMPLE_INTERVAL_MS;
const uint8_t ADC_RESOLUTION_BITS = 14;
const float DEFAULT_ADC_VREF = 5.0f;  // Arduino UNO R4 WiFi ADC reference
const float DEFAULT_MAX_PRESSURE_PSI = 5000.0f;  // Sensor full scale range
const float DEFAULT_SENSOR_GAIN = 1.0f;
const float DEFAULT_SENSOR_OFFSET = 0.0f;

// Individual Sensor Defaults - A1 (System Pressure, 4-20mA)
const float DEFAULT_A1_MAX_PRESSURE_PSI = 5000.0f;  // 4-20mA current loop range
const float DEFAULT_A1_ADC_VREF = 5.0f;             // ADC reference voltage
const float DEFAULT_A1_SENSOR_GAIN = 1.0f;          // Calibration gain
const float DEFAULT_A1_SENSOR_OFFSET = 0.0f;        // Calibration offset

// Individual Sensor Defaults - A5 (Filter Pressure, 0-4.5V)
const float DEFAULT_A5_MAX_PRESSURE_PSI = 30.0f;    // 0-30 PSI absolute range
const float DEFAULT_A5_ADC_VREF = 5.0f;             // ADC reference voltage  
const float DEFAULT_A5_SENSOR_GAIN = 1.0f;          // Calibration gain (4.5V/5V = 0.9 for full scale)
const float DEFAULT_A5_SENSOR_OFFSET = 0.0f;        // Calibration offset

// 4-20mA Current Loop Configuration (with 250Ω resistor)
const float CURRENT_LOOP_MIN_VOLTAGE = 1.0f;  // 4mA × 250Ω = 1V (0 PSI)
const float CURRENT_LOOP_MAX_VOLTAGE = 5.0f;  // 20mA × 250Ω = 5V (5000 PSI)
const float CURRENT_LOOP_RESISTOR_OHMS = 250.0f;  // Shunt resistor value

// Dual Pressure Sensor Pins
const uint8_t HYDRAULIC_PRESSURE_PIN = A1;      // Main hydraulic pressure (A1)
const uint8_t HYDRAULIC_OIL_PRESSURE_PIN = A5;  // Hydraulic oil pressure (A5)

// System Error Indicator
const uint8_t SYSTEM_ERROR_LED_PIN = 9;         // Malfunction indicator lamp (MIL)

// Pressure Sensor Specifications
const float SENSOR_MIN_VOLTAGE = 0.0f;          // 0V reference
const float SENSOR_MAX_VOLTAGE = 5.0f;          // Default sensor full-scale (most ratiometric 0-4.5V sensors)

// Main hydraulic channel (A1) extended scaling:
//   Electrical span 0–5.0V represents -25% .. +125% of nominal hydraulic max (1.5 * nominal span).
//   0V  -> -0.25 * nominalMax   (discarded after clamp)
//   5V  -> +1.25 * nominalMax   (reported as nominalMax after clamp)
// Rationale:
//   * Provides headroom so slight sensor over-range does not instantly saturate reading.
//   * Negative zone absorbs noise / offset drift without going below 0 after clamp.
//   * Operator display + safety logic always see a bounded 0..nominal range.
//   * Safety threshold (SAFETY_THRESHOLD_PSI) intentionally compares against clamped value only.
// NOTE: If future requirements need raw (unclamped) pressure, preserve pre-clamp value before applying bounds.
const float HYDRAULIC_MAX_PRESSURE_PSI = 5000.0f;   // Nominal displayed/safety range (clamp target)
const float MAIN_PRESSURE_EXT_NEG_FRAC = 0.25f;     // 25% below zero head-room
const float MAIN_PRESSURE_EXT_POS_FRAC = 0.25f;     // 25% above nominal head-room
const float MAIN_PRESSURE_EXT_FSV = 5.0f;           // Extended full-scale voltage for A1 mapping

// Safety Constants
const float SAFETY_THRESHOLD_PSI = 2750.0f;
const float SAFETY_HYSTERESIS_PSI = 10.0f;

// Sequence Control Constants
const unsigned long DEFAULT_SEQUENCE_STABLE_MS = 50;
const unsigned long DEFAULT_SEQUENCE_START_STABLE_MS = 100;
const unsigned long DEFAULT_SEQUENCE_TIMEOUT_MS = 30000;
const unsigned long SEQUENCE_STAGE_TIMEOUT_MS = 30000;

// Memory Management
const size_t SHARED_BUFFER_SIZE = 128;
const size_t TOPIC_BUFFER_SIZE = 64;
const size_t COMMAND_BUFFER_SIZE = 80;
const size_t MAX_CMD_LENGTH = 16;

// EEPROM Configuration
const uint32_t CALIB_MAGIC = 0x43414C49; // 'CALI'
const int CALIB_EEPROM_ADDR = 0;

// Filter Types
enum FilterMode { 
    FILTER_NONE = 0, 
    FILTER_MEDIAN3 = 1, 
    FILTER_EMA = 2 
};

// System States
enum SystemState {
    SYS_INITIALIZING,
    SYS_CONNECTING,
    SYS_RUNNING,
    SYS_ERROR,
    SYS_SAFE_MODE,
    SYS_EMERGENCY_STOP
};

// Command validation
extern const char* const ALLOWED_COMMANDS[];
extern const char* const ALLOWED_SET_PARAMS[];