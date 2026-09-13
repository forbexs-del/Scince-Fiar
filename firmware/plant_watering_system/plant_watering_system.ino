/*
  Smart Automated Plant Watering System - Firmware
  ESP32 FireBeetle + DFRobot capacitive soil moisture sensor (SEN0308) +
  DFRobot peristaltic pump (DFR0523) via relay, monitored/controlled through Blynk.

  Experimental container: sensor reading drives the pump automatically.
  Control container: sensor (if wired) is logging-only and must never reach
  the pump-trigger logic - watering for that container stays manual, by hand.

  Two calibration constants in config.h are placeholders until Monday's
  Day-0 calibration (see ../../CALIBRATION.md). Everything else here can be
  written, compiled, and logic-tested with no hardware attached (see
  testForcedReading() below).
*/

#include "secrets.h"      // WiFi + Blynk credentials - copy from secrets.h.example, do NOT commit
#include "config.h"       // pins, calibration placeholders, dosing/safety constants

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <BlynkTimer.h>

BlynkTimer timer;

// ---- State ----
bool autoWateringEnabled = true;
bool pumpIsOn = false;
unsigned long pumpOffAtMs = 0;
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

// Starts one full watering event: runs the pump for the time needed to
// deliver ML_PER_WATERING_EVENT at the calibrated PUMP_ML_PER_SEC rate.
// Non-blocking (no delay()) so Blynk.run() keeps servicing while it runs;
// checkPumpOffTimer() in loop() turns the pump off when the duration elapses.
// Per brief 4.2: once started, the full dose runs even if the moisture
// threshold gets re-crossed mid-dose - no partial/interrupted triggers.
void startWateringEvent() {
  if (pumpIsOn) return; // already mid-dose, ignore

  unsigned long durationMs = (unsigned long)((ML_PER_WATERING_EVENT / PUMP_ML_PER_SEC) * 1000.0f);
  setPump(true);
  pumpOffAtMs = millis() + durationMs;

  Serial.print("Watering event started, planned duration ms: ");
  Serial.println(durationMs);
}

void checkPumpOffTimer() {
  if (pumpIsOn && millis() >= pumpOffAtMs) {
    setPump(false);
    lastWateringEndMs = millis();
    totalWaterDeliveredMl += ML_PER_WATERING_EVENT;
    Blynk.virtualWrite(V_TOTAL_WATER_ML, totalWaterDeliveredMl);
    Serial.print("Watering event finished. Total water delivered (mL): ");
    Serial.println(totalWaterDeliveredMl);
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
  if (autoWateringEnabled && !pumpIsOn && minTimeBetweenWateringsElapsed()) {
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
  Serial.print(",pump=");
  Serial.println(pumpIsOn ? "ON" : "OFF");
}

// ---------------------------------------------------------------------------
// Blynk callbacks
// ---------------------------------------------------------------------------

// Manual override button (V4): forces one watering event immediately,
// bypassing the threshold check and the min-time-between-waterings guard.
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

  timer.setInterval(SENSOR_POLL_INTERVAL_MS, pollSensorsAndMaybeWater);
  timer.setInterval(LOG_INTERVAL_MS, logData);

  Serial.println("Setup complete.");
}

void loop() {
  Blynk.run();
  timer.run();
  checkPumpOffTimer(); // must run every loop iteration, not just on a timer interval
}
