#include <WiFi.h>
#include "esp_camera.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// Network credentials (replace with your own)
const char* ssid = "realme C11";
const char* password = "omokarahisi";

// Camera pin definitions (unchanged as per your working configuration)
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
const int stream_delay = 100;  // Delay between frames in milliseconds

//
// Handle MJPEG stream asynchronously
//
void handleStream(AsyncWebServerRequest *request) {
  AsyncWebServerResponse *response = request->beginChunkedResponse("multipart/x-mixed-replace; boundary=frame", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    static camera_fb_t *fb = NULL;
    static size_t bytesSent = 0;
    static bool headerSent = false;

    if (!fb) {
      fb = esp_camera_fb_get();
      if (!fb) {
        Serial.println("Camera capture failed");
        return 0;
      }
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
      delay(stream_delay);  // Delay between frames
    }

    return toSend;
  });
  request->send(response);
}

//
// Handle slider control commands with detailed logging
//
void handleControl(AsyncWebServerRequest *request) {
  Serial.println("Received control request");
  if (request->hasParam("var") && request->hasParam("val")) {
    String param = request->getParam("var")->value();
    int value = request->getParam("val")->value().toInt();
    Serial.print("Control command: ");
    Serial.print(param);
    Serial.print(" = ");
    Serial.println(value);

    sensor_t* s = esp_camera_sensor_get();
    if (!s) {
      Serial.println("Sensor not found");
      request->send(500, "text/plain", "Sensor not found");
      return;
    }

    int res = -1;
    if (param == "brightness") {
      Serial.print("Current brightness: ");
      Serial.println(s->status.brightness);
      res = s->set_brightness(s, value);
      Serial.print("New brightness: ");
      Serial.println(s->status.brightness);
    } else if (param == "contrast") {
      Serial.print("Current contrast: ");
      Serial.println(s->status.contrast);
      res = s->set_contrast(s, value);
      Serial.print("New contrast: ");
      Serial.println(s->status.contrast);
    } else if (param == "saturation") {
      Serial.print("Current saturation: ");
      Serial.println(s->status.saturation);
      res = s->set_saturation(s, value);
      Serial.print("New saturation: ");
      Serial.println(s->status.saturation);
    } else if (param == "sharpness") {
      Serial.print("Current sharpness: ");
      Serial.println(s->status.sharpness);
      res = s->set_sharpness(s, value);
      Serial.print("New sharpness: ");
      Serial.println(s->status.sharpness);
    }

    if (res != 0) {
      Serial.println("Failed to set parameter");
      request->send(500, "text/plain", "Failed to set parameter");
    } else {
      request->send(200, "text/plain", "OK");
    }
  } else {
    request->send(400, "text/plain", "Missing parameters");
  }
}

//
// Serve the web page with the live stream and slider controls
//
void handleRoot(AsyncWebServerRequest *request) {
  const char html[] PROGMEM = R"rawliteral(
<html>
  <head>
    <title>ESP32-CAM Stream & Control</title>
    <style>
      body { font-family: Arial; text-align: center; }
    </style>
    <script>
      function setParam(param, value) {
        console.log("Slider changed: " + param + " = " + value);
        var xhr = new XMLHttpRequest();
        xhr.open("GET", "/control?var=" + param + "&val=" + value + "&t=" + new Date().getTime(), true);
        xhr.send();
      }
    </script>
  </head>
  <body>
    <h1>ESP32-CAM Stream & Control</h1>
    <img src="/stream" style="width:640px" /><br/><br/>
    <label>Brightness:</label><br/>
    <input type="range" min="-2" max="2" step="1" value="0" oninput="setParam('brightness', this.value)" /><br/><br/>
    <label>Contrast:</label><br/>
    <input type="range" min="-2" max="2" step="1" value="2" oninput="setParam('contrast', this.value)" /><br/><br/>
    <label>Saturation:</label><br/>
    <input type="range" min="-2" max="2" step="1" value="0" oninput="setParam('saturation', this.value)" /><br/><br/>
    <label>Sharpness:</label><br/>
    <input type="range" min="-2" max="2" step="1" value="2" oninput="setParam('sharpness', this.value)" /><br/><br/>
  </body>
</html>
)rawliteral";
  request->send(200, "text/html", html);
}

//
// Setup: initialize camera, Wi-Fi, and web server endpoints
//
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.xclk_freq_hz = 15000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 10;  // High quality (lower number means higher quality)
  config.frame_size   = FRAMESIZE_VGA;  // 640x480 for better text capture

  // Use PSRAM if available for frame buffer
  if (psramFound()) {
    config.fb_location = CAMERA_FB_IN_PSRAM;
    Serial.println("Using PSRAM for frame buffer");
  } else {
    config.fb_location = CAMERA_FB_IN_DRAM;
    Serial.println("Using DRAM for frame buffer");
  }
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x\n", err);
    return;
  }

  sensor_t* s = esp_camera_sensor_get();
  s->set_contrast(s, 2);    // High contrast for text
  s->set_sharpness(s, 2);   // High sharpness for text
  s->set_brightness(s, 0);
  s->set_saturation(s, 0);
  // s->set_exposure_ctrl(s, false);  // Optional: disable auto-exposure for manual control

  Serial.printf("Free heap after camera init: %u bytes\n", ESP.getFreeHeap());
  Serial.println("Connecting to Wi-Fi...");

  WiFi.begin(ssid, password);
  int timeout = 100;
  while (WiFi.status() != WL_CONNECTED && timeout > 0) {
    delay(500);
    Serial.print(".");
    timeout--;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to Wi-Fi.");
    return;
  }

  // Set up server endpoints
  server.on("/", handleRoot);
  server.on("/stream", handleStream);
  server.on("/control", handleControl);
  server.begin();
  Serial.print("Camera Stream Ready! Go to: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/");
}

//
// Main loop: nothing needed with AsyncWebServer
//
void loop() {
  // AsyncWebServer handles requests in the background
}