#include "config.h"
#include "platform.h"
#include "log.h"
#include <LittleFS.h>

Config cfg;
const char *PASS_PLACEHOLDER = "********";
static const char *CFG_FILE = "/config.json";

String Config::effectiveHostname() const {
  if (hostname.length()) return hostname;
  return String(F("sdm-gateway-")) + hardwareId();
}

void configDefaults(Config &c) {
  c = Config();
  MeterCfg m;   // one SDM630 on address 1, topic "1" -> same topics as the old firmware
  c.meters.push_back(m);
}

static void putPass(JsonDocument &doc, const char *key, const String &v, bool forUi) {
  if (forUi) doc[key] = v.length() ? PASS_PLACEHOLDER : "";
  else doc[key] = v;
}

void configToJson(const Config &c, JsonDocument &doc, bool forUi) {
  doc["hostname"] = c.hostname;
  putPass(doc, "adminPass", c.adminPass, forUi);

  doc["ssid"] = c.ssid;
  putPass(doc, "wifiPass", c.wifiPass, forUi);
  putPass(doc, "apPass", c.apPass, forUi);
  doc["apFallbackSec"] = c.apFallbackSec;
  doc["staRetrySec"] = c.staRetrySec;
  doc["staticIp"] = c.staticIp;
  doc["ip"] = c.ip;
  doc["gateway"] = c.gateway;
  doc["subnet"] = c.subnet;
  doc["dns"] = c.dns;

  doc["mqttEnabled"] = c.mqttEnabled;
  doc["mqttHost"] = c.mqttHost;
  doc["mqttPort"] = c.mqttPort;
  doc["mqttUser"] = c.mqttUser;
  putPass(doc, "mqttPass", c.mqttPass, forUi);
  doc["mqttClientId"] = c.mqttClientId;
  doc["baseTopic"] = c.baseTopic;
  doc["retain"] = c.retain;
  doc["decimals"] = c.decimals;
  doc["format"] = c.format;
  doc["publishSystem"] = c.publishSystem;
  doc["publishSec"] = c.publishSec;
  doc["systemIntervalSec"] = c.systemIntervalSec;
  doc["haEnabled"] = c.haEnabled;
  doc["haPrefix"] = c.haPrefix;

  doc["baud"] = c.baud;
  doc["serialFmt"] = c.serialFmt;
  doc["timeoutMs"] = c.timeoutMs;
  doc["pollMs"] = c.pollMs;
  doc["holdingSec"] = c.holdingSec;
  doc["gapMs"] = c.gapMs;
  doc["swapSerial"] = c.swapSerial;
  doc["rxPin"] = c.rxPin;
  doc["txPin"] = c.txPin;
  doc["dePin"] = c.dePin;

  doc["ntpServer"] = c.ntpServer;
  doc["tz"] = c.tz;

  doc["wdNoWifiMin"] = c.wdNoWifiMin;
  doc["wdNoMqttMin"] = c.wdNoMqttMin;
  doc["wdNoModbusMin"] = c.wdNoModbusMin;

  JsonArray arr = doc["meters"].to<JsonArray>();
  for (const auto &m : c.meters) {
    JsonObject o = arr.add<JsonObject>();
    o["enabled"] = m.enabled;
    o["addr"] = m.addr;
    o["type"] = m.type;
    o["name"] = m.name;
    o["topic"] = m.topic;
    o["mask"] = m.mask;
  }
}

template <typename T>
static void get(const JsonDocument &doc, const char *key, T &out) {
  JsonVariantConst v = doc[key];
  if (!v.isNull() && v.is<T>()) out = v.as<T>();
}

static void getStr(const JsonDocument &doc, const char *key, String &out, size_t maxLen = 64) {
  JsonVariantConst v = doc[key];
  if (v.is<const char *>()) {
    out = v.as<const char *>();
    out.trim();
    if (out.length() > maxLen) out = out.substring(0, maxLen);
  }
}

static void getPass(const JsonDocument &doc, const char *key, String &out) {
  JsonVariantConst v = doc[key];
  if (!v.is<const char *>()) return;
  String s = v.as<const char *>();
  if (s == PASS_PLACEHOLDER) return;   // unchanged
  if (s.length() > 64) s = s.substring(0, 64);
  out = s;
}

static String cleanTopic(String t) {
  t.trim();
  t.replace("#", "");
  t.replace("+", "");
  t.replace(" ", "_");
  return t;
}

bool configFromJson(Config &c, const JsonDocument &doc, String &err) {
  getStr(doc, "hostname", c.hostname, 32);
  getPass(doc, "adminPass", c.adminPass);

  getStr(doc, "ssid", c.ssid, 32);
  getPass(doc, "wifiPass", c.wifiPass);
  getPass(doc, "apPass", c.apPass);
  get(doc, "apFallbackSec", c.apFallbackSec);
  get(doc, "staRetrySec", c.staRetrySec);
  get(doc, "staticIp", c.staticIp);
  getStr(doc, "ip", c.ip, 15);
  getStr(doc, "gateway", c.gateway, 15);
  getStr(doc, "subnet", c.subnet, 15);
  getStr(doc, "dns", c.dns, 15);

  get(doc, "mqttEnabled", c.mqttEnabled);
  getStr(doc, "mqttHost", c.mqttHost, 64);
  get(doc, "mqttPort", c.mqttPort);
  getStr(doc, "mqttUser", c.mqttUser, 64);
  getPass(doc, "mqttPass", c.mqttPass);
  getStr(doc, "mqttClientId", c.mqttClientId, 32);
  getStr(doc, "baseTopic", c.baseTopic, 96);
  c.baseTopic = cleanTopic(c.baseTopic);
  get(doc, "retain", c.retain);
  get(doc, "decimals", c.decimals);
  get(doc, "format", c.format);
  get(doc, "publishSystem", c.publishSystem);
  get(doc, "publishSec", c.publishSec);
  get(doc, "systemIntervalSec", c.systemIntervalSec);
  get(doc, "haEnabled", c.haEnabled);
  getStr(doc, "haPrefix", c.haPrefix, 32);
  c.haPrefix = cleanTopic(c.haPrefix);

  get(doc, "baud", c.baud);
  getStr(doc, "serialFmt", c.serialFmt, 3);
  get(doc, "timeoutMs", c.timeoutMs);
  get(doc, "pollMs", c.pollMs);
  get(doc, "holdingSec", c.holdingSec);
  get(doc, "gapMs", c.gapMs);
  get(doc, "swapSerial", c.swapSerial);
  get(doc, "rxPin", c.rxPin);
  get(doc, "txPin", c.txPin);
  get(doc, "dePin", c.dePin);

  getStr(doc, "ntpServer", c.ntpServer, 64);
  getStr(doc, "tz", c.tz, 64);

  get(doc, "wdNoWifiMin", c.wdNoWifiMin);
  get(doc, "wdNoMqttMin", c.wdNoMqttMin);
  get(doc, "wdNoModbusMin", c.wdNoModbusMin);

  // sanity limits
  if (c.decimals > 6) c.decimals = 6;
  if (c.format > FMT_BOTH) c.format = FMT_TOPICS;
  if (c.timeoutMs < 50) c.timeoutMs = 50;
  if (c.timeoutMs > 5000) c.timeoutMs = 5000;
  if (c.pollMs < 500) c.pollMs = 500;
  if (c.gapMs > 1000) c.gapMs = 1000;
  if (c.apFallbackSec < 10) c.apFallbackSec = 10;
  if (c.staRetrySec < 20) c.staRetrySec = 20;
  if (c.baud < 1200 || c.baud > 115200) c.baud = 9600;
  if (c.serialFmt != "8N1" && c.serialFmt != "8E1" && c.serialFmt != "8O1" && c.serialFmt != "8N2") c.serialFmt = "8N1";
  if (c.apPass.length() > 0 && c.apPass.length() < 8) { err = F("AP password must be empty or at least 8 characters"); return false; }
  if (c.haPrefix.length() == 0) c.haPrefix = "homeassistant";

  JsonArrayConst arr = doc["meters"].as<JsonArrayConst>();
  if (!arr.isNull()) {
    std::vector<MeterCfg> meters;
    for (JsonObjectConst o : arr) {
      if (meters.size() >= MAX_METERS) { err = F("Too many meters"); return false; }
      MeterCfg m;
      m.enabled = o["enabled"] | true;
      int addr = o["addr"] | 0;
      if (addr < 1 || addr > 247) { err = F("Meter address must be 1..247"); return false; }
      m.addr = addr;
      m.type = o["type"] | "630";
      const Profile *p = findProfile(m.type);
      if (!p) { err = String(F("Unknown meter type ")) + m.type; return false; }
      m.name = o["name"] | "";
      m.name.trim();
      if (m.name.length() > 32) m.name = m.name.substring(0, 32);
      m.topic = cleanTopic(o["topic"] | "");
      while (m.topic.endsWith("/")) m.topic.remove(m.topic.length() - 1);
      m.mask = o["mask"] | "";
      if (m.mask.length() != p->count) m.mask = "";
      for (const auto &other : meters)
        if (other.addr == m.addr) { err = String(F("Duplicate meter address ")) + addr; return false; }
      meters.push_back(m);
    }
    c.meters = meters;
  }
  return true;
}

bool configLoad() {
  configDefaults(cfg);
  File f = LittleFS.open(CFG_FILE, "r");
  if (!f) {
    logf("No config file, using defaults");
    return false;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, f);
  f.close();
  if (e) {
    logf("Config parse error: %s - using defaults", e.c_str());
    return false;
  }
  Config c;
  configDefaults(c);
  String err;
  if (!configFromJson(c, doc, err)) {
    logf("Config invalid: %s - using defaults", err.c_str());
    return false;
  }
  cfg = c;
  logf("Config loaded (%u meters)", (unsigned)cfg.meters.size());
  return true;
}

bool configSave(const Config &c) {
  JsonDocument doc;
  configToJson(c, doc, false);
  File f = LittleFS.open("/config.tmp", "w");
  if (!f) {
    logf("Config save failed (open)");
    return false;
  }
  size_t n = serializeJson(doc, f);
  f.close();
  if (n == 0) return false;
  LittleFS.remove(CFG_FILE);
  bool ok = LittleFS.rename("/config.tmp", CFG_FILE);
  logf("Config saved (%u bytes) %s", (unsigned)n, ok ? "ok" : "FAILED");
  return ok;
}

void configFactoryReset() {
  LittleFS.remove(CFG_FILE);
  logf("Factory reset: config removed");
}
