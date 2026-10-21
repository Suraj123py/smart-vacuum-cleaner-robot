#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"
#include <ESP32Servo.h>

// ===================
// Camera Model - ESP32S3 EYE
// ===================
#define CAMERA_MODEL_ESP32S3_EYE
#include "camera_pins.h"

// ===========================
// WiFi AP Credentials
// ===========================
const char* ssid = "ESP32-CAM-Infinity";
const char* password = "suraj";

httpd_handle_t camera_http = NULL;

// ===========================
// Hardware Pins (S3 Safe Mapping)
// ===========================
const int trigLeft = 1;
const int echoLeft = 2;
const int trigFront = 41;
const int echoFront = 42;
const int trigRight = 47;
const int echoRight = 48;

const int irPin = 38;

const int leftMotorForward = 10;
const int leftMotorBackward = 11;
const int rightMotorForward = 12;
const int rightMotorBackward = 13;
const int escPin = 14; 

Servo esc;

// ===========================
// Robot State Variables
// ===========================
bool autoMode = false;
bool vacOn = false;
int distLeft = 0, distFront = 0, distRight = 0;
int floorDetected = 0;

int targetFanPower = 1500;
int currentFanPower = 1500;

unsigned long actionStart = 0;
unsigned long actionDuration = 0;
bool actionActive = false;

// Function Prototypes
void moveForward();
void moveBackward();
void turnLeftPivot();
void turnRightPivot();
void stopMotors();
void smoothFanTo(int targetPower);
int getDistance(int trigPin, int echoPin);
void startAction(unsigned long duration);

// ========================================
// Home Page HTML
// ========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Smart Vacuum Cleaner</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{background:#111827;display:flex;align-items:center;justify-content:center;min-height:100vh;font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;padding:20px}
.app-shell{background:#0a0e1a;border-radius:20px;overflow:hidden;width:360px;min-height:720px;display:flex;flex-direction:column;border:1px solid #1e2540}
.status-bar{background:#0a0e1a;padding:10px 18px 4px;display:flex;justify-content:space-between;align-items:center}
.status-bar span{color:#8892b0;font-size:11px}
.status-dot{width:7px;height:7px;border-radius:50%;background:#3ecf8e;display:inline-block;margin-right:5px}
.app-header{padding:16px 18px 10px;background:#0d1117}
.app-title{color:#e2e8f0;font-size:20px;font-weight:500;letter-spacing:.5px}
.nav-tabs{display:flex;background:#0d1117;padding:0 12px 12px;gap:8px}
.nav-btn{flex:1;padding:9px 4px;border-radius:10px;border:1px solid #1e2540;background:transparent;color:#8892b0;font-size:11px;font-weight:500;cursor:pointer;display:flex;flex-direction:column;align-items:center}
.nav-btn.active{background:#1a2340;border-color:#4a90d9;color:#4a90d9}
.panel{display:none;flex:1;flex-direction:column;padding:14px;gap:12px;background:#0a0e1a}
.panel.active{display:flex}
.card{background:#0d1117;border-radius:14px;border:1px solid #1e2540;padding:14px}
.dpad{display:grid;grid-template-columns:1fr 1fr 1fr;grid-template-rows:1fr 1fr 1fr;gap:6px;width:180px;margin:0 auto}
.dpad-btn{border-radius:10px;border:1px solid #1e2540;background:#131a2e;color:#e2e8f0;cursor:pointer;display:flex;align-items:center;justify-content:center;height:52px;user-select:none}
.dpad-center{background:#1a2340;border-color:#e24b4a}
.big-btn{width:100%;padding:14px;border-radius:12px;border:none;font-size:14px;font-weight:500;cursor:pointer;margin-bottom:5px}
.btn-start{background:#1a3d2b;color:#3ecf8e}
.btn-stop{background:#3d1515;color:#e24b4a}
.btn-vacuum{background:#1a2d50;color:#4a90d9}
.cam-preview{background:#070c18;border-radius:12px;border:1px solid #1e2540;aspect-ratio:4/3;display:flex;align-items:center;justify-content:center;position:relative;overflow:hidden}
.cam-label{position:absolute;top:8px;left:10px;background:rgba(10,14,26,.8);border-radius:5px;padding:2px 8px;font-size:10px;color:#3ecf8e}
</style>
</head>
<body>
<div class="app-shell">
  <div class="status-bar">
    <span><span class="status-dot"></span>ESP32-S3 Connected</span>
    <span>192.168.4.1</span>
  </div>
  <div class="app-header">
    <div class="app-title">Smart Vacuum</div>
    <div class="app-sub">ESP32-S3 · OV2640</div>
  </div>
  <div class="nav-tabs">
    <button class="nav-btn active" onclick="switchPanel('auto')">Auto</button>
    <button class="nav-btn" onclick="switchPanel('voice')">Voice</button>
    <button class="nav-btn" onclick="switchPanel('camera')">Camera</button>
  </div>

  <div class="panel active" id="panel-auto">
    <div class="card">
      <div style="color:white;display:flex;justify-content:space-between;"><span>Vacuum</span><span id="vac-state">OFF</span></div>
      <div style="color:white;display:flex;justify-content:space-between;"><span>Dist</span><span id="dist-val">0 cm</span></div>
    </div>
    <div class="dpad">
      <div></div><button class="dpad-btn" onpointerdown="dirPress('F')" onpointerup="dirRelease()">▲</button><div></div>
      <button class="dpad-btn" onpointerdown="dirPress('L')" onpointerup="dirRelease()">◀</button>
      <button class="dpad-btn dpad-center" onclick="doStop()">■</button>
      <button class="dpad-btn" onpointerdown="dirPress('R')" onpointerup="dirRelease()">▶</button>
      <div></div><button class="dpad-btn" onpointerdown="dirPress('B')" onpointerup="dirRelease()">▼</button><div></div>
    </div>
    <button class="big-btn btn-vacuum" onclick="toggleVacuum()">Toggle Vacuum</button>
    <button class="big-btn btn-start" onclick="startAuto()">Start Auto</button>
    <button class="big-btn btn-stop" onclick="stopAuto()">Stop All</button>
  </div>

  <div class="panel" id="panel-voice">
    <div style="text-align:center;padding:10px;">
      <button class="big-btn btn-vacuum" onclick="toggleMic()">🎤 Start Listening</button>
      <p id="mic-status" style="color:#8892b0;margin-top:5px;">Tap to speak</p>
      <textarea id="v-transcript" style="width:100%;height:60px;background:#131a2e;color:#4a90d9;border:1px solid #1e2540;padding:10px;"></textarea>
      <button class="big-btn btn-start" id="v-send-btn" onclick="voiceSend()" disabled>Send Command</button>
    </div>
  </div>

  <div class="panel" id="panel-camera">
    <div class="cam-preview" id="cam-preview">
      <div id="cam-offline" style="color:#4a5568">Camera Offline</div>
      <img src="" id="stream-img" style="width:100%;display:none;">
      <div class="cam-label" id="cam-live-badge" style="display:none">LIVE</div>
    </div>
    <button class="big-btn btn-vacuum" id="cam-btn" onclick="toggleStream()">Start/Stop Stream</button>
  </div>
</div>

<script>
var vacOn=false, autoOn=false, streamOn=false;
function esp32(path, onOk) {
  fetch(path).then(r => r.text()).then(d => { if(onOk) onOk(d); });
}

function switchPanel(name){
  ['auto','voice','camera'].forEach(n => {
    document.getElementById('panel-'+n).classList.toggle('active', n===name);
  });
}

function dirPress(cmd){ esp32('/move?dir='+cmd); }
function dirRelease(){ esp32('/move?dir=S'); }
function doStop(){ esp32('/move?dir=S'); autoOn=false; vacOn=false; }
function toggleVacuum(){ esp32('/vacuum?state='+(vacOn?'off':'on'), r => { vacOn=(r==='ON'); }); }
function startAuto(){ esp32('/auto?state=on'); autoOn=true; }
function stopAuto(){ esp32('/auto?state=off'); autoOn=false; }

function toggleStream() {
  streamOn = !streamOn;
  var img = document.getElementById('stream-img');
  var off = document.getElementById('cam-offline');
  var live = document.getElementById('cam-live-badge');
  if (streamOn) {
    img.src = '/stream'; img.style.display = 'block'; off.style.display = 'none'; live.style.display = 'block';
  } else {
    img.src = ''; img.style.display = 'none'; off.style.display = 'block'; live.style.display = 'none';
  }
}

var recognition = null;
if('webkitSpeechRecognition' in window) {
  recognition = new webkitSpeechRecognition();
  recognition.lang = 'hi-IN';
  recognition.onresult = (e) => {
    var t = e.results[0][0].transcript;
    document.getElementById('v-transcript').value = t;
    document.getElementById('v-send-btn').disabled = false;
  };
}
function toggleMic(){ if(recognition) recognition.start(); }
function voiceSend(){ esp32('/cmd?text='+encodeURIComponent(document.getElementById('v-transcript').value)); }

setInterval(() => {
  fetch('/status').then(r => r.json()).then(d => {
    document.getElementById('vac-state').textContent = d.vacuum ? 'ON' : 'OFF';
    document.getElementById('dist-val').textContent = d.distFront + ' cm';
  }).catch(()=>{});
}, 1000);
</script>
</body>
</html>
)rawliteral";

static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, index_html, strlen(index_html));
}

static esp_err_t stream_handler(httpd_req_t *req) {
  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t jpg_buf_len = 0;
  uint8_t *jpg_buf = NULL;
  char part_buf[96];
  res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) return res;
  while (true) {
    delay(10);
    fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; } 
    else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len);
        esp_camera_fb_return(fb); fb = NULL;
        if (!jpeg_converted) res = ESP_FAIL;
      } else {
        jpg_buf_len = fb->len; jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", (unsigned int)jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, (const char *)jpg_buf, jpg_buf_len);
    if (res == ESP_OK) res = httpd_resp_send_chunk(req, "\r\n", 2);
    if (fb) { esp_camera_fb_return(fb); fb = NULL; } 
    else if (jpg_buf) { free(jpg_buf); jpg_buf = NULL; }
    if (res != ESP_OK) break;
  }
  return res;
}

static esp_err_t status_handler(httpd_req_t *req) {
  char json[100];
  snprintf(json, sizeof(json), "{\"vacuum\":%s,\"autoMode\":%s,\"distFront\":%d}", vacOn ? "true" : "false", autoMode ? "true" : "false", distFront);
  httpd_resp_set_type(req, "application/json");
  return httpd_resp_send(req, json, strlen(json));
}

static esp_err_t move_handler(httpd_req_t *req) {
  char query[32], dir[10];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "dir", dir, sizeof(dir)) == ESP_OK) {
      if (strcmp(dir, "F") == 0) moveForward();
      else if (strcmp(dir, "B") == 0) moveBackward();
      else if (strcmp(dir, "L") == 0) turnLeftPivot();
      else if (strcmp(dir, "R") == 0) turnRightPivot();
      else if (strcmp(dir, "S") == 0) { stopMotors(); autoMode = false; }
    }
  }
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t auto_handler(httpd_req_t *req) {
  char query[32], state[10];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "state", state, sizeof(state)) == ESP_OK) {
      if (strcmp(state, "on") == 0) { autoMode = true; vacOn = true; }
      else { autoMode = false; vacOn = false; stopMotors(); }
    }
  }
  return httpd_resp_send(req, "OK", 2);
}

static esp_err_t vacuum_handler(httpd_req_t *req) {
  char query[32], state[10];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "state", state, sizeof(state)) == ESP_OK) {
      vacOn = (strcmp(state, "on") == 0);
    }
  }
  return httpd_resp_send(req, vacOn ? "ON" : "OFF", 3);
}

static esp_err_t cmd_handler(httpd_req_t *req) {
  char query[128], text[64];
  if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
    if (httpd_query_key_value(query, "text", text, sizeof(text)) == ESP_OK) {
      String cmd = String(text);
      if (cmd.indexOf("aage") != -1) { moveForward(); startAction(1000); }
      else if (cmd.indexOf("peeche") != -1) { moveBackward(); startAction(1000); }
      else if (cmd.indexOf("baayein") != -1) { turnLeftPivot(); startAction(500); }
      else if (cmd.indexOf("daayein") != -1) { turnRightPivot(); startAction(500); }
      else if (cmd.indexOf("ruko") != -1) { stopMotors(); autoMode = false; vacOn = false; }
    }
  }
  return httpd_resp_send(req, "OK", 2);
}

void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80; config.stack_size = 8192;
  httpd_uri_t index_uri = { .uri = "/", .method = HTTP_GET, .handler = index_handler };
  httpd_uri_t stream_uri = { .uri = "/stream", .method = HTTP_GET, .handler = stream_handler };
  httpd_uri_t status_uri = { .uri = "/status", .method = HTTP_GET, .handler = status_handler };
  httpd_uri_t move_uri = { .uri = "/move", .method = HTTP_GET, .handler = move_handler };
  httpd_uri_t auto_uri = { .uri = "/auto", .method = HTTP_GET, .handler = auto_handler };
  httpd_uri_t vac_uri = { .uri = "/vacuum", .method = HTTP_GET, .handler = vacuum_handler };
  httpd_uri_t cmd_uri = { .uri = "/cmd", .method = HTTP_GET, .handler = cmd_handler };
  if (httpd_start(&camera_http, &config) == ESP_OK) {
    httpd_register_uri_handler(camera_http, &index_uri);
    httpd_register_uri_handler(camera_http, &stream_uri);
    httpd_register_uri_handler(camera_http, &status_uri);
    httpd_register_uri_handler(camera_http, &move_uri);
    httpd_register_uri_handler(camera_http, &auto_uri);
    httpd_register_uri_handler(camera_http, &vac_uri);
    httpd_register_uri_handler(camera_http, &cmd_uri);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(trigLeft, OUTPUT); pinMode(echoLeft, INPUT);
  pinMode(trigFront, OUTPUT); pinMode(echoFront, INPUT);
  pinMode(trigRight, OUTPUT); pinMode(echoRight, INPUT);
  pinMode(irPin, INPUT);
  pinMode(leftMotorForward, OUTPUT); pinMode(leftMotorBackward, OUTPUT);
  pinMode(rightMotorForward, OUTPUT); pinMode(rightMotorBackward, OUTPUT);
  ESP32PWM::allocateTimer(0); esc.setPeriodHertz(50);
  esc.attach(escPin, 1000, 2000); esc.writeMicroseconds(1000); 
  delay(2000); esc.writeMicroseconds(1500);
  WiFi.mode(WIFI_AP); WiFi.softAP(ssid, password, 1, 0, 4);
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM; config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; config.pixel_format = PIXFORMAT_JPEG;
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM; config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 20; config.fb_count = 1; config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM; config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12; config.fb_count = 1;
  }
  esp_camera_init(&config);
  startCameraServer();
}

void loop() {
  targetFanPower = vacOn ? 1800 : 1500;
  smoothFanTo(targetFanPower);
  if (actionActive) {
    if (millis() - actionStart >= actionDuration) { actionActive = false; stopMotors(); }
    return;
  }
  if (autoMode) {
    distLeft = getDistance(trigLeft, echoLeft);
    distFront = getDistance(trigFront, echoFront);
    distRight = getDistance(trigRight, echoRight);
    floorDetected = digitalRead(irPin);
    if (floorDetected == HIGH) { moveBackward(); startAction(700); }
    else if (distFront > 20 && distLeft > 20 && distRight > 20) moveForward();
    else if (distFront <= 20) { if (distLeft > distRight) turnLeftPivot(); else turnRightPivot(); startAction(500); }
    else if (distLeft <= 15) { turnRightPivot(); startAction(400); }
    else if (distRight <= 15) { turnLeftPivot(); startAction(400); }
    else stopMotors();
  }
}

void startAction(unsigned long duration) {
  delay(80); actionStart = millis(); actionDuration = duration; actionActive = true;
}

void smoothFanTo(int targetPower) {
  if (targetPower == currentFanPower) return;
  int step = (targetPower > currentFanPower) ? 5 : -5;
  while (currentFanPower != targetPower) {
    currentFanPower += step; esc.writeMicroseconds(currentFanPower); delay(2);
  }
}

int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW); delayMicroseconds(2);
  digitalWrite(trigPin, HIGH); delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, 15000);
  int d = duration * 0.034 / 2;
  return (d == 0) ? 300 : d;
}

void moveForward() { digitalWrite(leftMotorForward, HIGH); digitalWrite(leftMotorBackward, LOW); digitalWrite(rightMotorForward, HIGH); digitalWrite(rightMotorBackward, LOW); }
void moveBackward() { digitalWrite(leftMotorForward, LOW); digitalWrite(leftMotorBackward, HIGH); digitalWrite(rightMotorForward, LOW); digitalWrite(rightMotorBackward, HIGH); }
void turnLeftPivot() { digitalWrite(leftMotorForward, LOW); digitalWrite(leftMotorBackward, HIGH); digitalWrite(rightMotorForward, HIGH); digitalWrite(rightMotorBackward, LOW); }
void turnRightPivot() { digitalWrite(leftMotorForward, HIGH); digitalWrite(leftMotorBackward, LOW); digitalWrite(rightMotorForward, LOW); digitalWrite(rightMotorBackward, HIGH); }
void stopMotors() { digitalWrite(leftMotorForward, LOW); digitalWrite(leftMotorBackward, LOW); digitalWrite(rightMotorForward, LOW); digitalWrite(rightMotorBackward, LOW); }