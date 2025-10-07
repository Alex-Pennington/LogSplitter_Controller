#ifndef TCA9548A_MULTIPLEXER_H
#define TCA9548A_MULTIPLEXER_H

#include <Arduino.h>
#include <Wire.h>

extern TwoWire Wire1;

/**
 * @class TCA9548A_Multiplexer
 * @brief Class for managing TCA9548A I2C multiplexer
 * 
 * The TCA9548A is an 8-channel I2C multiplexer that allows multiple devices
 * with the same I2C address to be connected to a single I2C bus.
 */
class TCA9548A_Multiplexer {
public:
    // Constructor
    TCA9548A_Multiplexer(uint8_t address = 0x70);
    
    // Initialization
    bool begin();
    
    // Channel selection (0-7)
    bool selectChannel(uint8_t channel);
    
    // Disable all channels
    bool disableAllChannels();
    
    // Get current channel selection
    uint8_t getCurrentChannel();
    
    // Check if multiplexer is responding
    bool isConnected();
    
    // Channel management constants
    static const uint8_t MUX_CHANNEL_0 = 0;
    static const uint8_t MUX_CHANNEL_1 = 1;
    static const uint8_t MUX_CHANNEL_2 = 2;
    static const uint8_t MUX_CHANNEL_3 = 3;
    static const uint8_t MUX_CHANNEL_4 = 4;
    static const uint8_t MUX_CHANNEL_5 = 5;
    static const uint8_t MUX_CHANNEL_6 = 6;
    static const uint8_t MUX_CHANNEL_7 = 7;
    static const uint8_t NO_CHANNEL = 0xFF;

private:
    uint8_t _address;
    uint8_t _currentChannel;
    bool _initialized;
};

#endif // TCA9548A_MULTIPLEXER_H