#pragma once
#include <Arduino.h>

enum WiFiState : uint8_t {
  WIFI_TRY_CONNECT = 0,
  WIFI_CONNECTED   = 1,
  WIFI_AP_MODE     = 2
};

struct WiFiStatus {
  volatile bool connected = false;
  volatile WiFiState state = WIFI_TRY_CONNECT;
  char ip[16] = {0};     // "192.168.1.10"
  int  rssi = 0;
};

extern WiFiStatus wifiStatus;

// Optional callback: called when WiFi just became connected
typedef void (*WiFiOnConnectedCb)();

void wifiBegin(WiFiOnConnectedCb cb = nullptr);

// Start FreeRTOS task (ESP32)
void startWiFiTask(uint32_t stackWords = 4096, UBaseType_t prio = 2);

// Utilities
bool wifiIsConnected();
String wifiGetSSID();
