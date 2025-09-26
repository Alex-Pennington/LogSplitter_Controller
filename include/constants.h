#pragma once

#include <Arduino.h>

// System Constants
const unsigned long WATCHDOG_TIMEOUT_MS = 8000;
const unsigned long MAIN_LOOP_TIMEOUT_MS = 5000;

// Network Constants
const char* const BROKER_HOST = "159.203.138.46";
const int BROKER_PORT = 1883;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const uint8_t MAX_CONNECT_RETRIES = 3;

// MQTT Topics
const char TOPIC_PUBLISH[] PROGMEM = "r4/example/pub";
const char TOPIC_SUBSCRIBE[] PROGMEM = "r4/example/sub";
const char TOPIC_CONTROL[] PROGMEM = "r4/control";
const char TOPIC_CONTROL_RESP[] PROGMEM = "r4/control/resp";
const char TOPIC_PRESSURE[] PROGMEM = "r4/pressure";
const char TOPIC_HYDRAULIC_SYSTEM_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_system";
const char TOPIC_HYDRAULIC_FILTER_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_filter";
const char TOPIC_PRESSURE_STATUS[] PROGMEM = "r4/pressure/status";
const char TOPIC_SEQUENCE_STATUS[] PROGMEM = "r4/sequence/status";
const char TOPIC_SEQUENCE_EVENT[] PROGMEM = "r4/sequence/event";
const char TOPIC_SEQUENCE_STATE[] PROGMEM = "r4/sequence/state";

// Pin Configuration
const uint8_t WATCH_PINS[] = {2, 3, 4, 5, 6, 7};
const size_t WATCH_PIN_COUNT = sizeof(WATCH_PINS) / sizeof(WATCH_PINS[0]);
const unsigned long DEBOUNCE_DELAY_MS = 20;

// Limit Switch Configuration
const uint8_t LIMIT_EXTEND_PIN = 6;   // Cylinder fully extended limit switch
const uint8_t LIMIT_RETRACT_PIN = 7;  // Cylinder fully retracted limit switch

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
const float DEFAULT_ADC_VREF = 3.3f;
const float DEFAULT_MAX_PRESSURE_PSI = 3000.0f;
const float DEFAULT_SENSOR_GAIN = 1.0f;
const float DEFAULT_SENSOR_OFFSET = 0.0f;

// Dual Pressure Sensor Pins
const uint8_t HYDRAULIC_PRESSURE_PIN = A1;      // Main hydraulic pressure (A1)
const uint8_t HYDRAULIC_OIL_PRESSURE_PIN = A5;  // Hydraulic oil pressure (A5)

// Pressure Sensor Specifications
const float SENSOR_MIN_VOLTAGE = 0.0f;          // 0V reference
const float SENSOR_MAX_VOLTAGE = 4.5f;          // Default sensor full-scale (most ratiometric 0-4.5V sensors)

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
    SYS_SAFE_MODE
};

// Command validation
extern const char* const ALLOWED_COMMANDS[];
extern const char* const ALLOWED_SET_PARAMS[];