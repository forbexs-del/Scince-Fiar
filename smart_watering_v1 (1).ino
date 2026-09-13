/*
  Smart Automated Plant Watering System
  ESP32 FireBeetle + DFRobot SEN0308 capacitive soil moisture sensor
  + DFRobot DFR0523 peristaltic pump (switched via relay)
  + Blynk app for phone monitoring/control

  ---------------------------------------------------------------
  PULSE-AND-WAIT WATERING LOGIC (not a single big dose)
  ---------------------------------------------------------------
  The moisture sensor can't "see" water the instant it's pumped -
  it takes time to soak into the soil. So instead of dumping one
  large dose when the soil reads dry, this code:

    1. Pumps ONE small pulse of water (PULSE_VOLUME_ML)
    2. Waits (PULSE_WAIT_MS) for that water to soak in
    3. Re-checks the moisture sensor
    4. If still dry -> pulses again. If not -> done for this cycle.
    5. A safety cap (MAX_PULSES_PER_CYCLE) stops it from pulsing
       forever if a sensor reading ever gets stuck or glitches.

  Everything marked "CALIBRATE ME" is a placeholder until you run
  the Day 0 calibration steps (dry/wet sensor readings in real soil,
  and a timed pump run measured with a graduated cylinder).
*/

#define BLYNK_TEMPLATE_ID   "TMPLxxxxxx"       // TODO: from your Blynk console
#define BLYNK_TEMPLATE_NAME "Smart Watering"    // TODO
#define BLYNK_AUTH_TOKEN    "xxxxxxxxxxxxxxxx"  // TODO

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ---------------- WiFi ----------------
char ssid[] = "YOUR_WIFI_NAME";       // TODO
char pass[] = "YOUR_WIFI_PASSWORD";   // TODO

// ---------------- Pin assignments ----------------
const int MOISTURE_PIN = 34;   // ADC1-capable pin -> SEN0308 analog output
const int RELAY_PIN    = 26;   // Digital pin -> relay IN, which switches 12V to the pump

// Most cheap relay modules are ACTIVE-LOW (LOW turns the relay ON).
// Verify this against your actual relay/pump wiring before trusting it.
const int RELAY_ON  = LOW;
const int RELAY_OFF = HIGH;

// ---------------- CALIBRATE ME: sensor ----------------
// Fill these in on Day 0 after reading the sensor in bone-dry and
// fully-soaked soil. Capacitive sensors commonly read HIGHER when dry
// and LOWER when wet, but confirm this yourself - don't assume it.
int MOISTURE_DRY_RAW       = 3000;  // raw reading in bone-dry soil
int MOISTURE_WET_RAW       = 1200;  // raw reading in fully soaked soil
int MOISTURE_THRESHOLD_RAW = 2400;  // set between the two above

// ---------------- CALIBRATE ME: pump ----------------
float PUMP_ML_PER_SEC = 1.0;  // from your timed-run + graduated-cylinder test (average of 3 runs)

// ---------------- Watering logic settings (yours to tune) ----------------
float PULSE_VOLUME_ML          = 40.0;              // mL per pulse
unsigned long PULSE_WAIT_MS    = 10UL * 60UL * 1000UL; // 10 minutes between pulses
int MAX_PULSES_PER_CYCLE       = 6;                 // safety cap (6 x 40mL = 240mL ceiling)
unsigned long IDLE_CHECK_MS    = 15UL * 60UL * 1000UL; // how often to check moisture when not watering

// ---------------- Blynk virtual pins ----------------
#define V_MOISTURE      V0   // current raw moisture reading
#define V_ML_TODAY      V1   // total mL dispensed today
#define V_STATUS        V2   // status/warning text

// ---------------- State machine ----------------
enum WateringState { STATE_IDLE, STATE_PULSING, STATE_SOAKING };
WateringState waterState = STATE_IDLE;

unsigned long lastIdleCheck = 0;
unsigned long soakStartTime = 0;
int pulseCount = 0;
float totalMlToday = 0;

bool isSoilDry(int rawReading) {
  // TODO: flip this comparison if your Day 0 calibration shows the
  // opposite direction (wet = higher raw value on your specific sensor).
  return rawReading > MOISTURE_THRESHOLD_RAW;
}

void runPumpForMs(unsigned long ms) {
  // The pump only runs for a few seconds at a time, so a blocking
  // delay() here is fine - it's the 10-MINUTE wait between pulses
  // that must never use delay(), or Blynk/WiFi would drop out.
  digitalWrite(RELAY_PIN, RELAY_ON);
  delay(ms);
  digitalWrite(RELAY_PIN, RELAY_OFF);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  Blynk.run();
  unsigned long now = millis();

  switch (waterState) {

    case STATE_IDLE:
      if (now - lastIdleCheck >= IDLE_CHECK_MS) {
        lastIdleCheck = now;
        int raw = analogRead(MOISTURE_PIN);
        Blynk.virtualWrite(V_MOISTURE, raw);

        if (isSoilDry(raw)) {
          pulseCount = 0;
          waterState = STATE_PULSING;
        }
      }
      break;

    case STATE_PULSING:
      if (pulseCount >= MAX_PULSES_PER_CYCLE) {
        Blynk.virtualWrite(V_STATUS, "Max pulses reached - check sensor/soil");
        waterState = STATE_IDLE;
        break;
      }

      {
        unsigned long pumpMs = (unsigned long)((PULSE_VOLUME_ML / PUMP_ML_PER_SEC) * 1000.0);
        runPumpForMs(pumpMs);
        pulseCount++;
        totalMlToday += PULSE_VOLUME_ML;
        Blynk.virtualWrite(V_ML_TODAY, totalMlToday);
      }

      soakStartTime = now;
      waterState = STATE_SOAKING;
      break;

    case STATE_SOAKING:
      if (now - soakStartTime >= PULSE_WAIT_MS) {
        int raw = analogRead(MOISTURE_PIN);
        Blynk.virtualWrite(V_MOISTURE, raw);

        if (isSoilDry(raw)) {
          waterState = STATE_PULSING;  // still dry - pulse again
        } else {
          waterState = STATE_IDLE;     // satisfied for this cycle
        }
      }
      break;
  }
}

/*
  NOT YET HANDLED (fine for now, add later if you want):
  - totalMlToday never resets, so it accumulates across days rather
    than resetting at midnight. For your daily log you can just
    subtract yesterday's cumulative total, or add a simple day-rollover
    check once you have an easy way to get real time (e.g. an NTP call).
  - No manual override button is wired up yet, though V_STATUS and the
    virtual pin structure make it easy to add one in Blynk later.
*/
