#include "mqtt_mgr.h"
#include "platform.h"
#include "log.h"
#include "meters.h"
#include "wifi_mgr.h"
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <time.h>

static WiFiClient net;
static PubSubClient client(net);
static String base, statusTopic, controlTopic, haStatusTopic;
static uint32_t nextAttempt = 0, backoff = 2000, lastSystemPub = 0;
static int lastState = 0;
static uint32_t connectCount = 0;

// discovery queue
static bool dqActive = false, dqGateway = false;
static size_t dqMeter = 0;
static uint8_t dqReg = 0;

bool mqttConnected() { return client.connected(); }
String mqttBaseTopic() { return base; }

const char *mqttStateStr() {
  if (!cfg.mqttEnabled) return "disabled";
  if (!cfg.mqttHost.length()) return "no broker configured";
  if (client.connected()) return "connected";
  if (!wifiConnected()) return "waiting for WiFi";
  switch (lastState) {
    case MQTT_CONNECTION_TIMEOUT: return "connection timeout";
    case MQTT_CONNECT_FAILED: return "connect failed (broker unreachable?)";
    case MQTT_CONNECT_BAD_CREDENTIALS: return "bad username/password";
    case MQTT_CONNECT_UNAUTHORIZED: return "not authorized";
    case MQTT_CONNECTION_LOST: return "connection lost";
    default: return "disconnected";
  }
}

static String expandBase(const String &tpl) {
  String b = tpl;
  b.replace("{id}", hardwareId());
  b.replace("{host}", cfg.effectiveHostname());
  if (b.length() == 0) b = hardwareId() + "/PowerMeter/";
  if (!b.endsWith("/")) b += "/";
  return b;
}

static String sanitize(const String &s) {
  String o;
  o.reserve(s.length());
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (isalnum((unsigned char)c)) o += (char)tolower(c);
    else if (!o.endsWith("_")) o += '_';
  }
  while (o.endsWith("_")) o.remove(o.length() - 1);
  return o;
}

static String nodeId() { return String(F("sdm_")) + sanitize(hardwareId()); }

static bool publishLarge(const String &topic, const String &payload, bool retained) {
  if (!client.beginPublish(topic.c_str(), payload.length(), retained)) return false;
  client.write((const uint8_t *)payload.c_str(), payload.length());
  return client.endPublish();
}

static bool pub(const String &topic, const String &payload, bool retained) {
  if (topic.length() + payload.length() + 16 > 500) return publishLarge(topic, payload, retained);
  return client.publish(topic.c_str(), payload.c_str(), retained);
}

// ------------------------------------------------------------------ time helpers
static bool timeValid() { return time(nullptr) > 1600000000; }

static String fmtTime(time_t t) {   // same format as the old firmware: "HH:MM:SS DD/MM/YYYY"
  struct tm tm;
  localtime_r(&t, &tm);
  char buf[48];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d %02d/%02d/%04d", tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900);
  return String(buf);
}

static String fmtUptime(uint32_t s) {
  char buf[40];
  snprintf(buf, sizeof(buf), "%lud %02lu:%02lu:%02lu", (unsigned long)(s / 86400), (unsigned long)((s / 3600) % 24),
           (unsigned long)((s / 60) % 60), (unsigned long)(s % 60));
  return String(buf);
}

// ------------------------------------------------------------------ discovery
static String meterDevId(const MeterCfg &m) { return nodeId() + "_m" + m.addr; }

static String regObjectId(const MeterCfg &m, const RegDef &r) { return String(F("m")) + m.addr + "_" + sanitize(r.name); }

static String discTopic(const String &prefix, const String &objectId) {
  return prefix + "/sensor/" + nodeId() + "/" + objectId + "/config";
}

static void discoveryMeterReg(const Meter &m, uint8_t ri) {
  const RegDef &r = m.prof->regs[ri];
  const UnitInfo &u = unitInfo(r.unit);
  String obj = regObjectId(m.cfg, r);
  String regTopic = base + m.cfg.effectiveTopic();

  JsonDocument d;
  String nm = r.name;
  nm.replace("/", " ");
  d["name"] = nm;
  d["uniq_id"] = nodeId() + "_" + obj;
  d["obj_id"] = sanitize(m.cfg.displayName()) + "_" + sanitize(r.name);
  if (cfg.format == FMT_JSON) {
    d["stat_t"] = regTopic;
    d["val_tpl"] = String(F("{{ value_json['")) + r.name + F("'] }}");
  } else {
    d["stat_t"] = regTopic + "/" + r.name;
  }
  d["avty_t"] = statusTopic;
  if (*u.unit) d["unit_of_meas"] = u.unit;
  if (*u.devClass) d["dev_cla"] = u.devClass;
  if (*u.stateClass) d["stat_cla"] = u.stateClass;
  if (r.fc == 3) d["ent_cat"] = "diagnostic";
  // fresh values come every poll; mark unavailable if nothing arrived for a while
  uint32_t interval = max<uint32_t>(cfg.publishSec, cfg.pollMs / 1000);
  uint32_t expire = interval * 6;
  if (r.fc == 4 && expire >= 30) d["exp_aft"] = expire;
  JsonObject dev = d["dev"].to<JsonObject>();
  dev["ids"][0] = meterDevId(m.cfg);
  dev["name"] = m.cfg.displayName();
  dev["mf"] = "Eastron";
  dev["mdl"] = m.prof->model;
  dev["via_device"] = nodeId();

  String payload;
  serializeJson(d, payload);
  publishLarge(discTopic(cfg.haPrefix, obj), payload, true);
}

static void discoveryGateway(const String &prefix, bool remove) {
  struct E { const char *obj, *name, *topic, *tpl, *unit, *dc; };
  static const E ents[] = {
    { "rssi", "WiFi signal", "RSSI", "{{ value.split(' ')[0] | int }}", "dBm", "signal_strength" },
    { "uptime", "Uptime", "Uptime", nullptr, nullptr, nullptr },
  };
  for (const auto &e : ents) {
    String t = discTopic(prefix, e.obj);
    if (remove) {
      client.publish(t.c_str(), "", true);
      continue;
    }
    JsonDocument d;
    d["name"] = e.name;
    d["uniq_id"] = nodeId() + "_" + e.obj;
    d["stat_t"] = base + e.topic;
    d["avty_t"] = statusTopic;
    d["ent_cat"] = "diagnostic";
    if (e.tpl) d["val_tpl"] = e.tpl;
    if (e.unit) d["unit_of_meas"] = e.unit;
    if (e.dc) {
      d["dev_cla"] = e.dc;
      d["stat_cla"] = "measurement";
    }
    JsonObject dev = d["dev"].to<JsonObject>();
    dev["ids"][0] = nodeId();
    dev["name"] = cfg.effectiveHostname();
    dev["mf"] = "SDM630toMQTT";
    dev["mdl"] = String(F("Modbus gateway (")) + BOARD_NAME + ")";
    dev["sw"] = FW_VERSION;
    if (wifiConnected()) dev["cu"] = String(F("http://")) + WiFi.localIP().toString() + "/";
    String payload;
    serializeJson(d, payload);
    publishLarge(t, payload, true);
  }
}

static void discoveryStart() {
  if (!cfg.haEnabled) return;
  dqActive = true;
  dqGateway = cfg.publishSystem;
  dqMeter = 0;
  dqReg = 0;
}

// a few messages per loop() so the web UI / modbus keep running
static void discoveryStep() {
  if (!dqActive || !client.connected()) return;
  if (dqGateway) {
    discoveryGateway(cfg.haPrefix, false);
    dqGateway = false;
    return;
  }
  auto &ms = meters();
  for (int n = 0; n < 4; n++) {
    while (dqMeter < ms.size() && (!ms[dqMeter].cfg.enabled || dqReg >= ms[dqMeter].prof->count)) {
      dqMeter++;
      dqReg = 0;
    }
    if (dqMeter >= ms.size()) {
      dqActive = false;
      logf("MQTT: Home Assistant discovery published");
      return;
    }
    const Meter &m = ms[dqMeter];
    if (m.regEnabled(dqReg)) discoveryMeterReg(m, dqReg);
    dqReg++;
  }
}

void mqttCleanupDiscovery(const Config &next) {
  if (!client.connected() || !cfg.haEnabled) return;
  bool samePrefix = next.haEnabled && next.haPrefix == cfg.haPrefix;
  uint16_t removed = 0;
  for (const auto &m : meters()) {
    const MeterCfg *nm = nullptr;
    if (samePrefix)
      for (const auto &c : next.meters)
        if (c.addr == m.cfg.addr && c.type == m.cfg.type && c.enabled) nm = &c;
    for (uint8_t i = 0; i < m.prof->count; i++) {
      if (!m.regEnabled(i)) continue;
      if (nm && nm->regEnabled(i, m.prof->count)) continue;
      String t = discTopic(cfg.haPrefix, regObjectId(m.cfg, m.prof->regs[i]));
      client.publish(t.c_str(), "", true);
      if (++removed % 10 == 0) client.loop();
    }
  }
  if (cfg.publishSystem && !(samePrefix && next.publishSystem)) discoveryGateway(cfg.haPrefix, true);
  if (removed) logf("MQTT: removed %u Home Assistant entities", removed);
}

// ------------------------------------------------------------------ publishing
static void publishInitialState() {   // same topics as the old firmware
  String types;
  for (const auto &m : cfg.meters) {
    if (types.length()) types += ",";
    types += m.type;
  }
  client.publish((base + "MAC").c_str(), WiFi.macAddress().c_str(), cfg.retain);
  client.publish((base + "HardwareId").c_str(), hardwareId().c_str(), cfg.retain);
  client.publish((base + "Version").c_str(), FW_VERSION, cfg.retain);
  client.publish((base + "DeviceType").c_str(), types.c_str(), cfg.retain);
  client.publish((base + "IP").c_str(), WiFi.localIP().toString().c_str(), cfg.retain);
}

void mqttPublishSystem() {
  if (!client.connected() || !cfg.publishSystem) return;
  lastSystemPub = millis();
  uint32_t up = millis() / 1000;
  pub(base + "Uptime", fmtUptime(up), cfg.retain);
#if defined(ESP8266)
  pub(base + "VCC", String(vccMilliVolt()), cfg.retain);
#endif
  int rssi = WiFi.RSSI();
  int q = constrain(2 * (rssi + 100), 0, 100);
  pub(base + "RSSI", String(rssi) + " (" + q + "%)", cfg.retain);
  if (timeValid()) {
    time_t now = time(nullptr);
    pub(base + "LastSeenDateTime", fmtTime(now), cfg.retain);
    pub(base + "LastBootDateTime", fmtTime(now - up), cfg.retain);
    bool anyOnline = false;
    for (const auto &m : meters()) anyOnline |= m.online;
    if (anyOnline) pub(base + "LastConnectedDateTime", fmtTime(now), cfg.retain);
  }
}

void mqttOnCycleStart() {
  uint32_t every = (cfg.systemIntervalSec ? cfg.systemIntervalSec : cfg.publishSec) * 1000UL;
  if (!lastSystemPub || millis() - lastSystemPub >= every) mqttPublishSystem();
}

// per meter: time of the last publish (0 = never since (re)connect)
static std::vector<uint32_t> lastPub;

void mqttPublishMeter(size_t idx) {
  if (!client.connected()) return;
  if (lastPub.size() < meters().size()) lastPub.resize(meters().size(), 0);
  uint32_t now = millis();
  uint32_t since = lastPub[idx];
  if (since && now - since < cfg.publishSec * 1000UL) return;   // not due yet (Modbus keeps polling for the web UI)
  lastPub[idx] = now ? now : 1;
  const Meter &m = meters()[idx];
  String prefix = base + m.cfg.effectiveTopic();
  bool topics = cfg.format != FMT_JSON;
  bool json = cfg.format != FMT_TOPICS;
  String js;
  if (json) {
    js.reserve(64 + m.prof->count * 28);
    js = "{";
  }
  for (uint8_t i = 0; i < m.prof->count; i++) {
    // topics: everything read since the last publish (also rare holding registers)
    // json: always the complete object, so templates never miss a key
    bool updated = m.regUpdatedSince(i, since);
    if (!(json ? m.regHasValue(i) : updated)) continue;
    String v = m.regValue(i, cfg.decimals);
    if (topics && updated) client.publish((prefix + "/" + m.prof->regs[i].name).c_str(), v.c_str(), cfg.retain);
    if (json) {
      if (js.length() > 1) js += ",";
      js += "\"";
      js += m.prof->regs[i].name;
      js += "\":";
      if (m.prof->regs[i].fmt == F_HEX8) js += "\"" + v + "\"";
      else js += (v == "nan") ? String("null") : v;
    }
  }
  if (json) {
    js += "}";
    publishLarge(prefix, js, cfg.retain);
  }
}

// ------------------------------------------------------------------ connection
static void onMessage(char *topic, uint8_t *payload, unsigned int len) {
  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len && i < 64; i++) msg += (char)payload[i];
  msg.trim();
  String t = topic;
  if (t == haStatusTopic) {
    if (msg == "online") {
      logf("MQTT: Home Assistant restarted, republishing discovery");
      discoveryStart();
    }
    return;
  }
  if (t == controlTopic) {
    logf("MQTT command: %s", msg.c_str());
    if (msg == "reboot") requestReboot(1000);
    else if (msg == "discovery") discoveryStart();
    else if (msg == "system") mqttPublishSystem();
  }
}

static void tryConnect() {
  String cid = cfg.mqttClientId.length() ? cfg.mqttClientId : hardwareId();
  logf("MQTT: connecting to %s:%u as %s", cfg.mqttHost.c_str(), cfg.mqttPort, cid.c_str());
  bool ok;
  if (cfg.mqttUser.length())
    ok = client.connect(cid.c_str(), cfg.mqttUser.c_str(), cfg.mqttPass.c_str(), statusTopic.c_str(), 0, true, "offline");
  else
    ok = client.connect(cid.c_str(), nullptr, nullptr, statusTopic.c_str(), 0, true, "offline");
  lastState = client.state();
  if (!ok) {
    logf("MQTT: connect failed (%s), retry in %lu s", mqttStateStr(), (unsigned long)(backoff / 1000));
    nextAttempt = millis() + backoff;
    backoff = min<uint32_t>(backoff * 2, 60000);
    return;
  }
  backoff = 2000;
  connectCount++;
  lastPub.assign(meters().size(), 0);   // publish everything right after (re)connect
  logf("MQTT: connected");
  client.publish(statusTopic.c_str(), "online", true);
  client.subscribe(controlTopic.c_str());
  if (cfg.haEnabled) client.subscribe(haStatusTopic.c_str());
  publishInitialState();
  mqttPublishSystem();
  discoveryStart();
}

void mqttBegin() {
  base = expandBase(cfg.baseTopic);
  statusTopic = base + "status";
  controlTopic = base + "control";
  haStatusTopic = cfg.haPrefix + "/status";
  logf("MQTT base topic: %s", base.c_str());

#if defined(ESP32) && ESP_ARDUINO_VERSION_MAJOR < 3
  net.setTimeout(3);        // seconds on arduino-esp32 2.x
#else
  net.setTimeout(3000);     // milliseconds
#endif
  client.setBufferSize(512);
  client.setKeepAlive(30);
  client.setSocketTimeout(5);
  client.setCallback(onMessage);
  if (cfg.mqttHost.length()) client.setServer(cfg.mqttHost.c_str(), cfg.mqttPort);   // cfg lives forever
}

void mqttLoop() {
  if (!cfg.mqttEnabled || !cfg.mqttHost.length()) return;
  if (!wifiConnected()) {
    if (client.connected()) client.disconnect();
    return;
  }
  if (!client.connected()) {
    if ((int32_t)(millis() - nextAttempt) >= 0) tryConnect();
    return;
  }
  client.loop();
  discoveryStep();
}
