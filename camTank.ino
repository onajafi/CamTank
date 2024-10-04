/* LEDC Fade Arduino Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/


#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ElegantOTA.h>
#include "config.h"

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

WebServer server(80);

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

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", []() {
    server.send(200, "text/plain", "Hi! This is camTank. github.com/onajafi");
  });

  pinMode(LED_PIN, OUTPUT);
  pinMode(IO_PIN_12, OUTPUT);
  pinMode(IO_PIN_13, OUTPUT);
  pinMode(IO_PIN_14, OUTPUT);
  pinMode(IO_PIN_15, OUTPUT);

  analogWrite(LED_PIN, LEDC_MAX_LIGHT);
  delay(500);
  analogWrite(LED_PIN, 0);
  delay(500);

  server.on("/led/on", []() {
    analogWrite(LED_PIN, LEDC_MAX_LIGHT);
    digitalWrite(IO_PIN_12, HIGH);
    digitalWrite(IO_PIN_13, HIGH);
    digitalWrite(IO_PIN_14, HIGH);
    digitalWrite(IO_PIN_15, HIGH);
    server.send(200);
  });

  server.on("/led/off", []() {
    analogWrite(LED_PIN, 0);
    digitalWrite(IO_PIN_12, LOW);
    digitalWrite(IO_PIN_13, LOW);
    digitalWrite(IO_PIN_14, LOW);
    digitalWrite(IO_PIN_15, LOW);
    server.send(200);
  });

  server.on("/motor/left", []() {
    if(server.args() == 0){
      server.send(400, "text/plain", "Please provide 'power' from 0 to " + String(MAX_MOTOR_PWM) +"\n");
    }
    int power = 0;
    for(int i=0; i<server.args(); i++){
      if(server.argName(i) == "power"){
        power = server.arg(i).toInt();
        power = power>MAX_MOTOR_PWM? MAX_MOTOR_PWM : power;
        power = power<0? 0 : power;
      }
    }
    // analogWrite(LED_PIN, 0);
    analogWrite(IO_PIN_12, power);
    server.send(200, "text/plain", "power set to " + String(power) + "\n");
    
  });

  ElegantOTA.begin(&server);    // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  server.begin();
  Serial.println("HTTP server started");

  // Initialize serial communication at 115200 bits per second:
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }

  // // Setup timer with given frequency, resolution and attach it to a led pin with auto-selected channel
  // ledcAttach(LED_PIN, LEDC_BASE_FREQ, LEDC_TIMER_12_BIT);

  // Setup and start fade on led (duty from 0 to 4095)
  // ledcFade(LED_PIN, LEDC_START_DUTY, LEDC_TARGET_DUTY, LEDC_FADE_TIME);
  // Serial.println("LED Fade on started.");

  // // Wait for fade to end
  // delay(LEDC_FADE_TIME);

  // // Setup and start fade off led and use ISR (duty from 4095 to 0)
  // ledcFadeWithInterrupt(LED_PIN, LEDC_TARGET_DUTY, LEDC_START_DUTY, LEDC_FADE_TIME, LED_FADE_ISR);
  // Serial.println("LED Fade off started.");
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
  server.handleClient();
  ElegantOTA.loop();
}

