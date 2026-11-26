# Tubercle-Edge Blades with Current-Signature Load Matching for a Quieter, Cooler Mixer Grinder

**atomQUEST 2026 | Round 3 | Track 3: Mixer Grinder **

**Team Wayne Enterprises**  
Jaypee Institute of Information Technology, Noida

---

## Overview

Conventional straight-edged mixer-grinder blades generate turbulent flow, contributing to fluctuating motor loads, noise, heat build-up and inefficient energy consumption.

Our project explores a biomimetic blade design inspired by the tubercles found on humpback whale flippers. By modifying the blade edges, we aim to improve fluid flow and reduce the load experienced by the motor.

We developed two blade variants — **Tubercle and Hybrid (Tubercle + Serrated)** — and compared them against a standard mixer-grinder blade.

Alongside the blade modifications, we developed an intelligent control system incorporating food-type selection, motor-current sensing, temperature monitoring and an overheat alert mechanism.

The project combines mechanical design, embedded electronics and computational fluid dynamics (CFD) to explore a more efficient and safer mixer-grinder system.

---

## Key Features

- **Biomimetic Blade Design:** Two modified blade geometries inspired by humpback whale flipper tubercles.
- **Comparative Testing:** Standard, tubercle and hybrid blades tested using the same jar and motor.
- **Smart Food-Type Selection:** Predefined grinding profiles selected through a rotary encoder and displayed on an OLED.
- **Motor-Current Monitoring:** Current sensing to identify motor loading conditions.
- **Temperature Monitoring:** A temperature sensor positioned near the motor.
- **Overheat Alert:** Buzzer, LED and OLED warning when the temperature crosses the defined threshold.
- **Adaptive Cooling:** Pulse OFF duration increases with rising temperature.
- **Safety Mechanisms:** Relay-based motor cut-off, overload protection logic and sensor-fault lock-out implemented in firmware.

---

## The Three Blade Designs

<details>
<summary><strong>1. Standard Blade (Baseline)</strong></summary>

The conventional straight-edged mixer-grinder blade serves as the control for comparative testing.

</details>

<details>
<summary><strong>2. Tubercle Blade</strong></summary>

Inspired by the leading-edge tubercles of humpback whale flippers, this design incorporates sinusoidal bumps along the blade edge.

The design aims to influence flow behaviour, reduce drag and produce a more stable motor load.

</details>

<details>
<summary><strong>3. Hybrid Blade</strong></summary>

The hybrid blade combines tubercle-inspired bumps with serrations along the blade edge.

It explores the combined effect of two different edge modifications on flow behaviour and motor loading.

</details>

All three blade designs were tested using the same jar and motor assembly.

---

## Prototype and System Architecture

The physical prototype integrates the grinder assembly with an Arduino-based monitoring and control system.

### Hardware Components

| Component | Purpose |
|---|---|
| Arduino UNO | Main controller |
| 0.96-inch SSD1306 OLED | Mode selection and system status |
| Rotary Encoder (HW-040) | Food preset selection and start/stop |
| ACS712 Current Sensor (20A) | Motor-current sensing |
| LM35DZ Temperature Sensor | Motor temperature monitoring |
| Relay Module | Motor switching and cut-off |
| Active Buzzer | Overheat alert |
| LED | Visual warning |
| Fuse | Hardware protection |
| Grinder Motor and Jar | Grinding assembly |

### Working

1. The user selects a predefined food profile through the rotary encoder.
2. The selected mode and system information are displayed on the OLED.
3. The controller monitors motor current and temperature.
4. The firmware uses the current measurements for load classification and adaptive control.
5. As temperature rises, the adaptive pulse-OFF logic increases cooling intervals.
6. If the overheat threshold is crossed, the system triggers the buzzer, LED and OLED warning and stops the motor through the relay.

The Arduino also streams live operating information to the serial monitor.

### Available Presets

- Orange
- Coconut
- Rice
- Spices

Preset timings and current thresholds are initial engineering values and require further calibration using actual food loads.

---

## Experimental Setup

The blades were tested using:

- Universal mixer-grinder motor and jar
- Regulated DC bench power supply (Aplab LQ 6324T)
- Keysight EDUX1052A digital oscilloscope
- Arduino-based monitoring setup

**Testing conditions:** Approximately 30–31.5 V DC bench supply.

Each blade was tested with two food materials:

- Rice
- Daal

The supply voltage and current readings were recorded for each run. Electrical input power was calculated using:

\[
P=V\times I
\]

Oscilloscope peak-to-peak (Pk-Pk) readings were also recorded to compare the observed motor signal.

---

## Experimental Results

### 1. Supply Current and Input Power

| Blade | Food | Voltage (V) | Current (A) | Input Power (W) |
|---|---|---:|---:|---:|
| Standard | Rice | 30.9 | 0.72 | 22.2 |
| Standard | Daal | 30.9 | 0.76 | 23.5 |
| Tubercle | Rice | 30.3 | 0.54 | 16.4 |
| Tubercle | Daal | 30.3 | 0.52 | 15.8 |
| Hybrid | Rice | 31.5 | 0.67 | 21.1 |
| Hybrid | Daal | 31.5 | 0.69 | 21.7 |

### 2. Reduction Compared to Standard Blade

| Blade | Food | Current Reduction | Input Power Reduction |
|---|---|---:|---:|
| Tubercle | Rice | 25.0% | 26.5% |
| Tubercle | Daal | 31.6% | 32.9% |
| Hybrid | Rice | 6.9% | 5.1% |
| Hybrid | Daal | 9.2% | 7.4% |

**Key Observation:** The tubercle blade demonstrated the highest reduction in supply current and electrical input power across both food materials.

### 3. Oscilloscope Results

| Blade | Rice Pk-Pk | Daal Pk-Pk |
|---|---:|---:|
| Standard | >9.68 V | 11.0 V |
| Tubercle | 330 mV | 378 mV |
| Hybrid | 273 mV | 13.2 V |

*Note: Oscilloscope vertical scales and time bases differed between captures. These readings are indicative and should not be treated as a directly controlled waveform comparison.*

---

## CFD Simulation

COMSOL Computational Fluid Dynamics (CFD) simulations were conducted at rotational speeds ranging from **100–500 RPM**.

The simulation predicted approximately:

- 13% reduction in drag torque.
- 13% reduction in hydrodynamic power.

These predictions were obtained for the tubercle blade relative to the standard blade.

The experimental results showed a larger reduction in electrical input power for the tubercle blade. 

---

## Smart Safety and Thermal-Control System

The prototype incorporates a temperature-based safety mechanism to prevent overheating.

### Protection Mechanisms

| Protection | Implemented Logic |
|---|---|
| Thermal Trip | Motor cut-off at 70°C |
| Thermal Reset | Automatic reset below 60°C |
| Overload Protection | RMS current above 3.4 A for 2 seconds triggers a trip |
| Sensor Fault | Lock-out if temperature reading leaves the 2–110°C range |
| Adaptive Cooling | Longer OFF intervals as temperature rises |
| User Abort | Manual stop through encoder switch |

### Overheat Demonstration

During the live demonstration:

- The OLED displayed **OVERHEAT!**
- The motor-temperature reading was displayed.
- The buzzer activated.
- The status LED illuminated.
- The display indicated the cooldown condition.

The overheat alert was demonstrated in the prototype video.

**Important:** The live AC-side overload protection, sensor-fault lock-out and 70°C cut-off under a running grinder load have not yet been independently demonstrated. AC testing remains future work.

---

## Cost Analysis

### Prototype Bill of Materials

| Component | Cost (₹) |
|---|---:|
| Stainless-steel blades (8 pieces) | 1,500 |
| Arduino UNO | 339 |
| SSD1306 OLED | 251 |
| HW-040 Rotary Encoder | 258 |
| ACS712 Current Sensor | 184 |
| LM35DZ Temperature Sensor | 170 |
| 5V Relay Module | 149 |
| LED and Active Buzzer | 79 |
| Passives, Breadboard and Jumper Wires | 375 |
| **Total Prototype Cost** | **₹3,305** |

### Estimated Manufacturing Cost at Scale

At a production scale of approximately 1,000+ units, the estimated BOM cost could reduce to:

**₹900–₹1,200 per unit**

This estimate assumes the replacement of retail modules with bare ICs, a custom PCB and bulk-manufactured blades.

---

## Power Consumption

The measured DC bench-supply input power was:

| Blade | Rice | Daal |
|---|---:|---:|
| Standard | 22.2 W | 23.5 W |
| Tubercle | 16.4 W | 15.8 W |
| Hybrid | 21.1 W | 21.7 W |

These values represent electrical input power supplied to the DC motor during bench testing, not the mains power consumption of a commercial mixer grinder.

---

## Limitations and Future Scope

### Current Limitations

- Only one observation was recorded per blade/food combination.
- Supply voltage varied between blade tests.
- Oscilloscope scales and time bases were not identical.
- AC mains-powered grinder testing has not been completed.
- Noise levels, temperature rise, grind time and grind quality have not been quantitatively measured.
- Overload trip and sensor-fault lock-out require further validation.
- CFD results are simulation-based predictions.

### Future Work

1. Conduct supervised AC-side testing of the complete grinder system.
2. Validate relay cut-off, overload protection and thermal trip under actual operating conditions.
3. Perform at least three test runs per blade and food combination.
4. Record RMS current, peak current, temperature rise and electrical power at controlled supply conditions.
5. Measure sound levels (dB(A)) at a fixed distance.
6. Compare grinding time and final grind quality.
7. Calibrate preset timings and current thresholds using actual food loads.
8. Further optimise blade geometry using CFD and experimental validation.

---


## Team

**Team Wayne Enterprises**  
Jaypee Institute of Information Technology, Noida

- Archita Gupta — ECE, 3rd Year
- Bhavan Kumar Bhardwaj — ECE, 3rd Year
- Shubhi Jain — ECE, 3rd Year

**atomQUEST 2026 | Track 3: Mixer Grinder**

---

## License

Copyright © 2026 Team Wayne Enterprises, Jaypee Institute of Information Technology, Noida. All rights reserved.

This repository is published for viewing and evaluation only. No part of this project, including its blade designs, CAD files, drawings, source code, images, experimental results and documentation, may be copied, modified, distributed or used commercially without prior written permission from Team Wayne Enterprises.
