#pragma once

#include "constants.h"
#include "nau7802_sensor.h"
#include "mcp9600_sensor.h"

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
    void updateLCDDisplay();
};