#pragma once

#include <Arduino.h>

// System Constants
const unsigned long WATCHDOG_TIMEOUT_MS = 15000;
const unsigned long MAIN_LOOP_TIMEOUT_MS = 10000;

// Network Constants
const char* const BROKER_HOST = "159.203.138.46";
const int BROKER_PORT = 1883;
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
const unsigned long WIFI_CONNECT_CHECK_INTERVAL_MS = 500;
const unsigned long MQTT_RECONNECT_INTERVAL_MS = 5000;
const unsigned long NETWORK_STABILITY_TIME_MS = 10000;
const uint8_t MAX_CONNECT_RETRIES = 3;
const uint8_t MAX_WIFI_RETRIES = 3;
const uint8_t MAX_MQTT_RETRIES = 3;

// MQTT Topics
const char TOPIC_PUBLISH[] PROGMEM = "r4/example/pub";
const char TOPIC_SUBSCRIBE[] PROGMEM = "r4/example/sub";
const char TOPIC_CONTROL[] PROGMEM = "r4/control";
const char TOPIC_CONTROL_RESP[] PROGMEM = "r4/control/resp";
const char TOPIC_PRESSURE[] PROGMEM = "r4/pressure";
const char TOPIC_HYDRAULIC_SYSTEM_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_system";
const char TOPIC_HYDRAULIC_FILTER_PRESSURE[] PROGMEM = "r4/pressure/hydraulic_filter";
const char TOPIC_HYDRAULIC_SYSTEM_VOLTAGE[] PROGMEM = "r4/pressure/hydraulic_system_voltage";
const char TOPIC_HYDRAULIC_FILTER_VOLTAGE[] PROGMEM = "r4/pressure/hydraulic_filter_voltage";
const char TOPIC_PRESSURE_STATUS[] PROGMEM = "r4/pressure/status";
const char TOPIC_SEQUENCE_STATUS[] PROGMEM = "r4/sequence/status";
const char TOPIC_SEQUENCE_EVENT[] PROGMEM = "r4/sequence/event";
const char TOPIC_SEQUENCE_STATE[] PROGMEM = "r4/sequence/state";
const char TOPIC_SEQUENCE_STAGE[] PROGMEM = "r4/sequence/stage";
const char TOPIC_SEQUENCE_ACTIVE[] PROGMEM = "r4/sequence/active";
const char TOPIC_SEQUENCE_ELAPSED[] PROGMEM = "r4/sequence/elapsed";

// Pin Configuration
const uint8_t WATCH_PINS[] = {2, 3, 4, 5, 6, 7};
const size_t WATCH_PIN_COUNT = sizeof(WATCH_PINS) / sizeof(WATCH_PINS[0]);
const unsigned long DEBOUNCE_DELAY_MS = 5;  // Reduced for fast-moving hydraulic cylinder limit switches

// Pin-specific debounce delays (milliseconds)
const unsigned long LIMIT_SWITCH_DEBOUNCE_MS = 10;  // Pins 6,7 - Balanced for moving cylinder with switch bounce filtering
const unsigned long BUTTON_DEBOUNCE_MS = 15;        // Pins 2,3,4,5 - Normal for manual buttons

// Limit Switch Configuration
const uint8_t LIMIT_EXTEND_PIN = 6;   // Cylinder fully extended limit switch
const uint8_t LIMIT_RETRACT_PIN = 7;  // Cylinder fully retracted limit switch

// Relay Configuration Labels
const uint8_t RELAY_EXTEND = 1;       // Relay 1 - Cylinder extend (hydraulic valve)
const uint8_t RELAY_RETRACT = 2;      // Relay 2 - Cylinder retract (hydraulic valve)
const uint8_t RELAY_ENGINE_STOP = 8;  // Relay 8 - Engine stop relay (safety)
// Note: RELAY_POWER_PIN = 9 (relay board power control)

// Safety System Pin Configuration
const uint8_t ENGINE_STOP_PIN = 12;    // Pin 12 - Emergency stop input (from PINS.md)
const uint8_t SYSTEM_ERROR_LED_PIN = 9; // Pin 9 - System Error LED (from PINS.md)

// Relay Configuration
const unsigned long RELAY_BAUD = 115200;
const uint8_t RELAY_POWER_PIN = 9;
const uint8_t MAX_RELAYS = 9;

// Pressure Sensor Configuration
const unsigned long SAMPLE_INTERVAL_MS = 100;
const unsigned long SAMPLE_WINDOW_MS = 1000;
const size_t SAMPLE_WINDOW_COUNT = SAMPLE_WINDOW_MS / SAMPLE_INTERVAL_MS;
const uint8_t ADC_RESOLUTION_BITS = 10;  // Arduino UNO R4 WiFi has 10-bit ADC (1024 counts)
const float DEFAULT_ADC_VREF = 5.0f;     // Arduino UNO R4 WiFi uses 5V reference voltage
const float DEFAULT_MAX_PRESSURE_PSI = 3000.0f;
const float DEFAULT_SENSOR_GAIN = 1.0f;
const float DEFAULT_SENSOR_OFFSET = 0.0f;

// Dual Pressure Sensor Pins
const uint8_t HYDRAULIC_PRESSURE_PIN = A1;      // Main hydraulic pressure (A1)
const uint8_t HYDRAULIC_OIL_PRESSURE_PIN = A5;  // Hydraulic oil pressure (A5)

// Pressure Sensor Specifications (0-4.5V sensors)
const float SENSOR_MIN_VOLTAGE = 0.0f;    // 0V = 0 PSI
const float SENSOR_MAX_VOLTAGE = 4.5f;    // 4.5V = max PSI
const float HYDRAULIC_MAX_PRESSURE_PSI = 3000.0f; // Hydraulic system pressure range

// Individual sensor defaults (A1 - Main Hydraulic)  
const float DEFAULT_A1_MAX_PRESSURE_PSI = 5000.0f;  // Change this to your sensor's max range
const float DEFAULT_A1_ADC_VREF = 5.0f;  // Corrected to 5V reference
const float DEFAULT_A1_SENSOR_GAIN = 1.0f;
const float DEFAULT_A1_SENSOR_OFFSET = 0.0f;

// Individual sensor defaults (A5 - Hydraulic Oil)
const float DEFAULT_A5_MAX_PRESSURE_PSI = 30.0f;
const float DEFAULT_A5_ADC_VREF = 5.0f;  // Corrected to 5V reference
const float DEFAULT_A5_SENSOR_GAIN = 1.0f;
const float DEFAULT_A5_SENSOR_OFFSET = 0.0f;

// Current loop sensor constants (4-20mA)
const float CURRENT_LOOP_MIN_VOLTAGE = 1.0f;  // 4mA × 250Ω = 1V
const float CURRENT_LOOP_MAX_VOLTAGE = 5.0f;  // 20mA × 250Ω = 5V

// Pressure sensor extension constants
const float MAIN_PRESSURE_EXT_NEG_FRAC = 0.2f;
const float MAIN_PRESSURE_EXT_POS_FRAC = 0.3f;
const float MAIN_PRESSURE_EXT_FSV = 5.0f;

// Safety Constants
const float SAFETY_THRESHOLD_PSI = 2500.0f;
const float SAFETY_HYSTERESIS_PSI = 10.0f;

// Pressure-based sequence control
const float EXTEND_PRESSURE_LIMIT_PSI = 2300.0f;  // Pressure that triggers extend limit reached
const float RETRACT_PRESSURE_LIMIT_PSI = 2300.0f; // Pressure that triggers retract limit reached

// Sequence Control Constants
const unsigned long DEFAULT_SEQUENCE_STABLE_MS = 15;  // Reduced for fast limit switch detection on moving cylinder
const unsigned long DEFAULT_SEQUENCE_START_STABLE_MS = 100;
const unsigned long DEFAULT_SEQUENCE_TIMEOUT_MS = 30000;
const unsigned long SEQUENCE_STAGE_TIMEOUT_MS = 30000;

// Memory Management
const size_t SHARED_BUFFER_SIZE = 256;  // Increased for comprehensive status output
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