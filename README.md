# XToys ESP32-C3 WebSocket Controller v2

A multi-mode WebSocket controller for ESP32-C3 Super Mini that interfaces with [XToys](https://xtoys.app) to control various pleasure devices via DRV8833 motor driver and optocouplers.

## Modes

| # | Mode | LED Color | Motor A | Motor B | Optocouplers |
|---|------|-----------|---------|---------|--------------|
| 1 | **Vibe & Suck** | Magenta | Vibration intensity | Suction pump | - |
| 2 | **Full Machine** | Orange | Thrust speed | Rotation speed | Vibe/Suck/Power buttons |
| 3 | **Handjob Sim** | Cyan | Oscillating stroke | Vibration | - |
| 4 | **Blowjob Sim** | Hot Pink | Pulsing suction | Tongue/movement | - |
| 5 | **Tease Mode** | Purple | Auto vibe (edging) | Auto suction | - |

## Hardware

```
ESP32-C3 Super Mini
├── DRV8833 Motor Driver
│   ├── Motor A (GPIO2/3) - Primary motor
│   └── Motor B (GPIO4/5) - Secondary motor
└── Optocouplers (for external device button simulation)
    ├── Vibe Button (GPIO6)
    ├── Suck Button (GPIO7)
    └── Power Button (GPIO10)
```

## Wiring

```
ESP32-C3          DRV8833              MOTORS
────────          ───────              ──────
GPIO2  ────────►  AIN1  ─────────────► Motor A+
GPIO3  ────────►  AIN2  ─────────────► Motor A-
GPIO4  ────────►  BIN1  ─────────────► Motor B+
GPIO5  ────────►  BIN2  ─────────────► Motor B-
5V     ────────►  VCC
GND    ────────►  GND
                  VM ◄──────────────── Motor Power (3-10V)

Optocouplers (with 220Ω resistors):
GPIO6  ──[220Ω]──► Opto 1 (Vibe button)
GPIO7  ──[220Ω]──► Opto 2 (Suck button)
GPIO10 ──[220Ω]──► Opto 3 (Power button)
```

## Button Controls

- **Short press** - Cycle through modes
- **Double press** - Show current mode (color + blinks)
- **Long press (3s)** - Reset WiFi, enter config mode

## LED Status

| Color | State |
|-------|-------|
| Red pulse | No WiFi connection |
| Yellow solid | WiFi OK, waiting for XToys |
| Green solid | Client connected |
| Purple pulse | Config mode (AP active) |
| Mode color | Shown briefly on mode change |

## WebSocket Commands

Connect to `ws://[device-ip]:81`

### Mode 1: Vibe & Suck
```json
{"vibe": 75, "suck": 50}
{"intensity": 80}           // Both channels
```

### Mode 2: Full Machine
```json
{"thrust": 70, "rotation": 50}
{"vibeBtn": true}           // Hold vibe button
{"vibeBtn": 100}            // Pulse 100ms
{"suckBtn": true, "powerBtn": 150}
```

### Mode 3: Handjob Sim
```json
{"stroke": 80, "vibe": 60}
{"speed": 90}               // Stroke speed alias
```

### Mode 4: Blowjob Sim
```json
{"suction": 70, "rate": 80, "speed": 50}
{"depth": 90}               // Suction depth alias
```

### Mode 5: Tease Mode
```json
{"start": true}             // Begin auto-edging
{"active": false}           // Stop
{"intensity": 85}           // Set target intensity
```

### Simple Format
```
vibe:75
suck:50
stroke:80
tease:1
```

## Tease Mode Details

Automated edging with 4 phases:
1. **Build** (3-8s) - Gradually increases to random target (60-95%)
2. **Hold** (2-5s) - Maintains peak with random vibe bursts
3. **Drop** (0.5-2s) - Quick decrease
4. **Rest** (1-4s) - Low random stimulation

LED pulses green with intensity, adds blue at >80%.

## Setup

1. Flash firmware to ESP32-C3
2. Connect to WiFi `XToys-ESP32-Setup`
3. Open `192.168.4.1` in browser
4. Select network and default mode
5. Connect XToys to `ws://[device-ip]:81`

## Required Libraries

- `WebSockets` by Markus Sattler
- `ArduinoJson` by Benoit Blanchon
- `Adafruit NeoPixel`

## License

MIT
