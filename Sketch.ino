#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// 1. Hardware Pin Definitions
#define PIN_PWM   5   // Motor PWM Output (Pin 5)
#define PIN_CLK   6   // Rotary Encoder CLK
#define PIN_DT    7   // Rotary Encoder DT
#define PIN_SEL   3   // Rotary Encoder Button Switch
#define PIN_SAFE  4   // Safety Lid Switch (LOW = Lid Closed / Safe)
#define PIN_TEMP  A0  // Motor Temp Sensor Input (A0)

// Thermal Safety Thresholds (in °C)
#define TEMP_BASE      25.0  // Room temperature
#define TEMP_WARM      50.0  // Temperature where thermal rest scaling begins
#define TEMP_MAX_CUT   70.0  // Hard thermal shutdown limit

// 2. Display Setup
Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// 3. Preset Structure
struct Preset {
  const char* name;
  uint8_t speedPct;  // Speed 0 - 100%
  uint16_t seconds;  // Total run duration in seconds
  uint8_t pulseOn;   // Pulse ON duration (0 = continuous base)
  uint8_t pulseOff;  // Base pulse OFF duration
};

Preset presets[] = {
  {"Orange",  60, 60, 0, 0}, // Continuous mode base
  {"Spices",  90, 30, 5, 2}, // Pulsing mode (5s ON / 2s OFF base)
  {"Coconut", 75, 60, 0, 0}, // Continuous mode base
  {"Rice",    85, 45, 8, 3}  // Pulsing mode (8s ON / 3s OFF base)
};

const int N_PRESETS = sizeof(presets) / sizeof(presets[0]);

// 4. System States & Global Variables
enum State { IDLE, RUNNING, DONE, FAULT_LID, FAULT_TEMP };
State state = IDLE;

int sel = 0;
int lastClk;
unsigned long startMs = 0;
unsigned long lastBtnMs = 0;
float currentTemp = 25.0;

// Motor driver output
void setMotor(uint8_t pct) { 
  analogWrite(PIN_PWM, map(pct, 0, 100, 0, 255)); 
}

void stopMotor() { 
  setMotor(0); 
}

// Read Motor Temperature (mapped from pot 20°C - 80°C)
float readTemperature() {
  int rawADC = analogRead(PIN_TEMP);
  return map(rawADC, 0, 1023, 20, 80);
}

bool isSafe() { 
  return digitalRead(PIN_SAFE) == LOW; 
}

void drawScreen(const char* line1, const char* line2 = "", const char* line3 = "") {
  oled.clearDisplay();
  oled.setTextSize(2);
  oled.setTextColor(WHITE);
  oled.setCursor(0, 0);
  oled.println(line1);
  
  oled.setTextSize(1);
  oled.setCursor(0, 30);
  oled.println(line2);

  oled.setCursor(0, 48);
  oled.println(line3);
  oled.display();
}

void setup() {
  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SEL, INPUT_PULLUP);
  pinMode(PIN_SAFE, INPUT_PULLUP);
  
  pinMode(PIN_PWM, OUTPUT);
  stopMotor();

  oled.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  drawScreen("Atomberg", "Smart Grinder", "Temp Control V3");
  delay(1000);

  lastClk = digitalRead(PIN_CLK);
}

void loop() {
  currentTemp = readTemperature();

  // Instant safety checks during RUNNING
  if (state == RUNNING) {
    if (!isSafe()) {
      stopMotor();
      state = FAULT_LID;
    } else if (currentTemp >= TEMP_MAX_CUT) {
      stopMotor();
      state = FAULT_TEMP;
    }
  }

  // Rotary Encoder Navigation
  int clk = digitalRead(PIN_CLK);
  if (state == IDLE && clk != lastClk && clk == LOW) {
    if (digitalRead(PIN_DT) != clk) {
      sel = (sel + 1) % N_PRESETS;
    } else {
      sel = (sel + N_PRESETS - 1) % N_PRESETS;
    }
  }
  lastClk = clk;

  // Debounced Button Press
  bool btnPressed = (digitalRead(PIN_SEL) == LOW) && (millis() - lastBtnMs > 300);
  if (btnPressed) {
    lastBtnMs = millis();
  }

  // Main State Machine
  switch (state) {
    case IDLE: {
      char tBuf[24];
      sprintf(tBuf, "Temp: %d C | Click Start", (int)currentTemp);
      drawScreen(presets[sel].name, tBuf);

      if (btnPressed && isSafe()) {
        if (currentTemp < TEMP_MAX_CUT) {
          startMs = millis();
          state = RUNNING;
        } else {
          state = FAULT_TEMP;
        }
      }
      break;
    }

    case RUNNING: {
      Preset &p = presets[sel];
      unsigned long elapsedSec = (millis() - startMs) / 1000;

      if (elapsedSec >= p.seconds) {
        stopMotor();
        state = DONE;
        break;
      }

      // Dynamic Temperature-Based Timing Calculation
      bool motorOn = true;
      float calculatedOffTime = 0.0;

      if (p.pulseOn == 0) {
        // CONTINUOUS MODE (Orange / Coconut):
        // If temp > 50°C, insert a thermal cooling pause every 10 seconds of grinding
        if (currentTemp > TEMP_WARM) {
          calculatedOffTime = (currentTemp - TEMP_WARM) * 0.2; // Increases rest duration with heat
          uint16_t cyclePeriod = 10 + (uint16_t)calculatedOffTime;
          motorOn = (elapsedSec % cyclePeriod) < 10;
        }
      } else {
        // PULSING MODE (Spices / Rice):
        // Base off-time increases proportionally as temperature rises above 50°C
        calculatedOffTime = p.pulseOff;
        if (currentTemp > TEMP_WARM) {
          calculatedOffTime = p.pulseOff + ((currentTemp - TEMP_WARM) * 0.25);
        }
        uint16_t cyclePeriod = p.pulseOn + (uint16_t)calculatedOffTime;
        motorOn = (elapsedSec % cyclePeriod) < p.pulseOn;
      }

      // Soft-Start Ramping (First 2 Seconds)
      uint8_t currentPct = p.speedPct;
      if (millis() - startMs < 2000) {
        currentPct = (p.speedPct * (millis() - startMs)) / 2000;
      }

      setMotor(motorOn ? currentPct : 0);

      // Status Display
      char buf1[24], buf2[24];
      sprintf(buf1, "%lus left | %d C", p.seconds - elapsedSec, (int)currentTemp);

      if (calculatedOffTime > 0.0) {
        int wholePart = (int)calculatedOffTime;
        int decPart = (int)((calculatedOffTime - wholePart) * 10);
        sprintf(buf2, "Cool Rest: %d.%ds", wholePart, decPart);
      } else {
        sprintf(buf2, "Mode: Continuous");
      }

      drawScreen(p.name, buf1, buf2);

      if (btnPressed) {
        stopMotor();
        state = IDLE;
      }
      break;
    }

    case DONE:
      drawScreen("Done!", "Press to reset");
      if (btnPressed) state = IDLE;
      break;

    case FAULT_LID:
      drawScreen("LID OPEN", "Close lid, press");
      if (btnPressed && isSafe()) state = IDLE;
      break;

    case FAULT_TEMP:
      char errBuf[24];
      sprintf(errBuf, "Motor Hot: %d C", (int)currentTemp);
      drawScreen("OVERHEAT!", errBuf, "Cool down motor");
      if (btnPressed && currentTemp < (TEMP_MAX_CUT - 5.0)) state = IDLE;
      break;
  }

  delay(10);
}
