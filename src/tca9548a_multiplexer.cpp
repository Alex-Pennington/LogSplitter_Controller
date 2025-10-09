#include "tca9548a_multiplexer.h"

TCA9548A_Multiplexer::TCA9548A_Multiplexer(uint8_t address) {
    _address = address;
    _currentChannel = NO_CHANNEL;
    _initialized = false;
}

bool TCA9548A_Multiplexer::begin() {
    if (isConnected()) {
        _initialized = true;
        disableAllChannels();
        return true;
    }
    return false;
}

bool TCA9548A_Multiplexer::selectChannel(uint8_t channel) {
    if (!_initialized) {
        return false;
    }
    
    if (channel > 7) {
        return false;
    }
    
    Wire1.beginTransmission(_address);
    Wire1.write(1 << channel);  // Set the bit for the desired channel
    uint8_t error = Wire1.endTransmission();
    
    if (error == 0) {
        _currentChannel = channel;
        return true;
    }
    
    return false;
}

bool TCA9548A_Multiplexer::disableAllChannels() {
    if (!_initialized) {
        return false;
    }
    
    Wire1.beginTransmission(_address);
    Wire1.write(0);  // Disable all channels
    uint8_t error = Wire1.endTransmission();
    
    if (error == 0) {
        _currentChannel = NO_CHANNEL;
        return true;
    }
    
    return false;
}

uint8_t TCA9548A_Multiplexer::getCurrentChannel() {
    return _currentChannel;
}

bool TCA9548A_Multiplexer::isConnected() {
    Wire1.beginTransmission(_address);
    uint8_t error = Wire1.endTransmission();
    return (error == 0);
}