#pragma once

// ---------------------------------------------------------------------------
// Pin assignments (ESP32 FireBeetle)
// ---------------------------------------------------------------------------
// PIN_SENSOR_EXPERIMENTAL is GPIO36 (silkscreen label "A0") - confirmed
// against the physical FireBeetle board, which breaks out A0/IO36, A1/IO39,
// A2/IO34, A3/IO35 as its analog pins (no IO32/IO33 header on this board).
// All of these are ADC1 pins, safe to read with WiFi active.
#define PIN_SENSOR_EXPERIMENTAL   36   // capacitive sensor signal wire ("A" wire) - drives the pump
#define PIN_PUMP_SIGNAL           25   // servo-style PPM signal wire from the pump's 3-pin Gravity connector - silkscreen label "IO25/D2"

// The DFR0523 "Digital Peristaltic Pump" has its own onboard driver - it's
// controlled by a hobby-servo-style signal, not a simple on/off switch, so no
// relay or MOSFET is needed. 1500us is the pump's documented "stop" position
// (same convention as a standard servo's center). PUMP_RUN_US picks a
// direction/speed to dispense water - if the pump runs backward (or doesn't
// run) at this value, try the mirrored value on the other side of 1500 (e.g.
// 1900 <-> 1100) rather than assuming something is broken.
#define PUMP_STOP_US   1500
#define PUMP_RUN_US    2400   // full speed - at 1900 the motor hummed but couldn't turn the rollers (bench test)

// ---------------------------------------------------------------------------
// Calibration placeholders - UNKNOWN until Monday's Day-0 calibration.
// See ../../CALIBRATION.md for the exact procedure. Do not trust these
// numbers for a real watering decision until they're replaced.
// ---------------------------------------------------------------------------
#define MOISTURE_RAW_DRY        3000   // PLACEHOLDER: analogRead() in bone-dry soil
#define MOISTURE_RAW_WET        1200   // PLACEHOLDER: analogRead() in fully saturated soil
#define MOISTURE_THRESHOLD_PCT    35   // start watering when moisture % drops below this (0% = dry, 100% = saturated)
#define MOISTURE_TARGET_PCT       55   // stop pulsing once moisture % reaches this - must be > MOISTURE_THRESHOLD_PCT, gives headroom so it doesn't immediately re-trigger

#define PUMP_ML_PER_SEC          0.5f  // measured: 8 mL in 16 s at PUMP_RUN_US 2400, jug on the floor (re-measure if lift height changes)

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
