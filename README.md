# XToys ESP32-C3 WebSocket Receiver

Simple WebSocket receiver for [XToys](https://xtoys.app). All logic is handled server-side - this just receives commands and sets outputs.

## Outputs

- **Motor A** (GPIO2/3) - DRV8833 channel A, bidirectional, -100 to +100
- **Motor B** (GPIO4/5) - DRV8833 channel B, bidirectional, -100 to +100
- **Opto 1** (GPIO6) - Optocoupler, on/off
- **Opto 2** (GPIO7) - Optocoupler, on/off
- **Opto 3** (GPIO10) - Optocoupler, on/off

## Modes

Modes just define what the outputs represent (reported to XToys):

| # | Mode | Motor A | Motor B |
|---|------|---------|---------|
| 1 | Vibe + Suck | vibration | suction |
| 2 | Thrust + Rotate | thrust | rotation |
| 3 | Thrust + Vibe | thrust | vibration |
| 4 | Suck + Speed | suction | speed |
| 5 | Dual Vibe | vibe1 | vibe2 |

## Commands

Connect to `ws://[device-ip]:81`

### JSON Format
```json
{"a": 75, "b": 50}
{"motorA": 100, "motorB": -50}
{"vibe": 80, "suck": 60}
{"thrust": 70, "rotate": 40}
{"o1": true, "o2": false}
{"stop": true}
```

### Simple Format
```
a:75
b:-50
o1:1
o2:0
stop:1
```

## Device Info (sent on connect)
```json
{
  "type": "deviceInfo",
  "mode": "Vibe + Suck",
  "modeId": 0,
  "motorA": "vibe",
  "motorB": "suck",
  "optos": 3
}
```

## Wiring

```
ESP32-C3          DRV8833
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

## Controls

- **Short press** - Cycle mode
- **Double press** - Show mode (blinks)
- **Long press (3s)** - Reset WiFi

## LED

| Color | State |
|-------|-------|
| Red pulse | No WiFi |
| Yellow | WiFi OK, no client |
| Green | Connected |
| Purple pulse | Config mode |

## Setup

1. Flash to ESP32-C3
2. Connect to `XToys-ESP32-Setup` WiFi
3. Open `192.168.4.1`
4. Configure WiFi and mode
5. Connect XToys to `ws://[ip]:81`

## Libraries

- `WebSockets` by Markus Sattler
- `ArduinoJson` by Benoit Blanchon
- `Adafruit NeoPixel`
