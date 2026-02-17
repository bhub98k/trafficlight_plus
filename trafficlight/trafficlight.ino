#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>


const char* ssid = "S23+";
const char* password = "@!abc123";
const char* server_url = "http://192.168.109.58:5000/get_light_status";
const uint8_t RED[]    = {12, 16};
const uint8_t YELLOW[] = {13, 17};
const uint8_t GREEN[]  = {14, 18};
const uint8_t BLUE[]   = {15, 19};
bool isBlueMode = false;
bool isXGreen = true;
int  currentGreenSeconds = 10;
unsigned long lastSwitchTime = 0;
bool inTransition = false;
unsigned long lastScanTime = 0;

void checkForAmbulance() {
  int n = WiFi.scanNetworks();
  bool ambulanceNear = false;

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    int rssi = WiFi.RSSI(i);

    if (ssid == "ambulance" && rssi > -30) {
      Serial.printf("Ambulance Detected! RSSI: %d\n", rssi);
      ambulanceNear = true;
      break;
    }
  }

  if (ambulanceNear && !isBlueMode) {
    HTTPClient http;
    http.begin("http://192.168.109.58:5000/emergency"); 
    int httpCode = http.POST(""); 
    Serial.printf("🔵 Emergency request sent, status: %d\n", httpCode);
    http.end();
  }
}

void setAllLightsLow() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(RED[i], LOW);
    digitalWrite(YELLOW[i], LOW);
    digitalWrite(GREEN[i], LOW);
    digitalWrite(BLUE[i], LOW);
  }
}

void applyLights() {
  setAllLightsLow();

  if (isBlueMode) {
    digitalWrite(BLUE[isXGreen ? 0 : 1], HIGH);
    digitalWrite(RED[isXGreen ? 1 : 0], HIGH);
    Serial.println("🟦 Blue Mode ON");
    return;
  }

  digitalWrite(GREEN[isXGreen ? 0 : 1], HIGH);
  digitalWrite(RED[isXGreen ? 1 : 0], HIGH);
  Serial.printf("🟢 Current Green Direction: %s, Duration: %d sec\n", isXGreen ? "X" : "Y", currentGreenSeconds);
}

void switchDirection() {
  inTransition = true;
  Serial.println("🟡 Switching with yellow transition...");

  setAllLightsLow();
  digitalWrite(YELLOW[isXGreen ? 0 : 1], HIGH);
  digitalWrite(RED[isXGreen ? 1 : 0], HIGH);
  delay(3000);

  isXGreen = !isXGreen;
  applyLights();

  lastSwitchTime = millis();
  inTransition = false;
}

void setup() {
  Serial.begin(115200);
  
  for (int i = 0; i < 2; i++) {
    pinMode(RED[i], OUTPUT);
    pinMode(YELLOW[i], OUTPUT);
    pinMode(GREEN[i], OUTPUT);
    pinMode(BLUE[i], OUTPUT);
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n WiFi Connected");

  applyLights();
  lastSwitchTime = millis();
}

void loop() {
  unsigned long now = millis();
  
  if (now - lastScanTime > 1000) {
    checkForAmbulance();
    lastScanTime = now;
  }
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(server_url);
    int httpCode = http.GET();

    if (httpCode == 200) {
      String payload = http.getString();
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        bool newBlueMode = doc["blue"];
        String priority = doc["priority"];
        int greenSec = doc["green_sec"];

        currentGreenSeconds = greenSec;
        isBlueMode = newBlueMode;

        if (isBlueMode) {
          applyLights();  
        } else {
          if (!inTransition && (millis() - lastSwitchTime >= currentGreenSeconds * 1000)) {
            switchDirection();
          }
        }
      } else {
        Serial.println("⚠️ JSON parse failed");
      }
    } else {
      Serial.printf("⚠️ HTTP Error: %d\n", httpCode);
    }

    http.end();
  } else {
    Serial.println("⚠️ Lost WiFi connection!");
  }

  delay(1000);
}
