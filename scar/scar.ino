#include <WiFi.h>

const char* ap_ssid = "ambulance";        
const char* ap_password = "123456789";  

void setup() {
  Serial.begin(115200);


  WiFi.mode(WIFI_AP);
  

  WiFi.softAP(ap_ssid, ap_password);

  Serial.println("ESP32 is now an Access Point");
  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());  
}

void loop() {

}
