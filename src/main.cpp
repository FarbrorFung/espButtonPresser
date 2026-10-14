#include "HardwareSerial.h"
#include "WiFiType.h"
#include "esp32-hal.h"
#include <Arduino.h>
#include <WiFi.h>
#include "credentials.h"

int status = WL_IDLE_STATUS;

// Startup - Start serial output for monitoring, Connect to Wifi
void setup() {
  // Serial Connection.
  Serial.begin(115200);
  delay(2000); // delay so esp don't print before monitor is connected.
  Serial.println("Program Started!");

  // Begin connectig to Wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (status != WL_CONNECTED) { // Connection loop
    status = WiFi.status();
    Serial.println("Not Connected");
    delay(5000);
  }

  Serial.println("Success!\nYou are connected to: " + WiFi.SSID()); // Connected!
  Serial.println("Ip Address: " + WiFi.localIP().toString());
}

void loop() {}
