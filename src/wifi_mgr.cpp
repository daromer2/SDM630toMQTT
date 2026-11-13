#include "wifi_mgr.h"
#include "platform.h"
#include "config.h"
#include "log.h"
#include <DNSServer.h>

#if defined(ESP8266)
  #include <ESP8266mDNS.h>
#else
  #include <ESPmDNS.h>
#endif

static const uint32_t CONNECT_TIMEOUT_MS = 20000;   // one station attempt
static const uint32_t AP_CLIENT_GRACE_MS = 10UL * 60 * 1000;   // don't disturb an AP user longer than this

enum WState { W_AP_ONLY, W_CONNECTING, W_CONNECTED, W_FALLBACK };
static WState st = W_AP_ONLY;
static uint32_t tAttempt = 0, downSince = 0, tRetry = 0, apClientsSince = 0;
static bool apOn = false;
static DNSServer dns;
static bool mdnsStarted = false;

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool wifiApActive() { return apOn; }
String wifiApSsid() { return String(F("SDM-Gateway-")) + hardwareId(); }
uint8_t wifiApClients() { return apOn ? WiFi.softAPgetStationNum() : 0; }
uint32_t wifiDownSinceMs() { return wifiConnected() ? 0 : downSince; }

const char *wifiStateStr() {
  switch (st) {
    case W_AP_ONLY: return "setup AP (no WiFi configured)";
    case W_CONNECTING: return apOn ? "connecting (AP fallback active)" : "connecting";
    case W_CONNECTED: return "connected";
    case W_FALLBACK: return "AP fallback, WiFi retry pending";
  }
  return "?";
}

static void startAp() {
  if (apOn) return;
  WiFi.mode(WIFI_AP_STA);   // STA part stays idle unless we call begin(); needed for network scans
  const char *pass = cfg.apPass.length() >= 8 ? cfg.apPass.c_str() : nullptr;
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  WiFi.softAP(wifiApSsid().c_str(), pass);
  dns.setErrorReplyCode(DNSReplyCode::NoError);
  dns.start(53, "*", IPAddress(192, 168, 4, 1));
  apOn = true;
  apClientsSince = 0;
  logf("AP started: %s (%s) http://192.168.4.1", wifiApSsid().c_str(), pass ? "password protected" : "open");
}

static void stopAp() {
  if (!apOn) return;
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  apOn = false;
  logf("AP stopped");
}

static void staBegin() {
  if (cfg.staticIp) {
    IPAddress ip, gw, sn, d;
    if (ip.fromString(cfg.ip) && gw.fromString(cfg.gateway) && sn.fromString(cfg.subnet)) {
      if (!d.fromString(cfg.dns)) d = gw;
      WiFi.config(ip, gw, sn, d);
    } else {
      logf("Static IP config invalid, using DHCP");
    }
  }
  WiFi.begin(cfg.ssid.c_str(), cfg.wifiPass.c_str());
  tAttempt = millis();
  st = W_CONNECTING;
  logf("WiFi: connecting to '%s'", cfg.ssid.c_str());
}

// Stop the station so it does not scan in the background (keeps the AP stable).
static void staStop() {
  WiFi.disconnect(false);
}

static void onConnected() {
  st = W_CONNECTED;
  downSince = 0;
  logf("WiFi connected: %s  RSSI %d dBm", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin(cfg.effectiveHostname().c_str());
    if (mdnsStarted) MDNS.addService("http", "tcp", 80);
  }
}

void wifiBegin() {
  WiFi.persistent(false);       // never write WiFi settings to flash on every begin()
  WiFi.disconnect(true);
  delay(50);
  String host = cfg.effectiveHostname();
#if defined(ESP8266)
  WiFi.hostname(host);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
  WiFi.setHostname(host.c_str());
#endif
  WiFi.setAutoReconnect(true);
  downSince = millis();

  if (cfg.ssid.length() == 0) {
    st = W_AP_ONLY;
    startAp();
    return;
  }
  WiFi.mode(WIFI_STA);
#if defined(ESP32)
  WiFi.setSleep(false);
#endif
  staBegin();
}

void wifiLoop() {
  if (apOn) dns.processNextRequest();
#if defined(ESP8266)
  if (mdnsStarted) MDNS.update();
#endif

  uint32_t now = millis();
  bool up = WiFi.status() == WL_CONNECTED;

  if (apOn) {
    if (WiFi.softAPgetStationNum() > 0) {
      if (!apClientsSince) apClientsSince = now ? now : 1;
    } else {
      apClientsSince = 0;
    }
  }

  switch (st) {
    case W_AP_ONLY:
      return;

    case W_CONNECTING:
      if (up) {
        onConnected();
        return;
      }
      if (now - tAttempt > CONNECT_TIMEOUT_MS) {
        if (!downSince) downSince = tAttempt;
        if (apOn || now - downSince >= cfg.apFallbackSec * 1000UL) {
          // give up for now: AP on, station quiet until next retry
          staStop();
          startAp();
          st = W_FALLBACK;
          tRetry = now + cfg.staRetrySec * 1000UL;
          logf("WiFi: not reachable, next try in %u s", cfg.staRetrySec);
        } else {
          logf("WiFi: still not connected, restarting attempt");
          staStop();
          staBegin();
        }
      }
      return;

    case W_CONNECTED:
      if (!up) {
        downSince = now;
        tAttempt = now;     // the SDK auto reconnects; we take over after CONNECT_TIMEOUT_MS
        st = W_CONNECTING;
        logf("WiFi: connection lost");
        return;
      }
      // station is up: switch the fallback AP off once nobody is using it (or after the grace time)
      if (apOn && (apClientsSince == 0 || now - apClientsSince > AP_CLIENT_GRACE_MS)) stopAp();
      return;

    case W_FALLBACK:
      if (up) {   // can happen if the SDK reconnected by itself
        onConnected();
        return;
      }
      if ((int32_t)(now - tRetry) >= 0) {
        // retrying makes the radio scan, which briefly disturbs AP clients -> wait while someone
        // is using the AP, but not forever (power outage: router may come back while phone is still connected)
        if (apClientsSince && now - apClientsSince < AP_CLIENT_GRACE_MS) {
          tRetry = now + 15000;
          return;
        }
        staBegin();
      }
      return;
  }
}
