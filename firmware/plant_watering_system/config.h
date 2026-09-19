#pragma once

// ---------------------------------------------------------------------------
// Pin assignments (ESP32 FireBeetle)
// ---------------------------------------------------------------------------
// Sensor pins are ADC1 (GPIO32-39) on purpose: ADC2 pins share hardware with
// WiFi and give garbage readings once WiFi is connected. Cross-check these
// against the FireBeetle ESP32 silkscreen when wiring the real board (this
// was an open item in the brief - not physically confirmed yet).
#define PIN_SENSOR_EXPERIMENTAL   32   // capacitive sensor, experimental container - drives the pump
#define PIN_SENSOR_CONTROL        33   // capacitive sensor, control container - LOGGING ONLY, never wired into pump logic
#define PIN_PUMP_RELAY            25   // digital out -> relay module IN pin -> switches 12V to the DFR0523 pump

// Most cheap single-channel relay boards are "active LOW" (a LOW signal
// energizes the relay). Flip this to false if the pump runs backwards, or if
// a MOSFET driver board is used instead of a relay (those are typically
// active HIGH). This was an open decision in the brief - confirm Monday.
#define RELAY_ACTIVE_LOW true

// ---------------------------------------------------------------------------
// Calibration placeholders - UNKNOWN until Monday's Day-0 calibration.
// See ../../CALIBRATION.md for the exact procedure. Do not trust these
// numbers for a real watering decision until they're replaced.
// ---------------------------------------------------------------------------
#define MOISTURE_RAW_DRY        3000   // PLACEHOLDER: analogRead() in bone-dry soil
#define MOISTURE_RAW_WET        1200   // PLACEHOLDER: analogRead() in fully saturated soil
#define MOISTURE_THRESHOLD_PCT    35   // start watering when moisture % drops below this (0% = dry, 100% = saturated)
#define MOISTURE_TARGET_PCT       55   // stop pulsing once moisture % reaches this - must be > MOISTURE_THRESHOLD_PCT, gives headroom so it doesn't immediately re-trigger

#define PUMP_ML_PER_SEC          2.5f  // PLACEHOLDER: from graduated-cylinder calibration, averaged over 3 runs

// ---------------------------------------------------------------------------
// Dosing - incremental "pulse and check" watering instead of one big dump.
// Each pulse delivers PULSE_ML, then the pump sits idle for SOAK_SETTLE_MS so
// the water has time to soak in before the next moisture check. This repeats
// until MOISTURE_TARGET_PCT is reached or MAX_PULSES_PER_EVENT is hit (a
// safety ceiling so a stuck/miscalibrated sensor can't keep dosing forever
// and flood the container).
// ---------------------------------------------------------------------------
#define PLANTS_PER_CONTAINER        8
#define ML_PER_PLANT               30
#define PULSE_ML                   40    // mL delivered per pulse
#define MAX_PULSES_PER_EVENT        6    // ceiling: 6 x 40 mL = 240 mL max per event (same total budget as the old single-dose design)
#define SOAK_SETTLE_MS   (3UL * 60UL * 1000UL)   // wait 3 min after a pulse before re-checking moisture - tune Monday, capacitive sensors lag true soil moisture

// ---------------------------------------------------------------------------
// Safety guards
// ---------------------------------------------------------------------------
#define MIN_MS_BETWEEN_WATERINGS   (2UL * 60UL * 60UL * 1000UL)   // 2 hours - tune once you see real cycle behavior
#define MOISTURE_SAMPLES_TO_AVERAGE  10                            // debounce: average N reads before deciding

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define SENSOR_POLL_INTERVAL_MS   (60UL * 1000UL)          // check moisture every 1 min
#define LOG_INTERVAL_MS           (15UL * 60UL * 1000UL)   // log a data line every 15 min
