---
title: "XToys ESP32-C3"
subtitle: "WebSocket Receiver Documentation"
author: "Tuna-Toys"
date: "2025"
geometry: margin=2cm
fontsize: 11pt
documentclass: article
colorlinks: true
linkcolor: magenta
urlcolor: cyan
toccolor: magenta
header-includes:
  - \usepackage{xcolor}
  - \usepackage{pagecolor}
  - \usepackage{fancyhdr}
  - \usepackage{sectsty}
  - \usepackage{tikz}
  - \usepackage{tcolorbox}
  - \usepackage{enumitem}
  - \usepackage{booktabs}
  - \usepackage{colortbl}
  - \usepackage{array}
  - \usepackage{fancyvrb}
  - \tcbuselibrary{skins,breakable}
  - \definecolor{darkbg}{HTML}{1a1a2e}
  - \definecolor{cardbg}{HTML}{252545}
  - \definecolor{codebg}{HTML}{0d0d1a}
  - \definecolor{accent}{HTML}{ff6b9d}
  - \definecolor{accent2}{HTML}{00d4ff}
  - \definecolor{textmain}{HTML}{e8e8e8}
  - \definecolor{textsub}{HTML}{a0a0b0}
  - \definecolor{warning}{HTML}{ff8800}
  - \pagecolor{darkbg}
  - \color{textmain}
  - \allsectionsfont{\color{accent}}
  - \subsectionfont{\color{accent2}}
  - \pagestyle{fancy}
  - \fancyhf{}
  - \fancyhead[L]{\color{textsub}\small XToys ESP32-C3}
  - \fancyhead[R]{\color{textsub}\small \thepage}
  - \renewcommand{\headrulewidth}{0.5pt}
  - \renewcommand{\headrule}{\hbox to\headwidth{\color{accent}\leaders\hrule height \headrulewidth\hfill}}
  - \fancyfoot[C]{\color{textsub}\small tuna-toys}
  - \newtcolorbox{infobox}{colback=cardbg,colframe=accent2,boxrule=1pt,arc=4pt,left=8pt,right=8pt,top=8pt,bottom=8pt,breakable}
  - \arrayrulecolor{accent}
  - \setlist[itemize]{label=\textcolor{accent}{$\bullet$}}
---

\begin{titlepage}
\pagecolor{darkbg}
\begin{center}
\vspace*{2cm}

{\fontsize{50}{60}\selectfont\textcolor{accent}{\textbf{XToys}}}

\vspace{0.5cm}

{\fontsize{30}{36}\selectfont\textcolor{textmain}{ESP32-C3}}

\vspace{0.3cm}

{\Large\textcolor{accent2}{WebSocket Receiver}}

\vspace{2cm}

\begin{tikzpicture}
\draw[accent, line width=2pt] (0,0) circle (2cm);
\draw[accent2, line width=1.5pt] (0,0) circle (1.5cm);
\fill[accent] (0,0) circle (0.3cm);
\foreach \i in {0,45,...,315} {
  \draw[accent, line width=1pt] (\i:1.7cm) -- (\i:2.3cm);
}
\end{tikzpicture}

\vspace{2cm}

{\large\textcolor{textsub}{Complete Hardware \& Software Documentation}}

\vspace{1cm}

{\textcolor{textsub}{v1.0 --- 2025}}

\vspace{\fill}

{\small\textcolor{accent}{github.com/Tuna-Toys/xtoys\_esp32\_c3}}

\end{center}
\end{titlepage}

\newpage
\tableofcontents
\newpage

# Introduction

The **XToys ESP32-C3 WebSocket Receiver** connects your DIY hardware to XToys.app --- the browser-based teledildonics platform.

## What It Does

\begin{infobox}
\textcolor{accent}{$\bullet$} Connects to your WiFi network\\
\textcolor{accent}{$\bullet$} Hosts a WebSocket server (port 81)\\
\textcolor{accent}{$\bullet$} Receives commands from XToys\\
\textcolor{accent}{$\bullet$} Controls 2 bidirectional motors via DRV8833\\
\textcolor{accent}{$\bullet$} Controls 3 optocoupler outputs\\
\textcolor{accent}{$\bullet$} RGB LED status feedback
\end{infobox}

## What It Doesn't Do

This is a **dumb receiver**. All intelligence lives in XToys:

- No pattern generation
- No data storage
- No cloud connection
- No internet required

It listens. It responds. That's it.

\newpage

# Hardware

## Components

| Component | Qty | Purpose |
|:----------|:---:|:--------|
| ESP32-C3 Super Mini | 1 | Controller |
| DRV8833 Motor Driver | 1 | Dual H-bridge |
| PC817 Optocoupler | 3 | Button simulation |
| 220 Ohm Resistor | 3 | LED current limiting |
| DC Motors | 1-2 | Your toys |
| 5V USB Power | 1 | ESP32 power |
| 3-10V DC Supply | 1 | Motor power |

## ESP32-C3 Super Mini

| Spec | Value |
|:-----|:------|
| CPU | 32-bit RISC-V @ 160MHz |
| RAM | 400KB SRAM |
| Flash | 4MB |
| WiFi | 802.11 b/g/n (2.4GHz) |
| Size | 22.5mm x 18mm |
| Power | 5V USB-C |

**Built-in:** RGB LED (GPIO8), Boot Button (GPIO9)

## Pin Assignments

| GPIO | Function | Type |
|:----:|:---------|:-----|
| 2 | Motor A IN1 | PWM |
| 3 | Motor A IN2 | PWM |
| 4 | Motor B IN1 | PWM |
| 5 | Motor B IN2 | PWM |
| 6 | Optocoupler 1 | Digital |
| 7 | Optocoupler 2 | Digital |
| 8 | RGB LED | WS2812 |
| 9 | Boot Button | Input |
| 10 | Optocoupler 3 | Digital |

\newpage

# Wiring

## System Overview

```
+--------------------------------------------------------------+
|                    ESP32-C3 SUPER MINI                       |
|                                                              |
|    USB-C =============================================       |
|         ||                                                   |
|    +----++----+                                              |
|    |  5V IN  |                                               |
|    +----+----+                                               |
|         |                                                    |
|    GPIO2 ----------+                                         |
|    GPIO3 ----------+----------> DRV8833 ------> MOTOR A      |
|    GPIO4 ----------+----------> DRV8833 ------> MOTOR B      |
|    GPIO5 ----------+                 ^                       |
|                                      |                       |
|    GPIO6 ---[220R]---> OPTO 1       3-10V                    |
|    GPIO7 ---[220R]---> OPTO 2       MOTOR                    |
|    GPIO10 --[220R]---> OPTO 3       POWER                    |
|                                                              |
|    GPIO8 > RGB LED (built-in)                                |
|    GPIO9 > BUTTON (built-in)                                 |
|                                                              |
+--------------------------------------------------------------+
```

## DRV8833 Wiring

```
ESP32-C3              DRV8833                    MOTORS
--------              -------                    ------

GPIO2  -------------> AIN1 -+
                            +---> AOUT1/2 ------> Motor A
GPIO3  -------------> AIN2 -+

GPIO4  -------------> BIN1 -+
                            +---> BOUT1/2 ------> Motor B
GPIO5  -------------> BIN2 -+

5V     -------------> VCC
GND    -------------> GND
                      VM <----------------------- 3-10V DC
```

## Motor Control Logic

| IN1 | IN2 | Result |
|:---:|:---:|:-------|
| PWM | LOW | Forward |
| LOW | PWM | Reverse |
| LOW | LOW | Coast |
| HIGH | HIGH | Brake |

**PWM:** 20kHz frequency, 8-bit resolution

\newpage

## Optocoupler Wiring

```
    ESP32                    PC817                  TARGET
    -----                    -----                  ------

                          +--------+
   GPIO ----[220R]--------| LED    |
                          |   |    |
   GND -------------------|  GND   |
                          |        |
                          | Photo- +-------------> Button (+)
                          |  trans |
                          |   |    +-------------> Button (-)
                          +--------+

   GPIO LOW  = LED ON  = Transistor ON  = Button "pressed"
   GPIO HIGH = LED OFF = Transistor OFF = Button "released"
```

Optocouplers provide **electrical isolation** --- your ESP32 stays safe!

\newpage

# Operating Modes

Modes tell XToys what your motors do. Short press the button to cycle.

## Mode 1: Vibe + Suck \textcolor{accent}{(Magenta)}

| Output | Function |
|:-------|:---------|
| Motor A | Vibration intensity |
| Motor B | Suction pump |

## Mode 2: Thrust + Rotate \textcolor{warning}{(Orange)}

| Output | Function |
|:-------|:---------|
| Motor A | Thrust speed |
| Motor B | Rotation speed |

## Mode 3: Thrust + Vibe \textcolor{accent2}{(Cyan)}

| Output | Function |
|:-------|:---------|
| Motor A | Thrust speed |
| Motor B | Vibration |

## Mode 4: Suck + Speed \textcolor{accent}{(Pink)}

| Output | Function |
|:-------|:---------|
| Motor A | Suction |
| Motor B | Movement speed |

## Mode 5: Dual Vibe \textcolor{green}{(Green)}

| Output | Function |
|:-------|:---------|
| Motor A | Vibration 1 |
| Motor B | Vibration 2 |

\newpage

# LED Status

| Color | Pattern | Meaning |
|:------|:--------|:--------|
| Red | Pulsing | No WiFi connection |
| Yellow | Solid | WiFi OK, waiting for XToys |
| Green | Solid | XToys connected! |
| Purple | Pulsing | WiFi config mode |
| Orange | Flashing | Long press warning |
| Various | Brief flash | Mode change indicator |

# Button Controls

| Action | How | Result |
|:-------|:----|:-------|
| Cycle Mode | Short press | Next mode |
| Show Mode | Double tap | LED blinks mode number |
| Factory Reset | Hold 3 sec | Clears WiFi, restarts |

\newpage

# WebSocket Protocol

## Connection

```
ws://[DEVICE-IP]:81
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

## Commands

### JSON Format

```json
{"a": 75, "b": 50}
{"motorA": -100, "motorB": 100}
{"vibe": 80, "suck": 60}
{"thrust": 70, "rotate": 40}
{"o1": true, "o2": false, "o3": true}
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

## Parameter Names

| Motor A | Motor B | Optos |
|:--------|:--------|:------|
| a, motorA | b, motorB | o1, opto1 |
| vibe, vibe1 | vibe2 | o2, opto2 |
| thrust | rotate | o3, opto3 |
| suck (mode 4) | suck, speed | |

## Value Ranges

| Output | Range |
|:-------|:------|
| Motors | -100 to +100 |
| Optos | true/false or 1/0 |

Negative motor values = reverse direction.

\newpage

# Setup Guide

## Step 1: Arduino Libraries

Install via Library Manager:

- **WebSockets** --- Markus Sattler
- **ArduinoJson** --- Benoit Blanchon
- **Adafruit NeoPixel**

## Step 2: Board Settings

| Setting | Value |
|:--------|:------|
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | Enabled |
| Flash Size | 4MB |
| Partition | Default 4MB with spiffs |

## Step 3: Flash Firmware

1. Connect ESP32-C3 via USB-C
2. Select COM port
3. Upload sketch
4. Wait for completion

## Step 4: WiFi Setup

1. Power on --- LED pulses purple
2. Connect to WiFi: **XToys-ESP32-Setup**
3. Browser opens (or go to 192.168.4.1)
4. Select your network
5. Enter password
6. Pick default mode
7. Save --- device restarts

## Step 5: Find IP Address

- Check router DHCP list
- Serial monitor @ 115200 baud
- Network scanner app

## Step 6: Connect XToys

1. Open XToys.app
2. Add WebSocket device
3. URL: ws://[IP]:81
4. Connect and enjoy!

\newpage

# Troubleshooting

## Red LED (pulsing)

**Problem:** Can't connect to WiFi

- Check SSID/password
- Move closer to router
- 2.4GHz only (no 5GHz)
- Long press to reset

## Yellow LED but no connection

**Problem:** WebSocket failing

- Verify IP address
- Same network as phone?
- Firewall blocking port 81?
- Try reconnecting in XToys

## Motors not working

**Problem:** Commands received but no movement

- Check DRV8833 wiring
- Motor power connected?
- Voltage in 3-10V range?
- Test: {"a": 100}

## Device keeps resetting

**Problem:** Power issues

- Use quality USB supply (5V 1A+)
- Add 100uF cap on motor power
- Separate ESP32 and motor power

## Optos not triggering

**Problem:** Button simulation failing

- 220 Ohm resistors installed?
- Check LED polarity
- Correct mode selected?

\newpage

# Specifications

## Electrical

| Parameter | Value |
|:----------|:------|
| ESP32 Voltage | 5V DC (USB-C) |
| Motor Voltage | 3-10V DC |
| Motor Current | 1.5A cont. / 2A peak |
| Opto LED Current | ~10mA |
| Total Power | ~500mA typical |

## Timing

| Parameter | Value |
|:----------|:------|
| PWM Frequency | 20kHz |
| PWM Resolution | 8-bit (256 steps) |
| WebSocket Port | 81 |
| Button Debounce | 50ms |
| Long Press | 3000ms |
| Double Click Window | 400ms |

## Network

| Parameter | Value |
|:----------|:------|
| WiFi Standard | 802.11 b/g/n |
| WiFi Band | 2.4GHz |
| Config SSID | XToys-ESP32-Setup |
| Config IP | 192.168.4.1 |
| Serial Baud | 115200 |

## Firmware

| Parameter | Value |
|:----------|:------|
| Flash Usage | ~300KB |
| RAM Usage | ~50KB |
| Settings Storage | ~1KB |

\newpage

# Safety

## General

- **Adults only**
- Disconnect power before wiring changes
- Don't exceed voltage ratings
- Waterproof if needed for your application

## Electrical

- Use protected power supplies
- DRV8833 has thermal shutdown
- Motors stop on disconnect
- Long press = emergency reset

## Thermal

- DRV8833 gets warm under load
- Provide ventilation
- Heatsink for sustained >1A

\vspace{2cm}

\begin{center}
{\Large\textcolor{accent}{Happy Building!}}

\vspace{0.5cm}

{\textcolor{textsub}{github.com/Tuna-Toys/xtoys\_esp32\_c3}}

\vspace{0.3cm}

{\small\textcolor{textsub}{MIT License --- Do whatever you want}}
\end{center}
