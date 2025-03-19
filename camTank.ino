#include "esp_camera.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include "esp_http_server.h"
// #include <ElegantOTA.h>
#include "config.h"


//For the camera library
#define CAMERA_MODEL_AI_THINKER 
#include "camera_pins.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}


// use 12 bit precision for LEDC timer
#define LEDC_TIMER_12_BIT 12

// use 5000 Hz as a LEDC base frequency
#define LEDC_BASE_FREQ 5000

// fade LED PIN (replace with LED_BUILTIN constant for built-in LED)
#define LED_PIN 4
#define IO_PIN_12 12
#define IO_PIN_13 13
#define IO_PIN_14 14
#define IO_PIN_15 15
#define MAX_MOTOR_PWM 1023

// define starting duty, target duty and maximum fade time
#define LEDC_START_DUTY  (0)
#define LEDC_TARGET_DUTY (8)
#define LEDC_FADE_TIME   (100)
#define LEDC_MAX_LIGHT   (2)

bool fade_ended = false;  // status of LED fade
bool fade_on = true;

void ARDUINO_ISR_ATTR LED_FADE_ISR() {
  fade_ended = true;
}

// Function headers
void startCameraServer();
void addCustomUriCallback(char* uri_str, esp_err_t custom_handler_func(httpd_req_t *req));

static esp_err_t parse_get(httpd_req_t *req, char **obuf) {
  char *buf = NULL;
  size_t buf_len = 0;

  buf_len = httpd_req_get_url_query_len(req) + 1;
  if (buf_len > 1) {
    buf = (char *)malloc(buf_len);
    if (!buf) {
      httpd_resp_send_500(req);
      return ESP_FAIL;
    }
    if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
      *obuf = buf;
      return ESP_OK;
    }
    free(buf);
  }
  httpd_resp_send_404(req);
  return ESP_FAIL;
}

void setup() {
  //X-Configure the serial
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();


  //0-Configure the camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }


  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }
  //------------------------------------

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }
  s->set_vflip(s, 1);

  //X-
  // WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);


  //X-Wait for Wifi connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  startCameraServer();



  pinMode(LED_PIN, OUTPUT);
  pinMode(IO_PIN_12, OUTPUT);
  pinMode(IO_PIN_13, OUTPUT);
  pinMode(IO_PIN_14, OUTPUT);
  pinMode(IO_PIN_15, OUTPUT);

  analogWrite(LED_PIN, LEDC_MAX_LIGHT);
  delay(500);
  analogWrite(LED_PIN, 0);
  delay(500);


  addCustomUriCallback("/led/on", [](httpd_req_t *req) {
    analogWrite(LED_PIN, LEDC_MAX_LIGHT);
    digitalWrite(IO_PIN_12, HIGH);
    digitalWrite(IO_PIN_13, HIGH);
    digitalWrite(IO_PIN_14, HIGH);
    digitalWrite(IO_PIN_15, HIGH);
    return httpd_resp_send(req, NULL, 0);
  });

  addCustomUriCallback("/led/off", [](httpd_req_t *req) {
    analogWrite(LED_PIN, 0);
    digitalWrite(IO_PIN_12, LOW);
    digitalWrite(IO_PIN_13, LOW);
    digitalWrite(IO_PIN_14, LOW);
    digitalWrite(IO_PIN_15, LOW);
    return httpd_resp_send(req, NULL, 0);
  });

  addCustomUriCallback("/motor/left", [](httpd_req_t *req) {
    char *buf = NULL;
    char _power[32];

    if (parse_get(req, &buf) != ESP_OK) {
      return ESP_FAIL;
    }
    if(httpd_query_key_value(buf, "power", _power, sizeof(_power)) != ESP_OK){
      String msg("Please provide 'power' from 0 to " + String(MAX_MOTOR_PWM) +"\n");
      return httpd_resp_send(req, msg.c_str(), msg.length());
    }
    free(buf);

    int power = atoi(_power);
    power = power>MAX_MOTOR_PWM? MAX_MOTOR_PWM : power;
    power = power<0? 0 : power;
    
    analogWrite(IO_PIN_12, power);
    String msg("power set to " + String(power) + "\n");
    return httpd_resp_send(req, msg.c_str(), msg.length());
    
  });

  //X-
  // ElegantOTA.begin(&server);    // Start ElegantOTA
  // // ElegantOTA callbacks
  // ElegantOTA.onStart(onOTAStart);
  // ElegantOTA.onProgress(onOTAProgress);
  // ElegantOTA.onEnd(onOTAEnd);

  // // Initialize serial communication at 115200 bits per second:
  // Serial.begin(115200);
  // while (!Serial) {
  //   delay(10);
  // }

}

void loop() {
  // Check if fade_ended flag was set to true in ISR
  if (fade_ended) {
    // Serial.println("LED fade ended");
    fade_ended = false;

    // Check if last fade was fade on
    if (fade_on) {
      ledcFadeWithInterrupt(LED_PIN, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      // Serial.println("LED Fade off started.");
      fade_on = false;
    } else {
      ledcFadeWithInterrupt(LED_PIN, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
      // Serial.println("LED Fade on started.");
      fade_on = true;
    }
  }
  // ElegantOTA.loop();
}

