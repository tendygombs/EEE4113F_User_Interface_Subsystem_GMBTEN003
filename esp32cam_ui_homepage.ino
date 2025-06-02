// esp32cam_ui_homepage.ino - Full UI with Homepage Design and Button Functions
#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include "SPIFFS.h"
#include <FS.h>

const char* ssid = "47";
const char* password = "wYSiTTvLJiH";

WebServer server(80);

#define FLASH_LED_PIN 4

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

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
  } else {
    Serial.println("Camera initialized successfully");
  }
}

const char homepage_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Wildlife Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { background: #111; color: #eee; font-family: Arial, sans-serif; text-align: center; }
    h1 { color: #58f; margin-top: 10px; }
    p { font-size: 18px; }
    img { max-width: 90%; margin: 10px auto; border-radius: 12px; }
    button {
      display: block;
      width: 80%;
      margin: 10px auto;
      padding: 14px;
      font-size: 16px;
      background: #58f;
      color: white;
      border: none;
      border-radius: 8px;
    }
  </style>
</head>
<body>
  <h1>WELCOME</h1>
  <div style="display: flex; justify-content: center; align-items: center; gap: 20px; flex-wrap: wrap;">
    <img src="/penguins" alt="African Penguins" style="max-width: 45%; border-radius: 12px;">
    <img src="/stream" id="stream" style="border: 2px solid #58f; max-width: 45%; border-radius: 12px;">
  </div>
  <p>Select an option below:</p>
  <button onclick="captureImage()">Capture a Screenshot</button>
  <button onclick="window.location='/download'">Download Images</button>
  <button onclick="window.location='/health'">System Health Check</button>
  <button onclick="window.location='/library'">Image Library</button>
  <script>
    function captureImage() {
      fetch('/capture')
        .then(response => {
          if (!response.ok) throw new Error('Capture failed');
          return response.text();
        })
        .then(msg => alert("Screenshot successfully taken."))
        .catch(err => alert("Failed to take screenshot."));
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", homepage_html);
}

void handlePenguinImage() {
  File file = SPIFFS.open("/penguins.webp", "r");
  if (!file) {
    server.send(404, "text/plain", "Image not found");
    return;
  }
  server.streamFile(file, "image/webp");
  file.close();
}

void handleStream() {
  WiFiClient client = server.client();
  String boundary = "frame";
  client.print("HTTP/1.1 200 OK\r\nContent-Type: multipart/x-mixed-replace; boundary=" + boundary + "\r\n\r\n");
  while (1) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera frame buffer failed");
      continue;
    }
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
  Serial.println("Capture request received");

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Capture failed: No frame buffer");
    return;
  }
  Serial.printf("Captured image size: %d bytes\n", fb->len);

  char filename[32];
  snprintf(filename, sizeof(filename), "/pic_%lu.jpg", millis());

  Serial.printf("Saving image to: %s\n", filename);

  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    esp_camera_fb_return(fb);
    server.send(500, "text/plain", "Capture failed: File open failed");
    return;
  }

  size_t written = file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  Serial.printf("Wrote %u bytes to %s\n", written, filename);

  if (written != fb->len) {
    Serial.println("File write incomplete");
    server.send(500, "text/plain", "Capture failed: File write incomplete");
    return;
  }

  Serial.println("Screenshot saved successfully");
  server.send(200, "text/plain", "Screenshot saved");
}

void handleDownload() {
  server.send(200, "text/plain", "[TODO] Bundle and download all images");
}

void handleHealth() {
  String report = "ESP32-CAM Health:\n";
  report += "Uptime: " + String(millis() / 1000) + " seconds\n";
  report += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n";
  report += "Chip Temperature: (N/A on ESP32-CAM)\n";
  server.send(200, "text/plain", report);
}

void handleLibrary() {
  File root = SPIFFS.open("/");
  String page = "<html><body style='background:#111;color:white;text-align:center;'>";
  page += "<h2>Image Library</h2><div>";

  while (File file = root.openNextFile()) {
    if (!file.isDirectory()) {
      String name = file.name();
      page += "<img src='" + name + "' width='300'><br><hr>";
    }
    file.close();
  }

  page += "</div></body></html>";
  server.send(200, "text/html", page);
}

void setup() {
  Serial.begin(115200);
  Serial.println("Starting WiFi...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.println(WiFi.localIP());

  Serial.println("Mounting SPIFFS...");
  if(!SPIFFS.begin(true)){
    Serial.println("SPIFFS Mount Failed");
    return;
  }
  Serial.println("SPIFFS mounted successfully");

  // No SD card mounting here since none inserted

  setupCamera();

  server.on("/", handleRoot);
  server.on("/penguins", handlePenguinImage);
  server.on("/stream", HTTP_GET, handleStream);
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/download", handleDownload);
  server.on("/health", handleHealth);
  server.on("/library", handleLibrary);
  server.begin();
  Serial.println("Server started");

  server.serveStatic("/", SPIFFS, "/");
}

void loop() {
  server.handleClient();
}
