# Tubercle-Edge Blades for a Quieter, Smoother BLDC Mixer Grinder

**atomQUEST 2026 | Round 2 | Track 3: Mixer Grinder (Atomberg problem statement)**
**Team Wayne Enterprises**, Jaypee Institute of Information Technology, Noida

Straight-edged mixer-grinder blades shed turbulent, unstable flow. The motor load keeps fluctuating, which shows up as current spikes and ripple, and the result is more noise, more heat and wasted energy.

Our idea is to borrow the whale-flipper (tubercle) leading edge: the bumps shed vortices, giving steadier flow and lower drag on the BLDC motor. We built and tested two new blades against the existing straight-edge blade, and simulated a smart thermal-control routine on an Arduino.

---

## Contents

| File | What it is |
|---|---|
| `blade_plan.dwg` | CAD plan of the **tubercle blade** |
| `blade_plan_dimensioned__1_.png` | Dimensioned drawing of the tubercle blade |
| `scale_hybrid_blade_planform__1_.dwg` | CAD plan of the **hybrid blade** (serrated + tubercle) |
| `hybrid_blade_planform_dimensioned__1_.png` | Dimensioned drawing of the hybrid blade |
| `Sketch.ino` | Arduino UNO smart thermal-control firmware (simulated in Wokwi) |
| `diagram.json` | Wokwi circuit diagram for the sketch |

> Tip: rename files with `(1)`-style suffixes (e.g. `blade_plan_dimensioned.png`) and update the links below.

---

## The three blades

All blades are point-symmetric (the bottom edge is the top edge rotated 180° about the hub centre) and share the same basic planform.

| Parameter | Value |
|---|---|
| Overall length | 70.0 mm |
| Centre (flat zone) width | 14.0 mm |
| End width | 10.5 mm |
| Taper | ≈ 4.0° (10.5 → 14.0 mm over about 24.5 to 24.9 mm) |
| Hub hole | Ø7.0 mm (R3.5) |
| Blade thickness | 1.5 mm (from the atomQUEST Round 2 brief; a 2D plan does not store thickness) |
| Max width incl. tubercle crests | 15.88 mm |

### 1. Normal blade (baseline)
The existing straight-edge blade, used as the baseline.

### 2. Tubercle blade
Leading-edge bumps (sinusoidal) on each long edge.

| Tubercle parameter | Value |
|---|---|
| Tubercle zone | 21.0 mm = 6 × 3.5 mm pitch |
| Pitch (λ) | 3.5 mm |
| Crest to trough | 1.3 mm |
| Crest / trough radius | R0.52 |
| Zone start | x = 45.5 mm |

![Tubercle blade dimensioned plan](blade_plan_dimensioned__1_.png)

### 3. Hybrid blade
Tubercle leading edge **plus** a serrated (saw-tooth) trailing edge, so each blade carries both zones.

| Serrated parameter | Value |
|---|---|
| Serrated zone | 19.7 mm, pitch 2.62 mm |
| Crest to root | 0.87 mm |
| Tip angle | ≈ 113° (straight flanks) |
| Teeth | 7 full teeth + 1 truncated tooth at the taper end (first crest x = 4.8, last crest x = 23.2) |

The tubercle zone on the hybrid blade is the same as above (21.0 mm = 6 × 3.5 mm).

![Hybrid blade dimensioned planform](hybrid_blade_planform_dimensioned__1_.png)

---

## Test setup

- BLDC mixer-grinder base
- Lab DC supply
- Digital oscilloscope (motor signal on channel 1)

We measured the **peak-to-peak (Pk-Pk)** of the motor signal. A lower Pk-Pk means a smoother load on the motor. Each blade was tested with an **empty jar** and with **water** in the jar.

## Results

| Blade | Empty jar (Pk-Pk) | Water (Pk-Pk) |
|---|---|---|
| Normal (baseline) | 4.78 V (1 V/div, 5 ms/div) | 14.2 V (2 V/div, 2 ms/div) |
| Tubercle | 3.1 V (2 V/div, 2 ms/div) | 2.23 V (500 mV/div, 1 ms/div); 3.6 to 4.5 V live at 5 ms/div |
| Hybrid | 1.85 V (500 mV/div, 10 ms/div) | 3.14 V (1 V/div, 1 ms/div); 6.9 V at 5 ms/div |

Key takeaways:

- Under water load, both new blades run far smoother than the normal blade (at least 68% lower for tubercle and 51% lower for hybrid, using the longer 5 ms/div window).
- Empty jar: the hybrid blade is the smoothest (at least 61% lower than normal).
- Percentages use the longer time base, which can only raise Pk-Pk, so they are conservative.
- The readings were taken at different oscilloscope time bases, so the next step is to re-test all blades at one fixed time base.

---

## Smart thermal control (simulation)

`Sketch.ino` and `diagram.json` simulate a smart-grinder controller on an **Arduino UNO** in [Wokwi](https://wokwi.com). It is a simulation only; we plan to bring this logic onto the hardware prototype.

### What it does

- **Preset selection** with a rotary encoder, shown on an SSD1306 OLED.
- **Soft start**: the motor speed ramps up over the first 2 seconds.
- **Temperature-based cool-rest**: above 50 °C the controller adds rest time between grinding bursts, and the rest gets longer as the motor gets hotter.
- **Lid interlock**: opening the lid stops the motor immediately.
- **Overheat cut-off**: the motor is stopped at 70 °C, and it will not restart until it has cooled by at least 5 °C.

| Preset | Speed | Run time | Mode |
|---|---|---|---|
| Orange | 60% | 60 s | Continuous |
| Spices | 90% | 30 s | Pulsing, 5 s ON / 2 s OFF base |
| Coconut | 75% | 60 s | Continuous |
| Rice | 85% | 45 s | Pulsing, 8 s ON / 3 s OFF base |

State machine: `IDLE → RUNNING → DONE`, with `FAULT_LID` and `FAULT_TEMP` fault states.

### Wiring (as in `diagram.json`)

| Part | Arduino pin |
|---|---|
| Motor output (PWM, shown as an LED via a resistor) | D5 |
| Rotary encoder CLK / DT / SW | D6 / D7 / D3 |
| Lid switch (LOW = lid closed) | D4 |
| Temperature input (potentiometer stands in for a sensor, mapped 20 to 80 °C) | A0 |
| OLED SSD1306 SDA / SCL | A4 / A5 |

### Run it

1. Open [wokwi.com](https://wokwi.com) and create a new Arduino UNO project.
2. Replace the default `sketch.ino` with `Sketch.ino` and `diagram.json` with the file from this repo.
3. Add the libraries **Adafruit GFX Library** and **Adafruit SSD1306** (Wokwi Library Manager, or a `libraries.txt` file).
4. Press **Start Simulation**. Turn the encoder to pick a preset, click to start, and move the potentiometer above 50 °C to see the cool-rest, or to 70 °C to trigger OVERHEAT.

---

## Status

**Done**
- Tubercle and hybrid blades built and fitted to the jar
- Test rig: BLDC base, lab DC supply, oscilloscope
- Readings taken for empty jar and water vs the normal blade
- Smart thermal-control logic simulated (Arduino)

**Pending**
- Re-test all blades at one fixed scope time base
- Noise (dB) and heat measurements
- CFD drag-torque (Cd) comparison
- Move the controller from simulation to the hardware prototype

**Safety / fail-safe**
- Tests run on a current-monitored bench supply
- Rounded tubercle edges, no sharp bump tips
- Simulated: overheat alert and cool-down rest
- Planned: jar-lock detection and overload cut-off

---

## Team

**Team Wayne Enterprises**, Jaypee Institute of Information Technology, Noida

- Archita Gupta
- Bhavan Kumar Bharadwaj
- Shubhi Jain

## License

**Copyright © 2026 Team Wayne Enterprises, Jaypee Institute of Information Technology, Noida. All rights reserved.**

This repository is published for viewing and evaluation only. No part of it, including the blade designs, drawings, CAD files, images, results and source code, may be copied, modified, distributed or used commercially without prior written permission from Team Wayne Enterprises.
