# Smart Automated Plant Watering System

Science fair engineering project comparing soil-moisture-triggered automated
irrigation (experimental) against fixed-schedule hand watering (control) on
Cherriette F1 radishes over a ~30 day trial, measuring plant growth/health
and water efficiency (root fresh mass in g ÷ total water used in L).

## Repo layout

- `firmware/plant_watering_system/` - the ESP32 sketch
  - `plant_watering_system.ino` - main firmware
  - `config.h` - pin assignments, calibration constants, dosing/safety constants
  - `secrets.h.example` - template for WiFi/Blynk credentials (copy to `secrets.h`, which is gitignored)
- `CALIBRATION.md` - Day-0 steps to turn the placeholder constants in `config.h` into real numbers

## Hardware

- ESP32 FireBeetle
- DFRobot peristaltic pump, DFR0523 (12V), switched via a relay module
- Primary soil moisture sensor: DFRobot IP65 capacitive SEN0308 (backup: SEN0193)
- 2x Sterilite 20-Quart Clear Storage Bins as containers
- Miracle-Gro Organic Outdoor Potting Mix, ~15cm depth
- Single perforated soaker line per container
- Blynk app for phone monitoring/control

## Firmware setup (Arduino IDE)

1. Install the ESP32 board package (File > Preferences > Additional Board
   Manager URLs, add DFRobot's or Espressif's ESP32 URL, then install via
   Boards Manager). Select an ESP32 Dev Module-compatible board for the
   FireBeetle ESP32.
2. Install libraries via Library Manager: **Blynk** (the current Blynk IoT
   library, which provides `BlynkSimpleEsp32.h` and `BlynkTimer.h`).
3. In `firmware/plant_watering_system/`, copy `secrets.h.example` to
   `secrets.h` and fill in your WiFi credentials and Blynk template
   ID/name/auth token (from Blynk.Console).
4. Open `plant_watering_system.ino` and flash.

## Blynk app setup

Create a template with these datastreams (virtual pins), matching what the
firmware writes/reads:

| Pin | Type   | Purpose                                      |
|-----|--------|-----------------------------------------------|
| V0  | Double | Experimental container moisture (%)           |
| V1  | Double | Control container moisture (%) - logging only |
| V2  | Integer| Pump state (0/1)                              |
| V3  | Double | Total water delivered (mL, running total)     |
| V4  | Integer| Manual override button - forces one watering  |
| V5  | Integer| Auto-watering enable/disable switch           |

Add matching widgets on the dashboard (gauges/labels for V0-V3, a button for
V4, a switch for V5).

## What's already done vs. what needs Monday's hardware

**Done now (logic-complete, testable without hardware):**
- WiFi + Blynk connection
- Sensor read + averaging/debouncing
- Threshold-crossing decision logic
- Incremental pulsed dosing: 40 mL pulses with a soak/re-check between each,
  stopping once target moisture is reached or a 6-pulse (240 mL) ceiling
  hits - no single big dump that could overflow the soil
- Minimum time-between-waterings safeguard
- Manual override + auto-watering enable/disable via Blynk
- Serial CSV logging stub
- A no-hardware logic self-test (`testForcedReading()` in the `.ino`)

**Needs Monday's hardware (see `CALIBRATION.md`):**
- Real `MOISTURE_RAW_DRY` / `MOISTURE_RAW_WET` / `PUMP_ML_PER_SEC` values
- Confirming `RELAY_ACTIVE_LOW` against the actual relay module
- Confirming pin assignments against the physical FireBeetle board and breadboard layout
- Real end-to-end test (sensor -> ESP32 -> relay -> pump -> water)

## Open decisions (carried over from the project brief, not yet finalized)

- **Data logging destination**: currently Serial-only (copy by hand into the
  measurement spreadsheet). Blynk cloud history or an SD card module are the
  alternatives if manual copying becomes a burden over 30 days.
- **Control-container sensor**: firmware assumes it IS wired in (pin 33,
  logging only). If you decide not to add a second sensor, just stop reading
  `PIN_SENSOR_CONTROL` in `pollSensorsAndMaybeWater()` - nothing else changes.
- **Soaker line inlet connection**: documentation-only, doesn't affect firmware.
- **Pulse size / target moisture / soak time**: `PULSE_ML` (40), `MOISTURE_TARGET_PCT`
  (55%), and `SOAK_SETTLE_MS` (3 min) in `config.h` are starting points, not
  measured values - tune them once you see how fast the soil actually
  absorbs a pulse and how the sensor reading responds.
