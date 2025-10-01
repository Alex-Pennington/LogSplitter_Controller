#ifndef HEARTBEAT_ANIMATION_H
#define HEARTBEAT_ANIMATION_H

#include <Arduino.h>

/**
 * @class HeartbeatAnimation
 * @brief Manages heartbeat animation on Arduino R4 WiFi LED matrix
 * 
 * Provides a pulsing heartbeat animation with configurable heart rate,
 * brightness control, and enable/disable functionality.
 */
class HeartbeatAnimation {
private:
  uint8_t currentFrame;
  unsigned long lastFrameTime;
  bool isEnabled;
  uint16_t currentBPM;
  uint8_t currentBrightness;
  
  // Frame durations for heartbeat rhythm (milliseconds)
  unsigned long frameDurations[4];
  
  void calculateFrameTiming();
  
public:
  HeartbeatAnimation();
  
  /**
   * Initialize the LED matrix and start animation
   */
  void begin();
  
  /**
   * Update animation - call this frequently in main loop
   */
  void update();
  
  /**
   * Enable heartbeat animation
   */
  void enable();
  
  /**
   * Disable heartbeat animation and clear display
   */
  void disable();
  
  /**
   * Set LED matrix brightness
   * @param brightness 0-255 (0=off, 255=max brightness)
   */
  void setBrightness(uint8_t brightness);
  
  /**
   * Check if animation is currently enabled
   * @return true if enabled, false if disabled
   */
  bool getEnabled() const;
  
  /**
   * Set heartbeat rate
   * @param bpm Beats per minute (30-200 typical range)
   */
  void setHeartRate(uint16_t bpm);
  
  /**
   * Get current heart rate
   * @return Current BPM setting
   */
  uint16_t getHeartRate() const;
  
  /**
   * Get current brightness
   * @return Current brightness (0-255)
   */
  uint8_t getBrightness() const;
  
  /**
   * Clear the LED matrix display
   */
  void clear();
  
  /**
   * Display a specific frame (for testing/debugging)
   * @param frameNumber Frame to display (0-3)
   */
  void displayFrame(uint8_t frameNumber);
};

#endif // HEARTBEAT_ANIMATION_H