#include "watchdog.h"
#include "platform.h"
#include "config.h"
#include "log.h"
#include "wifi_mgr.h"
#include "mqtt_mgr.h"
#include "meters.h"

static const uint32_t LOOP_WDT_SEC = 45;
static const uint32_t MIN_FREE_HEAP = 6000;

#if defined(ESP8266)
  #include <Ticker.h>
static Ticker wdtTicker;
static volatile uint32_t lastFeed = 0;

static void IRAM_ATTR wdtCheck() {
  // runs from a timer; if loop() has been stuck (while still yielding) for too long, hard reset
  if (millis() - lastFeed > LOOP_WDT_SEC * 1000UL) ESP.reset();
}
#else
  #include <esp_task_wdt.h>
#endif

void watchdogBegin() {
#if defined(ESP8266)
  ESP.wdtEnable(WDTO_8S);   // SDK soft + hardware watchdog (on by default, made explicit)
  lastFeed = millis();
  wdtTicker.attach(1.0f, wdtCheck);
#else
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
  esp_task_wdt_config_t c = {};
  c.timeout_ms = LOOP_WDT_SEC * 1000;
  c.idle_core_mask = 0;
  c.trigger_panic = true;
  if (esp_task_wdt_init(&c) == ESP_ERR_INVALID_STATE) esp_task_wdt_reconfigure(&c);
  #else
  esp_task_wdt_init(LOOP_WDT_SEC, true);
  #endif
  esp_task_wdt_add(NULL);   // the Arduino loop task
#endif
  logf("Watchdog armed (%lu s)", (unsigned long)LOOP_WDT_SEC);
}

void watchdogFeed() {
#if defined(ESP8266)
  lastFeed = millis();
  ESP.wdtFeed();
#else
  esp_task_wdt_reset();
#endif
}

static uint32_t lastCheck = 0;
static uint32_t mqttDownSince = 0;

void watchdogLoop() {
  uint32_t now = millis();
  if (now - lastCheck < 1000) return;
  lastCheck = now;

  if (freeHeap() < MIN_FREE_HEAP) {
    logf("WATCHDOG: free heap %lu bytes - rebooting", (unsigned long)freeHeap());
    requestReboot(100);
    return;
  }

  // WiFi: only when a network is configured and nobody is using the fallback AP
  uint32_t ds = wifiDownSinceMs();
  if (cfg.wdNoWifiMin && cfg.ssid.length() && ds && wifiApClients() == 0 &&
      now - ds > cfg.wdNoWifiMin * 60000UL) {
    logf("WATCHDOG: WiFi down for %u min - rebooting", cfg.wdNoWifiMin);
    requestReboot(100);
    return;
  }

  // MQTT: only counts while WiFi is up
  bool mqttWanted = cfg.mqttEnabled && cfg.mqttHost.length() && wifiConnected();
  if (!mqttWanted || mqttConnected()) {
    mqttDownSince = 0;
  } else if (!mqttDownSince) {
    mqttDownSince = now;
  } else if (cfg.wdNoMqttMin && now - mqttDownSince > cfg.wdNoMqttMin * 60000UL) {
    logf("WATCHDOG: MQTT down for %u min - rebooting", cfg.wdNoMqttMin);
    requestReboot(100);
    return;
  }

  // Modbus
  if (cfg.wdNoModbusMin && !meters().empty()) {
    uint32_t last = metersLastAnyOkMs();
    if (now - last > cfg.wdNoModbusMin * 60000UL) {
      logf("WATCHDOG: no meter answered for %u min - rebooting", cfg.wdNoModbusMin);
      requestReboot(100);
    }
  }
}
