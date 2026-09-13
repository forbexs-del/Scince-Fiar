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

1. Wire the pump to the relay and the relay to the ESP32 per the wiring in
   `README.md`, with the pump's output tube running into a graduated
   cylinder (not into soil yet).
2. Run the pump for a fixed, known duration - 10 seconds is a good starting
   point. Easiest way: temporarily call `setPump(true)` in `setup()`,
   `delay(10000)`, then `setPump(false)`, and re-flash. (Revert this
   afterward - it's a one-off calibration hack, not something to leave in.)
3. Measure the mL collected in the graduated cylinder.
4. Repeat 3 times total and average: `mL/sec = average_mL / 10`.
5. Put that value into `PUMP_ML_PER_SEC` in `config.h`.

## 3. After both are done

- Set `MOISTURE_THRESHOLD_PCT` if the default (35%) doesn't match what you
  want to test - this is a design choice, not something you measure.
- Double-check `RELAY_ACTIVE_LOW` in `config.h`: if the pump runs when it
  shouldn't (or vice versa) the first time you flip `setPump()`, this is
  the constant to flip.
- Confirm pin assignments in `config.h` against the actual FireBeetle ESP32
  board layout and your breadboard wiring - `PIN_SENSOR_EXPERIMENTAL` (32),
  `PIN_SENSOR_CONTROL` (33), and `PIN_PUMP_RELAY` (25) were chosen as safe
  ADC1/digital pins but weren't checked against physical hardware yet.
- Run a real end-to-end test: dry soil in the experimental container should
  trigger a full 240 mL watering event through the relay and pump.
