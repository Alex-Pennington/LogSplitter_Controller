#pragma once

#include "constants.h"
#include "nau7802_sensor.h"
#include "mcp9600_sensor.h"
#include "ina219_sensor.h"
#include "mcp3421_sensor.h"

class MonitorSystem {
public:
    MonitorSystem();
    
    void begin();
    void update();
    
    // Data collection
    void readSensors();
    float getLocalTemperature() const;
    float getRemoteTemperature() const;
    float getLocalTemperatureF() const;
    float getRemoteTemperatureF() const;
    float getHumidity() const;
    
    // MCP9600 Temperature Sensor functions
    bool isTemperatureSensorReady();
    void getTemperatureSensorStatus(char* buffer, size_t bufferSize);
    void setTemperatureOffset(float localOffset, float remoteOffset);
    MCP9600Sensor* getTemperatureSensor(); // Access to sensor for debug control
    
    // NAU7802 Load Cell functions
    float getWeight();
    float getFilteredWeight();
    long getRawWeight() const;
    bool isWeightSensorReady();
    NAU7802Status getWeightSensorStatus() const;
    
    // NAU7802 Calibration functions
    bool calibrateWeightSensorZero();
    bool calibrateWeightSensorScale(float knownWeight);
    void tareWeightSensor();
    bool saveWeightCalibration();
    bool loadWeightCalibration();
    
    // INA219 Power Monitor functions
    float getBusVoltage();
    float getShuntVoltage();
    float getCurrent();
    float getPower();
    bool isPowerSensorReady();
    void getPowerSensorStatus(char* buffer, size_t bufferSize);
    INA219_Sensor* getPowerSensor(); // Access to sensor for debug control
    
    // MCP3421 ADC functions
    float getAdcVoltage();
    int32_t getAdcRawValue();
    float getFilteredAdcVoltage(uint8_t samples = 5);
    bool isAdcSensorReady();
    void getAdcSensorStatus(char* buffer, size_t bufferSize);
    MCP3421_Sensor* getAdcSensor(); // Access to sensor for configuration
    
    // System monitoring
    unsigned long getUptime() const;
    unsigned long getFreeMemory() const;
    SystemState getSystemState() const;
    void setSystemState(SystemState state);
    
    // Status reporting
    void getStatusString(char* buffer, size_t bufferSize);
    void publishStatus();
    void publishHeartbeat();
    
    // Digital I/O
    bool getDigitalInput(uint8_t pin) const;
    void setDigitalOutput(uint8_t pin, bool state);
    
    // Configuration
    void setPublishInterval(unsigned long interval);
    void setHeartbeatInterval(unsigned long interval);
    unsigned long getPublishInterval() const;
    unsigned long getHeartbeatInterval() const;

private:
    // System state
    SystemState currentState;
    unsigned long systemStartTime;
    
    // Sensor readings
    float localTemperature;
    float remoteTemperature;
    float humidity;
    unsigned long lastSensorRead;
    
    // MCP9600 Temperature Sensor
    mutable MCP9600Sensor temperatureSensor;
    unsigned long lastTemperatureRead;
    bool lastSensorAvailable;
    
    // NAU7802 Load Cell Sensor
    mutable NAU7802Sensor weightSensor;
    float currentWeight;
    long currentRawWeight;
    unsigned long lastWeightRead;
    
    // INA219 Power Monitor Sensor
    mutable INA219_Sensor powerSensor;
    float currentVoltage;
    float currentCurrent;
    float currentPower;
    unsigned long lastPowerRead;
    
    // MCP3421 ADC Sensor
    mutable MCP3421_Sensor adcSensor;
    float currentAdcVoltage;
    int32_t currentAdcRaw;
    unsigned long lastAdcRead;
    
    // Publishing intervals
    unsigned long publishInterval;
    unsigned long heartbeatInterval;
    unsigned long lastStatusPublish;
    unsigned long lastHeartbeat;
    
    // Digital I/O states
    bool digitalInputStates[8];
    bool digitalOutputStates[8];
    
    // Helper functions
    void initializePins();
    void readAnalogSensors();
    void readDigitalInputs();
    void readTemperatureSensor();
    void readWeightSensor();
    void readPowerSensor();
    void readAdcSensor();
    void updateLCDDisplay();
};