// esp32cam_ui_toggle_mode.ino
// Toggle between stream mode and capture mode for reliability

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "47";
const char* password = "wYSiTTvLJiH";

WebServer server(80);

#define FLASH_LED_PIN 4
bool isCaptureMode = false;

// ==== Camera Configuration Function ====
void setupCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = 5;
  config.pin_d1 = 18;
  config.pin_d2 = 19;
  config.pin_d3 = 21;
  config.pin_d4 = 36;
  config.pin_d5 = 39;
  config.pin_d6 = 34;
  config.pin_d7 = 35;
  config.pin_xclk = 0;
  config.pin_pclk = 22;
  config.pin_vsync = 25;
  config.pin_href = 23;
  config.pin_sscb_sda = 26;
  config.pin_sscb_scl = 27;
  config.pin_pwdn = 32;
  config.pin_reset = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  esp_camera_init(&config);
}

// ==== Web UI HTML ====
const char homepage_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background: #111; color: #eee; font-family: Arial; text-align: center; }
    h1 { color: #58f; }
    button { padding: 12px 20px; font-size: 16px; margin: 8px; background: #58f; color: white; border: none; border-radius: 8px; }
    img { max-width: 90%; margin: 10px auto; border-radius: 12px; }
  </style>
</head>
<body>
  <h1>ESP32-CAM Toggle Mode</h1>
  <div id="mode"></div>
  <button onclick="switchMode()" id="modeBtn">🔄 Switch Mode</button>
  <div id="streamView" style="display:none;">
    <img id="stream" src="/stream">
  </div>
  <div id="captureView" style="display:none;">
    <button onclick="captureImage()">📸 Take Screenshot</button><br>
    <img id="photo" style="display:none; margin-top:15px; max-width: 480px;">
  </div>
  <script>
    let currentMode = 'stream';

    function switchMode() {
      fetch('/switch').then(() => {
        location.reload();
      });
    }

    function captureImage() {
      fetch('/capture')
        .then(response => response.blob())
        .then(blob => {
          const url = URL.createObjectURL(blob);
          const img = document.getElementById('photo');
          img.src = url;
          img.style.display = 'block';
        })
        .catch(error => console.error("Capture error:", error));
    }

    window.onload = () => {
      fetch('/mode').then(res => res.text()).then(mode => {
        currentMode = mode;
        document.getElementById('mode').innerText = `Current Mode: ${mode.toUpperCase()}`;
        if (mode === 'stream') {
          document.getElementById('streamView').style.display = 'block';
        } else {
          document.getElementById('captureView').style.display = 'block';
        }
      });
    };
  </script>
</body>
</html>
)rawliteral";

// ==== Request Handlers ====
void handleRoot() {
  server.send_P(200, "text/html", homepage_html);
}

void handleMode() {
  server.send(200, "text/plain", isCaptureMode ? "capture" : "stream");
}

void handleSwitchMode() {
  esp_camera_deinit();
  isCaptureMode = !isCaptureMode;
  if (!isCaptureMode) setupCamera(); // reinit only for stream mode
  server.send(200, "text/plain", "OK");
}

void handleStream() {
  if (isCaptureMode) {
    server.send(403, "text/plain", "Not in stream mode");
    return;
  }
  WiFiClient client = server.client();
  String boundary = "frame";
  client.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n\r\n");
  while (1) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;
    client.printf("--%s\r\n", boundary.c_str());
    client.println("Content-Type: image/jpeg");
    client.printf("Content-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");
    esp_camera_fb_return(fb);
    if (!client.connected()) break;
  }
}

void handleCapture() {
  if (!isCaptureMode) {
    server.send(403, "text/plain", "Not in capture mode");
    return;
  }
  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }
  server.sendHeader("Content-Type", "image/jpeg");
  server.sendHeader("Content-Length", String(fb->len));
  server.send(200);
  WiFiClient client = server.client();
  client.write(fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// ==== Setup and Loop ====
void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  setupCamera();

  server.on("/", handleRoot);
  server.on("/mode", handleMode);
  server.on("/switch", handleSwitchMode);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.begin();
  Serial.println("Server started");
}

void loop() {
  server.handleClient();
}

