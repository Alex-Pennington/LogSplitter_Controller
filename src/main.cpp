// Simple Arduino R4 WiFi + MQTT example
// - Uses WiFiNINA for WiFi on the UNO R4 WiFi
// - Uses ArduinoMqttClient for MQTT
// - Expects `include/arduino_secrets.h` to provide SECRET_SSID and SECRET_PASS

#include <Arduino.h>
#include <WiFiS3.h>
#include <string.h>

#include <ArduinoMqttClient.h>
#include "arduino_secrets.h"
#include <EEPROM.h>

// MQTT broker settings (change to your broker)
const char* broker = "159.203.138.46"; // public test broker
const int brokerPort = 1883;

// Choose client type depending on available WiFi library
// Use WiFiClient type provided by the included WiFi library
WiFiClient wifiClient;

MqttClient mqttClient(wifiClient);

// clientId will be generated from MAC in setup() as r4-<last3bytes>
char clientIdBuf[32];
const char* clientId = clientIdBuf;
const char* publishTopic = "r4/example/pub";
const char* subscribeTopic = "r4/example/sub";

#ifndef MQTT_USER
const char* mqttUser = ""; // empty = no auth
#else
const char* mqttUser = MQTT_USER;
#endif
#ifndef MQTT_PASS
const char* mqttPass = "";
#else
const char* mqttPass = MQTT_PASS;
#endif

float pressure = 0;

unsigned long lastPublish = 0;
const unsigned long publishInterval = 5000;

// Input pins to watch (digital pins 2..5) and normally-closed limit switches on 6..7
// Pins 2..5: active-low switches (pressed -> LOW)
// Pins 6..7: normally-closed limit switches (triggered -> OPEN -> HIGH with INPUT_PULLUP)
const uint8_t watchPins[] = {2, 3, 4, 5, 6, 7};
const size_t watchCount = sizeof(watchPins) / sizeof(watchPins[0]);

// Debounce/state tracking
bool pinState[watchCount];
unsigned long lastDebounceTime[watchCount];
// Track previous raw reading to debounce correctly
bool lastReading[watchCount];
const unsigned long debounceDelay = 20; // ms

// Per-pin mode: true = normally-closed (NC, active when pin reads HIGH),
// false = normally-open (NO, active when pin reads LOW with INPUT_PULLUP)
bool pinIsNC[watchCount] = { false };

// Forward declaration for MQTT connect helper (defined below)
bool connectMQTT();

// Forward declaration for relay command helper (defined below)
void handleRelayCommandTokens(char *relayToken, char *stateToken, bool fromMqtt);
// Forward declaration for low-level relay send helper (defined below)
void sendRelayCommand(int relayNumber, bool on);


// Forward declarations for debug helpers (defined below)
void printWiFiStatus();
void printWiFiInfo();
void scanNetworksDebug();
// publish sequence/telemetry status
void publishSequenceStatus();
// debug helper forward declaration
void printWatchedPins();
// timestamped debug printf helper
void debugPrintf(const char *fmt, ...);

// Control framework (stubs)
void controlInit();
void controlStep();
void handleInputChange(uint8_t pin, bool state);
void handlePressureToRelays(float pressureVal);

// --- Analog sampling for A1 ------------------------------------------------
// Sample every 100 ms and compute running average over the last 1 second
const unsigned long sampleIntervalMs = 100; // 100 ms between samples
const unsigned long sampleWindowMs = 1000;  // running window length (1 second)
const size_t sampleWindowCount = sampleWindowMs / sampleIntervalMs; // 10 samples
uint16_t a1Samples[sampleWindowCount];
size_t a1SampleIndex = 0; // next index to overwrite
size_t a1SamplesFilled = 0; // how many valid samples currently in buffer (<= sampleWindowCount)
unsigned long a1SamplesSum = 0; // running sum for O(1) average
unsigned long lastA1SampleMillis = 0;
// ADC resolution to request from the core. Use highest common value (12-14 bits) on modern boards.
// This value is used to calculate the conversion from ADC counts -> volts.
const uint8_t adcResolutionBits = 14;
// ADC reference voltage in volts (change to match your board's reference). Common: 3.3V
float adcVref = 3.3f;
// Pressure mapping: 0..adcVref -> 0..maxPressurePsi
float maxPressurePsi = 3000.0f;
// Simple calibration applied after mapping: finalPsi = psi * sensorGain + sensorOffset
float sensorGain = 1.0f;
float sensorOffset = 0.0f;

// EEPROM storage layout
struct CalibStorage {
	uint32_t magic; // identifies valid storage
	float adcVref;
	float maxPressurePsi;
	float sensorGain;
	float sensorOffset;
	uint8_t filterMode;
	float emaAlpha;
	uint8_t pinModesBitmap; // bit i = 1 -> pinIsNC[i] == true
	uint32_t seqStableMs;
	uint32_t seqStartStableMs;
	uint32_t seqTimeoutMs;
};
const uint32_t CALIB_MAGIC = 0x43414C49; // 'CALI'
const int CALIB_EEPROM_ADDR = 0; // start

// Filtering
enum FilterMode { FILTER_NONE = 0, FILTER_MEDIAN3 = 1, FILTER_EMA = 2 };
FilterMode filterMode = FILTER_MEDIAN3;
// EMA state
float emaValue = 0.0f;
float emaAlpha = 0.2f; // smoothing factor (mutable)

// Relay board serial (Serial1) settings
const unsigned long relayBaud = 115200; // assume 9600 baud for relay board; change if needed
bool relayBoardPowered = false; // set true after R9 ON sent
// Echo everything from Serial1 to Serial (debugging)
bool relayEcho = true; // default enabled while debugging
// Auto-control settings: map inputs to relays R1..R4, pressure controls R5
bool autoControlEnabled = true;
float pressureHighThreshold = 2500.0f; // PSI to turn high-pressure relay on
float pressureLowThreshold = 500.0f; // PSI to turn it off
float pressureHysteresis = 25.0f; // hysteresis around thresholds
// Track relay states to avoid sending duplicate commands
bool relayStates[10] = {false}; // index 1..9 used

// Sequence state for 3+5 multi-stage action
// sequenceActive: when true the state machine owns R1/R2 until completion or abort
bool sequenceActive = false;
// sequenceStage: 0 = inactive, 1 = R1 active (waiting for pin6), 2 = R2 active (waiting for pin7)
uint8_t sequenceStage = 0;
// Sequence timing: require stable limit switch for this many ms before accepting (debounce)
unsigned long sequenceStableMs = 50; // must see stable limit for 50ms
// Sequence timeout: abort the sequence if it runs longer than this (ms)
unsigned long sequenceTimeoutMs = 30000; // default 30s
// Additional start-debounce: require 3+5 to be stable for this long before starting sequence
unsigned long sequenceStartStableMs = 100; // ms (increased from 30ms to be robust against loop jitter)
// Internal timers
unsigned long sequenceStageEnterMillis = 0; // when current stage started
unsigned long sequenceLastLimitChangeMillis = 0; // last time limit input changed state while sequence active
// track when 3+5 became active for start debounce
unsigned long sequenceStartCandidateMillis = 0;
// last published sequence state for MQTT (avoid duplicate publishes)
int lastPublishedSequenceState = 0;

// Pending single-press handling: defer immediate R1 toggles to allow detecting 3+5 sequence
int pendingPressPin = 0; // 3 if pending R1 press
unsigned long pendingPressMillis = 0;

// Set relay state if changed (1..9)
void setRelayState(int relayNumber, bool on) {
	if (relayNumber < 1 || relayNumber > 9) return;
	if (relayStates[relayNumber] == on) return; // no change
	relayStates[relayNumber] = on;
	sendRelayCommand(relayNumber, on);
	// publish updated sequence/relay status for telemetry consumers
	publishSequenceStatus();
	// debug print with timestamp
	debugPrintf("[DBG] setRelayState R%d -> %s\n", relayNumber, on ? "ON" : "OFF");
}

// Send a relay command to Serial1 in the format "R<digit> ON" or "R<digit> OFF"
void sendRelayCommand(int relayNumber, bool on) {
	if (relayNumber < 1 || relayNumber > 9) return; // only 1..9 allowed (9 = power)
	char buf[16];
	snprintf(buf, sizeof(buf), "R%d %s", relayNumber, on ? "ON" : "OFF");
	Serial1.println(buf);
	// small delay to allow board to process
	delay(10);
	// mark powered true if R9 ON
	if (relayNumber == 9 && on) relayBoardPowered = true;
	debugPrintf("Relay cmd -> %s\n", buf);
}


// Read A1, update circular buffer and running sum; returns current average as float
float sampleA1AndGetAverage() {
	unsigned long now = millis();
	int raw = analogRead(A1);

	// Apply simple filtering to raw sample before adding to buffer
	if (filterMode == FILTER_MEDIAN3) {
		// median of 3: read two quick extra samples
		int r1 = analogRead(A1);
		int r2 = analogRead(A1);
		int vals[3] = { raw, r1, r2 };
		// sort 3 values - simple
		for (int i = 0; i < 2; ++i) for (int j = i+1; j < 3; ++j) if (vals[j] < vals[i]) { int t = vals[i]; vals[i] = vals[j]; vals[j] = t; }
		raw = vals[1];
	} else if (filterMode == FILTER_EMA) {
		if (a1SamplesFilled == 0 && a1SampleIndex == 0 && emaValue == 0.0f) {
			emaValue = (float)raw;
		} else {
			emaValue = emaAlpha * (float)raw + (1.0f - emaAlpha) * emaValue;
		}
		raw = (int)emaValue;
	}

	// remove oldest value if buffer is full
	if (a1SamplesFilled == sampleWindowCount) {
		a1SamplesSum -= a1Samples[a1SampleIndex];
	} else {
		a1SamplesFilled++;
	}

	// store new sample
	a1Samples[a1SampleIndex] = (uint16_t)raw;
	a1SamplesSum += raw;

	// advance index
	a1SampleIndex = (a1SampleIndex + 1) % sampleWindowCount;

	// compute average over filled samples
	if (a1SamplesFilled == 0) return 0.0f;
	return (float)a1SamplesSum / (float)a1SamplesFilled;
}

// EEPROM helpers: load/save calibration values
void loadCalibration() {
	CalibStorage s;
	EEPROM.get(CALIB_EEPROM_ADDR, s);
	if (s.magic == CALIB_MAGIC) {
		adcVref = s.adcVref;
		maxPressurePsi = s.maxPressurePsi;
		sensorGain = s.sensorGain;
		sensorOffset = s.sensorOffset;
		// restore filter settings
		if (s.filterMode <= 2) filterMode = (FilterMode)s.filterMode;
		if (s.emaAlpha > 0.0f && s.emaAlpha <= 1.0f) emaAlpha = s.emaAlpha;
		emaValue = 0.0f; // reset EMA state

		// restore sequence timing if present (non-zero)
		// restore sequence timing if present and within sane bounds
		// ignore obviously invalid values (e.g., 0xFFFFFFFF) which may come from corrupted EEPROM
		if (s.seqStableMs > 0 && s.seqStableMs <= 10000) {
			sequenceStableMs = s.seqStableMs;
		} else if (s.seqStableMs != 0) {
			debugPrintf("[EEPROM] ignored invalid seqStableMs=%lu\n", (unsigned long)s.seqStableMs);
		}
		if (s.seqStartStableMs > 0 && s.seqStartStableMs <= 10000) {
			sequenceStartStableMs = s.seqStartStableMs;
		} else if (s.seqStartStableMs != 0) {
			debugPrintf("[EEPROM] ignored invalid seqStartStableMs=%lu\n", (unsigned long)s.seqStartStableMs);
		}
		if (s.seqTimeoutMs > 0 && s.seqTimeoutMs <= 600000) {
			sequenceTimeoutMs = s.seqTimeoutMs;
		} else if (s.seqTimeoutMs != 0) {
			debugPrintf("[EEPROM] ignored invalid seqTimeoutMs=%lu\n", (unsigned long)s.seqTimeoutMs);
		}
		// restore per-pin modes from bitmap
		for (size_t i = 0; i < watchCount; ++i) {
			pinIsNC[i] = (s.pinModesBitmap & (1 << i)) != 0;
			// refresh debounced state based on current pin reading and mode
			if (pinIsNC[i]) {
				pinState[i] = digitalRead(watchPins[i]) == HIGH;
				lastReading[i] = pinState[i];
			} else {
				pinState[i] = digitalRead(watchPins[i]) == LOW;
				lastReading[i] = pinState[i];
			}
			lastDebounceTime[i] = millis();
		}
		Serial.println("Loaded calibration from EEPROM");
	} else {
		Serial.println("No valid calibration in EEPROM; using defaults");
	}
}

// ---------------------- Control framework stubs -------------------------
// These are intentionally minimal; implement control logic here.
void controlInit() {
	// initialize control state or timers here
}

void controlStep() {
	// periodic control tasks: handle sequence timeouts and stable-limit checks
	// Process pending single-press mappings even when sequence not active
	unsigned long now = millis();

	// If a sequence start candidate timer exists, check it here so the sequence can start
	// even if handleInputChange returned early due to pending press logic.
	if (!sequenceActive && sequenceStartCandidateMillis > 0) {
		unsigned long elapsed = now - sequenceStartCandidateMillis;
		// diagnostic: log evaluation of sequence start candidate
		debugPrintf("[DBG] evaluating sequence start candidate elapsed=%lu/%lu ms\n", elapsed, sequenceStartStableMs);
		if (elapsed >= sequenceStartStableMs) {
			// verify both pins still active
			int cidx3 = -1, cidx5 = -1;
			for (size_t ii = 0; ii < watchCount; ++ii) {
				if (watchPins[ii] == 3) cidx3 = ii;
				if (watchPins[ii] == 5) cidx5 = ii;
			}
			bool both = (cidx3 >= 0 && cidx5 >= 0) ? (pinState[cidx3] && pinState[cidx5]) : false;
			if (both) {
				// start sequence
				sequenceActive = true;
				sequenceStage = 1;
				sequenceStageEnterMillis = now;
				sequenceLastLimitChangeMillis = 0;
				sequenceStartCandidateMillis = 0;
				Serial.println("Sequence started (controlStep): R1 until limit pin6");
				printWatchedPins();
				setRelayState(1, true);
				setRelayState(2, false);
				publishSequenceStatus();
				// clear any pending press since sequence now owns R1
				pendingPressPin = 0;
				pendingPressMillis = 0;
				return; // sequence takes precedence; skip pending apply below
			} else {
				// not both still active -> cancel candidate
				sequenceStartCandidateMillis = 0;
			}
		}
	}
	if (!sequenceActive && pendingPressPin == 3 && pendingPressMillis > 0) {
		if (now - pendingPressMillis >= sequenceStartStableMs) {
			// No sequence started during pending window; apply mapping for pin3
			int idx3 = -1;
			for (size_t ii = 0; ii < watchCount; ++ii) if (watchPins[ii] == 3) { idx3 = ii; break; }
			if (idx3 >= 0) {
				Serial.println("[DBG] pending press timeout: applying pin3 mapping");
				setRelayState(1, pinState[idx3]);
			}
			pendingPressPin = 0;
			pendingPressMillis = 0;
			// publish status
			publishSequenceStatus();
		}
	}

	// If no sequence active, skip sequence-specific checks
	if (!sequenceActive) return;

	// If sequence exceeded timeout, abort
	if (sequenceStageEnterMillis > 0 && (now - sequenceStageEnterMillis) > sequenceTimeoutMs) {
		Serial.println("Sequence timeout: aborting");
		// update state then turn off outputs and publish status
		sequenceActive = false;
		sequenceStage = 0;
		sequenceStageEnterMillis = 0;
		sequenceLastLimitChangeMillis = 0;
		setRelayState(1, false);
		setRelayState(2, false);
		publishSequenceStatus();
		if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("timeout_abort"); mqttClient.endMessage(); } }
		return;
	}

	// For stage-specific stable checks we rely on the debounced pinState[] values and
	// ensure the limit stays active for sequenceStableMs before accepting it.
	int idx6 = -1, idx7 = -1;
	for (size_t ii = 0; ii < watchCount; ++ii) {
		if (watchPins[ii] == 6) idx6 = ii;
		if (watchPins[ii] == 7) idx7 = ii;
	}

	if (sequenceStage == 1 && idx6 >= 0) {
		if (pinState[idx6]) {
			// limit true; ensure stable
			if (sequenceLastLimitChangeMillis == 0) sequenceLastLimitChangeMillis = now;
			if (now - sequenceLastLimitChangeMillis >= sequenceStableMs) {
				// accept transition
				Serial.println("Sequence: stable limit6 detected in controlStep -> switching to R2");
				printWatchedPins();
				// advance stage first so published status reflects new stage
				sequenceStage = 2;
				sequenceStageEnterMillis = now;
				sequenceLastLimitChangeMillis = 0;
				setRelayState(1, false);
				setRelayState(2, true);
				publishSequenceStatus();
				if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("switched_to_R2"); mqttClient.endMessage(); } }
			}
		} else {
			// limit not active; reset stable timer
			sequenceLastLimitChangeMillis = 0;
		}
	} else if (sequenceStage == 2 && idx7 >= 0) {
		if (pinState[idx7]) {
			if (sequenceLastLimitChangeMillis == 0) sequenceLastLimitChangeMillis = now;
			if (now - sequenceLastLimitChangeMillis >= sequenceStableMs) {
				Serial.println("Sequence: stable limit7 detected in controlStep -> completing sequence");
				printWatchedPins();
				// clear state then turn off relay and publish
				sequenceActive = false;
				sequenceStage = 0;
				sequenceStageEnterMillis = 0;
				sequenceLastLimitChangeMillis = 0;
				setRelayState(2, false);
				publishSequenceStatus();
				if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("complete"); mqttClient.endMessage(); } }
			}
		} else {
			sequenceLastLimitChangeMillis = 0;
		}
	}
}

void handleInputChange(uint8_t pin, bool state) {
	// Called when an input is debounced and changes state
	// pin: hardware pin number (e.g., 2..5), state:true=active (pressed)
	debugPrintf("Input changed: pin %d -> %d\n", pin, state ? 1 : 0);

	// Sequence control state (see globals below)
	// If a sequence is active, certain inputs are handled by the sequence state machine
	// Otherwise fall back to simple direct mapping for pins 2 and 3

	// Find indices for pins 3 and 5 in watchPins
	int idx3 = -1, idx5 = -1, idx6 = -1, idx7 = -1;
	for (size_t ii = 0; ii < watchCount; ++ii) {
		if (watchPins[ii] == 3) idx3 = ii;
		if (watchPins[ii] == 5) idx5 = ii;
		if (watchPins[ii] == 6) idx6 = ii;
		if (watchPins[ii] == 7) idx7 = ii;
	}

	// Determine whether pins 3 and 5 are both active. Use the incoming 'state' for the pin
	// that triggered this callback to avoid races where pinState[] hasn't been updated yet.
	bool state3 = false, state5 = false;
	if (idx3 >= 0) state3 = (pin == 3) ? state : pinState[idx3];
	if (idx5 >= 0) state5 = (pin == 5) ? state : pinState[idx5];
	bool seq3and5 = (idx3 >= 0 && idx5 >= 0) ? (state3 && state5) : false;

	// If sequence not active, handle potential sequence start candidate first
	if (!sequenceActive) {
		if (seq3and5) {
			// Both buttons are currently active; set a start candidate timer and return
			if (sequenceStartCandidateMillis == 0) sequenceStartCandidateMillis = millis();
				debugPrintf("[DBG] sequence start candidate set\n");
			return; // don't proceed to pending single-press mapping
		} else {
			// If we had a candidate and both are no longer held, check if candidate timed out to start
			if (sequenceStartCandidateMillis != 0) {
				unsigned long now = millis();
				if (now - sequenceStartCandidateMillis >= sequenceStartStableMs) {
					// verify both pins are still active (use pinState which was updated prior to this call)
					bool both = (idx3 >= 0 && idx5 >= 0) ? (pinState[idx3] && pinState[idx5]) : false;
					if (both) {
						// Start the sequence
						sequenceActive = true;
						sequenceStage = 1; // stage 1: R1 ON until pin6 active
						sequenceStageEnterMillis = now;
						sequenceLastLimitChangeMillis = 0;
						sequenceStartCandidateMillis = 0;
						Serial.println("Sequence started: R1 until limit pin6");
						Serial.print("[DBG] start time="); Serial.println(sequenceStageEnterMillis);
						printWatchedPins();
						setRelayState(1, true); // R1 on
						setRelayState(2, false); // ensure R2 off
						// publish status after starting
						publishSequenceStatus();
						if (mqttClient.connected()) {
							if (mqttClient.beginMessage("r4/sequence/state")) { mqttClient.print("start"); mqttClient.endMessage(); }
							if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("started_R1"); mqttClient.endMessage(); }
						}
						return; // sequence takes precedence
					}
				}
				// otherwise cancel the candidate
				sequenceStartCandidateMillis = 0;
			}
		}
	}

	// If not starting a sequence and pin 3 changed, defer immediate mapping briefly to allow a simultaneous press of pin5
	if (!sequenceActive && pin == 3) {
		if (state) {
			// pin3 pressed: set pending press and wait for sequenceStartStableMs to see if pin5 also pressed
			pendingPressPin = 3;
			pendingPressMillis = millis();
			debugPrintf("[DBG] pending press registered for pin3\n");
			return;
		} else {
			// release of pin3: if pending, clear it
			if (pendingPressPin == 3) {
				pendingPressPin = 0;
				pendingPressMillis = 0;
			}
			// If not pending, fall through to normal mapping
		}
	}

	// If sequence active, progress or abort based on inputs and limits
	if (sequenceActive) {
		// If 3 or 5 released, abort the sequence and turn off R1/R2
		if (!seq3and5) {
			Serial.println("Sequence aborted: 3+5 released");
			printWatchedPins();
			setRelayState(1, false);
			setRelayState(2, false);
			sequenceActive = false;
			sequenceStage = 0;
			sequenceStageEnterMillis = 0;
			sequenceLastLimitChangeMillis = 0;
			publishSequenceStatus();
			if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/state")) { mqttClient.print("abort"); mqttClient.endMessage(); } if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("aborted_released"); mqttClient.endMessage(); } }
			if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/control/resp")) { mqttClient.print("Sequence aborted"); mqttClient.endMessage(); } }
			return;
		}

		// If in stage 1, check limit on pin6
		if (sequenceStage == 1) {
			if (idx6 >= 0 && pinState[idx6]) {
				// limit reached: stop R1, start R2
				Serial.println("Sequence: limit pin6 reached -> switch to R2");
				setRelayState(1, false);
				setRelayState(2, true);
				sequenceStage = 2;
				sequenceStageEnterMillis = millis();
				sequenceLastLimitChangeMillis = 0;
				if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/state")) { mqttClient.print("stage2"); mqttClient.endMessage(); } if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("switched_to_R2"); mqttClient.endMessage(); } }
			}
			return;
		}

		// If in stage 2, check limit on pin7
		if (sequenceStage == 2) {
			if (idx7 >= 0 && pinState[idx7]) {
				// final limit reached: stop R2 and finish
				Serial.println("Sequence: limit pin7 reached -> sequence complete");
				setRelayState(2, false);
				sequenceActive = false;
				sequenceStage = 0;
				sequenceStageEnterMillis = 0;
				sequenceLastLimitChangeMillis = 0;
				if (mqttClient.connected()) { if (mqttClient.beginMessage("r4/sequence/state")) { mqttClient.print("complete"); mqttClient.endMessage(); } if (mqttClient.beginMessage("r4/sequence/event")) { mqttClient.print("complete"); mqttClient.endMessage(); } }
			}
			return;
		}

		return; // safety net: sequence handled
	}

	// Default direct mapping when no sequence active
	if (pin == 2) {
		setRelayState(2, state);
	} else if (pin == 3) {
		// if we reached here and there's no pending press, apply mapping
		if (pendingPressPin != 3) setRelayState(1, state);
	}

	// Clear previous simple two-button R4 action (no longer used)
}

void handlePressureToRelays(float pressureVal) {
	// Safety: if pressure exceeds safety threshold, shut down relays
	static bool safetyActive = false;
	const float safetyThreshold = 2750.0f; // PSI
	if (pressureVal >= safetyThreshold) {
		if (!safetyActive) {
			safetyActive = true;
			// turn off relays R1..R2 (leave R9 as power control if desired)
			for (int r = 1; r <= 2; ++r) setRelayState(r, false);
			// log and publish an alert
			Serial.print("SAFETY: pressure "); Serial.print(pressureVal); Serial.println(" >= threshold, shutting down relays");
			if (mqttClient.connected()) {
				if (mqttClient.beginMessage("r4/control/resp")) {
					mqttClient.print("SAFETY: pressure exceeded, outputs disabled");
					mqttClient.endMessage();
				}
			}
		}
	} else {
		// clear safety if pressure falls back below threshold
		if (safetyActive && pressureVal < safetyThreshold - 10.0f) {
			safetyActive = false;
			Serial.print("SAFETY: pressure "); Serial.print(pressureVal); Serial.println(" below threshold, safety cleared");
			if (mqttClient.connected()) {
				if (mqttClient.beginMessage("r4/control/resp")) {
					mqttClient.print("SAFETY: cleared");
					mqttClient.endMessage();
				}
			}
		}
	}
}


void saveCalibration() {
	CalibStorage s;
	s.magic = CALIB_MAGIC;
	s.adcVref = adcVref;
	s.maxPressurePsi = maxPressurePsi;
	s.sensorGain = sensorGain;
	s.sensorOffset = sensorOffset;
	s.filterMode = (uint8_t)filterMode;
	s.emaAlpha = emaAlpha;
	// pack pinIsNC[] into bitmap
	uint8_t bmp = 0;
	for (size_t i = 0; i < watchCount && i < 8; ++i) if (pinIsNC[i]) bmp |= (1 << i);
	s.pinModesBitmap = bmp;
	// persist sequence timing values
	s.seqStableMs = (uint32_t)sequenceStableMs;
	s.seqStartStableMs = (uint32_t)sequenceStartStableMs;
	s.seqTimeoutMs = (uint32_t)sequenceTimeoutMs;
	EEPROM.put(CALIB_EEPROM_ADDR, s);
	Serial.println("Calibration saved to EEPROM");
}


void connectWiFi() {
	if (WiFi.status() == WL_CONNECTED) return;
	Serial.print("Connecting to WiFi '");
	Serial.print(SECRET_SSID);
	Serial.println("'...");

	int status = WiFi.begin(SECRET_SSID, SECRET_PASS);
	unsigned long start = millis();
	while (status != WL_CONNECTED && millis() - start < 20000) {
		delay(500);
		Serial.print('.');
		status = WiFi.status();
	}

	if (WiFi.status() == WL_CONNECTED) {
		Serial.println();
		Serial.print("Connected, IP: ");
		Serial.println(WiFi.localIP());
		printWiFiInfo();
	} else {
		Serial.println();
		Serial.println("Failed to connect to WiFi");
		printWiFiStatus();
		scanNetworksDebug();
	}
}

// Debug helpers -------------------------------------------------------------
void printWiFiStatus() {
	uint8_t s = WiFi.status();
	Serial.print("WiFi status: ");
	switch (s) {
		case WL_NO_SHIELD: Serial.println("WL_NO_SHIELD"); break;
		case WL_IDLE_STATUS: Serial.println("WL_IDLE_STATUS"); break;
		case WL_NO_SSID_AVAIL: Serial.println("WL_NO_SSID_AVAIL"); break;
		case WL_SCAN_COMPLETED: Serial.println("WL_SCAN_COMPLETED"); break;
		case WL_CONNECTED: Serial.println("WL_CONNECTED"); break;
		case WL_CONNECT_FAILED: Serial.println("WL_CONNECT_FAILED"); break;
		case WL_CONNECTION_LOST: Serial.println("WL_CONNECTION_LOST"); break;
		case WL_DISCONNECTED: Serial.println("WL_DISCONNECTED"); break;
		default: Serial.println(s); break;
	}
}

void printWiFiInfo() {
	Serial.print("Firmware: ");
	Serial.println(WiFi.firmwareVersion());

	uint8_t mac[6];
	if (WiFi.macAddress(mac)) {
		Serial.print("MAC: ");
		for (int i = 0; i < 6; ++i) {
			if (i) Serial.print(":");
			if (mac[i] < 16) Serial.print('0');
			Serial.print(mac[i], HEX);
		}
		Serial.println();
	}

	Serial.print("Local IP: ");
	Serial.println(WiFi.localIP());
	Serial.print("Gateway: ");
	Serial.println(WiFi.gatewayIP());
	Serial.print("Subnet: ");
	Serial.println(WiFi.subnetMask());
	Serial.print("RSSI: ");
	Serial.println(WiFi.RSSI());
}

void scanNetworksDebug() {
	Serial.println("Scanning for networks...");
	int n = WiFi.scanNetworks();
	Serial.print(n);
	Serial.println(" networks found");
	for (int i = 0; i < n; ++i) {
		Serial.print(i);
		Serial.print(": ");
		Serial.print(WiFi.SSID(i));
		Serial.print(" (RSSI: ");
		Serial.print(WiFi.RSSI(i));
		Serial.print(")");
		Serial.print("  Encryption: ");
		Serial.println(WiFi.encryptionType(i));
	}
}

// Debug helper: print current watched pin states
void printWatchedPins() {
	Serial.print("[DBG] pins:");
	for (size_t i = 0; i < watchCount; ++i) {
		Serial.print(" "); Serial.print(watchPins[i]); Serial.print(":"); Serial.print(pinState[i] ? "ON" : "OFF");
	}
	Serial.println();
}

// Simple printf-style helper that prefixes messages with a millisecond timestamp.
// Usage: debugPrintf("[DBG] message x=%d\n", x);
void debugPrintf(const char *fmt, ...) {
	char buf[256];
	unsigned long ts = millis();
	int off = snprintf(buf, sizeof(buf), "[TS %6lu] ", ts);
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf + off, sizeof(buf) - off, fmt, ap);
	va_end(ap);
	Serial.print(buf);
}

// Publish a compact sequence status to MQTT topic r4/sequence/status
void publishSequenceStatus() {
	if (!mqttClient.connected()) return;
	// Build a simple payload: stage=<n> active=<0|1> stageMillis=<since> relays=R1:0,R2:1,... seqStableMs=.. seqStartStableMs=.. seqTimeoutMs=..
	char payload[256];
	unsigned long now = millis();
	unsigned long stageElapsed = 0;
	if (sequenceStageEnterMillis > 0) stageElapsed = now - sequenceStageEnterMillis;
	int off = 0;
	off += snprintf(payload + off, sizeof(payload) - off, "stage=%u active=%d elapsed=%lu", (unsigned)sequenceStage, sequenceActive ? 1 : 0, stageElapsed);
	off += snprintf(payload + off, sizeof(payload) - off, " seqStableMs=%lu seqStartStableMs=%lu seqTimeoutMs=%lu", (unsigned long)sequenceStableMs, (unsigned long)sequenceStartStableMs, (unsigned long)sequenceTimeoutMs);
	off += snprintf(payload + off, sizeof(payload) - off, " relays=");
	for (int r = 1; r <= 9; ++r) {
		off += snprintf(payload + off, sizeof(payload) - off, "R%d:%d", r, relayStates[r] ? 1 : 0);
		if (r < 9) off += snprintf(payload + off, sizeof(payload) - off, ",");
	}
	if (mqttClient.beginMessage("r4/sequence/status")) {
		mqttClient.print(payload);
		mqttClient.endMessage();
	}
}

bool connectMQTT() {
	if (mqttClient.connected()) return true;
	if (WiFi.status() != WL_CONNECTED) {
		connectWiFi();
		if (WiFi.status() != WL_CONNECTED) return false;
	}

	Serial.print("Connecting to MQTT broker ");
	Serial.print(broker);
	Serial.print(":");
	Serial.println(brokerPort);

	// Ensure the MQTT client uses our stable clientId and credentials (if any)
	mqttClient.setId(clientId);
	if (strlen(mqttUser) > 0 || strlen(mqttPass) > 0) {
		mqttClient.setUsernamePassword(mqttUser, mqttPass);
	}

	// Connect using the library-supported connect(host, port) overload.
	if (!mqttClient.connect(broker, brokerPort)) {
		Serial.print("MQTT connect failed, error: ");
		Serial.println(mqttClient.connectError());
		return false;
	}

	Serial.println("MQTT connected");
	// Subscribe to a topic
	if (!mqttClient.subscribe(subscribeTopic)) {
		Serial.println("Failed to subscribe to topic");
	}
	// Subscribe to control topic for remote calibration commands
	if (!mqttClient.subscribe("r4/control")) {
		Serial.println("Failed to subscribe to control topic r4/control");
	}

	return true;
}

void messageCallback(int messageSize) {
	// Read incoming message into a buffer (up to reasonable size)
	char buf[256];
	int idx = 0;
	while (mqttClient.available() && idx < (int)sizeof(buf)-1) {
		buf[idx++] = (char)mqttClient.read();
	}
	buf[idx] = '\0';
	Serial.print("Incoming MQTT message on control topic: "); Serial.println(buf);

	// Tokenize first word to decide action: help, show, set
	char *cmd = strtok(buf, " ");
	if (!cmd) return;

	char resp[256];
	resp[0] = '\0';

	if (strcasecmp(cmd, "help") == 0) {
		snprintf(resp, sizeof(resp), "Commands: help, show, pins (serial-only), set vref <val>, set maxpsi <val>, set gain <val>, set offset <val>, set filter <none|median3|ema>, set emaalpha <val>, set pinmode <pin> <nc|no>, set seqstable <ms>, set seqstartstable <ms>, set seqtimeout <ms>, set autocontrol <on|off>, set phigh <val>, set plow <val>, relay R<n> ON|OFF");
	} else if (strcasecmp(cmd, "show") == 0) {
		// build a multi-field response and include sequence settings + relay states
		char relbuf[128];
		int off = 0;
		off += snprintf(relbuf + off, sizeof(relbuf) - off, "relays:");
		for (int r = 1; r <= 9; ++r) {
			off += snprintf(relbuf + off, sizeof(relbuf) - off, " R%d=%s", r, relayStates[r] ? "ON" : "OFF");
		}
		snprintf(resp, sizeof(resp), "adcVref=%.6f maxPressurePsi=%.2f sensorGain=%.6f sensorOffset=%.6f filter=%s emaAlpha=%.3f seqStableMs=%lu seqStartStableMs=%lu seqTimeoutMs=%lu %s",
			adcVref, maxPressurePsi, sensorGain, sensorOffset,
			(filterMode==FILTER_NONE?"none":(filterMode==FILTER_MEDIAN3?"median3":"ema")), emaAlpha,
			(unsigned long)sequenceStableMs, (unsigned long)sequenceStartStableMs, (unsigned long)sequenceTimeoutMs,
			relbuf);
	} else if (strcasecmp(cmd, "set") == 0) {
			char *sub = strtok(NULL, " ");
			char *val = strtok(NULL, " ");
			if (!sub || !val) return;
			if (strcasecmp(sub, "vref") == 0) { adcVref = atof(val); saveCalibration(); snprintf(resp, sizeof(resp), "vref set %.6f", adcVref); }
			else if (strcasecmp(sub, "maxpsi") == 0) { maxPressurePsi = atof(val); saveCalibration(); snprintf(resp, sizeof(resp), "maxpsi set %.2f", maxPressurePsi); }
			else if (strcasecmp(sub, "gain") == 0) { sensorGain = atof(val); saveCalibration(); snprintf(resp, sizeof(resp), "gain set %.6f", sensorGain); }
			else if (strcasecmp(sub, "offset") == 0) { sensorOffset = atof(val); saveCalibration(); snprintf(resp, sizeof(resp), "offset set %.6f", sensorOffset); }
			else if (strcasecmp(sub, "filter") == 0) {
				if (strcasecmp(val, "none") == 0) { filterMode = FILTER_NONE; saveCalibration(); snprintf(resp, sizeof(resp), "filter set none"); }
				else if (strcasecmp(val, "median3") == 0) { filterMode = FILTER_MEDIAN3; saveCalibration(); snprintf(resp, sizeof(resp), "filter set median3"); }
				else if (strcasecmp(val, "ema") == 0) { filterMode = FILTER_EMA; saveCalibration(); snprintf(resp, sizeof(resp), "filter set ema"); }
				else { snprintf(resp, sizeof(resp), "unknown filter %s", val); }
			}
			else if (strcasecmp(sub, "emaalpha") == 0) { float v = atof(val); if (v > 0.0f && v <= 1.0f) { emaAlpha = v; saveCalibration(); snprintf(resp, sizeof(resp), "emaalpha set %.3f", emaAlpha); } else { snprintf(resp, sizeof(resp), "invalid emaalpha %s", val); } }
			else if (strcasecmp(sub, "pinmode") == 0) {
				char *pinTok = strtok(NULL, " ");
				char *modeTok = strtok(NULL, " ");
				if (!pinTok || !modeTok) { snprintf(resp, sizeof(resp), "Usage: set pinmode <pin> <nc|no>"); }
				else {
					int pin = atoi(pinTok);
					// find index in watchPins
					int idx = -1;
					for (size_t ii = 0; ii < watchCount; ++ii) if (watchPins[ii] == (uint8_t)pin) { idx = ii; break; }
					if (idx < 0) { snprintf(resp, sizeof(resp), "pin %d not in watch list", pin); }
					else {
						if (strcasecmp(modeTok, "nc") == 0) pinIsNC[idx] = true;
						else if (strcasecmp(modeTok, "no") == 0) pinIsNC[idx] = false;
						else { snprintf(resp, sizeof(resp), "unknown mode %s", modeTok); goto pm_end; }
						// persist pin modes
						saveCalibration();
						// update pinState/lastReading based on new mode
						if (pinIsNC[idx]) {
							pinState[idx] = digitalRead(watchPins[idx]) == HIGH;
							lastReading[idx] = pinState[idx];
						} else {
							pinState[idx] = digitalRead(watchPins[idx]) == LOW;
							lastReading[idx] = pinState[idx];
						}
						snprintf(resp, sizeof(resp), "pin %d mode set to %s", pin, pinIsNC[idx] ? "NC" : "NO");
					}
				}
			pm_end: ;
			}
			else if (strcasecmp(sub, "relayecho") == 0) { if (strcasecmp(val, "on") == 0) { relayEcho = true; snprintf(resp, sizeof(resp), "relayEcho ON"); } else if (strcasecmp(val, "off") == 0) { relayEcho = false; snprintf(resp, sizeof(resp), "relayEcho OFF"); } else { snprintf(resp, sizeof(resp), "Usage: set relayecho on|off"); } saveCalibration(); }
				else if (strcasecmp(sub, "seqstable") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceStableMs = v; saveCalibration(); snprintf(resp, sizeof(resp), "seqStableMs set %lu", sequenceStableMs); }
				else if (strcasecmp(sub, "seqstartstable") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceStartStableMs = v; saveCalibration(); snprintf(resp, sizeof(resp), "seqStartStableMs set %lu", sequenceStartStableMs); }
				else if (strcasecmp(sub, "seqtimeout") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceTimeoutMs = v; saveCalibration(); snprintf(resp, sizeof(resp), "seqTimeoutMs set %lu", sequenceTimeoutMs); }
			else { snprintf(resp, sizeof(resp), "unknown set target %s", sub); }
	} else if (strcasecmp(cmd, "relay") == 0) {
		char *relayTok = strtok(NULL, " ");
		char *stateTok = strtok(NULL, " ");
		if (!relayTok || !stateTok) {
			snprintf(resp, sizeof(resp), "Usage: relay R<n> ON|OFF");
		} else {
			// Delegate to helper which will send commands over Serial1 and publish ack when fromMqtt=true
			handleRelayCommandTokens(relayTok, stateTok, true);
			// helper already published an ack; clear resp to avoid duplicate
			resp[0] = '\0';
		}
	} else {
		snprintf(resp, sizeof(resp), "unknown command %s", cmd);
	}

	// Publish acknowledgement/response to r4/control/resp
	if (resp[0] != '\0') {
		if (mqttClient.beginMessage("r4/control/resp")) { mqttClient.print(resp); mqttClient.endMessage(); }
		Serial.print("MQTT control response: "); Serial.println(resp);
	}
}

// Helper to parse relay commands from tokens: expects token after 'relay' like "R3" and a state token "ON"/"OFF"
void handleRelayCommandTokens(char *relayToken, char *stateToken, bool fromMqtt) {
	if (!relayToken || !stateToken) return;
	// relayToken expected like R1..R9 or r1..r9
	if ((relayToken[0] != 'R' && relayToken[0] != 'r') || strlen(relayToken) < 2) return;
	int num = atoi(relayToken + 1);
	bool on = (strcasecmp(stateToken, "ON") == 0);
	if (num < 1 || num > 9) {
		if (fromMqtt && mqttClient.beginMessage("r4/control/resp")) { mqttClient.print("invalid relay number"); mqttClient.endMessage(); }
		Serial.println("Invalid relay number");
		return;
	}
	// Ensure relay board powered by sending R9 ON first if not powered
	if (!relayBoardPowered && num != 9) {
		sendRelayCommand(9, true);
		delay(50);
	}
	sendRelayCommand(num, on);
	// ack
	char ack[64];
	snprintf(ack, sizeof(ack), "relay %s %s", relayToken, on ? "ON" : "OFF");
	if (fromMqtt && mqttClient.beginMessage("r4/control/resp")) { mqttClient.print(ack); mqttClient.endMessage(); }
	Serial.print("Relay ack: "); Serial.println(ack);
}

void setup() {
	Serial.begin(115200);
	while (!Serial) { delay(10); }

	// Initialize Serial1 for relay board and power it (R9 ON must be sent first)
	Serial1.begin(relayBaud);
	delay(50);
	sendRelayCommand(9, false); // power the relay add-on board.  'R9 OFF' powers on the relay add-on board

	Serial.println("UNO R4 WiFi MQTT example starting...");

	connectWiFi();

	// Initialize control framework (user-provided logic will live here)
	controlInit();

	// Build a stable clientId from the MAC address: r4-<last3bytes>
	uint8_t mac[6];
	if (WiFi.macAddress(mac)) {
		snprintf(clientIdBuf, sizeof(clientIdBuf), "r4-%02X%02X%02X", mac[3], mac[4], mac[5]);
	} else {
		// fallback
		snprintf(clientIdBuf, sizeof(clientIdBuf), "r4-unknown");
	}
	Serial.print("Using MQTT clientId: "); Serial.println(clientId);

	mqttClient.onMessage(messageCallback);
	connectMQTT();

	lastPublish = millis();

	// Initialize watch pins and state
	// Default mode: treat pins as normally-open (NO); user can change to NC later
	for (size_t i = 0; i < watchCount; ++i) {
		
		// Default per-pin mode: set pins 6 and 7 to NO (false) by default
		if (watchPins[i] == 6 || watchPins[i] == 7) pinIsNC[i] = false;

		// Determine initial active state depending on pin mode
		bool v;
		if (pinIsNC[i]) {
			// normally-closed: active when pin reads HIGH (open)
			pinMode(watchPins[i], INPUT);
			v = digitalRead(watchPins[i]) == HIGH;
		} else {
			// normally-open with INPUT_PULLUP: active when pin reads LOW
			pinMode(watchPins[i], INPUT_PULLUP);
			v = digitalRead(watchPins[i]) == LOW;
		}
		pinState[i] = v;
		lastReading[i] = v;
		lastDebounceTime[i] = millis();
			// Publish initial state to per-pin topic and a debug topic
			char topicBuf[48];
			snprintf(topicBuf, sizeof(topicBuf), "r4/inputs/%u", watchPins[i]);
			//Serial.print("Pin "); Serial.print(i); Serial.print(" -> "); Serial.println(watchPins[i]);
			//Serial.print("MQTT connected? "); Serial.println(mqttClient.connected() ? "yes" : "no");
			if (!mqttClient.connected()) {
				Serial.println("Attempting MQTT reconnect before publish...");
				if (!connectMQTT()) {
					Serial.println("MQTT reconnect failed — skipping MQTT publish for pin state");
					return;
				}
			}
				if (!mqttClient.connected()) {
				Serial.println("Attempting MQTT reconnect before publish...");
				if (!connectMQTT()) {
					Serial.println("MQTT reconnect failed — skipping MQTT publish for pin state");
					return;
				}
				}
				
				if (mqttClient.beginMessage(topicBuf)) {
					char payload[64];
					if (lastReading[i]) {
						snprintf(payload, sizeof(payload), "ON %d", 1);
					} else {
						snprintf(payload, sizeof(payload), "OFF %d", 0);
					}
					mqttClient.print(payload);
					mqttClient.endMessage();
					Serial.print("MQTT published to "); Serial.print(topicBuf); Serial.print(" payload="); Serial.println(lastReading[i]);
				} else {
					Serial.print("Failed to begin MQTT message for pin state (error=");
					Serial.print(mqttClient.connectError());
					Serial.println(")");
				}
  }

	// Initialize A1 sampling buffer
	for (size_t i = 0; i < sampleWindowCount; ++i) {
		a1Samples[i] = 0;
	}
	a1SampleIndex = 0;
	a1SamplesFilled = 0;
	a1SamplesSum = 0;
	lastA1SampleMillis = millis();

	// Configure ADC resolution (some cores ignore this if unsupported)
	// Request the highest common resolution (12 bits) so analogRead() returns 0..4095
	Serial.print("Setting ADC resolution to "); Serial.print(adcResolutionBits); Serial.println(" bits (if supported by core)");
	analogReadResolution(adcResolutionBits);

	// Load calibration from EEPROM (if present)
	loadCalibration();
}

void loop() {
	// Ensure MQTT connected and process incoming messages
	if (!mqttClient.connected()) {
		if (connectMQTT()) {
			// Re-subscribe after reconnect
			mqttClient.subscribe(subscribeTopic);
		} else {
			// Wait and retry
			delay(2000);
			return;
		}
	}

	mqttClient.poll();

	// --- A1 sampling every 100 ms (non-blocking)
	unsigned long now = millis();
	if (now - lastA1SampleMillis >= sampleIntervalMs) {
		lastA1SampleMillis = now;
		float avgCounts = sampleA1AndGetAverage();
		// Compute volts from ADC counts using configured resolution and Vref
		float maxCount = (float)((1UL << adcResolutionBits) - 1UL);
		float volts = 0.0f;
		if (maxCount > 0.0f) volts = (avgCounts * adcVref) / maxCount;
		// Map volts (0..adcVref) to PSI (0..maxPressurePsi)
		float psi = 0.0f;
		if (adcVref > 0.0f) psi = (volts / adcVref) * maxPressurePsi;

		// Apply calibration (gain + offset)
		float calibratedPsi = psi * sensorGain + sensorOffset;
		// store PSI in global pressure for publishing elsewhere
		pressure = calibratedPsi;

		// If auto-control enabled, control R5 based on pressure with hysteresis
		// delegate pressure -> relays logic to the control framework
		handlePressureToRelays(pressure);

		// Print ADC average, volts and PSI
		//Serial.print("A1 avg counts: "); Serial.print(avgCounts, 2);
		//Serial.print("  V: "); Serial.print(volts, 3);
		//Serial.print(" V  PSI(raw): "); Serial.print(psi, 2);
		//Serial.print("  PSI(cal): "); Serial.println(calibratedPsi, 2);
	}

	// Run periodic control step (sequence timeout and stable-limit checks)
	controlStep();

	// Echo any data from Serial1 (relay controller) to main Serial for debugging
	if (relayEcho && Serial1.available()) {
		while (Serial1.available()) {
			int b = Serial1.read();
			if (b >= 0) Serial.write((uint8_t)b);
		}
	}

	// Non-blocking Serial CLI: read a line when available and process commands
	if (Serial.available()) {
		static char lineBuf[80];
		static uint8_t linePos = 0;
		while (Serial.available()) {
			char c = Serial.read();
			if (c == '\r') continue; // ignore CR
			if (c == '\n') {
				lineBuf[linePos] = '\0';
				linePos = 0;
				// process command in lineBuf
				if (strlen(lineBuf) == 0) break;
				// Tokenize simple commands
				char *cmd = strtok(lineBuf, " ");
				if (cmd == NULL) break;
				if (strcasecmp(cmd, "help") == 0) {
					Serial.println("Commands: help, pins, show, set vref <val>, set maxpsi <val>, set gain <val>, set offset <val>, set pinmode <pin> <nc|no>, set seqstable <ms>, set seqstartstable <ms>, set seqtimeout <ms>, set autocontrol <on|off>, set phigh <val>, set plow <val>, relay R<n> ON|OFF");
				} else if (strcasecmp(cmd, "show") == 0) {
					Serial.print("adcVref="); Serial.print(adcVref, 4);
					Serial.print("  maxPressurePsi="); Serial.print(maxPressurePsi, 2);
					Serial.print("  sensorGain="); Serial.print(sensorGain, 4);
					Serial.print("  sensorOffset="); Serial.println(sensorOffset, 4);
					Serial.print("  filter=");
					if (filterMode == FILTER_NONE) Serial.print("none"); else if (filterMode == FILTER_MEDIAN3) Serial.print("median3"); else Serial.print("ema");
					Serial.print("  emaAlpha="); Serial.println(emaAlpha, 3);
					// sequence settings
					Serial.print("sequence: active="); Serial.print(sequenceActive ? "1" : "0");
					Serial.print(" stage="); Serial.print(sequenceStage);
					Serial.print(" seqStableMs="); Serial.print(sequenceStableMs);
					Serial.print(" seqStartStableMs="); Serial.print(sequenceStartStableMs);
					Serial.print(" seqTimeoutMs="); Serial.println(sequenceTimeoutMs);
					// relay states
					Serial.print("relays:");
					for (int r = 1; r <= 9; ++r) {
						Serial.print(" R"); Serial.print(r); Serial.print("="); Serial.print(relayStates[r] ? "ON" : "OFF");
					}
					Serial.println();
				} else if (strcasecmp(cmd, "set") == 0) {
					char *sub = strtok(NULL, " ");
					char *val = strtok(NULL, " ");
					if (!sub || !val) { Serial.println("Usage: set vref|maxpsi|gain|offset|filter|emaalpha|pinmode <value>"); }
					else if (strcasecmp(sub, "vref") == 0) { adcVref = atof(val); saveCalibration(); Serial.print("adcVref set to "); Serial.println(adcVref, 6); }
					else if (strcasecmp(sub, "maxpsi") == 0) { maxPressurePsi = atof(val); saveCalibration(); Serial.print("maxPressurePsi set to "); Serial.println(maxPressurePsi, 2); }
					else if (strcasecmp(sub, "gain") == 0) { sensorGain = atof(val); saveCalibration(); Serial.print("sensorGain set to "); Serial.println(sensorGain, 6); }
					else if (strcasecmp(sub, "offset") == 0) { sensorOffset = atof(val); saveCalibration(); Serial.print("sensorOffset set to "); Serial.println(sensorOffset, 6); }
					else if (strcasecmp(sub, "filter") == 0) { if (strcasecmp(val, "none") == 0) filterMode = FILTER_NONE; else if (strcasecmp(val, "median3") == 0) filterMode = FILTER_MEDIAN3; else if (strcasecmp(val, "ema") == 0) filterMode = FILTER_EMA; saveCalibration(); Serial.print("filter set to "); if (filterMode==FILTER_NONE) Serial.println("none"); else if (filterMode==FILTER_MEDIAN3) Serial.println("median3"); else Serial.println("ema"); }
					else if (strcasecmp(sub, "emaalpha") == 0) { float v = atof(val); if (v>0 && v<=1.0) { emaAlpha = v; saveCalibration(); Serial.print("emaAlpha set to "); Serial.println(emaAlpha, 3);} else Serial.println("emaalpha must be >0 and <=1"); }
					else if (strcasecmp(sub, "pinmode") == 0) {
						// expect: set pinmode <pin> <nc|no>
						char *pinTok = val; // val is the first token after 'pinmode'
						char *modeTok = strtok(NULL, " ");
						if (!pinTok || !modeTok) { Serial.println("Usage: set pinmode <pin> <nc|no>"); }
						else {
							int pin = atoi(pinTok);
							int idx = -1;
							for (size_t ii = 0; ii < watchCount; ++ii) if (watchPins[ii] == (uint8_t)pin) { idx = ii; break; }
							if (idx < 0) { Serial.print("pin "); Serial.print(pin); Serial.println(" not in watch list"); }
							else {
								if (strcasecmp(modeTok, "nc") == 0) pinIsNC[idx] = true;
								else if (strcasecmp(modeTok, "no") == 0) pinIsNC[idx] = false;
								else { Serial.print("unknown mode "); Serial.println(modeTok); }
								// persist pin mode change
								saveCalibration();
								// update state records according to new mode
								if (pinIsNC[idx]) {
									pinState[idx] = digitalRead(watchPins[idx]) == HIGH;
									lastReading[idx] = pinState[idx];
								} else {
									pinState[idx] = digitalRead(watchPins[idx]) == LOW;
									lastReading[idx] = pinState[idx];
								}
								Serial.print("pin "); Serial.print(pin); Serial.print(" mode set to "); Serial.println(pinIsNC[idx] ? "NC" : "NO");
							}
						}
					}
					else if (strcasecmp(sub, "seqstable") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceStableMs = v; saveCalibration(); Serial.print("sequenceStableMs set to "); Serial.println(sequenceStableMs); }
					else if (strcasecmp(sub, "seqstartstable") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceStartStableMs = v; saveCalibration(); Serial.print("sequenceStartStableMs set to "); Serial.println(sequenceStartStableMs); }
					else if (strcasecmp(sub, "seqtimeout") == 0) { unsigned long v = strtoul(val, NULL, 10); sequenceTimeoutMs = v; saveCalibration(); Serial.print("sequenceTimeoutMs set to "); Serial.println(sequenceTimeoutMs); }
					else { Serial.println("Unknown set target. Use vref|maxpsi|gain|offset|filter|emaalpha|pinmode"); }
					// publish ack over MQTT if connected
					if (mqttClient.connected()) {
						char resp[128];
						snprintf(resp, sizeof(resp), "cli: set %s %s", sub, val);
						if (mqttClient.beginMessage("r4/control/resp")) { mqttClient.print(resp); mqttClient.endMessage(); }
					}
				} else if (strcasecmp(cmd, "relay") == 0) {
					char *relayTok = strtok(NULL, " ");
					char *stateTok = strtok(NULL, " ");
					if (!relayTok || !stateTok) { Serial.println("Usage: relay R<n> ON|OFF"); }
					else { handleRelayCommandTokens(relayTok, stateTok, false); }
				} else {
					Serial.println("Unknown command. Type 'help' for commands.");
				}
				// Additional Serial-only command: list pins and modes
				if (strcasecmp(cmd, "pins") == 0) {
					Serial.println("Watched pins:");
					for (size_t ii = 0; ii < watchCount; ++ii) {
						Serial.print("pin "); Serial.print(watchPins[ii]);
						Serial.print(" mode="); Serial.print(pinIsNC[ii] ? "NC" : "NO");
						Serial.print(" state="); Serial.println(pinState[ii] ? "ON" : "OFF");
					}
				}
				break;
			} else {
				if (linePos + 1 < sizeof(lineBuf)) lineBuf[linePos++] = c;
			}
		}
	}

	// Periodically publish a message
	if (millis() - lastPublish >= publishInterval) {
		lastPublish = millis();

		char payload[64];
		snprintf(payload, sizeof(payload), "Hello from UNO R4 at %lu", lastPublish);

			// ArduinoMqttClient uses beginMessage/print/endMessage to publish
			if (mqttClient.beginMessage(publishTopic)) {
				mqttClient.print(payload);
				mqttClient.endMessage();
				//Serial.print("MQTT published to "); Serial.print(publishTopic); Serial.print(" payload="); Serial.println(payload);
			} else {
				Serial.println("Publish failed (beginMessage returned false)");
			}

			if (mqttClient.beginMessage("r4/pressure")) {
				// Format PSI to two decimals. Use snprintf with float support in this toolchain.
				snprintf(payload, sizeof(payload), "pressure %.2f", pressure);
				mqttClient.print(payload);
				mqttClient.endMessage();
				//Serial.print("MQTT published to "); Serial.print("r4/pressure"); Serial.print(" payload="); Serial.println(payload);
			} else {
				Serial.println("Publish failed (beginMessage returned false)");
			}
	}

	// Check watched input pins (debounced)
	// (debug: prints when raw reading differs and when debounced change is applied)
	for (size_t i = 0; i < watchCount; ++i) {
		uint8_t pin = watchPins[i];
		bool reading;
		if (pin == 6 || pin == 7) {
			// NC limit switches: active when HIGH
			reading = digitalRead(pin) == LOW;
		} else {
			// regular active-low switches
			reading = digitalRead(pin) == LOW;
		}

		if (reading != lastReading[i]) {
			// raw reading changed
			//Serial.print("[RAW] pin "); Serial.print(pin); Serial.print(" reading="); Serial.println(reading);
			// reset debounce timer
			lastDebounceTime[i] = millis();
			lastReading[i] = reading;
		}

		if ((millis() - lastDebounceTime[i]) > debounceDelay) {
			// if the raw reading has been stable longer than debounce delay
			if (lastReading[i] != pinState[i]) {
				// debounced change
				//Serial.print("[DEBOUNCED] pin "); Serial.print(pin); Serial.print(" -> "); Serial.println(lastReading[i]);
				pinState[i] = lastReading[i];

				// notify control framework of input changes
				handleInputChange(pin, pinState[i]);

				char topicBuf[48];
				snprintf(topicBuf, sizeof(topicBuf), "r4/inputs/%d", pin);

				//Serial.print("Pin "); Serial.print(i); Serial.print(" -> "); Serial.println(lastReading[i]);
				//Serial.print("MQTT connected? "); Serial.println(mqttClient.connected() ? "yes" : "no");

				if (!mqttClient.connected()) {
				Serial.println("Attempting MQTT reconnect before publish...");
				if (!connectMQTT()) {
					Serial.println("MQTT reconnect failed — skipping MQTT publish for pin state");
					return;
				}
				}
				
				if (mqttClient.beginMessage(topicBuf)) {
					char payload[64];
					if (lastReading[i]) {
						snprintf(payload, sizeof(payload), "ON %d", 1);
					} else {
						snprintf(payload, sizeof(payload), "OFF %d", 0);
					}
					mqttClient.print(payload);
					mqttClient.endMessage();
					Serial.print("MQTT published to "); Serial.print(topicBuf); Serial.print(" payload="); Serial.println(lastReading[i]);
				} else {
					Serial.print("Failed to begin MQTT message for pin state (error=");
					Serial.print(mqttClient.connectError());
					Serial.println(")");
				}
      		}
		}
    mqttClient.poll();
	}
}

