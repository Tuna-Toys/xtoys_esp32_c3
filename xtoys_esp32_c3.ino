/*
 * XToys ESP32-C3 WebSocket Controller
 *
 * Hardware:
 *   - ESP32-C3 Super Mini
 *   - DRV8833 Dual Motor Driver (2 bidirectional channels)
 *   - Optocoupler outputs for button simulation
 *   - WS2812 RGB LED (built-in on GPIO8)
 *
 * Modes (short press to cycle):
 *   1. Basic Vibrator    - 1 motor channel intensity
 *   2. Dual Vibrator     - 2 independent motor channels
 *   3. Stroker           - Motor A forward/reverse with position control
 *   4. Milking Machine   - Speed (Motor A) + Suction pulse (Motor B)
 *   5. Edge-O-Matic      - Motor + Optocoupler triggers for device control
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
// Built-in
#define BUTTON_PIN        9     // Boot button
#define LED_PIN           8     // WS2812 RGB LED

// DRV8833 Motor Driver
// Motor A (Channel 1) - Vibration/Stroke/Main motor
#define MOTOR_A_IN1       2
#define MOTOR_A_IN2       3
// Motor B (Channel 2) - Secondary motor/suction
#define MOTOR_B_IN1       4
#define MOTOR_B_IN2       5

// Optocoupler outputs (active LOW typically)
#define OPTO_1            6     // Button simulation 1 (power/mode)
#define OPTO_2            7     // Button simulation 2 (intensity/pattern)
#define OPTO_3            10    // Button simulation 3 (extra)

// ===================== CONFIGURATION =====================
#define NUM_LEDS          1
#define PWM_FREQ          20000   // 20kHz - quiet motor operation
#define PWM_RESOLUTION    8       // 8-bit (0-255)

// ===================== DEVICE MODES =====================
enum DeviceMode {
  MODE_BASIC_VIBRATOR = 0,
  MODE_DUAL_VIBRATOR,
  MODE_STROKER,
  MODE_MILKING,
  MODE_EDGE_O_MATIC,
  MODE_COUNT
};

const char* MODE_NAMES[] = {
  "Basic Vibrator",
  "Dual Vibrator",
  "Stroker",
  "Milking Machine",
  "Edge-O-Matic"
};

// Mode colors for identification (shown briefly on mode change)
const uint32_t MODE_COLORS[] = {
  0xFF0080,  // Pink - Basic vibe
  0x8000FF,  // Purple - Dual vibe
  0x00FF80,  // Cyan-green - Stroker
  0xFFFF00,  // Yellow - Milking
  0xFF4000   // Orange - Edge
};

// ===================== GLOBAL OBJECTS =====================
Adafruit_NeoPixel led(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsServer webSocket(81);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// ===================== STATE VARIABLES =====================
DeviceMode currentMode = MODE_BASIC_VIBRATOR;
bool wifiConnected = false;
bool clientConnected = false;
bool configMode = false;

// Motor state
int motorA_speed = 0;      // -100 to 100 (negative = reverse)
int motorB_speed = 0;
bool motorA_brake = false;
bool motorB_brake = false;

// Optocoupler state
bool opto1_state = false;
bool opto2_state = false;
bool opto3_state = false;

// Stroker state
int strokerPosition = 50;   // 0-100 target position
int strokerSpeed = 50;      // stroke speed
unsigned long lastStrokerUpdate = 0;
int currentStrokerPos = 50;
bool strokerDirection = true; // true = forward

// Milking state
int milkingSpeed = 0;
int suctionIntensity = 0;
int suctionCycleMs = 1000;
unsigned long lastSuctionCycle = 0;
bool suctionPhase = false;

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

// WiFi credentials
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
    .card{background:rgba(255,255,255,0.05);border-radius:12px;padding:20px;margin-bottom:20px;backdrop-filter:blur(10px)}
    label{display:block;color:#aaa;margin:15px 0 5px;font-size:14px}
    input,select{width:100%;padding:12px;border:1px solid #333;border-radius:8px;background:#1a1a2e;color:#fff;font-size:16px}
    input:focus,select:focus{outline:none;border-color:#ff6b9d}
    .btn{background:linear-gradient(135deg,#ff6b9d,#c44569);border:none;color:#fff;padding:15px;font-size:16px;cursor:pointer;border-radius:8px;width:100%;margin-top:20px}
    .btn:hover{transform:translateY(-2px);box-shadow:0 5px 20px rgba(255,107,157,0.4)}
    .networks{max-height:180px;overflow-y:auto;background:#0a0a15;border-radius:8px;margin:10px 0}
    .net{padding:12px;cursor:pointer;border-bottom:1px solid #222;display:flex;justify-content:space-between}
    .net:hover{background:#ff6b9d22}
    .net:last-child{border:none}
    .signal{color:#666;font-size:12px}
    .modes{display:grid;gap:8px}
    .mode{background:#0a0a15;padding:10px 12px;border-radius:6px;font-size:13px}
    .mode b{color:#ff6b9d}
    h3{color:#ff6b9d;margin:0 0 15px;font-size:16px}
    .ip{text-align:center;color:#666;margin-top:20px;font-size:12px}
  </style>
</head>
<body>
  <div class="c">
    <h1>XToys ESP32</h1>
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
          <option value="0">Basic Vibrator - Single motor</option>
          <option value="1">Dual Vibrator - Two motors</option>
          <option value="2">Stroker - Position control</option>
          <option value="3">Milking Machine - Speed + suction</option>
          <option value="4">Edge-O-Matic - Motor + triggers</option>
        </select>
      </div>

      <div class="card">
        <h3>Available Modes</h3>
        <div class="modes">
          <div class="mode"><b>1.</b> Basic Vibrator - Motor A intensity</div>
          <div class="mode"><b>2.</b> Dual Vibrator - Motors A & B independent</div>
          <div class="mode"><b>3.</b> Stroker - Bidirectional position/speed</div>
          <div class="mode"><b>4.</b> Milking - Continuous + pulsing suction</div>
          <div class="mode"><b>5.</b> Edge-O-Matic - Motors + opto triggers</div>
        </div>
      </div>

      <button type="submit" class="btn">Save & Connect</button>
    </form>
    <div class="ip">WebSocket Port: 81</div>
  </div>
<script>
fetch('/scan').then(r=>r.json()).then(n=>{
  let h='';
  n.forEach(x=>{
    let s=x.rssi>-50?'███':x.rssi>-70?'██░':x.rssi>-85?'█░░':'░░░';
    h+='<div class="net" onclick="document.getElementById(\'ssid\').value=\''+x.ssid+'\'">'+x.ssid+'<span class="signal">'+s+' '+x.rssi+'</span></div>';
  });
  document.getElementById('nets').innerHTML=h||'No networks found';
});
</script>
</body>
</html>
)rawliteral";

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n=== XToys ESP32-C3 Controller ===");

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
  pinMode(OPTO_1, OUTPUT);
  pinMode(OPTO_2, OUTPUT);
  pinMode(OPTO_3, OUTPUT);
  digitalWrite(OPTO_1, HIGH);  // Opto off (assuming active LOW)
  digitalWrite(OPTO_2, HIGH);
  digitalWrite(OPTO_3, HIGH);

  // Stop motors
  stopMotors();

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

    // Reconnect if disconnected
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
// DRV8833 truth table:
// IN1=H, IN2=L -> Forward
// IN1=L, IN2=H -> Reverse
// IN1=L, IN2=L -> Coast (stop)
// IN1=H, IN2=H -> Brake

void setMotorA(int speed) {  // speed: -100 to 100
  motorA_speed = constrain(speed, -100, 100);
  uint8_t pwm = map(abs(motorA_speed), 0, 100, 0, 255);

  if (motorA_speed > 0) {
    ledcWrite(MOTOR_A_IN1, pwm);
    ledcWrite(MOTOR_A_IN2, 0);
  } else if (motorA_speed < 0) {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, pwm);
  } else {
    ledcWrite(MOTOR_A_IN1, 0);
    ledcWrite(MOTOR_A_IN2, 0);
  }
}

void setMotorB(int speed) {
  motorB_speed = constrain(speed, -100, 100);
  uint8_t pwm = map(abs(motorB_speed), 0, 100, 0, 255);

  if (motorB_speed > 0) {
    ledcWrite(MOTOR_B_IN1, pwm);
    ledcWrite(MOTOR_B_IN2, 0);
  } else if (motorB_speed < 0) {
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, pwm);
  } else {
    ledcWrite(MOTOR_B_IN1, 0);
    ledcWrite(MOTOR_B_IN2, 0);
  }
}

void brakeMotorA() {
  ledcWrite(MOTOR_A_IN1, 255);
  ledcWrite(MOTOR_A_IN2, 255);
  motorA_brake = true;
}

void brakeMotorB() {
  ledcWrite(MOTOR_B_IN1, 255);
  ledcWrite(MOTOR_B_IN2, 255);
  motorB_brake = true;
}

void stopMotors() {
  setMotorA(0);
  setMotorB(0);
  motorA_brake = false;
  motorB_brake = false;
}

// ===================== OPTOCOUPLER CONTROL =====================
void setOpto(int channel, bool state) {
  // Active LOW - state=true means opto conducts (simulates button press)
  int pin = (channel == 1) ? OPTO_1 : (channel == 2) ? OPTO_2 : OPTO_3;
  digitalWrite(pin, state ? LOW : HIGH);

  if (channel == 1) opto1_state = state;
  else if (channel == 2) opto2_state = state;
  else opto3_state = state;

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
    case MODE_STROKER:
      // Automatic stroking based on position/speed
      if (strokerSpeed > 0 && now - lastStrokerUpdate > (100 - strokerSpeed)) {
        lastStrokerUpdate = now;

        if (strokerDirection) {
          currentStrokerPos += 2;
          if (currentStrokerPos >= 100) strokerDirection = false;
        } else {
          currentStrokerPos -= 2;
          if (currentStrokerPos <= 0) strokerDirection = true;
        }

        // Convert position to motor direction
        int motorSpeed = map(currentStrokerPos, 0, 100, -100, 100);
        setMotorA(motorSpeed);
      }
      break;

    case MODE_MILKING:
      // Continuous rotation + pulsing suction
      setMotorA(milkingSpeed);

      if (suctionIntensity > 0) {
        int cycleTime = map(suctionIntensity, 1, 100, 2000, 200);
        if (now - lastSuctionCycle > cycleTime / 2) {
          lastSuctionCycle = now;
          suctionPhase = !suctionPhase;
          setMotorB(suctionPhase ? suctionIntensity : 0);
        }
      }
      break;

    default:
      break;
  }
}

// ===================== WEBSOCKET HANDLING =====================
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(wsEvent);
  Serial.printf("WebSocket server on port 81\n");
  Serial.printf("Connect XToys to: ws://%s:81\n", WiFi.localIP().toString().c_str());
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Disconnected\n", num);
      clientConnected = false;
      stopMotors();
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
  StaticJsonDocument<300> doc;
  doc["type"] = "deviceInfo";
  doc["mode"] = MODE_NAMES[currentMode];
  doc["modeId"] = currentMode;
  doc["ip"] = WiFi.localIP().toString();

  JsonArray features = doc.createNestedArray("features");
  switch (currentMode) {
    case MODE_BASIC_VIBRATOR:
      features.add("intensity");
      break;
    case MODE_DUAL_VIBRATOR:
      features.add("intensity1");
      features.add("intensity2");
      break;
    case MODE_STROKER:
      features.add("position");
      features.add("speed");
      break;
    case MODE_MILKING:
      features.add("speed");
      features.add("suction");
      break;
    case MODE_EDGE_O_MATIC:
      features.add("intensity");
      features.add("trigger1");
      features.add("trigger2");
      features.add("trigger3");
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
  DeserializationError err = deserializeJson(doc, msg);

  if (!err) {
    // JSON commands
    processJsonCommand(doc);
  } else {
    // Simple format: "value" or "cmd:value"
    processSimpleCommand(msg);
  }
}

void processJsonCommand(JsonDocument& doc) {
  // Universal stop
  if (doc["cmd"] == "stop" || doc["stop"] == true) {
    stopMotors();
    setOpto(1, false);
    setOpto(2, false);
    setOpto(3, false);
    return;
  }

  // Mode-specific handling
  switch (currentMode) {
    case MODE_BASIC_VIBRATOR:
      if (doc.containsKey("intensity") || doc.containsKey("v") || doc.containsKey("value")) {
        int v = doc["intensity"] | doc["v"] | doc["value"] | 0;
        setMotorA(v);
      }
      break;

    case MODE_DUAL_VIBRATOR:
      if (doc.containsKey("intensity1") || doc.containsKey("v1")) {
        setMotorA(doc["intensity1"] | doc["v1"] | 0);
      }
      if (doc.containsKey("intensity2") || doc.containsKey("v2")) {
        setMotorB(doc["intensity2"] | doc["v2"] | 0);
      }
      // Also support single intensity for both
      if (doc.containsKey("intensity")) {
        int v = doc["intensity"];
        setMotorA(v);
        setMotorB(v);
      }
      break;

    case MODE_STROKER:
      if (doc.containsKey("position") || doc.containsKey("pos")) {
        strokerPosition = doc["position"] | doc["pos"] | 50;
        // Direct position mode - convert 0-100 to motor direction
        int motorDir = map(strokerPosition, 0, 100, -100, 100);
        setMotorA(motorDir);
      }
      if (doc.containsKey("speed")) {
        strokerSpeed = doc["speed"] | 50;
      }
      break;

    case MODE_MILKING:
      if (doc.containsKey("speed")) {
        milkingSpeed = doc["speed"] | 0;
      }
      if (doc.containsKey("suction")) {
        suctionIntensity = doc["suction"] | 0;
      }
      break;

    case MODE_EDGE_O_MATIC:
      if (doc.containsKey("intensity") || doc.containsKey("v")) {
        setMotorA(doc["intensity"] | doc["v"] | 0);
      }
      if (doc.containsKey("trigger1") || doc.containsKey("t1")) {
        setOpto(1, doc["trigger1"] | doc["t1"] | false);
      }
      if (doc.containsKey("trigger2") || doc.containsKey("t2")) {
        setOpto(2, doc["trigger2"] | doc["t2"] | false);
      }
      if (doc.containsKey("trigger3") || doc.containsKey("t3")) {
        setOpto(3, doc["trigger3"] | doc["t3"] | false);
      }
      // Pulse commands (duration in ms)
      if (doc.containsKey("pulse1")) {
        pulseOpto(1, doc["pulse1"]);
      }
      if (doc.containsKey("pulse2")) {
        pulseOpto(2, doc["pulse2"]);
      }
      if (doc.containsKey("pulse3")) {
        pulseOpto(3, doc["pulse3"]);
      }
      break;
  }
}

void processSimpleCommand(String& msg) {
  msg.trim();

  // Check for "channel:value" format
  int sep = msg.indexOf(':');
  if (sep > 0) {
    String ch = msg.substring(0, sep);
    int val = msg.substring(sep + 1).toInt();

    if (ch == "a" || ch == "1" || ch == "m1") {
      setMotorA(val);
    } else if (ch == "b" || ch == "2" || ch == "m2") {
      setMotorB(val);
    } else if (ch == "o1" || ch == "t1") {
      setOpto(1, val > 0);
    } else if (ch == "o2" || ch == "t2") {
      setOpto(2, val > 0);
    } else if (ch == "o3" || ch == "t3") {
      setOpto(3, val > 0);
    }
  } else {
    // Single value - apply to primary output
    int val = msg.toInt();
    setMotorA(val);
  }
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
  Serial.println("Connect to WiFi: XToys-ESP32-Setup");
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
    "<html><body style='background:#1a1a2e;color:#fff;text-align:center;padding:50px;font-family:system-ui'>"
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
      // Long press - reset to config
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

  // Process clicks after debounce window
  if (clickCount > 0 && now - lastClickTime > DOUBLE_CLICK_MS) {
    if (clickCount >= 2) {
      // Double click - show mode
      showModeColor();
      blinkMode();
    } else {
      // Single click - cycle mode
      cycleMode();
    }
    clickCount = 0;
  }

  // Long press visual feedback
  if (buttonPressed && now - buttonPressStart > 1500) {
    if ((now / 150) % 2) setLED(255, 100, 0);
  }
}

void cycleMode() {
  stopMotors();
  currentMode = (DeviceMode)((currentMode + 1) % MODE_COUNT);

  prefs.begin("xtoys", false);
  prefs.putUInt("mode", currentMode);
  prefs.end();

  Serial.printf("Mode: %s\n", MODE_NAMES[currentMode]);

  // Notify client
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

  // Pulse animation
  if (ledPulseUp) {
    ledPulse += 4;
    if (ledPulse >= 250) ledPulseUp = false;
  } else {
    ledPulse -= 4;
    if (ledPulse <= 10) ledPulseUp = true;
  }

  if (configMode) {
    setLED(ledPulse, 0, ledPulse);  // Purple pulse
  }
  else if (!wifiConnected) {
    setLED(ledPulse, 0, 0);  // Red pulse
  }
  else if (!clientConnected) {
    setLED(255, 180, 0);  // Yellow solid
  }
  else {
    setLED(0, 255, 0);  // Green solid
  }
}
