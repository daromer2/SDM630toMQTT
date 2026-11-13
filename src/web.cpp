#include "web.h"
#include "web_index.h"
#include "platform.h"
#include "config.h"
#include "log.h"
#include "meters.h"
#include "mqtt_mgr.h"
#include "wifi_mgr.h"
#include "modbus_rtu.h"
#include <ArduinoJson.h>

static WebServerT server(80);
static UpdateServerT updater;

static bool auth() {
  if (cfg.adminPass.length() == 0) return true;
  if (server.authenticate("admin", cfg.adminPass.c_str())) return true;
  server.requestAuthentication();
  return false;
}

static void sendJson(JsonDocument &doc, int code = 200) {
  String out;
  serializeJson(doc, out);
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", out);
}

static void sendError(int code, const String &msg) {
  JsonDocument d;
  d["error"] = msg;
  sendJson(d, code);
}

// ---- chunked writer for big responses (keeps RAM use small on the ESP8266)
struct Chunker {
  String buf;
  Chunker() {
    buf.reserve(600);
    server.sendHeader("Cache-Control", "no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "application/json", "");
  }
  void add(const String &s) {
    buf += s;
    if (buf.length() > 512) flush();
  }
  void flush() {
    if (buf.length()) server.sendContent(buf);
    buf = "";
  }
  void end() {
    flush();
    server.sendContent("");
  }
};

static String jsonStr(const char *s) {
  String o = "\"";
  for (; *s; s++) {
    if (*s == '"' || *s == '\\') o += '\\';
    o += *s;
  }
  return o + "\"";
}

// ------------------------------------------------------------------ handlers
static void handleIndex() {
  if (!auth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleStatus() {
  if (!auth()) return;
  JsonDocument d;
  d["fw"] = FW_VERSION;
  d["board"] = BOARD_NAME;
  d["id"] = hardwareId();
  d["host"] = cfg.effectiveHostname();
  d["uptime"] = millis() / 1000;
  d["heap"] = freeHeap();
  d["reset"] = resetReasonStr();
  d["vcc"] = vccMilliVolt();
  JsonObject w = d["wifi"].to<JsonObject>();
  w["state"] = wifiStateStr();
  w["connected"] = wifiConnected();
  w["ssid"] = cfg.ssid;
  w["ip"] = wifiConnected() ? WiFi.localIP().toString() : String("");
  w["rssi"] = wifiConnected() ? WiFi.RSSI() : 0;
  w["ap"] = wifiApActive();
  w["apSsid"] = wifiApSsid();
  w["apClients"] = wifiApClients();
  w["mac"] = WiFi.macAddress();
  JsonObject m = d["mqtt"].to<JsonObject>();
  m["state"] = mqttStateStr();
  m["connected"] = mqttConnected();
  m["base"] = mqttBaseTopic();
  d["cycles"] = metersCycleCount();
  d["scanning"] = scanRunning();
  JsonArray arr = d["meters"].to<JsonArray>();
  uint32_t now = millis();
  for (const auto &mt : meters()) {
    JsonObject o = arr.add<JsonObject>();
    o["name"] = mt.cfg.displayName();
    o["addr"] = mt.cfg.addr;
    o["type"] = mt.prof->id;
    o["enabled"] = mt.cfg.enabled;
    o["online"] = mt.online;
    o["ok"] = mt.okCount;
    o["err"] = mt.errCount;
    o["lastErr"] = mt.lastError ? ModbusRtu::errorStr(mt.lastError) : "";
    o["age"] = mt.lastOkMs ? (now - mt.lastOkMs) / 1000 : -1;
    o["pollMs"] = mt.lastCycleMs;
    o["topic"] = mqttBaseTopic() + mt.cfg.effectiveTopic();
  }
  sendJson(d);
}

static void handleValues() {
  if (!auth()) return;
  size_t idx = server.arg("m").toInt();
  auto &ms = meters();
  if (idx >= ms.size()) {
    sendError(404, F("no such meter"));
    return;
  }
  const Meter &m = ms[idx];
  Chunker c;
  c.add("{\"values\":[");
  bool first = true;
  for (uint8_t i = 0; i < m.prof->count; i++) {
    if (!m.regEnabled(i)) continue;
    const RegDef &r = m.prof->regs[i];
    String s = first ? "" : ",";
    first = false;
    s += "{\"n\":";
    s += jsonStr(r.name);
    s += ",\"u\":";
    s += jsonStr(unitInfo(r.unit).unit);
    s += ",\"v\":";
    if (!m.regHasValue(i)) s += "null";
    else s += jsonStr(m.regValue(i, cfg.decimals).c_str());
    s += "}";
    c.add(s);
  }
  c.add("]}");
  c.end();
}

static void handleProfiles() {
  if (!auth()) return;
  Chunker c;
  c.add("[");
  for (size_t p = 0; p < profileCount(); p++) {
    const Profile &pr = profileAt(p);
    String s = p ? "," : "";
    s += "{\"id\":" + jsonStr(pr.id) + ",\"label\":" + jsonStr(pr.label) + ",\"regs\":[";
    c.add(s);
    for (uint8_t i = 0; i < pr.count; i++) {
      String r = i ? "," : "";
      r += "[" + jsonStr(pr.regs[i].name) + "," + jsonStr(unitInfo(pr.regs[i].unit).unit) + "," + pr.regs[i].fc + "]";
      c.add(r);
    }
    c.add("]}");
  }
  c.add("]");
  c.end();
}

static void handleGetConfig() {
  if (!auth()) return;
  JsonDocument d;
  configToJson(cfg, d, true);
  d["_board"] = BOARD_NAME;
  d["_maxMeters"] = MAX_METERS;
  sendJson(d);
}

static void handlePostConfig() {
  if (!auth()) return;
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, server.arg("plain"));
  if (e) {
    sendError(400, String(F("Bad JSON: ")) + e.c_str());
    return;
  }
  Config next = cfg;   // start from current so missing fields / password placeholders keep their value
  String err;
  if (!configFromJson(next, doc, err)) {
    sendError(400, err);
    return;
  }
  mqttCleanupDiscovery(next);
  if (!configSave(next)) {
    sendError(500, F("Could not write config"));
    return;
  }
  JsonDocument d;
  d["ok"] = true;
  d["reboot"] = true;
  sendJson(d);
  requestReboot(1500);
}

static void handleScan() {
  if (!auth()) return;
  int n = WiFi.scanComplete();
  JsonDocument d;
  if (n == WIFI_SCAN_FAILED) {
    WiFi.scanNetworks(true);
    d["running"] = true;
  } else if (n == WIFI_SCAN_RUNNING) {
    d["running"] = true;
  } else {
    JsonArray a = d["networks"].to<JsonArray>();
    for (int i = 0; i < n && i < 30; i++) {
      JsonObject o = a.add<JsonObject>();
      o["ssid"] = WiFi.SSID(i);
      o["rssi"] = WiFi.RSSI(i);
#if defined(ESP8266)
      o["open"] = WiFi.encryptionType(i) == ENC_TYPE_NONE;
#else
      o["open"] = WiFi.encryptionType(i) == WIFI_AUTH_OPEN;
#endif
    }
    WiFi.scanDelete();
    d["running"] = false;
  }
  sendJson(d);
}

static void sendMeterScan() {
  JsonDocument d;
  d["running"] = scanRunning();
  d["from"] = scanFrom();
  d["to"] = scanTo();
  d["current"] = scanCurrent();
  JsonArray a = d["found"].to<JsonArray>();
  for (const auto &h : scanResults()) {
    JsonObject o = a.add<JsonObject>();
    o["addr"] = h.addr;
    o["type"] = h.type;
    o["threePhase"] = h.threePhase;
    o["v1"] = roundf(h.v1 * 10) / 10;
    o["v2"] = roundf(h.v2 * 10) / 10;
    o["v3"] = roundf(h.v3 * 10) / 10;
  }
  sendJson(d);
}

static void handleMeterScanGet() {
  if (!auth()) return;
  sendMeterScan();
}

static void handleMeterScanStart() {
  if (!auth()) return;
  int from = server.hasArg("from") ? server.arg("from").toInt() : 1;
  int to = server.hasArg("to") ? server.arg("to").toInt() : 247;
  if (from < 1 || to > 247 || from > to) {
    sendError(400, F("Address range must be within 1..247"));
    return;
  }
  if (!scanStart(from, to)) {
    sendError(409, F("Scan already running"));
    return;
  }
  sendMeterScan();
}

static void handleMeterScanStop() {
  if (!auth()) return;
  scanStop();
  sendMeterScan();
}

static void sendLogChunk(const char *data, size_t len, void *) { server.sendContent(data, len); }

static void handleLog() {
  if (!auth()) return;
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/plain", "");
  logForEach(sendLogChunk, nullptr);
  server.sendContent("");
}

static void handleReboot() {
  if (!auth()) return;
  server.send(200, "application/json", "{\"ok\":true}");
  requestReboot(1000);
}

static void handleFactoryReset() {
  if (!auth()) return;
  configFactoryReset();
  server.send(200, "application/json", "{\"ok\":true}");
  requestReboot(1000);
}

static void handleNotFound() {
  // captive portal: send every unknown host / URL in AP mode to the setup page
  if (wifiApActive()) {
    String host = server.hostHeader();
    if (host != "192.168.4.1" && host != WiFi.localIP().toString()) {
      server.sendHeader("Location", "http://192.168.4.1/", true);
      server.send(302, "text/plain", "");
      return;
    }
  }
  server.send(404, "text/plain", "Not found");
}

void webBegin() {
  server.on("/", HTTP_GET, handleIndex);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/values", HTTP_GET, handleValues);
  server.on("/api/profiles", HTTP_GET, handleProfiles);
  server.on("/api/config", HTTP_GET, handleGetConfig);
  server.on("/api/config", HTTP_POST, handlePostConfig);
  server.on("/api/scan", HTTP_GET, handleScan);
  server.on("/api/meter-scan", HTTP_GET, handleMeterScanGet);
  server.on("/api/meter-scan", HTTP_POST, handleMeterScanStart);
  server.on("/api/meter-scan/stop", HTTP_POST, handleMeterScanStop);
  server.on("/api/log", HTTP_GET, handleLog);
  server.on("/api/reboot", HTTP_POST, handleReboot);
  server.on("/api/factory-reset", HTTP_POST, handleFactoryReset);
  server.onNotFound(handleNotFound);
  if (cfg.adminPass.length()) updater.setup(&server, "/update", "admin", cfg.adminPass.c_str());
  else updater.setup(&server, "/update");
  server.begin();
  logf("Web server started");
}

void webLoop() { server.handleClient(); }
