#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_VL53L0X.h>
#include <Adafruit_TCS34725.h>

// WiFi and MQTT settings
const char* ssid = "your_SSID";
const char* password = "your_PASSWORD";
const char* mqtt_server = "your_MQTT_SERVER";

// Drone settings
const char* drone_id = "drone_2"; // Change for each drone

WiFiClient espClient;
PubSubClient client(espClient);

// LIDAR and Color sensor objects
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  pinMode(D4, OUTPUT); // LED pin for status indication
  pinMode(D3, OUTPUT); // Example pin to stop flight controller

  // Initialize LIDAR and Color sensor
  if (!lox.begin()) {
    Serial.println(F("Failed to boot VL53L0X"));
    while (1);
  }

  if (!tcs.begin()) {
    Serial.println(F("Failed to boot TCS34725"));
    while (1);
  }
}

void setup_wifi() {
  delay(10);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == "drone/command" && message == "stop") {
    // Stop drone
    digitalWrite(D4, HIGH); // Turn on LED to indicate stop
    digitalWrite(D3, HIGH); // Send stop signal to flight controller
    // Add any additional code to stop the flight controller here
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect(drone_id)) {
      client.subscribe("drone/command");
    } else {
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  bool targetDetected = checkTarget();

  if (targetDetected) {
    client.publish("drone/status", "target_found");
    digitalWrite(D4, HIGH); // Indicate target found
  } else {
    digitalWrite(D4, LOW); // Indicate searching
  }

  delay(1000); // Adjust delay as necessary
}

bool checkTarget() {
  int height = measureHeight();
  if (height == 15) {
    int length = measureLength();
    int width = measureWidth();
    if (length == 15 && width == 15) {
      String color = detectColor();
      if (color == "green") {
        return true;
      }
    }
  }
  return false;
}

int measureHeight() {
  VL53L0X_RangingMeasurementData_t measure;
  lox.rangingTest(&measure, false);
  if (measure.RangeStatus != 4) {
    return measure.RangeMilliMeter / 10; // Convert mm to cm
  } else {
    return 0;
  }
}

int measureLength() {
  // Measure the length by taking multiple LIDAR measurements while moving slightly
  int totalLength = 0;
  int numMeasurements = 5; // Number of measurements to average
  for (int i = 0; i < numMeasurements; i++) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      totalLength += measure.RangeMilliMeter / 10; // Convert mm to cm
    }
    delay(50); // Small delay between measurements
  }
  return totalLength / numMeasurements; // Average length
}

int measureWidth() {
  // Measure the width by taking multiple LIDAR measurements while moving slightly
  int totalWidth = 0;
  int numMeasurements = 5; // Number of measurements to average
  for (int i = 0; i < numMeasurements; i++) {
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      totalWidth += measure.RangeMilliMeter / 10; // Convert mm to cm
    }
    delay(50); // Small delay between measurements
  }
  return totalWidth / numMeasurements; // Average width
}

String detectColor() {
  uint16_t r, g, b, c;
  tcs.getRawData(&r, &g, &b, &c);
  uint16_t colorTemp = tcs.calculateColorTemperature(r, g, b);
  uint16_t lux = tcs.calculateLux(r, g, b);

  // Simple thresholding to detect green color
  if (g > r && g > b) {
    return "green";
  }
  return "other";
}
