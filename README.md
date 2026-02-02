# XToys ESP32-C3 WebSocket Receiver

A compact WebSocket receiver that connects your DIY hardware to [XToys.app](https://xtoys.app) - the popular browser-based teledildonics platform.

**This device is a receiver only.** All the fun stuff (patterns, syncing, games, remote control) happens in XToys. This firmware just listens and actuates.

---

## What You Get

| Output | Pins | Range | Use For |
|--------|------|-------|---------|
| **Motor A** | GPIO 2, 3 | -100 to +100 | Vibration, thrust, rotation |
| **Motor B** | GPIO 4, 5 | -100 to +100 | Secondary motor, suction pump |
| **Opto 1** | GPIO 6 | On / Off | Button simulation |
| **Opto 2** | GPIO 7 | On / Off | Button simulation |
| **Opto 3** | GPIO 10 | On / Off | Button simulation |

Motors are bidirectional via DRV8833. Negative values = reverse direction.

---

## Hardware

```
┌─────────────────────────────────────────────────────────────┐
│                     ESP32-C3 Super Mini                      │
│                                                              │
│   Built-in: RGB LED (GPIO8), Boot Button (GPIO9)            │
│                                                              │
│   ┌─────────────┐     ┌─────────────┐     ┌─────────────┐   │
│   │  DRV8833    │     │ Optocouplers│     │   Motors    │   │
│   │  Motor      │     │ (PC817 x3)  │     │   3-10V     │   │
│   │  Driver     │     │             │     │             │   │
│   └─────────────┘     └─────────────┘     └─────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

### Wiring

```
ESP32-C3              DRV8833                 MOTORS
────────              ───────                 ──────
GPIO2  ─────────────► AIN1 ────────────────► Motor A (+)
GPIO3  ─────────────► AIN2 ────────────────► Motor A (-)
GPIO4  ─────────────► BIN1 ────────────────► Motor B (+)
GPIO5  ─────────────► BIN2 ────────────────► Motor B (-)
5V     ─────────────► VCC
GND    ─────────────► GND
                      VM ◄───────────────── 3-10V Motor Supply

OPTOCOUPLERS (active LOW, accent 220Ω resistor on LED side)
─────────────────────────────────────────────────────────────
GPIO6  ───[220Ω]───► Opto 1 LED+ ───► GND
GPIO7  ───[220Ω]───► Opto 2 LED+ ───► GND
GPIO10 ───[220Ω]───► Opto 3 LED+ ───► GND

Opto collector/emitter → wired across target device buttons
```

---

## Modes

Modes define what your motors are used for. XToys sees this info and can label controls accordingly.

| # | Mode | Motor A | Motor B | LED Color |
|---|------|---------|---------|-----------|
| 1 | **Vibe + Suck** | Vibration | Suction | Magenta |
| 2 | **Thrust + Rotate** | Thrust | Rotation | Orange |
| 3 | **Thrust + Vibe** | Thrust | Vibration | Cyan |
| 4 | **Suck + Speed** | Suction | Speed | Pink |
| 5 | **Dual Vibe** | Vibe 1 | Vibe 2 | Green |

**Short press** the boot button to cycle modes. The LED blinks to confirm.

---

## LED Status

| LED | Meaning |
|-----|---------|
| 🔴 Red pulse | No WiFi connection |
| 🟡 Yellow solid | WiFi connected, waiting for XToys |
| 🟢 Green solid | XToys connected - ready to go |
| 🟣 Purple pulse | WiFi setup mode (access point active) |
| 🟠 Orange flash | Long press detected (release to reset) |

---

## WebSocket Protocol

Connect XToys to: `ws://[device-ip]:81`

### On Connect
Device sends its configuration:
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

### Commands

**JSON format:**
```json
{"a": 75, "b": 50}              // Motor A at 75%, Motor B at 50%
{"a": -100, "b": 100}           // Motor A full reverse, B full forward
{"vibe": 80, "suck": 60}        // Using semantic names
{"thrust": 70, "rotate": 40}    // Different mode names work too
{"o1": true, "o2": false}       // Optocoupler control
{"opto1": true, "opto3": true}  // Alternate opto names
{"stop": true}                  // Emergency stop - kills everything
```

**Simple format:**
```
a:75
b:-50
o1:1
o2:0
stop:1
```

### Accepted Keys

| Motor A | Motor B | Optos |
|---------|---------|-------|
| `a`, `motorA` | `b`, `motorB` | `o1`, `opto1` |
| `vibe`, `vibe1` | `vibe2` | `o2`, `opto2` |
| `thrust` | `rotate` | `o3`, `opto3` |
| `suck` (mode 4) | `suck` (other modes) | |
| | `speed` | |

---

## First-Time Setup

1. **Flash** the firmware to your ESP32-C3
2. **Power on** - LED pulses purple
3. **Connect** your phone/laptop to WiFi: `XToys-ESP32-Setup`
4. **Open** browser to `192.168.4.1` (or wait for captive portal)
5. **Select** your WiFi network and pick a default mode
6. **Save** - device restarts and connects
7. **Note** the IP address (check serial monitor or router)
8. **Add** in XToys as a WebSocket device: `ws://[ip]:81`

---

## Button Controls

| Action | Duration | Result |
|--------|----------|--------|
| Short press | < 400ms | Cycle to next mode |
| Double press | 2 taps | Show current mode (LED blinks) |
| Long press | 3+ seconds | Factory reset WiFi settings |

---

## Required Libraries

Install via Arduino IDE Library Manager:

- **WebSockets** by Markus Sattler
- **ArduinoJson** by Benoit Blanchon
- **Adafruit NeoPixel**

---

## Arduino IDE Settings

- **Board:** ESP32C3 Dev Module
- **USB CDC On Boot:** Enabled
- **Flash Size:** 4MB
- **Partition Scheme:** Default 4MB with spiffs

---

## Serial Debug

Connect at **115200 baud** to see:
- WiFi connection status
- Client connect/disconnect events
- Every command received
- Mode changes

---

## Safety Notes

- Motors stop automatically when XToys disconnects
- Long press button to reset if WiFi credentials are wrong
- DRV8833 has built-in thermal protection
- Keep motor voltage within DRV8833 specs (3-10V)

---

## License

MIT - Do whatever you want with it.

---

*Built for the XToys community* 🎮
