/*
 * XToys ESP32-C3 WebSocket Controller v2
 *
 * Hardware:
 *   - ESP32-C3 Super Mini
 *   - DRV8833 Dual Motor Driver (2 bidirectional channels)
 *   - Optocoupler outputs for button simulation
 *   - WS2812 RGB LED (built-in on GPIO8)
 *
 * Modes (short press to cycle):
 *   1. Vibe & Suck      - Vibration motor + Suction pump
 *   2. Full Machine     - Thrust + Rotation + Vibe/Suction buttons
 *   3. Handjob Sim      - Oscillating stroke + Vibration
 *   4. Blowjob Sim      - Pulsing suction + Movement speed
 *   5. Tease Mode       - Automated edging with random patterns
 *
 * Button Controls (GPIO9 boot button):
 *   - Short press: Cycle through modes
 *   - Long press (3s): Enter WiFi config mode
 *   - Double press: Blink current mode number
 *
 * LED Status:
 *   - Red pulse: No WiFi
 *   - Yellow: WiFi OK, no client
 *   - Green: Client connected
 *   - Purple pulse: Config mode
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// ===================== PIN DEFINITIONS =====================
#define BUTTON_PIN        9     // Boot button
#define LED_PIN           8     // WS2812 RGB LED

// DRV8833 Motor Driver
#define MOTOR_A_IN1       2     // Motor A - Primary (thrust/vibe)
#define MOTOR_A_IN2       3
#define MOTOR_B_IN1       4     // Motor B - Secondary (rotation/suction)
#define MOTOR_B_IN2       5

// Optocoupler outputs (active LOW)
#define OPTO_VIBE         6     // External vibe control
#define OPTO_SUCK         7     // External suction control
#define OPTO_POWER        10    // Power/mode button

// ===================== CONFIGURATION =====================
#define NUM_LEDS          1
#define PWM_FREQ          20000
#define PWM_RESOLUTION    8

// ===================== DEVICE MODES =====================
enum DeviceMode {
  MODE_VIBE_SUCK = 0,       // Simple: vibe + suction
  MODE_FULL_MACHINE,        // Complex: thrust + rotation + button controls
  MODE_HANDJOB,             // Oscillating stroke + vibe
  MODE_BLOWJOB,             // Pulsing suction + speed
  MODE_TEASE,               // Automated edging patterns
  MODE_COUNT
};

const char* MODE_NAMES[] = {
  "Vibe & Suck",
  "Full Machine",
  "Handjob Sim",
  "Blowjob Sim",
  "Tease Mode"
};

// Mode colors (shown on mode change)
const uint32_t MODE_COLORS[] = {
  0xFF00FF,  // Magenta - Vibe & Suck
  0xFF4400,  // Orange - Full Machine
  0x00FFFF,  // Cyan - Handjob
  0xFF0066,  // Hot Pink - Blowjob
  0x9900FF   // Purple - Tease
};

// ===================== GLOBAL OBJECTS =====================
Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsServer webSocket(81);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// ===================== STATE VARIABLES =====================
DeviceMode currentMode = MODE_VIBE_SUCK;
bool wifiConnected = false;
bool clientConnected = false;
bool configMode = false;

// Motor values (0-100 for intensity, -100 to 100 for direction)
int vibeIntensity = 0;
int suckIntensity = 0;
int thrustSpeed = 0;
int rotationSpeed = 0;

// Handjob mode - oscillating stroke
int strokeSpeed = 50;          // How fast to oscillate
int strokePosition = 50;       // Current position 0-100
bool strokeDirection = true;   // true = forward
unsigned long lastStrokeUpdate = 0;

// Blowjob mode - pulsing suction
int blowSpeed = 0;             // Movement/tongue speed
int suctionDepth = 0;          // How deep the suction pulses
int suctionRate = 50;          // Pulse rate
bool suctionPhase = false;
unsigned long lastSuctionPulse = 0;

// Tease mode - automated edging
bool teaseActive = false;
int teaseIntensity = 0;
int teaseTarget = 0;
int teasePhase = 0;            // 0=build, 1=hold, 2=drop, 3=rest
unsigned long teasePhaseStart = 0;
unsigned long teasePhaseDuration = 0;
int teaseVibeBoost = 0;

// Button handling
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
int clickCount = 0;
unsigned long lastClickTime = 0;
const unsigned long LONG_PRESS_MS = 3000;
const unsigned long DOUBLE_CLICK_MS = 400;

// LED animation
unsigned long lastLedUpdate = 0;
uint8_t ledPulse = 0;
bool ledPulseUp = true;

// WiFi
String wifiSSID = "";
String wifiPass = "";

// ===================== HTML CONFIG PAGE =====================
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>XToys ESP32 Setup</title>
  <style>
    *{box-sizing:border-box}
    body{font-family:system-ui;background:linear-gradient(135deg,#1a1a2e,#16213e);color:#eee;min-height:100vh;margin:0;padding:20px}
    .c{max-width:420px;margin:0 auto}
    h1{color:#ff6b9d;text-align:center;margin-bottom:30px}
    .card{background:rgba(255,255,255,0.05);border-radius:12px;padding:20px;margin-bottom:20px}
    label{display:block;color:#aaa;margin:15px 0 5px;font-size:14px}
    input,select{width:100%;padding:12px;border:1px solid #333;border-radius:8px;background:#1a1a2e;color:#fff;font-size:16px}
    input:focus,select:focus{outline:none;border-color:#ff6b9d}
    .btn{background:linear-gradient(135deg,#ff6b9d,#c44569);border:none;color:#fff;padding:15px;font-size:16px;cursor:pointer;border-radius:8px;width:100%;margin-top:20px}
    .networks{max-height:180px;overflow-y:auto;background:#0a0a15;border-radius:8px;margin:10px 0}
    .net{padding:12px;cursor:pointer;border-bottom:1px solid #222;display:flex;justify-content:space-between}
    .net:hover{background:#ff6b9d22}
    .modes{display:grid;gap:8px}
    .mode{background:#0a0a15;padding:10px 12px;border-radius:6px;font-size:13px}
    .mode b{color:#ff6b9d}
    h3{color:#ff6b9d;margin:0 0 15px;font-size:16px}
  </style>
</head>
<body>
  <div class="c">
    <h1>XToys ESP32 v2</h1>
    <form action="/save" method="POST">
      <div class="card">
        <h3>WiFi Setup</h3>
        <label>Select Network</label>
        <div class="networks" id="nets">Scanning...</div>
        <input type="text" name="ssid" id="ssid" placeholder="Or enter SSID manually" required>
        <label>Password</label>
        <input type="password" name="pass" placeholder="WiFi Password">
      </div>

      <div class="card">
        <h3>Default Mode</h3>
        <select name="mode">
          <option value="0">Vibe & Suck - Vibration + Suction</option>
          <option value="1">Full Machine - Thrust/Rotate/Vibe/Suck</option>
          <option value="2">Handjob Sim - Stroke + Vibe</option>
          <option value="3">Blowjob Sim - Suction + Speed</option>
          <option value="4">Tease Mode - Auto Edging</option>
        </select>
      </div>

      <div class="card">
        <h3>Mode Details</h3>
        <div class="modes">
          <div class="mode"><b>1. Vibe & Suck</b> - Motor A: vibration, Motor B: suction pump</div>
          <div class="mode"><b>2. Full Machine</b> - Motors: thrust+rotation, Buttons: vibe/suck/power</div>
          <div class="mode"><b>3. Handjob</b> - Oscillating stroke motor + vibration</div>
          <div class="mode"><b>4. Blowjob</b> - Pulsing suction + tongue/movement</div>
          <div class="mode"><b>5. Tease</b> - Automated build-up, edge, and denial patterns</div>
        </div>
      </div>

      <button type="submit" class="btn">Save & Connect</button>
    </form>
  </div>
<script>
fetch('/scan').then(r=>r.json()).then(n=>{
  let h='';
  n.forEach(x=>{
    h+='<div class="net" onclick="document.getElementById(\'ssid\').value=\''+x.ssid+'\'">'+x.ssid+'<span>'+x.rssi+'dB</span></div>';
  });
  document.getElementById('nets').innerHTML=h||'No networks';
});
</script>
</body>
</html>
)rawliteral";

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== XToys ESP32-C3 Controller v2 ===");

  // Initialize LED
  led.begin();
  led.setBrightness(60);
  setLED(255, 0, 0);

  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Initialize motor driver pins with PWM
  ledcAttach(MOTOR_A_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_A_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_IN2, PWM_FREQ, PWM_RESOLUTION);

  // Initialize optocoupler pins
  pinMode(OPTO_VIBE, OUTPUT);
  pinMode(OPTO_SUCK, OUTPUT);
  pinMode(OPTO_POWER, OUTPUT);
  digitalWrite(OPTO_VIBE, HIGH);
  digitalWrite(OPTO_SUCK, HIGH);
  digitalWrite(OPTO_POWER, HIGH);

  stopAllOutputs();

  // Load settings
  prefs.begin("xtoys", false);
  wifiSSID = prefs.getString("ssid", "");
  wifiPass = prefs.getString("pass", "");
  currentMode = (DeviceMode)prefs.getUInt("mode", 0);
  prefs.end();

  Serial.printf("Mode: %s\n", MODE_NAMES[currentMode]);

  if (wifiSSID.length() > 0) {
    connectWiFi();
  } else {
    startConfigMode();
  }
}

// ===================== MAIN LOOP =====================
void loop() {
  handleButton();

  if (configMode) {
    dnsServer.processNextRequest();
    server.handleClient();
  } else {
    webSocket.loop();

    if (WiFi.status() != WL_CONNECTED && wifiConnected) {
      wifiConnected = false;
      clientConnected = false;
      Serial.println("WiFi lost, reconnecting...");
      connectWiFi();
    }

    // Mode-specific updates
    updateModeLogic();
  }

  updateLED();
  delay(1);
}

// ===================== MOTOR CONTROL =====================
void setMotorA(int speed) {
  speed = constrain(speed, -100, 100);
  uint8_t pwm = map(abs(speed), 0, 100, 0, 255);

  if (speed > 0) {
    ledcWrite(MOTOR_A_IN1, pwm);
    ledcWrite(MOTOR_A_IN2, 0);
  } else if (speed < 0) {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, pwm);
  } else {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, 0);
  }
}

void setMotorB(int speed) {
  speed = constrain(speed, -100, 100);
  uint8_t pwm = map(abs(speed), 0, 100, 0, 255);

  if (speed > 0) {
    ledcWrite(MOTOR_B_IN1, pwm);
    ledcWrite(MOTOR_B_IN2, 0);
  } else if (speed < 0) {
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, pwm);
  } else {
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, 0);
  }
}

void stopAllOutputs() {
  setMotorA(0);
  setMotorB(0);
  vibeIntensity = 0;
  suckIntensity = 0;
  thrustSpeed = 0;
  rotationSpeed = 0;
  strokeSpeed = 0;
  blowSpeed = 0;
  suctionDepth = 0;
  teaseActive = false;
  digitalWrite(OPTO_VIBE, HIGH);
  digitalWrite(OPTO_SUCK, HIGH);
  digitalWrite(OPTO_POWER, HIGH);
}

// ===================== OPTOCOUPLER CONTROL =====================
void setOpto(int channel, bool state) {
  int pin = (channel == 1) ? OPTO_VIBE : (channel == 2) ? OPTO_SUCK : OPTO_POWER;
  digitalWrite(pin, state ? LOW : HIGH);
  Serial.printf("Opto %d: %s\n", channel, state ? "ON" : "OFF");
}

void pulseOpto(int channel, int durationMs) {
  setOpto(channel, true);
  delay(durationMs);
  setOpto(channel, false);
}

// ===================== MODE-SPECIFIC LOGIC =====================
void updateModeLogic() {
  unsigned long now = millis();

  switch (currentMode) {
    case MODE_VIBE_SUCK:
      // Direct control - values set by WebSocket commands
      setMotorA(vibeIntensity);
      setMotorB(suckIntensity);
      break;

    case MODE_FULL_MACHINE:
      // Thrust on Motor A, Rotation on Motor B
      // Vibe/Suck controlled via optocouplers on external device
      setMotorA(thrustSpeed);
      setMotorB(rotationSpeed);
      break;

    case MODE_HANDJOB:
      // Oscillating stroke motion on Motor A
      if (strokeSpeed > 0) {
        int interval = map(strokeSpeed, 1, 100, 50, 5);
        if (now - lastStrokeUpdate > interval) {
          lastStrokeUpdate = now;

          int step = map(strokeSpeed, 1, 100, 1, 5);
          if (strokeDirection) {
            strokePosition += step;
            if (strokePosition >= 100) {
              strokePosition = 100;
              strokeDirection = false;
            }
          } else {
            strokePosition -= step;
            if (strokePosition <= 0) {
              strokePosition = 0;
              strokeDirection = true;
            }
          }

          // Convert position to bidirectional motor speed
          int motorSpeed = map(strokePosition, 0, 100, -100, 100);
          setMotorA(motorSpeed);
        }
      } else {
        setMotorA(0);
      }
      // Vibration on Motor B
      setMotorB(vibeIntensity);
      break;

    case MODE_BLOWJOB:
      // Pulsing suction on Motor A
      if (suctionDepth > 0) {
        int cycleTime = map(suctionRate, 1, 100, 2000, 200);
        if (now - lastSuctionPulse > cycleTime / 2) {
          lastSuctionPulse = now;
          suctionPhase = !suctionPhase;
          setMotorA(suctionPhase ? suctionDepth : suctionDepth / 3);
        }
      } else {
        setMotorA(0);
      }
      // Movement/tongue speed on Motor B
      setMotorB(blowSpeed);
      break;

    case MODE_TEASE:
      updateTeaseMode(now);
      break;
  }
}

void updateTeaseMode(unsigned long now) {
  if (!teaseActive) {
    setMotorA(0);
    setMotorB(0);
    return;
  }

  // Check if current phase is complete
  if (now - teasePhaseStart > teasePhaseDuration) {
    advanceTeasePhase(now);
  }

  // Smooth intensity changes
  int targetIntensity = 0;
  switch (teasePhase) {
    case 0: // Build up
      targetIntensity = map(now - teasePhaseStart, 0, teasePhaseDuration, 10, teaseTarget);
      break;
    case 1: // Hold at peak
      targetIntensity = teaseTarget;
      break;
    case 2: // Drop
      targetIntensity = map(now - teasePhaseStart, 0, teasePhaseDuration, teaseTarget, 5);
      break;
    case 3: // Rest
      targetIntensity = random(0, 15);
      break;
  }

  teaseIntensity = targetIntensity;

  // Apply with some variation
  int vibeOut = constrain(teaseIntensity + teaseVibeBoost, 0, 100);
  int suckOut = constrain(teaseIntensity - 10 + random(-5, 10), 0, 100);

  setMotorA(vibeOut);
  setMotorB(suckOut > 20 ? suckOut : 0);

  // Random vibe bursts
  if (random(100) < 3 && teasePhase == 1) {
    teaseVibeBoost = random(10, 30);
  } else {
    teaseVibeBoost = max(0, teaseVibeBoost - 1);
  }
}

void advanceTeasePhase(unsigned long now) {
  teasePhase = (teasePhase + 1) % 4;
  teasePhaseStart = now;

  switch (teasePhase) {
    case 0: // Build
      teasePhaseDuration = random(3000, 8000);
      teaseTarget = random(60, 95);
      Serial.printf("Tease: Building to %d over %lums\n", teaseTarget, teasePhaseDuration);
      break;
    case 1: // Hold
      teasePhaseDuration = random(2000, 5000);
      Serial.printf("Tease: Holding at %d for %lums\n", teaseTarget, teasePhaseDuration);
      break;
    case 2: // Drop
      teasePhaseDuration = random(500, 2000);
      Serial.println("Tease: Dropping...");
      break;
    case 3: // Rest
      teasePhaseDuration = random(1000, 4000);
      Serial.printf("Tease: Resting for %lums\n", teasePhaseDuration);
      break;
  }
}

void startTeaseMode() {
  teaseActive = true;
  teasePhase = 0;
  teasePhaseStart = millis();
  teaseTarget = random(60, 90);
  teasePhaseDuration = random(3000, 6000);
  teaseVibeBoost = 0;
  Serial.println("Tease mode activated!");
}

// ===================== WEBSOCKET HANDLING =====================
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(wsEvent);
  Serial.printf("WebSocket on ws://%s:81\n", WiFi.localIP().toString().c_str());
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected\n", num);
      clientConnected = false;
      stopAllOutputs();
      break;

    case WStype_CONNECTED:
      Serial.printf("[%u] Connected from %s\n", num, webSocket.remoteIP(num).toString().c_str());
      clientConnected = true;
      sendDeviceInfo(num);
      break;

    case WStype_TEXT:
      handleMessage(num, (char*)payload, length);
      break;

    default:
      break;
  }
}

void sendDeviceInfo(uint8_t num) {
  StaticJsonDocument<400> doc;
  doc["type"] = "deviceInfo";
  doc["mode"] = MODE_NAMES[currentMode];
  doc["modeId"] = currentMode;
  doc["ip"] = WiFi.localIP().toString();

  JsonArray features = doc.createNestedArray("features");
  switch (currentMode) {
    case MODE_VIBE_SUCK:
      features.add("vibe");
      features.add("suck");
      break;
    case MODE_FULL_MACHINE:
      features.add("thrust");
      features.add("rotation");
      features.add("vibeBtn");
      features.add("suckBtn");
      features.add("powerBtn");
      break;
    case MODE_HANDJOB:
      features.add("stroke");
      features.add("vibe");
      break;
    case MODE_BLOWJOB:
      features.add("suction");
      features.add("rate");
      features.add("speed");
      break;
    case MODE_TEASE:
      features.add("start");
      features.add("stop");
      features.add("intensity");
      break;
  }

  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(num, out);
}

void handleMessage(uint8_t num, char* payload, size_t len) {
  String msg = String(payload);
  Serial.printf("[%u] << %s\n", num, payload);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, msg) == DeserializationError::Ok) {
    processJsonCommand(doc);
  } else {
    processSimpleCommand(msg);
  }
}

void processJsonCommand(JsonDocument& doc) {
  // Universal stop
  if (doc["cmd"] == "stop" || doc["stop"] == true) {
    stopAllOutputs();
    sendStatus();
    return;
  }

  switch (currentMode) {
    case MODE_VIBE_SUCK:
      if (doc.containsKey("vibe")) vibeIntensity = doc["vibe"];
      if (doc.containsKey("suck")) suckIntensity = doc["suck"];
      if (doc.containsKey("intensity")) {
        vibeIntensity = doc["intensity"];
        suckIntensity = doc["intensity"];
      }
      break;

    case MODE_FULL_MACHINE:
      if (doc.containsKey("thrust")) thrustSpeed = doc["thrust"];
      if (doc.containsKey("rotation")) rotationSpeed = doc["rotation"];
      // Button controls for external device
      if (doc.containsKey("vibeBtn")) {
        if (doc["vibeBtn"].is<int>()) {
          pulseOpto(1, doc["vibeBtn"]);
        } else {
          setOpto(1, doc["vibeBtn"]);
        }
      }
      if (doc.containsKey("suckBtn")) {
        if (doc["suckBtn"].is<int>()) {
          pulseOpto(2, doc["suckBtn"]);
        } else {
          setOpto(2, doc["suckBtn"]);
        }
      }
      if (doc.containsKey("powerBtn")) {
        if (doc["powerBtn"].is<int>()) {
          pulseOpto(3, doc["powerBtn"]);
        } else {
          setOpto(3, doc["powerBtn"]);
        }
      }
      break;

    case MODE_HANDJOB:
      if (doc.containsKey("stroke")) strokeSpeed = doc["stroke"];
      if (doc.containsKey("vibe")) vibeIntensity = doc["vibe"];
      if (doc.containsKey("speed")) strokeSpeed = doc["speed"];
      break;

    case MODE_BLOWJOB:
      if (doc.containsKey("suction")) suctionDepth = doc["suction"];
      if (doc.containsKey("rate")) suctionRate = doc["rate"];
      if (doc.containsKey("speed")) blowSpeed = doc["speed"];
      if (doc.containsKey("depth")) suctionDepth = doc["depth"];
      break;

    case MODE_TEASE:
      if (doc.containsKey("start") && doc["start"] == true) {
        startTeaseMode();
      }
      if (doc.containsKey("active")) {
        if (doc["active"] == true) startTeaseMode();
        else teaseActive = false;
      }
      if (doc.containsKey("intensity")) {
        teaseTarget = doc["intensity"];
      }
      break;
  }

  sendStatus();
}

void processSimpleCommand(String& msg) {
  msg.trim();

  // Handle "param:value" format
  int sep = msg.indexOf(':');
  if (sep > 0) {
    String param = msg.substring(0, sep);
    int val = msg.substring(sep + 1).toInt();

    param.toLowerCase();

    if (param == "vibe" || param == "v") vibeIntensity = val;
    else if (param == "suck" || param == "s") suckIntensity = val;
    else if (param == "thrust" || param == "t") thrustSpeed = val;
    else if (param == "rotation" || param == "r") rotationSpeed = val;
    else if (param == "stroke") strokeSpeed = val;
    else if (param == "speed") {
      if (currentMode == MODE_HANDJOB) strokeSpeed = val;
      else if (currentMode == MODE_BLOWJOB) blowSpeed = val;
    }
    else if (param == "suction" || param == "depth") suctionDepth = val;
    else if (param == "rate") suctionRate = val;
    else if (param == "tease") {
      if (val > 0) startTeaseMode();
      else teaseActive = false;
    }
    else if (param == "a" || param == "m1") setMotorA(val);
    else if (param == "b" || param == "m2") setMotorB(val);
  } else {
    // Single value - apply to primary control
    int val = msg.toInt();
    switch (currentMode) {
      case MODE_VIBE_SUCK: vibeIntensity = val; break;
      case MODE_FULL_MACHINE: thrustSpeed = val; break;
      case MODE_HANDJOB: strokeSpeed = val; break;
      case MODE_BLOWJOB: suctionDepth = val; break;
      case MODE_TEASE:
        if (val > 0) startTeaseMode();
        else teaseActive = false;
        break;
    }
  }
}

void sendStatus() {
  StaticJsonDocument<200> doc;
  doc["mode"] = MODE_NAMES[currentMode];

  switch (currentMode) {
    case MODE_VIBE_SUCK:
      doc["vibe"] = vibeIntensity;
      doc["suck"] = suckIntensity;
      break;
    case MODE_FULL_MACHINE:
      doc["thrust"] = thrustSpeed;
      doc["rotation"] = rotationSpeed;
      break;
    case MODE_HANDJOB:
      doc["stroke"] = strokeSpeed;
      doc["vibe"] = vibeIntensity;
      break;
    case MODE_BLOWJOB:
      doc["suction"] = suctionDepth;
      doc["rate"] = suctionRate;
      doc["speed"] = blowSpeed;
      break;
    case MODE_TEASE:
      doc["active"] = teaseActive;
      doc["intensity"] = teaseIntensity;
      doc["phase"] = teasePhase;
      break;
  }

  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}

// ===================== WIFI FUNCTIONS =====================
void connectWiFi() {
  Serial.printf("Connecting to: %s\n", wifiSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    handleButton();
    updateLED();
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());
    startWebSocket();
    showModeColor();
  } else {
    Serial.println("\nConnection failed");
    wifiConnected = false;
  }
}

void startConfigMode() {
  configMode = true;
  Serial.println("=== CONFIG MODE ===");

  WiFi.mode(WIFI_AP);
  WiFi.softAP("XToys-ESP32-Setup");
  Serial.printf("AP IP: %s\n", WiFi.softAPIP().toString().c_str());

  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", CONFIG_HTML); });
  server.on("/scan", handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() { server.send(200, "text/html", CONFIG_HTML); });

  server.begin();
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"rssi\":" + WiFi.RSSI(i) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSave() {
  wifiSSID = server.arg("ssid");
  wifiPass = server.arg("pass");
  int mode = server.arg("mode").toInt();

  prefs.begin("xtoys", false);
  prefs.putString("ssid", wifiSSID);
  prefs.putString("pass", wifiPass);
  prefs.putUInt("mode", mode);
  prefs.end();

  server.send(200, "text/html",
    "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding:50px'>"
    "<h1 style='color:#ff6b9d'>Saved!</h1><p>Restarting...</p></body></html>");

  delay(1500);
  ESP.restart();
}

// ===================== BUTTON HANDLING =====================
void handleButton() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (pressed && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = now;
  }
  else if (!pressed && buttonPressed) {
    buttonPressed = false;
    unsigned long dur = now - buttonPressStart;

    if (dur >= LONG_PRESS_MS) {
      Serial.println("Long press -> Config mode");
      prefs.begin("xtoys", false);
      prefs.clear();
      prefs.end();
      ESP.restart();
    }
    else if (dur > 50) {
      if (now - lastClickTime < DOUBLE_CLICK_MS) {
        clickCount++;
      } else {
        clickCount = 1;
      }
      lastClickTime = now;
    }
  }

  if (clickCount > 0 && now - lastClickTime > DOUBLE_CLICK_MS) {
    if (clickCount >= 2) {
      showModeColor();
      blinkMode();
    } else {
      cycleMode();
    }
    clickCount = 0;
  }

  if (buttonPressed && now - buttonPressStart > 1500) {
    if ((now / 150) % 2) setLED(255, 100, 0);
  }
}

void cycleMode() {
  stopAllOutputs();
  currentMode = (DeviceMode)((currentMode + 1) % MODE_COUNT);

  prefs.begin("xtoys", false);
  prefs.putUInt("mode", currentMode);
  prefs.end();

  Serial.printf("Mode: %s\n", MODE_NAMES[currentMode]);

  if (clientConnected) {
    for (uint8_t i = 0; i < webSocket.connectedClients(); i++) {
      sendDeviceInfo(i);
    }
  }

  showModeColor();
  blinkMode();
}

void showModeColor() {
  uint32_t c = MODE_COLORS[currentMode];
  setLED((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
  delay(500);
}

void blinkMode() {
  int blinks = currentMode + 1;
  for (int i = 0; i < blinks; i++) {
    setLED(255, 255, 255);
    delay(150);
    setLED(0, 0, 0);
    delay(150);
  }
}

// ===================== LED FUNCTIONS =====================
void setLED(uint8_t r, uint8_t g, uint8_t b) {
  led.setPixelColor(0, led.Color(r, g, b));
  led.show();
}

void updateLED() {
  if (millis() - lastLedUpdate < 20) return;
  lastLedUpdate = millis();

  if (ledPulseUp) {
    ledPulse += 4;
    if (ledPulse >= 250) ledPulseUp = false;
  } else {
    ledPulse -= 4;
    if (ledPulse <= 10) ledPulseUp = true;
  }

  if (configMode) {
    setLED(ledPulse, 0, ledPulse);
  }
  else if (!wifiConnected) {
    setLED(ledPulse, 0, 0);
  }
  else if (!clientConnected) {
    setLED(255, 180, 0);
  }
  else {
    // In tease mode, LED pulses with intensity
    if (currentMode == MODE_TEASE && teaseActive) {
      uint8_t g = map(teaseIntensity, 0, 100, 50, 255);
      setLED(0, g, teaseIntensity > 80 ? 100 : 0);
    } else {
      setLED(0, 255, 0);
    }
  }
}
