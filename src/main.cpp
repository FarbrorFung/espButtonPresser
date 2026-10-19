#include "credentials.h"
#include "servo_config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <cstddef>
#include <cstdint>

#include "lwip/ip_addr.h"
#include "ping/ping_sock.h"

#include <ESP32Servo.h>

Servo myServo;

#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);

// GLOBAL VARIABLES

int status = WL_IDLE_STATUS;
volatile int transmitted_counts = 0;
volatile int received_counts = 0;
volatile int button_press_count = 0;

volatile unsigned long when_was_button_pressed;
volatile unsigned long last_check;

void push_the_button() {
  int pos;
  for (pos = 0; pos <= SERVO_ANGLE; pos += 1) {
    myServo.write(pos);
    delay(15);
  }
  delay(200);
  when_was_button_pressed = millis();
  myServo.write(0);
  Serial.println("Button hopefully pushed");
  button_press_count += 1;
}

static void on_ping_success(esp_ping_handle_t hdl, void *args) {
  uint8_t ttl;
  uint16_t seqno;
  uint32_t elapsed_time, recv_len;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time,
                       sizeof(elapsed_time));

  printf("%iu bytes from %s icmp_seq=%d ttl=%d time=%i ms\n", recv_len,
         ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void on_ping_timeout(esp_ping_handle_t hdl, void *args) {
  uint16_t seqno;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  printf("From %s icmp_seq=%d timeout\n", ipaddr_ntoa(&target_addr), seqno);
}

static void on_ping_end(esp_ping_handle_t hdl, void *args) {
  uint32_t transmitted;
  uint32_t received;
  uint32_t total_time_ms;
  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
                       sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
                       sizeof(total_time_ms));
  printf("%d packets transmitted, %d received, time %dms\n", transmitted,
         received, total_time_ms);

  transmitted_counts += transmitted;
  received_counts += received;
  last_check = millis();

  esp_ping_delete_session(hdl);

  // Move servo if all pings timeout
  if (received == 0) {
    if (WiFi.status() != WL_CONNECTED) {
      return;
    }
    Serial.println("Failed to receive packets, triggering servo!");
    push_the_button();
  }
}

void initialize_ping() {
  esp_ping_config_t config = ESP_PING_DEFAULT_CONFIG();
  config.count = 4;
  config.interval_ms = 2000;
  ipaddr_aton(IP_TO_PING, &config.target_addr);
  esp_ping_callbacks_t cbs;
  cbs.cb_args = NULL;
  cbs.on_ping_success = on_ping_success;
  cbs.on_ping_end = on_ping_end;
  cbs.on_ping_timeout = on_ping_timeout;

  esp_ping_handle_t ping;
  esp_ping_new_session(&config, &cbs, &ping);
  esp_ping_start(ping);
}

// Startup - Start serial output for monitoring, Connect to Wifi
void setup() {
  // Serial Connection.
  Serial.begin(115200);
  delay(2000); // delay so esp don't print before monitor is connected.
  Serial.println("Program Started!");

  // Setup the servo
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // Begin connectig to Wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (status != WL_CONNECTED) { // Connection loop
    status = WiFi.status();
    Serial.println("Not Connected");
    delay(5000);
  }

  Serial.println("Success!\nYou are connected to: " +
                 WiFi.SSID()); // Connected!
  Serial.println("Ip Address: " + WiFi.localIP().toString());

  // Setup Webserver routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "hello");
  });
  server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *response = request->beginResponseStream("text/plain");
    response->printf("Packets Transmitted: %i\n", transmitted_counts);
    response->printf("Packets Received: %i\n", received_counts);
    response->printf("Button Presses: %i\n", button_press_count);
    if (button_press_count > 0) {
      response->printf("Time since last button press: %lu s\n",
                       (millis() - when_was_button_pressed) / 1000);
    }
    response->printf("Last checked: %lu s ago\n",
                     (millis() - last_check) / 1000);
    request->send(response);
  });
  server.begin();
  Serial.println("Server should be started");
}

void loop() {
  while (WiFi.status() == WL_CONNECTED) {
    initialize_ping();
    delay(CHECK_INTERVAL);
  }

  status = WiFi.status(); // This runs if Wifi drops.

  while (status != WL_CONNECTED) {
    status = WiFi.status();
    Serial.println("Not Connected, retrying!");
    delay(5000);
  }
}
