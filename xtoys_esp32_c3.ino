/*
 * XToys ESP32-C3 WebSocket Receiver
 *
 * Simple receiver - all logic handled by XToys server.
 * Just receives commands and sets motor/optocoupler outputs.
 *
 * Hardware:
 *   - ESP32-C3 Super Mini
 *   - DRV8833 Dual Motor Driver
 *   - 3x Optocoupler outputs
 *
 * Modes define output mapping (reported to XToys):
 *   1. Vibe + Suck      - Motor A: vibration, Motor B: suction
 *   2. Thrust + Rotate  - Motor A: thrust,    Motor B: rotation
 *   3. Thrust + Vibe    - Motor A: thrust,    Motor B: vibration
 *   4. Suck + Speed     - Motor A: suction,   Motor B: speed
 *   5. Dual Vibe        - Motor A: vibe1,     Motor B: vibe2
 *
 * Button: Short=cycle mode, Long(3s)=reset WiFi
 * LED: Red=no WiFi, Yellow=waiting, Green=connected, Purple=config
 */

#include <WiFi.h>
#include <WebSocketsServer.h>
#include <Preferences.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// ===================== PINS =====================
#define BUTTON_PIN        9
#define LED_PIN           8

#define MOTOR_A_IN1       2
#define MOTOR_A_IN2       3
#define MOTOR_B_IN1       4
#define MOTOR_B_IN2       5

#define OPTO_1            6
#define OPTO_2            7
#define OPTO_3            10

// ===================== CONFIG =====================
#define PWM_FREQ          20000
#define PWM_RESOLUTION    8

// ===================== MODES =====================
enum DeviceMode {
  MODE_VIBE_SUCK = 0,     // A=vibe, B=suck
  MODE_THRUST_ROTATE,     // A=thrust, B=rotate
  MODE_THRUST_VIBE,       // A=thrust, B=vibe
  MODE_SUCK_SPEED,        // A=suck, B=speed
  MODE_DUAL_VIBE,         // A=vibe1, B=vibe2
  MODE_COUNT
};

const char* MODE_NAMES[] = {
  "Vibe + Suck",
  "Thrust + Rotate",
  "Thrust + Vibe",
  "Suck + Speed",
  "Dual Vibe"
};

// What each motor does per mode (for XToys info)
const char* MOTOR_A_FUNCTION[] = {"vibe", "thrust", "thrust", "suck", "vibe1"};
const char* MOTOR_B_FUNCTION[] = {"suck", "rotate", "vibe", "speed", "vibe2"};

const uint32_t MODE_COLORS[] = {
  0xFF00FF,  // Magenta
  0xFF4400,  // Orange
  0x00FFFF,  // Cyan
  0xFF0066,  // Pink
  0x00FF00   // Green
};

// ===================== GLOBALS =====================
Adafruit_NeoPixel led(1, LED_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsServer webSocket(81);
WebServer server(80);
DNSServer dnsServer;
Preferences prefs;

DeviceMode currentMode = MODE_VIBE_SUCK;
bool wifiConnected = false;
bool clientConnected = false;
bool configMode = false;

// Button
unsigned long buttonPressStart = 0;
bool buttonPressed = false;
int clickCount = 0;
unsigned long lastClickTime = 0;

// LED
unsigned long lastLedUpdate = 0;
uint8_t ledPulse = 0;
bool ledPulseUp = true;

String wifiSSID = "";
String wifiPass = "";

// ===================== HTML =====================
const char CONFIG_HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>XToys ESP32</title>
  <style>
    body{font-family:system-ui;background:#1a1a2e;color:#eee;padding:20px;margin:0}
    .c{max-width:400px;margin:0 auto}
    h1{color:#ff6b9d;text-align:center}
    .card{background:#ffffff10;border-radius:12px;padding:20px;margin:15px 0}
    label{color:#aaa;font-size:14px}
    input,select{width:100%;padding:12px;margin:8px 0;border:1px solid #333;border-radius:8px;background:#1a1a2e;color:#fff;box-sizing:border-box}
    .btn{background:#ff6b9d;border:none;color:#fff;padding:15px;width:100%;border-radius:8px;font-size:16px;cursor:pointer;margin-top:15px}
    .nets{max-height:150px;overflow-y:auto;background:#0a0a15;border-radius:8px;margin:8px 0}
    .net{padding:10px;cursor:pointer;border-bottom:1px solid #222}
    .net:hover{background:#ff6b9d22}
    .mode{background:#0a0a15;padding:8px 12px;border-radius:6px;margin:4px 0;font-size:13px}
    h3{color:#ff6b9d;margin:0 0 12px}
  </style>
</head>
<body>
  <div class="c">
    <h1>XToys ESP32</h1>
    <form action="/save" method="POST">
      <div class="card">
        <h3>WiFi</h3>
        <div class="nets" id="n">Scanning...</div>
        <input name="ssid" id="s" placeholder="SSID" required>
        <input type="password" name="pass" placeholder="Password">
      </div>
      <div class="card">
        <h3>Default Mode</h3>
        <select name="mode">
          <option value="0">Vibe + Suck</option>
          <option value="1">Thrust + Rotate</option>
          <option value="2">Thrust + Vibe</option>
          <option value="3">Suck + Speed</option>
          <option value="4">Dual Vibe</option>
        </select>
        <div class="mode">A=Motor A output, B=Motor B output</div>
        <div class="mode">+ 3 optocoupler button outputs</div>
      </div>
      <button class="btn" type="submit">Save & Connect</button>
    </form>
  </div>
<script>
fetch('/scan').then(r=>r.json()).then(d=>{
  let h='';d.forEach(x=>h+='<div class="net" onclick="s.value=\''+x.s+'\'">'+x.s+' ('+x.r+'dB)</div>');
  document.getElementById('n').innerHTML=h||'None found';
});
</script>
</body>
</html>
)";

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  Serial.println("\n=== XToys ESP32 Receiver ===");

  led.begin();
  led.setBrightness(60);
  setLED(255, 0, 0);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  ledcAttach(MOTOR_A_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_A_IN2, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_IN1, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(MOTOR_B_IN2, PWM_FREQ, PWM_RESOLUTION);

  pinMode(OPTO_1, OUTPUT);
  pinMode(OPTO_2, OUTPUT);
  pinMode(OPTO_3, OUTPUT);
  digitalWrite(OPTO_1, HIGH);
  digitalWrite(OPTO_2, HIGH);
  digitalWrite(OPTO_3, HIGH);

  stopAll();

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

// ===================== LOOP =====================
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
      connectWiFi();
    }
  }

  updateLED();
  delay(1);
}

// ===================== MOTOR CONTROL =====================
// speed: -100 to +100 (negative = reverse)
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

void setOpto(int ch, bool on) {
  int pin = (ch == 1) ? OPTO_1 : (ch == 2) ? OPTO_2 : OPTO_3;
  digitalWrite(pin, on ? LOW : HIGH);  // Active LOW
}

void stopAll() {
  setMotorA(0);
  setMotorB(0);
  setOpto(1, false);
  setOpto(2, false);
  setOpto(3, false);
}

// ===================== WEBSOCKET =====================
void startWebSocket() {
  webSocket.begin();
  webSocket.onEvent(wsEvent);
  Serial.printf("WebSocket: ws://%s:81\n", WiFi.localIP().toString().c_str());
}

void wsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t len) {
  switch (type) {
    case WStype_DISCONNECTED:
      clientConnected = false;
      stopAll();
      Serial.printf("[%u] Disconnected\n", num);
      break;

    case WStype_CONNECTED:
      clientConnected = true;
      Serial.printf("[%u] Connected\n", num);
      sendDeviceInfo(num);
      break;

    case WStype_TEXT:
      handleCommand((char*)payload);
      break;
  }
}

void sendDeviceInfo(uint8_t num) {
  StaticJsonDocument<256> doc;
  doc["type"] = "deviceInfo";
  doc["mode"] = MODE_NAMES[currentMode];
  doc["modeId"] = currentMode;
  doc["motorA"] = MOTOR_A_FUNCTION[currentMode];
  doc["motorB"] = MOTOR_B_FUNCTION[currentMode];
  doc["optos"] = 3;

  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(num, out);
}

void handleCommand(char* payload) {
  Serial.printf("CMD: %s\n", payload);

  StaticJsonDocument<200> doc;
  if (deserializeJson(doc, payload) == DeserializationError::Ok) {
    // JSON format
    if (doc["cmd"] == "stop" || doc["stop"] == true) {
      stopAll();
      return;
    }

    // Motor A - accepts multiple key names
    if (doc.containsKey("a")) setMotorA(doc["a"]);
    if (doc.containsKey("motorA")) setMotorA(doc["motorA"]);
    if (doc.containsKey("vibe")) setMotorA(doc["vibe"]);
    if (doc.containsKey("vibe1")) setMotorA(doc["vibe1"]);
    if (doc.containsKey("thrust")) setMotorA(doc["thrust"]);
    if (doc.containsKey("suck") && currentMode == MODE_SUCK_SPEED) setMotorA(doc["suck"]);

    // Motor B - accepts multiple key names
    if (doc.containsKey("b")) setMotorB(doc["b"]);
    if (doc.containsKey("motorB")) setMotorB(doc["motorB"]);
    if (doc.containsKey("suck") && currentMode != MODE_SUCK_SPEED) setMotorB(doc["suck"]);
    if (doc.containsKey("rotate")) setMotorB(doc["rotate"]);
    if (doc.containsKey("vibe2")) setMotorB(doc["vibe2"]);
    if (doc.containsKey("speed")) setMotorB(doc["speed"]);

    // Optocouplers
    if (doc.containsKey("o1")) setOpto(1, doc["o1"]);
    if (doc.containsKey("o2")) setOpto(2, doc["o2"]);
    if (doc.containsKey("o3")) setOpto(3, doc["o3"]);
    if (doc.containsKey("opto1")) setOpto(1, doc["opto1"]);
    if (doc.containsKey("opto2")) setOpto(2, doc["opto2"]);
    if (doc.containsKey("opto3")) setOpto(3, doc["opto3"]);

  } else {
    // Simple format: "a:50" or "b:-30" or "o1:1"
    String msg = String(payload);
    int sep = msg.indexOf(':');
    if (sep > 0) {
      String key = msg.substring(0, sep);
      int val = msg.substring(sep + 1).toInt();
      key.toLowerCase();

      if (key == "a" || key == "motora") setMotorA(val);
      else if (key == "b" || key == "motorb") setMotorB(val);
      else if (key == "o1" || key == "opto1") setOpto(1, val > 0);
      else if (key == "o2" || key == "opto2") setOpto(2, val > 0);
      else if (key == "o3" || key == "opto3") setOpto(3, val > 0);
      else if (key == "stop") stopAll();
    }
  }
}

// ===================== WIFI =====================
void connectWiFi() {
  Serial.printf("Connecting to %s\n", wifiSSID.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID.c_str(), wifiPass.c_str());

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    handleButton();
    updateLED();
    tries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    startWebSocket();
    showModeColor();
  } else {
    wifiConnected = false;
    Serial.println("Connection failed");
  }
}

void startConfigMode() {
  configMode = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("XToys-ESP32-Setup");
  dnsServer.start(53, "*", WiFi.softAPIP());

  server.on("/", []() { server.send(200, "text/html", CONFIG_HTML); });
  server.on("/scan", []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
      if (i) json += ",";
      json += "{\"s\":\"" + WiFi.SSID(i) + "\",\"r\":" + WiFi.RSSI(i) + "}";
    }
    json += "]";
    server.send(200, "application/json", json);
  });
  server.on("/save", HTTP_POST, []() {
    wifiSSID = server.arg("ssid");
    wifiPass = server.arg("pass");
    int mode = server.arg("mode").toInt();
    prefs.begin("xtoys", false);
    prefs.putString("ssid", wifiSSID);
    prefs.putString("pass", wifiPass);
    prefs.putUInt("mode", mode);
    prefs.end();
    server.send(200, "text/html", "<h1 style='color:#ff6b9d;text-align:center;margin-top:50px'>Saved! Restarting...</h1>");
    delay(1500);
    ESP.restart();
  });
  server.onNotFound([]() { server.send(200, "text/html", CONFIG_HTML); });
  server.begin();

  Serial.printf("Config AP: XToys-ESP32-Setup @ %s\n", WiFi.softAPIP().toString().c_str());
}

// ===================== BUTTON =====================
void handleButton() {
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  unsigned long now = millis();

  if (pressed && !buttonPressed) {
    buttonPressed = true;
    buttonPressStart = now;
  } else if (!pressed && buttonPressed) {
    buttonPressed = false;
    unsigned long dur = now - buttonPressStart;

    if (dur >= 3000) {
      // Long press - reset
      prefs.begin("xtoys", false);
      prefs.clear();
      prefs.end();
      ESP.restart();
    } else if (dur > 50) {
      if (now - lastClickTime < 400) clickCount++;
      else clickCount = 1;
      lastClickTime = now;
    }
  }

  if (clickCount > 0 && now - lastClickTime > 400) {
    if (clickCount >= 2) {
      showModeColor();
      blinkMode();
    } else {
      cycleMode();
    }
    clickCount = 0;
  }

  // Long press warning flash
  if (buttonPressed && now - buttonPressStart > 1500) {
    if ((now / 150) % 2) setLED(255, 100, 0);
  }
}

void cycleMode() {
  stopAll();
  currentMode = (DeviceMode)((currentMode + 1) % MODE_COUNT);
  prefs.begin("xtoys", false);
  prefs.putUInt("mode", currentMode);
  prefs.end();
  Serial.printf("Mode: %s\n", MODE_NAMES[currentMode]);

  // Notify connected clients
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
  for (int i = 0; i <= currentMode; i++) {
    setLED(255, 255, 255);
    delay(150);
    setLED(0, 0, 0);
    delay(150);
  }
}

// ===================== LED =====================
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
    setLED(ledPulse, 0, ledPulse);  // Purple pulse
  } else if (!wifiConnected) {
    setLED(ledPulse, 0, 0);  // Red pulse
  } else if (!clientConnected) {
    setLED(255, 180, 0);  // Yellow
  } else {
    setLED(0, 255, 0);  // Green
  }
}
