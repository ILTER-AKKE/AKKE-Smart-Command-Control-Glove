# AKKE — Smart Command and Control Glove

> *Akıllı Komuta Kontrol Eldiveni — a wearable gesture-to-audio command system for silent tactical communication.*

🌐 **Languages:** **English** · [Türkçe](README.tr.md)

[![Status](https://img.shields.io/badge/status-active--development-blue)]()
[![Platform](https://img.shields.io/badge/platform-ESP32--S3-informational)]()
[![Course](https://img.shields.io/badge/TEDU-CMPE%20491%2F492-red)]()

---

## Overview

**AKKE** is a wearable hand-gesture recognition system designed for environments where speech is impossible, unsafe, or operationally undesirable — military operations, search-and-rescue missions, and high-noise industrial sites.

The system consists of two cooperating ESP32 nodes:

1. **Glove unit** — captures hand pose and motion through 5 flex sensors and an MPU6050 IMU, classifies gestures on-device using a quantized TensorFlow Lite Micro model (Edge Impulse), and transmits the recognized command wirelessly.
2. **Receiver unit** — receives the command over ESP-NOW, plays the corresponding audio file from a DFPlayer Mini, and shows the active command, volume, and battery level on a 16×2 I²C LCD.

The end result: a wearer makes a hand sign, and within ~200 ms the team hears a clear voice command in their earpiece — without anyone speaking.

---

## Team

| Name | Department | Role |
|---|---|---|
| **Berk Çakmak** | Computer Engineering | Project Lead |
| Abdullah Esin | Computer Engineering (EEE double major) | Software & ML |
| Ömer Efe Dikici | Computer Engineering | Software & System Integration |
| Şevval Kurtulmuş | Electrical-Electronics Engineering (CMPE double major) | Hardware & Embedded |

**Supervisors:** Ali Berkol, Hüseyin Uğur Yıldız
**Jury:** Hakkı Gökhan İlk, Mehmet Evren Coşkun
**Institution:** TED University, Faculty of Engineering — CMPE 491 / 492 Senior Project

**Supported by:** HAVELSAN SUIT Program

---

## Supported Gestures

The current firmware classifies and transmits 10 commands:

| # | Command | Use case |
|---|---|---|
| 1 | Stop | Halt movement |
| 2 | Listen | Pay attention |
| 3 | Go go | Advance |
| 4 | Come | Approach |
| 5 | Stick together | Regroup |
| 6 | Slow down | Reduce pace |
| 7 | Fall back | Retreat |
| 8 | Take cover | Seek shelter |
| 9 | Move right | Lateral movement right |
| 10 | Move left | Lateral movement left |

The roadmap extends this set to 15 (check the area, spread out, get down, area clear, operation over).

---

## System Architecture

```
┌─────────────────── GLOVE UNIT ───────────────────┐         ┌──────────── RECEIVER UNIT ───────────┐
│                                                  │         │                                       │
│   5× Flex sensors ──┐                            │         │   ESP32 ──── DFPlayer Mini ─── Speaker│
│                     ├─► ESP32-S3 ──► TFLite      │ ESP-NOW │     │                                 │
│   MPU6050 (IMU) ────┘    (Edge Impulse model)    │ ───────►│     ├─── 16×2 I²C LCD (status)       │
│                                                  │ 2.4 GHz │     ├─── Potentiometer (volume)      │
│   Button ─► state machine                        │         │     └─── Battery voltage divider     │
│   RGB LED ─► visual feedback                     │         │                                       │
│                                                  │         │                                       │
└──────────────────────────────────────────────────┘         └───────────────────────────────────────┘
```

### Data flow

1. The wearer holds a hand pose for ~3 seconds (sample window controlled by the Edge Impulse impulse settings).
2. The glove samples 13 features per timestep: 5 normalized flex values, Kalman-filtered pitch and roll, 3-axis accelerometer (calibrated), 3-axis gyroscope (calibrated).
3. The TFLite Micro classifier runs on-device and outputs a label with a confidence score.
4. If confidence ≥ 75 %, the label is mapped to a command ID (1–10) and broadcast over ESP-NOW.
5. The receiver matches the command ID to an MP3 file on the DFPlayer SD card and plays it.

---

## Hardware

### Glove unit
| Component | Purpose | Pin / Bus |
|---|---|---|
| ESP32-S3 DevKit | Main MCU, on-device inference | — |
| 5× flex sensors | Finger bend (voltage divider) | GPIO 25, 33, 32, 35, 34 |
| MPU6050 | 3-axis accel + 3-axis gyro | I²C: SDA=21, SCL=22 |
| Push button | State transitions | GPIO 13 (INPUT_PULLUP) |
| RGB LED | Visual feedback | R=15, G=2, B=0 |
| Battery monitor | Voltage divider (R1=20 k, R2=10 k) | GPIO 39 |
| Li-Po + TP4056 + HT7333 | Power chain | — |

> ⚠️ **GPIO 0 caveat:** the blue channel uses GPIO 0, which is the ESP32 boot-mode select pin. Keep the LED OFF at boot (or pull it up) to avoid forcing the chip into download mode.

### Receiver unit
| Component | Purpose | Pin / Bus |
|---|---|---|
| ESP32 | Receiver MCU | — |
| DFPlayer Mini | MP3 playback from microSD | UART1: RX=16, TX=17 (9600 baud) |
| 16×2 I²C LCD | Status display (addr 0x27) | I²C: SDA=21, SCL=22 |
| Potentiometer | Live volume control (0–30) | GPIO 36 |
| Battery monitor | Voltage divider | GPIO 35 |
| Speaker (3 W, 8 Ω) | Audio output | DFPlayer SPK_1 / SPK_2 |

---

## State Machine (glove)

```
   ┌──────┐  button   ┌──────────────┐   auto   ┌───────┐  button   ┌─────────────┐
   │ IDLE ├──────────►│ CALIBRATING  ├─────────►│ READY ├──────────►│ INFERENCING │
   └──────┘           └──────────────┘          └───┬───┘           └──────┬──────┘
   (blue LED)         (yellow→red→purple)      (blue LED)            (yellow LED)
                                                    │                       │
                                                    └───────── auto ◄───────┘
```

### LED legend
| State | Color | Meaning |
|---|---|---|
| Boot | Blinking white (3×) | Power-on self check |
| IDLE / READY | Solid blue | Waiting for input |
| Calibration A | Yellow | Hold hand **open** |
| Calibration B | Red | Hold hand **closed (fist)** |
| Calibration C | Purple | Hold hand **steady** (IMU offset) |
| Inferencing | Yellow | Recording 3-second window |
| Success | Solid green (0.5 s) | Command accepted & sent |
| Low confidence / error | Red blink (2×) | < 75 % confidence or send failed |
| Low battery | Continuous red blink | Battery below threshold |

---

## ML Pipeline

- **Tooling:** Edge Impulse Studio (impulse design, training, deployment as Arduino library)
- **Window:** ~3 s, 13 features per timestep (5 flex + 2 Kalman angles + 6 IMU)
- **Model:** Dense neural network, int8 quantized (TFLite Micro)
- **On-device runtime:** Edge Impulse Arduino library (`ILTER-AKKE_1.3.3_inferencing.h`)
- **Confidence gating:** predictions below 75 % are discarded
- **Calibration:** per-session normalization for flex sensors + IMU offset estimation (no model retraining needed between users)

> **Note on the feature design:** the firmware feeds raw windowed time-series features directly to Edge Impulse, which performs its own DSP block (spectral analysis on the IMU channels). This replaces the earlier statistical-features approach (88 features = 11 channels × 8 statistics) and lets Edge Impulse choose the optimal extractor.

---

## Communication: ESP-NOW

The two ESP32s talk directly over ESP-NOW — no Wi-Fi access point, no router, no internet. The glove knows the receiver's MAC at compile time (`receiverMAC[]` in `inference.ino`) and sends a 1-byte payload:

```c
typedef struct {
  uint8_t fileNumber;  // 1..10
} SendData;
```

Round-trip latency from gesture-end to audio playback is typically under 300 ms.

> 🔧 **Setting up the link:** flash `reciever.ino` first, open the serial monitor, and copy the printed MAC address into `receiverMAC[]` in `inference.ino` before flashing the glove.

---

## Repository Layout

```
.
├── README.md                  # this file (English)
├── README.tr.md               # Türkçe sürüm
├── .gitignore
│
├── data_collection/                       # ── ML data pipeline ──
│   ├── data_collection.ino                # firmware variant that streams raw samples over serial
│   ├── data_collector.py                  # reads serial, writes labeled rows into dataset.csv
│   ├── count_labels.py                    # quick sanity check: samples per gesture
│   ├── split_for_edge_impulse.py          # converts dataset.csv into Edge Impulse-ready files
│   ├── dataset.csv                        # master dataset (one row per sample)
│   └── edge_impulse_data/                 # per-gesture files for Edge Impulse upload
│
├── ESP32-Libraries/                       # ── vendored Arduino libraries ──
│   ├── KalmanFilter-master/               # unpacked Kalman library (TKJ Electronics)
│   ├── Kalman.zip                         # original archive
│   └── ei-ilter-akke_1.3.3-arduino-1.0.5-impulse.zip   # exported Edge Impulse Arduino lib
│
├── inference/
│   └── inference.ino                      # glove-side firmware (ESP32-S3)
│
└── reciever/
    └── reciever.ino                       # receiver-side firmware (ESP32 + DFPlayer)
```

### What each script does

| File | Purpose |
|---|---|
| `data_collection.ino` | Firmware mode that performs calibration, then streams windowed samples over USB-Serial with a gesture label |
| `data_collector.py` | Host-side serial reader: prompts for the gesture label, appends rows to `dataset.csv` |
| `count_labels.py` | Prints how many recordings exist per gesture — useful to keep the dataset balanced |
| `split_for_edge_impulse.py` | Reshapes `dataset.csv` into the per-window file format Edge Impulse Studio expects on upload |

---

## Build & Flash

### Prerequisites
- Arduino IDE 2.x **or** PlatformIO
- ESP32 board package (Espressif Arduino core ≥ 3.0 for ESP-NOW v5 API)
- Arduino libraries:
  - `DFRobotDFPlayerMini` (install via Library Manager)
  - `LiquidCrystal_I2C` (install via Library Manager)
  - `Kalman` — bundled in `ESP32-Libraries/Kalman.zip` (Sketch → Include Library → Add .ZIP Library)
  - Edge Impulse model — bundled in `ESP32-Libraries/ei-ilter-akke_1.3.3-arduino-1.0.5-impulse.zip` (same: Add .ZIP Library)

> All non-trivial libraries the project depends on are vendored under `ESP32-Libraries/` so the build is reproducible without hunting the right version online.

### Steps
1. **Prepare the SD card:** create a `mp3` folder on the DFPlayer's microSD and add `0001.mp3` … `0010.mp3` corresponding to the 10 commands.
2. **Flash the receiver** (`reciever.ino`) on the receiver ESP32 first.
3. **Read its MAC address** from the serial monitor at 115200 baud and paste it into `receiverMAC[]` in `inference.ino`.
4. **Flash the glove** (`inference.ino`) on the ESP32-S3.
5. **Power on both units**, wait for the boot sequence, then press the button on the glove to start calibration.

---

## Data Collection Workflow

The model that ships in `inference.ino` was trained on data collected with the scripts in `data_collection/`. To extend the gesture set or retrain on a new wearer:

1. **Flash the data-collection firmware:** open `data_collection/data_collection.ino`, flash it to the glove ESP32-S3.
2. **Run the host collector:** plug the glove into a computer and run
   ```bash
   python data_collection/data_collector.py
   ```
   The script will prompt for a gesture label, then save every recorded window into `dataset.csv` with that label.
3. **Repeat per gesture** until you have ~50 samples per class (run `count_labels.py` to check balance).
4. **Prepare the dataset for Edge Impulse:**
   ```bash
   python data_collection/split_for_edge_impulse.py
   ```
   This emits per-window files into `edge_impulse_data/`.
5. **Upload to Edge Impulse Studio**, design the impulse, train, then **Deployment → Arduino Library**.
6. **Replace the bundled ZIP** in `ESP32-Libraries/` and re-flash `inference.ino`.

---

## Usage

1. Power on the glove → solid blue LED (IDLE).
2. **Press the button** to enter calibration:
   - Hold hand fully **open** until LED turns from yellow to red (~2.5 s).
   - Hold hand fully **closed** (fist) until LED turns from red to purple (~2.5 s).
   - Hold hand **steady** until LED returns to blue (~2.5 s).
3. The glove is now READY (solid blue).
4. **Press the button** to record a gesture (~3 s, yellow LED).
5. If confidence ≥ 75 %, green LED flashes and the audio plays on the receiver. Otherwise, the LED flashes red twice.
6. Repeat from step 4 for the next gesture.

---

## Known Issues / TODO

- Receiver firmware has non-UTF-8 byte sequences in `Serial.println` calls (originally emoji); these print as garbage. Strip or re-encode.
- `getBatteryPercentage()` is used inside `updateLCD()` before its declaration — works because of Arduino's auto-prototype generation but should be forward-declared explicitly for portability.
- `BUTTON_PIN = 13` and `BATTERY_PIN = 39` can both be sensitive on ESP32-S3; verify pin map against your specific dev board revision.
- Battery percentage curve is linearized in 5 % steps; consider a non-linear LUT mapped to actual Li-Po discharge curve.
- `checkBattery()` on the glove is currently commented out in `loop()`.

---

## Roadmap

- [ ] Expand gesture set from 10 → 15
- [ ] Add encryption layer on top of ESP-NOW (AES via `esp_now_set_pmk`)
- [ ] Migrate enclosure to a fully flexible PCB for the glove
- [ ] Add haptic feedback (small vibration motor) for command-sent confirmation
- [ ] Multi-receiver broadcast (squad mode)

---

## Acknowledgments

- **HAVELSAN SUIT Program** — for mentorship and technical support
- **TED University Faculty of Engineering** — capstone supervision and lab facilities
- Edge Impulse, Espressif, and the open-source embedded community

---

## License

To be defined. See `LICENSE` (pending).

---

<sub>AKKE © 2025–2026 — CMPE 491/492 Capstone Project, TED University</sub>
