# XToys ESP32-C3 WebSocket Controller

A multi-mode WebSocket controller for ESP32-C3 Super Mini that interfaces with the [XToys](https://xtoys.app) web application to control various devices via DRV8833 motor driver and optocouplers.

## Features

- **5 Operating Modes**: Basic Vibrator, Dual Vibrator, Stroker, Milking Machine, Edge-O-Matic
- **DRV8833 Motor Driver**: Dual H-bridge for bidirectional motor control
- **3 Optocoupler Outputs**: Button simulation for external device control
- **WiFi Captive Portal**: Easy network configuration
- **RGB LED Status**: Visual feedback for connection state and mode
- **Persistent Settings**: Mode and WiFi saved to flash

## Hardware

| Component | Purpose |
|-----------|---------|
| ESP32-C3 Super Mini | Main controller |
| DRV8833 | Dual motor driver (1.5A/channel) |
| PC817 Optocouplers (x3) | Button simulation |
| 220Ω Resistors (x3) | Optocoupler current limiting |

## Wiring

```
ESP32-C3          DRV8833
────────          ───────
GPIO2  ────────►  AIN1
GPIO3  ────────►  AIN2
GPIO4  ────────►  BIN1
GPIO5  ────────►  BIN2
5V     ────────►  VCC
GND    ────────►  GND
               VM ◄── Motor power (3-10V)

Optocouplers:
GPIO6  ──[220Ω]──► Opto 1
GPIO7  ──[220Ω]──► Opto 2
GPIO10 ──[220Ω]──► Opto 3
```

## Modes

| # | Mode | LED Color | Description |
|---|------|-----------|-------------|
| 1 | Basic Vibrator | Pink | Single motor intensity |
| 2 | Dual Vibrator | Purple | Two independent motors |
| 3 | Stroker | Cyan | Bidirectional position/speed |
| 4 | Milking Machine | Yellow | Rotation + pulsing suction |
| 5 | Edge-O-Matic | Orange | Motor + optocoupler triggers |

## Button Controls

- **Short press**: Cycle modes
- **Double press**: Show current mode (blinks)
- **Long press (3s)**: Reset WiFi, enter config mode

## LED Status

- **Red pulse**: No WiFi connection
- **Yellow solid**: WiFi OK, waiting for client
- **Green solid**: Client connected
- **Purple pulse**: Config mode

## WebSocket Commands

Connect to `ws://[device-ip]:81`

```json
{"intensity": 75}
{"intensity1": 50, "intensity2": 80}
{"position": 100, "speed": 60}
{"speed": 70, "suction": 50}
{"intensity": 80, "trigger1": true}
{"pulse1": 100}
{"cmd": "stop"}
```

Simple format: `75`, `a:50`, `b:80`, `t1:1`

## Required Libraries

Install via Arduino IDE Library Manager:
- `WebSockets` by Markus Sattler
- `ArduinoJson` by Benoit Blanchon
- `Adafruit NeoPixel`

## Setup

1. Flash the firmware to ESP32-C3
2. Connect to WiFi: `XToys-ESP32-Setup`
3. Configure at `192.168.4.1`
4. Connect XToys to `ws://[device-ip]:81`

## Documentation

See `XToys_ESP32_Documentation.html` for detailed wiring diagrams, flowcharts, and mode explanations.

## License

MIT
