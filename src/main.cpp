#include <WiFi.h>
#include "esp_camera.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Replace with your network credentials
const char* ssid = "realme C11";
const char* password = "omokarahisi";

// Camera pin definitions (adjust if necessary for your ESP32-CAM model)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      15
#define SIOD_GPIO_NUM       4
#define SIOC_GPIO_NUM       5
#define Y9_GPIO_NUM        16
#define Y8_GPIO_NUM        17
#define Y7_GPIO_NUM        18
#define Y6_GPIO_NUM        12
#define Y5_GPIO_NUM        10
#define Y4_GPIO_NUM         8
#define Y3_GPIO_NUM         9
#define Y2_GPIO_NUM        11
#define VSYNC_GPIO_NUM      6
#define HREF_GPIO_NUM       7
#define PCLK_GPIO_NUM      13

AsyncWebServer server(80);
const int stream_delay = 100;  // Delay between frames (ms)

// Set camera resolution
void setResolution(framesize_t frameSize) {
  sensor_t *s = esp_camera_sensor_get();
  s->set_framesize(s, frameSize);
}

// Handle MJPEG video stream
void handleStream(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    static camera_fb_t *fb = NULL;
    static size_t bytesSent = 0;
    static bool headerSent = false;

    if (!fb) {
      fb = esp_camera_fb_get();
      if (!fb) return 0;
      bytesSent = 0;
      headerSent = false;
    }

    if (!headerSent) {
      const char *header = "--frame\r\nContent-Type: image/jpeg\r\n\r\n";
      size_t headerLen = strlen(header);
      if (maxLen < headerLen) return 0;
      memcpy(buffer, header, headerLen);
      headerSent = true;
      return headerLen;
    }

    size_t remaining = fb->len - bytesSent;
    size_t toSend = min(maxLen, remaining);
    memcpy(buffer, fb->buf + bytesSent, toSend);
    bytesSent += toSend;

    if (bytesSent >= fb->len) {
      esp_camera_fb_return(fb);
      fb = NULL;
      const char *footer = "\r\n";
      if (maxLen - toSend >= 2) {
        memcpy(buffer + toSend, footer, 2);
        toSend += 2;
      }
      delay(stream_delay);
    }
    return toSend;
  });
  request->send(response);
}

// Handle settings adjustments
void handleControl(AsyncWebServerRequest *request) {
  if (request->hasParam("var") && request->hasParam("val")) {
    String param = request->getParam("var")->value();
    String valStr = request->getParam("val")->value();
    int value = valStr.toInt();

    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
      request->send(500, "text/plain", "Sensor not found");
      return;
    }

    int res = -1;
    if (param == "brightness") res = s->set_brightness(s, value);
    else if (param == "contrast") res = s->set_contrast(s, value);
    else if (param == "saturation") res = s->set_saturation(s, value);
    else if (param == "sharpness") res = s->set_sharpness(s, value);
    else if (param == "aec_value") res = s->set_aec_value(s, value);
    else if (param == "agc_gain") res = s->set_agc_gain(s, value);
    else if (param == "awb_gain") res = s->set_awb_gain(s, value);
    else if (param == "aec") res = s->set_exposure_ctrl(s, value);
    else if (param == "agc") res = s->set_gain_ctrl(s, value);
    else if (param == "awb") res = s->set_whitebal(s, value);
    else if (param == "vflip") res = s->set_vflip(s, value);
    else if (param == "hmirror") res = s->set_hmirror(s, value);
    else if (param == "colorbar") res = s->set_colorbar(s, value);
    else if (param == "special_effect") res = s->set_special_effect(s, value);
    else if (param == "resolution") {
      framesize_t frameSize;
      if (valStr == "QQVGA") frameSize = FRAMESIZE_QQVGA;
      else if (valStr == "QVGA") frameSize = FRAMESIZE_QVGA;
      else if (valStr == "VGA") frameSize = FRAMESIZE_VGA;
      else if (valStr == "SVGA") frameSize = FRAMESIZE_SVGA;
      else {
        request->send(400, "text/plain", "Invalid resolution");
        return;
      }
      setResolution(frameSize);
      request->send(200, "text/plain", "Resolution set to " + valStr);
      return;
    }

    if (res != 0) request->send(500, "text/plain", "Failed to set parameter");
    else request->send(200, "text/plain", "OK");
  } else {
    request->send(400, "text/plain", "Missing parameters");
  }
}

// Serve the updated webpage with side-by-side layout
void handleRoot(AsyncWebServerRequest *request) {
  const char html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32-CAM Control</title>
  <style>
    body { font-family: Arial, sans-serif; display: flex; margin: 0; padding: 20px; }
    .video-container { flex: 3; margin-right: 20px; }
    .settings-panel { flex: 1; background-color: #f0f0f0; padding: 20px; border-radius: 5px; }
    .settings-panel label { display: block; margin: 10px 0 5px; font-weight: bold; }
    .settings-panel input, .settings-panel select { width: 100%; box-sizing: border-box; }
    img { width: 100%; max-width: 800px; height: auto; }
  </style>
  <script>
    function setParam(param, value) {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", "/control?var=" + param + "&val=" + value + "&t=" + new Date().getTime(), true);
      xhr.send();
    }
    function setResolution() {
      var res = document.getElementById("resolution").value;
      setParam("resolution", res);
    }
    function setSpecialEffect() {
      var effect = document.getElementById("special_effect").value;
      setParam("special_effect", effect);
    }
  </script>
</head>
<body>
  <div class="video-container">
    <h1>Video Stream</h1>
    <img src="/stream" alt="Video Stream" />
  </div>
  <div class="settings-panel">
    <h2>Camera Settings</h2>
    <label>Brightness:</label>
    <input type="range" min="-2" max="2" value="0" oninput="setParam('brightness', this.value)" />
    <label>Contrast:</label>
    <input type="range" min="-2" max="2" value="2" oninput="setParam('contrast', this.value)" />
    <label>Saturation:</label>
    <input type="range" min="-2" max="2" value="0" oninput="setParam('saturation', this.value)" />
    <label>Sharpness:</label>
    <input type="range" min="-2" max="2" value="2" oninput="setParam('sharpness', this.value)" />
    <label>Exposure (AEC Value):</label>
    <input type="range" min="0" max="1200" step="10" value="500" oninput="setParam('aec_value', this.value)" />
    <label>Gain (AGC Value):</label>
    <input type="range" min="0" max="30" value="0" oninput="setParam('agc_gain', this.value)" />
    <label>White Balance Gain:</label>
    <input type="range" min="0" max="2" value="1" oninput="setParam('awb_gain', this.value)" />
    <label>Auto-Exposure (AEC):</label>
    <input type="checkbox" checked onchange="setParam('aec', this.checked ? 1 : 0)" />
    <label>Auto-Gain (AGC):</label>
    <input type="checkbox" checked onchange="setParam('agc', this.checked ? 1 : 0)" />
    <label>Auto-White Balance (AWB):</label>
    <input type="checkbox" checked onchange="setParam('awb', this.checked ? 1 : 0)" />
    <label>Vertical Flip:</label>
    <input type="checkbox" onchange="setParam('vflip', this.checked ? 1 : 0)" />
    <label>Horizontal Mirror:</label>
    <input type="checkbox" onchange="setParam('hmirror', this.checked ? 1 : 0)" />
    <label>Color Bar:</label>
    <input type="checkbox" onchange="setParam('colorbar', this.checked ? 1 : 0)" />
    <label>Resolution:</label>
    <select id="resolution" onchange="setResolution()">
      <option value="QQVGA">QQVGA (160x120)</option>
      <option value="QVGA">QVGA (320x240)</option>
      <option value="VGA" selected>VGA (640x480)</option>
      <option value="SVGA">SVGA (800x600)</option>
    </select>
    <label>Special Effect:</label>
    <select id="special_effect" onchange="setSpecialEffect()">
      <option value="0">None</option>
      <option value="1">Negative</option>
      <option value="2">Grayscale</option>
      <option value="3">Red Tint</option>
      <option value="4">Green Tint</option>
      <option value="5">Blue Tint</option>
      <option value="6">Sepia</option>
    </select>
  </div>
</body>
</html>
)rawliteral";
  request->send(200, "text/html", html);
}

// Setup function
void setup() {
  Serial.begin(115200);

  // Camera configuration
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 15000000;  // 15 MHz clock speed
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;  // High quality
  config.frame_size   = FRAMESIZE_VGA;  // Default resolution
  config.fb_count     = 1;
  config.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_contrast(s, 2);    // High contrast
  s->set_sharpness(s, 2);   // High sharpness
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  s->set_exposure_ctrl(s, 1);  // Enable AEC
  s->set_gain_ctrl(s, 1);      // Enable AGC
  s->set_whitebal(s, 1);       // Enable AWB

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED && millis() < 30000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWi-Fi connection failed");
    return;
  }

  // Start server
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/control", handleControl);
  server.begin();
  Serial.print("Camera ready at: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  // Async server handles requests
}