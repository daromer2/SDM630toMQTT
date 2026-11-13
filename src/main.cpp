/*
 * SDM630toMQTT v2 - Eastron SDM energy meters (Modbus RTU / RS485) -> MQTT + Home Assistant
 *
 * Boards: ESP8266 (Wemos D1 mini and clones), ESP32-S3.
 *
 * Everything in loop() is non-blocking: Modbus requests are sent and their answers collected
 * step by step, MQTT reconnects with back-off, WiFi is a small state machine with AP fallback.
 * A loop watchdog resets the chip if loop() ever stops running.
 */
#include "platform.h"
#include "log.h"
#include "config.h"
#include "meters.h"
#include "mqtt_mgr.h"
#include "web.h"
#include "wifi_mgr.h"
#include "watchdog.h"
#include <LittleFS.h>
#include <time.h>

void setup() {
  LOG_SERIAL.begin(115200);
#if defined(ESP32) && ARDUINO_USB_CDC_ON_BOOT && ARDUINO_USB_MODE
  Serial.setTxTimeoutMs(0);   // never block on logging when no USB host is connected
#endif
  logBegin(&LOG_SERIAL);
  logf("SDM630toMQTT %s on %s, id %s, reset reason: %s", FW_VERSION, BOARD_NAME, hardwareId().c_str(), resetReasonStr().c_str());

  watchdogBegin();

#if defined(ESP32)
  if (!LittleFS.begin(true)) logf("LittleFS mount failed");
#else
  if (!LittleFS.begin()) logf("LittleFS mount failed");
#endif
  configLoad();

  wifiBegin();
  metersBegin();
  mqttBegin();
  metersOnMeterDone(mqttPublishMeter);
  metersOnCycleStart(mqttOnCycleStart);
  webBegin();

#if defined(ESP8266)
  configTime(cfg.tz.c_str(), cfg.ntpServer.c_str());
#else
  configTzTime(cfg.tz.c_str(), cfg.ntpServer.c_str());
#endif
  logf("Setup done, free heap %lu", (unsigned long)freeHeap());
}

void loop() {
  watchdogFeed();
  wifiLoop();
  webLoop();
  metersLoop();
  mqttLoop();
  watchdogLoop();
  handleRebootRequest();
  delay(1);
}
