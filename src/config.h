// Persistent configuration (LittleFS /config.json), editable from the web UI.
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include "registers.h"

enum MqttFormat : uint8_t { FMT_TOPICS = 0, FMT_JSON = 1, FMT_BOTH = 2 };

struct MeterCfg {
  bool enabled = true;
  uint8_t addr = 1;          // Modbus slave address 1..247
  String type = "630";       // profile id
  String name;               // display name (UI / Home Assistant)
  String topic;              // sub topic, empty = Modbus address (old behaviour)
  String mask;               // per register '1'/'0'. Wrong length = all enabled

  bool regEnabled(uint8_t idx, uint8_t count) const {
    if (mask.length() != count) return true;
    return mask[idx] == '1';
  }
  String effectiveTopic() const { return topic.length() ? topic : String(addr); }
  String displayName() const { return name.length() ? name : (String(F("Meter ")) + addr); }
};

struct Config {
  // device
  String hostname;           // empty = "sdm-gateway-<id>"
  String adminPass;          // web UI password (user "admin"), empty = no auth

  // wifi
  String ssid, wifiPass;
  String apPass = "sdm630mqtt";  // fallback AP password (min 8 chars, empty = open)
  uint16_t apFallbackSec = 60;   // start AP after WiFi is down this long
  uint16_t staRetrySec = 60;     // while in AP fallback, retry WiFi this often
  bool staticIp = false;
  String ip, gateway, subnet = "255.255.255.0", dns;

  // mqtt
  bool mqttEnabled = true;
  String mqttHost;
  uint16_t mqttPort = 1883;
  String mqttUser, mqttPass;
  String mqttClientId;           // empty = hardware id (old behaviour)
  String baseTopic = "{id}/PowerMeter/";   // old default: <hardwareId>/PowerMeter/
  bool retain = false;
  uint8_t decimals = 2;
  uint8_t format = FMT_TOPICS;
  bool publishSystem = true;     // Uptime, VCC, RSSI, ... like the old firmware
  uint16_t publishSec = 10;        // MQTT / Home Assistant publish interval (0 = every poll cycle)
  uint16_t systemIntervalSec = 60; // Uptime, RSSI, ... interval (0 = with every publish)
  bool haEnabled = true;
  String haPrefix = "homeassistant";

  // modbus
  uint32_t baud = 9600;
  String serialFmt = "8N1";
  uint16_t timeoutMs = 500;
  uint32_t pollMs = 1500;           // old default 1.5 s
  uint32_t holdingSec = 900;        // old default 15 min
  uint16_t gapMs = 20;              // pause between requests
  bool swapSerial = false;          // ESP8266: UART0 on GPIO13(RX)/GPIO15(TX) instead of GPIO3/GPIO1
#if defined(ESP32)
  int8_t rxPin = 18, txPin = 17;
#else
  int8_t rxPin = 3, txPin = 1;
#endif
  int8_t dePin = -1;                // RS485 DE/RE pin, -1 = auto direction module

  // time
  String ntpServer = "pool.ntp.org";
  String tz = "UTC0";

  // watchdog
  uint16_t wdNoWifiMin = 60;        // reboot if WiFi is down this long (0 = off, ignored while someone uses the AP)
  uint16_t wdNoMqttMin = 30;        // reboot if MQTT is down this long while WiFi is up (0 = off)
  uint16_t wdNoModbusMin = 0;       // reboot if no meter answered this long (0 = off)

  std::vector<MeterCfg> meters;

  String effectiveHostname() const;
};

extern Config cfg;

void configDefaults(Config &c);
bool configLoad();
bool configSave(const Config &c);
void configFactoryReset();

// mask passwords when `forUi` is true
void configToJson(const Config &c, JsonDocument &doc, bool forUi);
// Parses and validates; existing passwords are kept when the UI sends the placeholder.
bool configFromJson(Config &c, const JsonDocument &doc, String &err);

extern const char *PASS_PLACEHOLDER;
