/*
  Smart Automated Plant Watering System - Firmware
  ESP32 FireBeetle + DFRobot capacitive soil moisture sensor (SEN0308) +
  DFRobot peristaltic pump (DFR0523) via relay, monitored/controlled through Blynk.

  Experimental container: sensor reading drives the pump automatically.
  Control container: sensor (if wired) is logging-only and must never reach
  the pump-trigger logic - watering for that container stays manual, by hand.

  Watering is delivered as small pulses (PULSE_ML each), not one big dump:
  pulse, pause to let it soak in, re-check moisture, repeat until the target
  moisture level is reached or a pulse-count ceiling is hit. This avoids
  overflowing/pooling the soil from dropping the whole dose at once.

  Two calibration constants in config.h are placeholders until Monday's
  Day-0 calibration (see ../../CALIBRATION.md). Everything else here can be
  written, compiled, and logic-tested with no hardware attached (see
  testForcedReading() below).
*/

#include "secrets.h"      // WiFi + Blynk credentials - copy from secrets.h.example, do NOT commit
#include "config.h"       // pins, calibration placeholders, dosing/safety constants

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---- State ----
bool autoWateringEnabled = true;
bool pumpIsOn = false;

// Plain millis()-based scheduling instead of BlynkTimer (removes a fragile
// library dependency - see pollSensorsAndMaybeWater()/logData() calls in loop()).
unsigned long lastSensorPollMs = 0;
unsigned long lastLogMs = 0;

enum WateringState { WATERING_IDLE, WATERING_PULSING, WATERING_SOAKING };
WateringState wateringState = WATERING_IDLE;
int pulsesThisEvent = 0;
unsigned long wateringStateDeadlineMs = 0; // when the current pulse/soak step ends

// Wraps around on purpose so the very first watering isn't blocked by the
// min-time-between-waterings guard (millis()-lastWateringEndMs is huge at boot).
unsigned long lastWateringEndMs = (unsigned long)0 - MIN_MS_BETWEEN_WATERINGS;
float totalWaterDeliveredMl = 0;

// ---- Virtual pins (Blynk datastreams - create these in the Blynk template) ----
#define V_MOISTURE_EXPERIMENTAL   V0   // %, read-only widget
#define V_MOISTURE_CONTROL        V1   // %, read-only widget, logging only
#define V_PUMP_STATE              V2   // 0/1, read-only widget
#define V_TOTAL_WATER_ML          V3   // running total, read-only widget
#define V_MANUAL_OVERRIDE         V4   // button, forces one watering event
#define V_AUTO_WATERING_ENABLED   V5   // switch, enables/disables auto watering

// ---------------------------------------------------------------------------
// Moisture reading
// ---------------------------------------------------------------------------

// Averages several reads to debounce a single noisy sample (per brief 4.1).
int averagedRawRead(int pin) {
  long sum = 0;
  for (int i = 0; i < MOISTURE_SAMPLES_TO_AVERAGE; i++) {
    sum += analogRead(pin);
    delay(10);
  }
  return sum / MOISTURE_SAMPLES_TO_AVERAGE;
}

// Converts a raw ADC reading to 0-100% using the two Day-0 calibration
// points. Clamped so a reading outside the calibrated range doesn't produce
// a nonsense percentage.
float rawToPercent(int raw) {
  float pct = (float)(MOISTURE_RAW_DRY - raw) * 100.0f / (float)(MOISTURE_RAW_DRY - MOISTURE_RAW_WET);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

// ---------------------------------------------------------------------------
// Pump control
// ---------------------------------------------------------------------------

void setPump(bool on) {
  pumpIsOn = on;
  bool activeLevel = RELAY_ACTIVE_LOW ? LOW : HIGH;
  bool idleLevel = RELAY_ACTIVE_LOW ? HIGH : LOW;
  digitalWrite(PIN_PUMP_RELAY, on ? activeLevel : idleLevel);
  Blynk.virtualWrite(V_PUMP_STATE, on ? 1 : 0);
}

// Starts one pulse: runs the pump for the time needed to deliver PULSE_ML at
// the calibrated PUMP_ML_PER_SEC rate. Non-blocking (no delay()) so
// Blynk.run() keeps servicing while it runs; updateWateringStateMachine() in
// loop() advances to the soak step when the pulse duration elapses.
void startPulse() {
  unsigned long durationMs = (unsigned long)((PULSE_ML / PUMP_ML_PER_SEC) * 1000.0f);
  setPump(true);
  wateringStateDeadlineMs = millis() + durationMs;
  wateringState = WATERING_PULSING;

  Serial.print("Pulse ");
  Serial.print(pulsesThisEvent + 1);
  Serial.print("/");
  Serial.print(MAX_PULSES_PER_EVENT);
  Serial.print(" started, duration ms: ");
  Serial.println(durationMs);
}

// Starts a full watering event: repeated pulse/soak/check cycles until
// MOISTURE_TARGET_PCT is reached or MAX_PULSES_PER_EVENT caps it.
void startWateringEvent() {
  if (wateringState != WATERING_IDLE) return; // event already in progress, ignore

  pulsesThisEvent = 0;
  startPulse();
}

// Drives the pulse -> soak -> re-check -> (pulse again or stop) cycle. Must
// be called every loop() iteration, not just on a timer interval, since pulse
// and soak durations aren't aligned to SENSOR_POLL_INTERVAL_MS.
void updateWateringStateMachine() {
  if (wateringState == WATERING_PULSING && millis() >= wateringStateDeadlineMs) {
    setPump(false);
    totalWaterDeliveredMl += PULSE_ML;
    pulsesThisEvent++;
    Blynk.virtualWrite(V_TOTAL_WATER_ML, totalWaterDeliveredMl);
    Serial.print("Pulse finished. Total water delivered (mL): ");
    Serial.println(totalWaterDeliveredMl);

    wateringState = WATERING_SOAKING;
    wateringStateDeadlineMs = millis() + SOAK_SETTLE_MS;

  } else if (wateringState == WATERING_SOAKING && millis() >= wateringStateDeadlineMs) {
    int raw = averagedRawRead(PIN_SENSOR_EXPERIMENTAL);
    float pct = rawToPercent(raw);
    Blynk.virtualWrite(V_MOISTURE_EXPERIMENTAL, pct);

    Serial.print("Post-pulse moisture check: ");
    Serial.print(pct);
    Serial.println("%");

    if (pct >= MOISTURE_TARGET_PCT) {
      Serial.println("Target moisture reached, watering event done.");
      wateringState = WATERING_IDLE;
      lastWateringEndMs = millis();
    } else if (pulsesThisEvent >= MAX_PULSES_PER_EVENT) {
      Serial.println("Max pulses reached for this event, stopping short of target (will retry after min-time guard).");
      wateringState = WATERING_IDLE;
      lastWateringEndMs = millis();
    } else {
      startPulse();
    }
  }
}

bool minTimeBetweenWateringsElapsed() {
  return (millis() - lastWateringEndMs) >= MIN_MS_BETWEEN_WATERINGS;
}

// ---------------------------------------------------------------------------
// Sensor poll + auto-watering decision
// ---------------------------------------------------------------------------

void pollSensorsAndMaybeWater() {
  int rawExperimental = averagedRawRead(PIN_SENSOR_EXPERIMENTAL);
  float pctExperimental = rawToPercent(rawExperimental);
  Blynk.virtualWrite(V_MOISTURE_EXPERIMENTAL, pctExperimental);

  int rawControl = averagedRawRead(PIN_SENSOR_CONTROL);
  float pctControl = rawToPercent(rawControl);
  Blynk.virtualWrite(V_MOISTURE_CONTROL, pctControl);

  Serial.print("Moisture % - experimental: ");
  Serial.print(pctExperimental);
  Serial.print("  control (log only): ");
  Serial.println(pctControl);

  // Control container's sensor is logging only - it never appears in this
  // decision. Do not add it here.
  // Gate on wateringState (not pumpIsOn): the pump is off during the soak
  // step too, but the event is still in progress and must not be restarted.
  if (autoWateringEnabled && wateringState == WATERING_IDLE && minTimeBetweenWateringsElapsed()) {
    if (pctExperimental < MOISTURE_THRESHOLD_PCT) {
      startWateringEvent();
    }
  }
}

// ---------------------------------------------------------------------------
// Data logging
// ---------------------------------------------------------------------------
// Destination wasn't finalized in the brief (Blynk history vs SD card vs
// manual serial capture - see README.md "Data logging"). Serial CSV is the
// safe default that works with zero extra hardware; copy lines into the
// measurement spreadsheet by hand, or swap this function for SD/Blynk-export
// logic once that decision is made.
void logData() {
  Serial.print("LOG,");
  Serial.print(millis());
  Serial.print(",totalWaterMl=");
  Serial.print(totalWaterDeliveredMl);
  Serial.print(",state=");
  switch (wateringState) {
    case WATERING_IDLE:    Serial.print("IDLE");    break;
    case WATERING_PULSING: Serial.print("PULSING"); break;
    case WATERING_SOAKING: Serial.print("SOAKING"); break;
  }
  Serial.print(",pulsesThisEvent=");
  Serial.println(pulsesThisEvent);
}

// ---------------------------------------------------------------------------
// Blynk callbacks
// ---------------------------------------------------------------------------

// Manual override button (V4): forces one watering event immediately,
// bypassing the threshold check and the min-time-between-waterings guard.
// Still runs as pulses up to MOISTURE_TARGET_PCT/MAX_PULSES_PER_EVENT, same
// as an auto-triggered event - it just skips the "should we start?" check.
// Intended for calibration/troubleshooting, not normal trial operation.
BLYNK_WRITE(V_MANUAL_OVERRIDE) {
  int pressed = param.asInt();
  if (pressed) {
    Serial.println("Manual override pressed: forcing watering event");
    startWateringEvent();
  }
}

BLYNK_WRITE(V_AUTO_WATERING_ENABLED) {
  autoWateringEnabled = param.asInt();
  Serial.print("Auto watering enabled: ");
  Serial.println(autoWateringEnabled ? "true" : "false");
}

BLYNK_CONNECTED() {
  // Sync app widget state with firmware state on (re)connect.
  Blynk.virtualWrite(V_AUTO_WATERING_ENABLED, autoWateringEnabled ? 1 : 0);
  Blynk.virtualWrite(V_TOTAL_WATER_ML, totalWaterDeliveredMl);
}

// ---------------------------------------------------------------------------
// Logic self-test - no hardware required
// ---------------------------------------------------------------------------
// Call this from setup() (see the commented-out line below) to sanity-check
// the threshold/dosing math against a fake sensor reading, with no ESP32
// pins, sensor, relay, or pump connected. Prints to Serial only; never
// touches the real pump pin.
void testForcedReading(int fakeRawReading) {
  float pct = rawToPercent(fakeRawReading);
  Serial.print("[TEST] fake raw=");
  Serial.print(fakeRawReading);
  Serial.print(" -> pct=");
  Serial.print(pct);
  Serial.print(" -> would water? ");
  Serial.println(pct < MOISTURE_THRESHOLD_PCT ? "YES" : "no");
}

// ---------------------------------------------------------------------------
// Setup / loop
// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(PIN_PUMP_RELAY, OUTPUT);
  setPump(false);
  // ADC1 pins (32-39) need no pinMode call for analogRead() on ESP32.

  // Uncomment to exercise the threshold logic before hardware arrives:
  // testForcedReading(MOISTURE_RAW_DRY);                                  // -> should print "no"
  // testForcedReading(MOISTURE_RAW_WET);                                  // -> should print "YES"
  // testForcedReading((MOISTURE_RAW_DRY + MOISTURE_RAW_WET) / 2);         // -> depends on MOISTURE_THRESHOLD_PCT

  Blynk.begin(BLYNK_AUTH_TOKEN, WIFI_SSID, WIFI_PASS);

  Serial.println("Setup complete.");
}

void loop() {
  Blynk.run();

  unsigned long now = millis();
  if (now - lastSensorPollMs >= SENSOR_POLL_INTERVAL_MS) {
    lastSensorPollMs = now;
    pollSensorsAndMaybeWater();
  }
  if (now - lastLogMs >= LOG_INTERVAL_MS) {
    lastLogMs = now;
    logData();
  }

  updateWateringStateMachine(); // must run every loop iteration, not just on a timer interval
}
