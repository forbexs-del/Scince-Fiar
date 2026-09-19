# Day-0 Calibration (do this Monday, before planting)

Two constants in `firmware/plant_watering_system/config.h` are placeholders
and must be replaced with real measurements before the automated watering
logic can be trusted. Do both before soil goes in the containers.

## 1. Sensor calibration (`MOISTURE_RAW_DRY`, `MOISTURE_RAW_WET`)

1. Flash the firmware with `Serial.begin(115200)` running and open the
   Serial Monitor - the sketch already prints the averaged raw reading
   every `SENSOR_POLL_INTERVAL_MS` (default: every 1 min) via
   `pollSensorsAndMaybeWater()`. For faster iteration during calibration,
   temporarily change `SENSOR_POLL_INTERVAL_MS` to a few seconds.
2. **Dry reading:** with the SEN0308 probe completely dry (out of soil, wiped
   off, sitting in open air), record the printed raw value. Repeat 2-3 times.
3. **Wet reading:** submerge the probe in a cup of water up to its rated
   line (do not exceed it - it's IP65, not fully submersible past that mark).
   Record the raw value. Repeat 2-3 times.
4. Average each set. Put the dry average into `MOISTURE_RAW_DRY` and the wet
   average into `MOISTURE_RAW_WET` in `config.h`.
5. Do this for both the experimental sensor and the control (logging-only)
   sensor if the second sensor is being wired in - if their raw ranges
   differ noticeably, you'll want separate calibration constants per sensor
   (flag this if it happens; the current code assumes both sensors share one
   calibration curve).
6. Sanity check: with the placeholders replaced, uncomment the
   `testForcedReading(...)` calls in `setup()` and confirm dry -> "no water"
   and wet -> "YES water" print as expected before moving on.

## 2. Pump calibration (`PUMP_ML_PER_SEC`)

1. Wire the pump's signal wire to the ESP32 pin, and its power wires to your
   separate 5-6V supply (not the ESP32/USB) - see `README.md`. Point the
   pump's output tube into a graduated cylinder (not into soil yet).
2. Run the pump for a fixed, known duration - 10 seconds is a good starting
   point. Easiest way: temporarily call `setPump(true)` in `setup()`,
   `delay(10000)`, then `setPump(false)`, and re-flash. (Revert this
   afterward - it's a one-off calibration hack, not something to leave in.)
3. **If nothing comes out (or it sucks air/runs the wrong way):** the pump's
   direction is flipped. Change `PUMP_RUN_US` in `config.h` to the mirrored
   value on the other side of 1500 (e.g. 1900 -> 1100) and try again.
4. Measure the mL collected in the graduated cylinder.
5. Repeat 3 times total and average: `mL/sec = average_mL / 10`.
6. Put that value into `PUMP_ML_PER_SEC` in `config.h`.

## 3. After both are done

- Set `MOISTURE_THRESHOLD_PCT` (start watering) and `MOISTURE_TARGET_PCT`
  (stop watering) if the defaults (35% / 55%) don't match what you want to
  test - these are design choices, not something you measure. Keep
  `MOISTURE_TARGET_PCT` comfortably above `MOISTURE_THRESHOLD_PCT`.
- Confirm pin assignments in `config.h` against the actual FireBeetle ESP32
  board layout and your breadboard wiring - `PIN_SENSOR_EXPERIMENTAL` (32),
  `PIN_SENSOR_CONTROL` (33), and `PIN_PUMP_SIGNAL` (25) were chosen as safe
  ADC1/digital pins but weren't checked against physical hardware yet.
- Watch a real pulse cycle end to end: dry soil in the experimental
  container should trigger a 40 mL pulse, pause for `SOAK_SETTLE_MS`, re-check
  moisture, and either pulse again or stop. Watch the Serial log for a few
  full cycles and confirm the moisture % is actually climbing pulse over
  pulse - if it isn't, `SOAK_SETTLE_MS` is probably too short for the
  reading to reflect the water that just went in.
