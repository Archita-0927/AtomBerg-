/*
==================================================================================
atomQUEST 2026 | Track 3: Mixer Grinder (Atomberg Problem Statement)
Team: Wayne Enterprises | Jaypee Institute of Information Technology, Noida
Project: Tubercle-Edge Blades with Current-Signature Load Matching
Firmware: mixer_final.ino (Production Competition Build)
Target Microcontroller: Arduino UNO (ATmega328P)
Libraries Required: Adafruit_SSD1306, Adafruit_GFX, Wire
==================================================================================
*/
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// ==================================================================================
// 1. HARDWARE PIN DEFINITIONS
// ==================================================================================
const uint8_t PIN_ACS712_OUT = A0; // Current sensor analog input (via 1k + 0.1uF filter)
const uint8_t PIN_LM35_OUT = A1; // LM35 temperature input (via 1k + 0.1uF filter)
const uint8_t PIN_ENC_CLK = 2; // Rotary encoder CLK (Hardware Interrupt INT0)
const uint8_t PIN_ENC_DT = 3; // Rotary encoder DT (Hardware Interrupt INT1)
const uint8_t PIN_ENC_SW = 4; // Rotary encoder Push Switch (Active LOW)
const uint8_t PIN_RELAY = 7; // 5V Relay module trigger
const uint8_t PIN_LED_ALERT = 8; // Warning & Status LED (via 1k resistor)
const uint8_t PIN_BUZZER = 9; // Active Buzzer (TMB12A05)
// OLED Display Configuration (128x64 I2C SSD1306)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_I2C_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
// ==================================================================================
// 2. CONFIGURABLE SYSTEM PARAMETERS
// ==================================================================================
// ACS712 Sensitivity:
// 0.185 for 5A (05B chip) -> ~3.5A RMS / 5A peak max
// 0.100 for 20A (20A chip) -> ~14A RMS max 
// 0.066 for 30A (30A chip)
const float ACS_SENSITIVITY = 0.185;
// Relay Module Trigger Logic:
// 0 = Active-HIGH (Relay turns ON when D7 is HIGH)
// 1 = Active-LOW (Relay turns ON when D7 is LOW)
const uint8_t RELAY_ACTIVE_LOW = 0;
// Rotary Encoder Hardware Configuration:
const int8_t ENC_STEPS_PER_DETENT = 2;
const uint8_t ENC_REVERSE = 0;
// Thermal Safety & Dynamic Cooling Parameters
const float TEMP_CUTOFF_C = 70.0; // Absolute safety cutoff (degrees C)
const float TEMP_RESET_C = 60.0; // Hysteresis cool-down restart threshold (degrees C)
const float TWARM_C = 40.0; // Threshold where pulse OFF-time dynamically expands
const float K_THERMAL_EXPAND = 100.0; // Expansion slope: +100 ms OFF per 1 deg C above TWARM_C
const float TEMP_OFFSET_C = 0.0; // Calibration offset for LM35 sensor
// Electrical Overload Parameters
const float I_TRIP_A = 3.4; // Overload trip current threshold (Amperes RMS)
const unsigned long TRIP_DELAY_MS= 2000; // Overload sustain duration before trip (ms)
const float MAINS_VOLTAGE = 230.0; // Standard Indian mains voltage for VA calculation
// ==================================================================================
// 3. FOOD PRESET DATA STRUCTURE
// ==================================================================================
struct Preset {
const char* name;
unsigned long t_on_ms; // Pulse ON duration (ms)
unsigned long t_off_base_ms; // Baseline pulse OFF duration (ms) at ambient
float lightA; // RMS current threshold below which load is LIGHT
float heavyA; // RMS current threshold above which load is HEAVY
};
const uint8_t NUM_PRESETS = 4;
Preset presets[NUM_PRESETS] = {
// Name, t_on, t_off, lightA, heavyA
{"Orange", 4000, 1500, 0.40, 1.20}, // Wet soft fruits / low viscosity
{"Coconut", 3500, 2000, 0.80, 2.20}, // Dense grating / chutney
{"Rice", 5000, 2500, 1.10, 2.80}, // Heavy batter / viscous paste
{"Spices", 3000, 2000, 0.60, 1.80} // Hard dry seeds / impact grinding
};
// ==================================================================================
// 4. FINITE STATE MACHINE & TELEMETRY VARIABLES
// ==================================================================================
enum SystemState {
STATE_CALIBRATING,
STATE_READY,
STATE_RUN_ON,
STATE_RUN_OFF,
STATE_OVERHEAT,
STATE_OVERLOAD,
STATE_SENSOR_ERR
};
SystemState currentState = STATE_CALIBRATING;
uint8_t selectedPreset = 0;
// Sensor Readings & Metrics
float zeroCurrentOffsetADC = 512.0;
float currentIrms = 0.0;
float currentTempC = 25.0;
const char* currentLoadTag = "IDLE";
// Timing Engine
unsigned long stateEnterTime_ms = 0;
unsigned long pulseDuration_ms = 0;
unsigned long overloadStartTime_ms = 0;
unsigned long lastSerialLog_ms = 0;
unsigned long lastOledRefresh_ms = 0;
unsigned long lastAlarmBlink_ms = 0;
bool alarmBlinkState = false;
// Rotary Encoder State Machine (Interrupt-Driven)
volatile int encoderTicks = 0;
volatile uint8_t lastEncodedState = 0;
bool lastButtonReading = HIGH;
unsigned long lastButtonDebounce_ms = 0;
// ==================================================================================
// 5. HARDWARE CONTROL HELPER FUNCTIONS
// ==================================================================================
// Relay Driver with strict polarity handling
void setRelay(bool turnOn) {
if (RELAY_ACTIVE_LOW) {
digitalWrite(PIN_RELAY, turnOn ? LOW : HIGH);
} else {
digitalWrite(PIN_RELAY, turnOn ? HIGH : LOW);
}
}
// Check if relay is currently energized
bool isRelayOn() {
int pinVal = digitalRead(PIN_RELAY);
if (RELAY_ACTIVE_LOW) {
return (pinVal == LOW);
} else {
return (pinVal == HIGH);
}
}
// Emergency cut-off: de-energize relay immediately
void safeShutdown() {
setRelay(false);
}
// ==================================================================================
// 6. ROTARY ENCODER INTERRUPT SERVICE ROUTINES
// ==================================================================================
void isrEncoder() {
  uint8_t msb = digitalRead(PIN_ENC_CLK);
  uint8_t lsb = digitalRead(PIN_ENC_DT);
  uint8_t encoded = (msb << 1) | lsb;
  uint8_t sum = (lastEncodedState << 2) | encoded;

  // Standard 2-bit quadrature state transitions (CW = +1, CCW = -1)
  int8_t delta = 0;
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) {
    delta = 1;
  } else if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) {
    delta = -1;
  }

  if (ENC_REVERSE) {
    delta = -delta;
  }

  encoderTicks += delta;
  lastEncodedState = encoded;
}

bool checkEncoderButton() {
  const unsigned long DEBOUNCE_MS = 50;
  bool reading = digitalRead(PIN_ENC_SW);

  if (reading != lastButtonReading) {
    lastButtonDebounce_ms = millis();
  }

  lastButtonReading = reading;

  if ((millis() - lastButtonDebounce_ms) > DEBOUNCE_MS) {
    static bool buttonHandled = false;
    if (reading == LOW && !buttonHandled) {
      buttonHandled = true;
      return true;
    } else if (reading == HIGH) {
      buttonHandled = false;
    }
  }

  return false;
}
// ==================================================================================
// 7. SENSOR SAMPLING & SIGNAL CONDITIONING
// ==================================================================================
// Calibrate ACS712 zero-current midpoint at boot (motor MUST be OFF)
void calibrateACS712Zero() {
safeShutdown();
unsigned long sumADC = 0;
const uint16_t SAMPLES = 1000;
for (uint16_t i = 0; i < SAMPLES; i++) {
sumADC += analogRead(PIN_ACS712_OUT);
delayMicroseconds(100);
}
zeroCurrentOffsetADC = (float)sumADC / (float)SAMPLES;
// Sanity check: 2.5V is count ~512; valid window is 450 to 575
if (zeroCurrentOffsetADC < 450.0 || zeroCurrentOffsetADC > 575.0) {
Serial.print(F("[WARN] ACS712 zero offset out of nominal range: "));
Serial.println(zeroCurrentOffsetADC);
} else {
Serial.print(F("[INIT] ACS712 zero offset calibrated: "));
Serial.println(zeroCurrentOffsetADC, 2);
}
}
// True RMS Current Calculation over integer 50 Hz mains cycles (100 ms = 5 cycles)
float readACSCurrentRMS() {
unsigned long sampleStart = millis();
unsigned long sampleCount = 0;
double sumSquaredDifferences = 0.0;
// Sample continuously across 100 milliseconds
while ((millis() - sampleStart) < 100) {
int raw = analogRead(PIN_ACS712_OUT);
float diff = (float)raw - zeroCurrentOffsetADC;
sumSquaredDifferences += (diff * diff);
sampleCount++;
}
if (sampleCount == 0) return 0.0;
// Mean Square in ADC counts
float meanSquare = sumSquaredDifferences / (float)sampleCount;
float rmsADC = sqrt(meanSquare);
// Convert ADC counts to Volts (5.0V reference over 1023 steps)
float rmsVolts = (rmsADC * 5.0) / 1023.0;
// Convert Volts to RMS Amperes via sensitivity
float irms = rmsVolts / ACS_SENSITIVITY;
// Threshold clamping: ignore low-level noise when motor is idle
if (irms < 0.08) {
irms = 0.0;
}
return irms;
}
// LM35 Precision Temperature Sensing with Moving Average & Range Validation
float readLM35Temperature() {
uint32_t sum = 0;
const uint8_t READS = 16;
for (uint8_t i = 0; i < READS; i++) {
sum += analogRead(PIN_LM35_OUT);
delayMicroseconds(50);
}
float avgADC = (float)sum / (float)READS;
// LM35DZ output: 10 mV / degree C (0V at 0 C)
// Temp = (avgADC * 5.0 / 1023.0) / 0.010 = avgADC * (500.0 / 1023.0)
float tempC = (avgADC * 500.0) / 1023.0 + TEMP_OFFSET_C;
return tempC;
}
// Classify Current-Signature Load Category
const char* evaluateLoadCategory(float irms, uint8_t presetIdx) {
if (currentState != STATE_RUN_ON && currentState != STATE_RUN_OFF) {
return "IDLE";
}
if (irms < presets[presetIdx].lightA) {
return "LIGHT";
} else if (irms <= presets[presetIdx].heavyA) {
return "OK";
} else {
return "HEAVY";
}
}
// Calculate Dynamic Adaptive OFF Cooling Duration
unsigned long calculateAdaptiveOffTime(float tempC, uint8_t presetIdx) {
unsigned long baseOff = presets[presetIdx].t_off_base_ms;
if (tempC > TWARM_C) {
float deltaT = tempC - TWARM_C;
unsigned long addedOff = (unsigned long)(deltaT * K_THERMAL_EXPAND);
return baseOff + addedOff;
}
return baseOff;
}
// ==================================================================================
// 8. OLED DISPLAY RENDERING ENGINE
// ==================================================================================
void updateDisplay() {
display.clearDisplay();
display.setTextColor(SSD1306_WHITE);
// Top Title Bar: Preset and Current State
display.setTextSize(1);
display.setCursor(0, 0);
display.print(F("MODE: "));
display.print(presets[selectedPreset].name);
// State Banner on top right
display.setCursor(84, 0);
switch (currentState) {
case STATE_CALIBRATING: display.print(F("[CAL]")); break;
case STATE_READY: display.print(F("[RDY]")); break;
case STATE_RUN_ON: display.print(F("[RUN]")); break;
case STATE_RUN_OFF: display.print(F("[OFF]")); break;
case STATE_OVERHEAT: display.print(F("[OHT]")); break;
case STATE_OVERLOAD: display.print(F("[OVL]")); break;
case STATE_SENSOR_ERR: display.print(F("[ERR]")); break;
}
// Divider Line
display.drawLine(0, 10, 127, 10, SSD1306_WHITE);
// Main Metrics Display
if (currentState == STATE_OVERHEAT) {
display.setTextSize(2);
display.setCursor(10, 18);
display.print(F("OVERHEAT!"));
display.setTextSize(1);
display.setCursor(8, 40);
display.print(F("Motor Temp: "));
display.print(currentTempC, 1);
display.print(F(" C"));
display.setCursor(8, 52);
display.print(F("Cooldown < 60 C"));
} else if (currentState == STATE_OVERLOAD) {
display.setTextSize(2);
display.setCursor(14, 18);
display.print(F("OVERLOAD!"));
display.setTextSize(1);
display.setCursor(8, 40);
display.print(F("Trip: "));
display.print(currentIrms, 2);
display.print(F(" A RMS"));
display.setCursor(8, 52);
display.print(F("Press SW to Reset"));
} else if (currentState == STATE_SENSOR_ERR) {
display.setTextSize(2);
display.setCursor(4, 18);
display.print(F("SENSOR ERR"));
display.setTextSize(1);
display.setCursor(8, 40);
display.print(F("LM35 Range Fault!"));
display.setCursor(8, 52);
display.print(F("Check Wiring / Pin A1"));
} else {
// Standard Dashboard View
// Line 1: RMS Current & Apparent Power
display.setTextSize(1);
display.setCursor(0, 14);
display.print(F("Irms: "));
display.print(currentIrms, 2);
display.print(F(" A ("));
display.print((int)(currentIrms * MAINS_VOLTAGE));
display.print(F("VA)"));
// Line 2: Motor Temperature & Dynamic Status
display.setCursor(0, 26);
display.print(F("Temp: "));
display.print(currentTempC, 1);
display.print(F(" C"));
if (currentTempC > TWARM_C) {
display.print(F(" [WARM]"));
}
// Line 3: Load Signature Classification
display.setCursor(0, 38);
display.print(F("Load: "));
display.setTextSize(1);
display.print(currentLoadTag);
// Line 4: Action Status / Pulse Timer Prompt
display.setCursor(0, 52);
if (currentState == STATE_READY) {
display.print(F(">> Press SW = START"));
} else if (currentState == STATE_RUN_ON) {
unsigned long elapsed = millis() - stateEnterTime_ms;
unsigned long remain = (pulseDuration_ms > elapsed) ? (pulseDuration_ms - elapsed) / 1000 : 0;
display.print(F("PULSE ON (rem: "));
display.print(remain);
display.print(F("s)"));
} else if (currentState == STATE_RUN_OFF) {
unsigned long elapsed = millis() - stateEnterTime_ms;
unsigned long remain = (pulseDuration_ms > elapsed) ? (pulseDuration_ms - elapsed) / 1000 : 0;
display.print(F("COOLING (rem: "));
display.print(remain);
display.print(F("s)"));
}
}
display.display();
}
// ==================================================================================
// 9. SERIAL TELEMETRY LOGGING 
// ==================================================================================
void logSerialTelemetry() {
// CSV Format: Timestamp_ms, State, Preset, Temp_C, Irms_A, Load_Class, Relay, Power_VA
Serial.print(millis());
Serial.print(F(","));
switch (currentState) {
  case STATE_CALIBRATING: Serial.print(F("CAL")); break;
case STATE_READY: Serial.print(F("READY")); break;
case STATE_RUN_ON: Serial.print(F("RUN_ON")); break;
case STATE_RUN_OFF: Serial.print(F("RUN_OFF")); break;
case STATE_OVERHEAT: Serial.print(F("OVERHEAT")); break;
case STATE_OVERLOAD: Serial.print(F("OVERLOAD")); break;
case STATE_SENSOR_ERR: Serial.print(F("SENSOR_ERR")); break;
}
Serial.print(F(","));
Serial.print(presets[selectedPreset].name);
Serial.print(F(","));
Serial.print(currentTempC, 2);
Serial.print(F(","));
Serial.print(currentIrms, 2);
Serial.print(F(","));
Serial.print(currentLoadTag);
Serial.print(F(","));
Serial.print(isRelayOn() ? 1 : 0);
Serial.print(F(","));
Serial.println(currentIrms * MAINS_VOLTAGE, 1);
}
// ==================================================================================
// 10. SETUP INITIALIZATION
// ==================================================================================
void setup() {
// Initialize Serial Monitor
Serial.begin(9600);
delay(100);
Serial.println(F("\n=================================================="));
Serial.println(F("atomQUEST 2026 | Track 3 | Team Wayne Enterprises"));
Serial.println(F("Mixer Grinder Controller - Initializing..."));
Serial.println(F("=================================================="));
pinMode(PIN_RELAY, OUTPUT);
safeShutdown(); // Ensure relay starts in strictly safe DE-ENERGIZED state
pinMode(PIN_LED_ALERT, OUTPUT);
digitalWrite(PIN_LED_ALERT, LOW);
pinMode(PIN_BUZZER, OUTPUT);
digitalWrite(PIN_BUZZER, LOW);
pinMode(PIN_ENC_CLK, INPUT_PULLUP);
pinMode(PIN_ENC_DT, INPUT_PULLUP);
pinMode(PIN_ENC_SW, INPUT_PULLUP);
attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), isrEncoder, CHANGE);
attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT), isrEncoder, CHANGE);
// Initialize OLED Display
if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR)) {
Serial.println(F("[ERROR] SSD1306 OLED initialization failed! Check wiring."));
while (1) {
// Rapid SOS blink on alert LED if I2C display fails
digitalWrite(PIN_LED_ALERT, HIGH);
delay(100);
digitalWrite(PIN_LED_ALERT, LOW);
delay(100);
}
}
display.clearDisplay();
display.setTextSize(1);
display.setTextColor(SSD1306_WHITE);
display.setCursor(16, 16);
display.println(F("Wayne Enterprises"));
display.setCursor(20, 32);
display.println(F("atomQUEST 2026"));
display.setCursor(18, 48);
display.println(F("Calibrating..."));
display.display();
// Calibrate ACS712 zero offset
calibrateACS712Zero();
// Initial sensor sanity reads
currentTempC = readLM35Temperature();
currentIrms = readACSCurrentRMS();
// Transition to Ready State
currentState = STATE_READY;
stateEnterTime_ms = millis();
Serial.println(F("[INIT] Calibration complete. System READY."));
Serial.println(F("Timestamp_ms,State,Preset,Temp_C,Irms_A,Load,Relay,Power_VA"));
}
// ==================================================================================
// 11. MAIN SUPERVISORY CONTROL LOOP
// ==================================================================================
void loop() {
unsigned long now = millis();
// ----------------------------------------------------
// A. SENSOR READS & SANITY CHECKS
// ----------------------------------------------------
currentTempC = readLM35Temperature();
currentIrms = readACSCurrentRMS();
currentLoadTag = evaluateLoadCategory(currentIrms, selectedPreset);
// Sensor Fault Guard: LM35 range check
if (currentTempC < 2.0 || currentTempC > 110.0) {
if (currentState != STATE_SENSOR_ERR) {
safeShutdown();
currentState = STATE_SENSOR_ERR;
stateEnterTime_ms = now;
Serial.println(F("[CRITICAL] LM35 SENSOR FAULT DETECTED! Shutting down relay."));
}
}
// Absolute Thermal Cutoff Guard (70 C)
if (currentTempC >= TEMP_CUTOFF_C && currentState != STATE_SENSOR_ERR) {
if (currentState != STATE_OVERHEAT) {
safeShutdown();
currentState = STATE_OVERHEAT;
stateEnterTime_ms = now;
Serial.println(F("[CRITICAL] THERMAL OVERHEAT TRIP! Relays cut off."));
}
}
// Electrical Overload Guard 
if (currentIrms >= I_TRIP_A && (currentState == STATE_RUN_ON || currentState == STATE_RUN_OFF)) {
if (overloadStartTime_ms == 0) {
overloadStartTime_ms = now;
} else if ((now - overloadStartTime_ms) > TRIP_DELAY_MS) {
safeShutdown();
currentState = STATE_OVERLOAD;
stateEnterTime_ms = now;
Serial.println(F("[CRITICAL] ELECTRICAL OVERLOAD TRIP! Motor cut off."));
}
} else {
overloadStartTime_ms = 0; // Reset overload timer if current falls back
}
// ----------------------------------------------------
// B. ROTARY ENCODER INPUT & PRESET SELECTION
// ----------------------------------------------------
if (currentState == STATE_READY) {
if (abs(encoderTicks) >= ENC_STEPS_PER_DETENT) {
int steps = encoderTicks / ENC_STEPS_PER_DETENT;
encoderTicks %= ENC_STEPS_PER_DETENT;
int nextPreset = (int)selectedPreset + steps;
while (nextPreset < 0) nextPreset += NUM_PRESETS;
selectedPreset = nextPreset % NUM_PRESETS;
Serial.print(F("[USER] Preset Selected: "));
Serial.println(presets[selectedPreset].name);
}
}
// Push Button Handling
if (checkEncoderButton()) {
if (currentState == STATE_READY) {
// Start pulse mode
currentState = STATE_RUN_ON;
pulseDuration_ms = presets[selectedPreset].t_on_ms;
stateEnterTime_ms = now;
setRelay(true);
digitalWrite(PIN_LED_ALERT, HIGH);
Serial.print(F("[STATE] Starting Pulse Cycle for Preset: "));
Serial.println(presets[selectedPreset].name);
} else if (currentState == STATE_RUN_ON || currentState == STATE_RUN_OFF) {
// Manual User Abort
safeShutdown();
digitalWrite(PIN_LED_ALERT, LOW);
digitalWrite(PIN_BUZZER, LOW);
currentState = STATE_READY;
stateEnterTime_ms = now;
Serial.println(F("[STATE] Pulse sequence aborted by user. System READY."));
} else if (currentState == STATE_OVERLOAD || currentState == STATE_SENSOR_ERR) {
// Alarm Reset Acknowledgment
if (currentTempC < TEMP_RESET_C && currentTempC >= 2.0 && currentTempC <= 110.0) {
digitalWrite(PIN_LED_ALERT, LOW);
digitalWrite(PIN_BUZZER, LOW);
currentState = STATE_READY;
stateEnterTime_ms = now;
Serial.println(F("[STATE] Alarm reset acknowledged. System READY."));
}
}
}
// Auto-recovery for Thermal Cutoff once cooled below TEMP_RESET_C (60 C)
if (currentState == STATE_OVERHEAT && currentTempC < TEMP_RESET_C) {
digitalWrite(PIN_LED_ALERT, LOW);
digitalWrite(PIN_BUZZER, LOW);
currentState = STATE_READY;
stateEnterTime_ms = now;
Serial.println(F("[STATE] Cooled below reset threshold (60 C). System READY."));
}
// ----------------------------------------------------
// C. FINITE STATE MACHINE: PULSE SEQUENCING
// ----------------------------------------------------
switch (currentState) {
case STATE_CALIBRATING:
break;
case STATE_READY:
safeShutdown();
digitalWrite(PIN_LED_ALERT, LOW);
digitalWrite(PIN_BUZZER, LOW);
break;
case STATE_RUN_ON:
// Relay ON pulse phase
setRelay(true);
digitalWrite(PIN_LED_ALERT, HIGH);
digitalWrite(PIN_BUZZER, LOW);
if ((now - stateEnterTime_ms) >= pulseDuration_ms) {
// Transition to OFF cooling phase
setRelay(false);
digitalWrite(PIN_LED_ALERT, LOW);
currentState = STATE_RUN_OFF;
pulseDuration_ms = calculateAdaptiveOffTime(currentTempC, selectedPreset);
stateEnterTime_ms = now;
Serial.print(F("[PULSE] ON finished. Dynamic OFF duration: "));
Serial.print(pulseDuration_ms);
Serial.println(F(" ms"));
}
break;
case STATE_RUN_OFF:
// Relay OFF cooling phase
safeShutdown();
digitalWrite(PIN_LED_ALERT, LOW);
digitalWrite(PIN_BUZZER, LOW);
if ((now - stateEnterTime_ms) >= pulseDuration_ms) {
// Transition back to ON pulse phase
currentState = STATE_RUN_ON;
pulseDuration_ms = presets[selectedPreset].t_on_ms;
stateEnterTime_ms = now;
setRelay(true);
digitalWrite(PIN_LED_ALERT, HIGH);
Serial.println(F("[PULSE] OFF finished. Starting next ON cycle."));
}
break;
case STATE_OVERHEAT:
case STATE_OVERLOAD:
case STATE_SENSOR_ERR:
safeShutdown();
// Alarm alert: 2 Hz audible and visual strobe
if ((now - lastAlarmBlink_ms) >= 250) {
lastAlarmBlink_ms = now;
alarmBlinkState = !alarmBlinkState;
digitalWrite(PIN_LED_ALERT, alarmBlinkState ? HIGH : LOW);
digitalWrite(PIN_BUZZER, alarmBlinkState ? HIGH : LOW);
}
break;
}
// ----------------------------------------------------
// D. PERIODIC TELEMETRY & DISPLAY REFRESH
// ----------------------------------------------------
// Refresh OLED at 5 Hz (every 200 ms)
if ((now - lastOledRefresh_ms) >= 200) {
lastOledRefresh_ms = now;
updateDisplay();
}
// Stream Serial CSV at 4 Hz (every 250 ms)
if ((now - lastSerialLog_ms) >= 250) {
lastSerialLog_ms = now;
logSerialTelemetry();
}
}
