---
title: "XToys ESP32-C3 WebSocket Receiver"
subtitle: "Complete Hardware & Software Documentation"
author: "Tuna-Toys"
date: "2025"
geometry: margin=2.5cm
fontsize: 11pt
colorlinks: true
linkcolor: magenta
urlcolor: blue
header-includes:
  - \usepackage{fancyhdr}
  - \pagestyle{fancy}
  - \fancyhead[L]{XToys ESP32-C3}
  - \fancyhead[R]{\thepage}
  - \fancyfoot[C]{}
---

\newpage

# Introduction

The **XToys ESP32-C3 WebSocket Receiver** is a compact, WiFi-enabled controller designed to bridge the gap between the popular XToys.app platform and your custom DIY hardware projects.

## What This Device Does

- Connects to your WiFi network
- Hosts a WebSocket server on port 81
- Receives commands from XToys
- Controls two bidirectional motor channels via DRV8833
- Controls three optocoupler outputs for button simulation
- Provides visual feedback via built-in RGB LED

## What This Device Does NOT Do

- Generate patterns or sequences (XToys handles this)
- Store usage data
- Connect to the internet (local network only)
- Require any cloud services

All the intelligence lives in XToys. This device simply listens and responds.

\newpage

# Hardware Overview

## Components Required

| Component | Quantity | Purpose |
|-----------|----------|---------|
| ESP32-C3 Super Mini | 1 | Main controller |
| DRV8833 Motor Driver | 1 | Dual H-bridge for motors |
| PC817 Optocoupler | 3 | Button simulation |
| 220Ω Resistor | 3 | Current limiting for optos |
| DC Motors | 1-2 | Vibration, thrust, rotation |
| Power Supply | 1 | 5V for ESP32, 3-10V for motors |

## ESP32-C3 Super Mini Specifications

- **Processor:** 32-bit RISC-V, 160MHz
- **Memory:** 400KB SRAM, 4MB Flash
- **Wireless:** WiFi 802.11 b/g/n, Bluetooth 5.0 LE
- **GPIO:** 11 usable pins
- **Built-in:** RGB LED (GPIO8), Boot button (GPIO9)
- **Power:** 5V via USB-C
- **Size:** 22.5mm x 18mm

## Pin Assignments

| GPIO | Function | Notes |
|------|----------|-------|
| 2 | Motor A IN1 | PWM output |
| 3 | Motor A IN2 | PWM output |
| 4 | Motor B IN1 | PWM output |
| 5 | Motor B IN2 | PWM output |
| 6 | Optocoupler 1 | Active LOW |
| 7 | Optocoupler 2 | Active LOW |
| 8 | WS2812 RGB LED | Built-in |
| 9 | Boot Button | Built-in, Active LOW |
| 10 | Optocoupler 3 | Active LOW |

\newpage

# Wiring Diagrams

## Complete System Wiring

```
                                POWER
                    ┌────────────────────────────┐
                    │                            │
                5V USB-C                    3-10V DC
                    │                            │
                    ▼                            ▼
    ┌───────────────────────────────┐    ┌──────────────┐
    │      ESP32-C3 Super Mini      │    │   DRV8833    │
    │                               │    │              │
    │  GPIO2 ────────────────────────────► AIN1        │
    │  GPIO3 ────────────────────────────► AIN2        │
    │  GPIO4 ────────────────────────────► BIN1        │
    │  GPIO5 ────────────────────────────► BIN2        │
    │  5V    ────────────────────────────► VCC         │
    │  GND   ──────┬─────────────────────► GND         │
    │              │                 │    │       VM◄──┼── Motor Power
    │              │                 │    │            │
    │  GPIO8 ► RGB LED (built-in)   │    │  AOUT1 ────┼──► Motor A
    │  GPIO9 ► Button (built-in)    │    │  AOUT2 ────┼──►
    │              │                 │    │            │
    │  GPIO6 ──────┼── [220Ω] ──► Opto 1  │  BOUT1 ────┼──► Motor B
    │  GPIO7 ──────┼── [220Ω] ──► Opto 2  │  BOUT2 ────┼──►
    │  GPIO10 ─────┼── [220Ω] ──► Opto 3  │            │
    │              │                 │    └──────────────┘
    └───────────────────────────────┘
                   │
                  GND
```

## DRV8833 Motor Control

The DRV8833 is a dual H-bridge motor driver capable of controlling two DC motors bidirectionally.

**Control Logic:**

| IN1 | IN2 | Motor Action |
|-----|-----|--------------|
| PWM | LOW | Forward at PWM speed |
| LOW | PWM | Reverse at PWM speed |
| LOW | LOW | Coast (free spin) |
| HIGH | HIGH | Brake (fast stop) |

**PWM Configuration:**

- Frequency: 20kHz (above audible range)
- Resolution: 8-bit (256 levels)
- Speed range: -100 to +100 mapped to 0-255 PWM

## Optocoupler Wiring

Optocouplers provide electrical isolation between the ESP32 and external devices.

```
    ESP32                PC817              Target Device
    ─────                ─────              ─────────────
                      ┌────────┐
   GPIO ──[220Ω]──────┤ LED    │
                      │   ↓    │
   GND ───────────────┤ LED    │
                      │        │
                      │ Photo- ├──────────► Button +
                      │ trans  │
                      │   ↓    ├──────────► Button -
                      └────────┘
```

When GPIO goes LOW, the LED illuminates, the phototransistor conducts, simulating a button press on the target device.

\newpage

# Software Configuration

## Operating Modes

The device supports five modes. Modes define what each motor output represents - this information is sent to XToys on connection.

| Mode | Motor A Function | Motor B Function | LED Color |
|------|------------------|------------------|-----------|
| 1 - Vibe + Suck | Vibration | Suction | Magenta |
| 2 - Thrust + Rotate | Thrust | Rotation | Orange |
| 3 - Thrust + Vibe | Thrust | Vibration | Cyan |
| 4 - Suck + Speed | Suction | Speed | Pink |
| 5 - Dual Vibe | Vibration 1 | Vibration 2 | Green |

**Changing Modes:**

- Short press the boot button to cycle through modes
- Mode is saved to flash memory and persists after power off
- Connected XToys clients are notified of mode changes

## LED Status Indicators

| LED State | Meaning |
|-----------|---------|
| Red pulsing | Not connected to WiFi |
| Yellow solid | WiFi connected, waiting for XToys |
| Green solid | XToys client connected |
| Purple pulsing | WiFi configuration mode |
| Orange flashing | Long press detected (warning) |
| Mode color | Shown briefly when mode changes |

## Button Functions

| Action | Timing | Result |
|--------|--------|--------|
| Short press | < 400ms | Cycle to next mode |
| Double press | 2 taps within 400ms | Blink current mode number |
| Long press | Hold 3+ seconds | Clear WiFi settings, restart |

\newpage

# WebSocket Protocol

## Connection

XToys connects via WebSocket to: `ws://[device-ip]:81`

## Device Information

On connection, the device sends its current configuration:

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

## Command Format

### JSON Commands

```json
{"a": 75, "b": 50}           // Set Motor A to 75, Motor B to 50
{"motorA": -100}             // Motor A full reverse
{"vibe": 80, "suck": 60}     // Using function names
{"o1": true, "o2": false}    // Optocoupler control
{"stop": true}               // Emergency stop
```

### Simple Text Commands

```
a:75          Motor A to 75%
b:-50         Motor B to -50% (reverse)
o1:1          Optocoupler 1 on
o2:0          Optocoupler 2 off
stop:1        Stop all outputs
```

## Accepted Parameter Names

**Motor A:**
`a`, `motorA`, `vibe`, `vibe1`, `thrust`, `suck` (in mode 4 only)

**Motor B:**
`b`, `motorB`, `vibe2`, `rotate`, `speed`, `suck` (in modes 1-3, 5)

**Optocouplers:**
`o1`/`opto1`, `o2`/`opto2`, `o3`/`opto3`

## Value Ranges

| Output | Range | Notes |
|--------|-------|-------|
| Motors | -100 to +100 | Negative = reverse direction |
| Optocouplers | true/false or 1/0 | Binary on/off |

\newpage

# Setup Guide

## Step 1: Install Arduino IDE Libraries

Open Arduino IDE Library Manager and install:

- **WebSockets** by Markus Sattler
- **ArduinoJson** by Benoit Blanchon
- **Adafruit NeoPixel**

## Step 2: Configure Arduino IDE

- **Board:** ESP32C3 Dev Module
- **USB CDC On Boot:** Enabled
- **Flash Size:** 4MB (32Mb)
- **Partition Scheme:** Default 4MB with spiffs

## Step 3: Flash the Firmware

1. Connect ESP32-C3 via USB-C
2. Select correct COM port
3. Click Upload
4. Wait for completion

## Step 4: WiFi Configuration

1. Power on the device (LED pulses purple)
2. Connect phone/laptop to WiFi: **XToys-ESP32-Setup**
3. Browser should open automatically (or go to 192.168.4.1)
4. Select your WiFi network from the list
5. Enter password
6. Choose default mode
7. Click Save

Device restarts and connects to your network.

## Step 5: Find Device IP

Options:
- Check your router's DHCP client list
- Open Arduino Serial Monitor at 115200 baud
- Use a network scanner app

## Step 6: Connect XToys

1. Open XToys.app
2. Add new WebSocket device
3. Enter: `ws://[your-device-ip]:81`
4. Connect and enjoy!

\newpage

# Troubleshooting

## LED stays red (pulsing)

**Problem:** Cannot connect to WiFi

**Solutions:**
- Verify SSID and password are correct
- Move device closer to router
- Long press button (3s) to reset WiFi settings
- Check if router is using 2.4GHz (5GHz not supported)

## LED is yellow but XToys won't connect

**Problem:** WebSocket connection failing

**Solutions:**
- Verify IP address is correct
- Ensure phone/computer is on same network
- Check firewall isn't blocking port 81
- Try refreshing XToys connection

## Motors not responding

**Problem:** Commands received but no movement

**Solutions:**
- Check DRV8833 wiring
- Verify motor power supply is connected (VM pin)
- Check motor voltage is within 3-10V range
- Test with simple command: `{"a": 100}`

## Device keeps resetting

**Problem:** Unstable power

**Solutions:**
- Use quality USB power supply (5V 1A minimum)
- Add 100µF capacitor across motor power supply
- Separate ESP32 power from motor power if needed

## Optocouplers not triggering

**Problem:** Target device buttons not activating

**Solutions:**
- Verify correct mode is selected
- Check 220Ω resistors are in place
- Confirm opto polarity (LED direction)
- Test target device button continuity

\newpage

# Technical Specifications

## Electrical

| Parameter | Value |
|-----------|-------|
| ESP32 Supply Voltage | 5V DC (USB-C) |
| Motor Supply Voltage | 3-10V DC |
| Motor Current (per channel) | 1.5A continuous, 2A peak |
| Optocoupler LED Current | ~10mA |
| Total Power Consumption | ~500mA typical |

## Timing

| Parameter | Value |
|-----------|-------|
| PWM Frequency | 20kHz |
| PWM Resolution | 8-bit (256 steps) |
| WebSocket Port | 81 |
| Button Debounce | 50ms |
| Long Press Threshold | 3000ms |
| Double Click Window | 400ms |

## Network

| Parameter | Value |
|-----------|-------|
| WiFi Standard | 802.11 b/g/n |
| WiFi Band | 2.4GHz only |
| Config AP SSID | XToys-ESP32-Setup |
| Config AP IP | 192.168.4.1 |
| Serial Baud Rate | 115200 |

\newpage

# Safety Information

## General Safety

- This device is intended for adult use only
- Do not use with water-based applications without proper waterproofing
- Disconnect power before making wiring changes
- Do not exceed motor voltage ratings

## Electrical Safety

- Use appropriate power supplies with overcurrent protection
- DRV8833 includes thermal shutdown protection
- Motors automatically stop when XToys disconnects
- Long press button provides emergency WiFi reset

## Thermal Considerations

- DRV8833 may get warm during continuous high-power operation
- Provide adequate ventilation
- Consider heatsink for sustained >1A operation

\newpage

# Appendix: Code Overview

## File Structure

```
xtoys_esp32_c3.ino    Main firmware (~520 lines)
```

## Key Functions

| Function | Purpose |
|----------|---------|
| `setup()` | Initialize hardware, load settings, connect WiFi |
| `loop()` | Handle button, WebSocket, LED updates |
| `setMotorA(speed)` | Set Motor A speed (-100 to +100) |
| `setMotorB(speed)` | Set Motor B speed (-100 to +100) |
| `setOpto(ch, state)` | Set optocoupler on/off |
| `handleCommand(payload)` | Parse and execute WebSocket commands |
| `sendDeviceInfo(num)` | Send mode info to connected client |
| `connectWiFi()` | Connect to saved WiFi network |
| `startConfigMode()` | Start WiFi configuration access point |

## Memory Usage

- Flash: ~300KB (of 4MB)
- RAM: ~50KB (of 400KB)
- Preferences: ~1KB (WiFi credentials, mode)

---

*Documentation Version 1.0*

*For support and updates, visit the GitHub repository.*
