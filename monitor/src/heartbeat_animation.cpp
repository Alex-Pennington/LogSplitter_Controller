#include "heartbeat_animation.h"
#include "Arduino_LED_Matrix.h"

ArduinoLEDMatrix matrix;

// Heart shape patterns for 12x8 LED matrix
// Using byte arrays for simpler pattern definition
// Each frame is 8 rows x 12 columns (12 bits per row)
const uint8_t heartbeat_frames[4][8][12] = {
  // Frame 0: Small heart (resting state)
  {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,1,1,0,0,0},
    {0,1,1,1,1,0,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,1,1,1,0,0,0,0,0},
    {0,0,0,0,0,1,0,0,0,0,0,0}
  },
  
  // Frame 1: Medium heart (building up)
  {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,0,0,1,1,1,0,0},
    {1,1,1,1,1,0,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,1,1,1,0,0,0,0,0}
  },
  
  // Frame 2: Large heart (peak beat)
  {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {1,1,1,1,1,0,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0}
  },
  
  // Frame 3: Medium heart (falling back)
  {
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,0,0,1,1,1,0,0},
    {1,1,1,1,1,0,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,1,1,1,0,0,0,0,0}
  }
};

HeartbeatAnimation::HeartbeatAnimation() {
  currentFrame = 0;
  lastFrameTime = 0;
  isEnabled = false;
  currentBPM = 72; // Default resting heart rate
  currentBrightness = 128; // Medium brightness
  calculateFrameTiming();
}

void HeartbeatAnimation::begin() {
  matrix.begin();
  isEnabled = true;
  currentFrame = 0;
  lastFrameTime = millis();
}

void HeartbeatAnimation::update() {
  if (!isEnabled) return;
  
  unsigned long currentTime = millis();
  
  // Check if it's time to advance to next frame
  if (currentTime - lastFrameTime >= frameDurations[currentFrame]) {
    currentFrame = (currentFrame + 1) % 4;
    lastFrameTime = currentTime;
    
    // Display current frame
    displayFrame(currentFrame);
  }
}

void HeartbeatAnimation::enable() {
  if (!isEnabled) {
    matrix.begin();
    isEnabled = true;
    currentFrame = 0;
    lastFrameTime = millis();
  }
}

void HeartbeatAnimation::disable() {
  if (isEnabled) {
    isEnabled = false;
    matrix.clear();
  }
}

void HeartbeatAnimation::setBrightness(uint8_t brightness) {
  currentBrightness = brightness;
  // Note: Arduino R4 WiFi LED Matrix doesn't support brightness control
  // This function is kept for API compatibility
}

bool HeartbeatAnimation::getEnabled() const {
  return isEnabled;
}

void HeartbeatAnimation::setHeartRate(uint16_t bpm) {
  // Clamp BPM to reasonable range
  if (bpm < 30) bpm = 30;
  if (bpm > 200) bpm = 200;
  
  currentBPM = bpm;
  calculateFrameTiming();
}

uint16_t HeartbeatAnimation::getHeartRate() const {
  return currentBPM;
}

uint8_t HeartbeatAnimation::getBrightness() const {
  return currentBrightness;
}

void HeartbeatAnimation::calculateFrameTiming() {
  // Calculate frame timing based on BPM
  // Each heartbeat cycle = 4 frames
  unsigned long totalCycleTime = (60000 / currentBPM); // milliseconds per beat
  
  // Distribute timing across 4 frames with realistic heartbeat rhythm
  frameDurations[0] = totalCycleTime * 0.25;  // Resting phase
  frameDurations[1] = totalCycleTime * 0.15;  // Quick rise
  frameDurations[2] = totalCycleTime * 0.20;  // Peak hold
  frameDurations[3] = totalCycleTime * 0.40;  // Recovery phase
}

void HeartbeatAnimation::clear() {
  matrix.clear();
}

void HeartbeatAnimation::displayFrame(uint8_t frameNumber) {
  if (frameNumber < 4) {
    // Convert pattern to a frame buffer and display
    uint32_t frame[3] = {0, 0, 0}; // Arduino R4 WiFi uses 3 uint32_t for frame data
    
    // Convert the byte pattern to the LED matrix format
    for (int row = 0; row < 8; row++) {
      for (int col = 0; col < 12; col++) {
        if (heartbeat_frames[frameNumber][row][col]) {
          // Calculate bit position and set in frame buffer
          int bitIndex = row * 12 + col;
          int wordIndex = bitIndex / 32;
          int bitPos = bitIndex % 32;
          if (wordIndex < 3) {
            frame[wordIndex] |= (1UL << (31 - bitPos));
          }
        }
      }
    }
    
    matrix.loadFrame(frame);
  }
}