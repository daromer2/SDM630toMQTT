// Thin abstraction over the ESP8266 and ESP32 Arduino cores.
#pragma once
#include <Arduino.h>

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESP8266WebServer.h>
  #include <ESP8266HTTPUpdateServer.h>
  using WebServerT    = ESP8266WebServer;
  using UpdateServerT = ESP8266HTTPUpdateServer;
  #define BOARD_NAME "ESP8266"
  #define MODBUS_SERIAL Serial
  #define LOG_SERIAL    Serial1
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WebServer.h>
  #include <HTTPUpdateServer.h>
  using WebServerT    = WebServer;
  using UpdateServerT = HTTPUpdateServer;
  #define BOARD_NAME "ESP32-S3"
  #define MODBUS_SERIAL Serial1
  #define LOG_SERIAL    Serial
#else
  #error "Only ESP8266 and ESP32(-S3) are supported"
#endif

#ifndef FW_VERSION
  #define FW_VERSION "2.0.0-dev"
#endif

#define MAX_METERS 10

// 6 hex digit hardware id (same format as the old firmware: ESP8266 chip id)
const String &hardwareId();
String resetReasonStr();
uint32_t freeHeap();
uint32_t vccMilliVolt();   // 0 when not available

// Request a clean reboot from loop() after `delayMs`.
void requestReboot(uint32_t delayMs = 500);
void handleRebootRequest();
