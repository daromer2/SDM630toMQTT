#include "meters.h"
#include "modbus_rtu.h"
#include "platform.h"
#include "log.h"
#include <algorithm>

static const uint16_t MAX_BLOCK_WORDS = 80;   // Eastron: max 40 parameters (80 registers) per request
static const uint16_t MAX_GAP_WORDS = 10;     // merge registers separated by small holes

static std::vector<Meter> s_meters;
static ModbusRtu bus;
static void (*s_onDone)(size_t) = nullptr;
static void (*s_onCycle)() = nullptr;

enum PState { P_IDLE, P_NEXT, P_WAIT };
static PState st = P_IDLE;
static size_t mi = 0, bi = 0;
static bool holdingDue = false, firstCycle = true;
static uint32_t tCycle = 0, tLastIo = 0, tHolding = 0, lastAnyOk = 0, cycles = 0;

std::vector<Meter> &meters() { return s_meters; }
void metersOnMeterDone(void (*cb)(size_t)) { s_onDone = cb; }
void metersOnCycleStart(void (*cb)()) { s_onCycle = cb; }
uint32_t metersLastAnyOkMs() { return lastAnyOk; }
uint32_t metersCycleCount() { return cycles; }

float Meter::regFloat(uint8_t i) const {
  if (regBlock[i] < 0) return NAN;
  uint16_t o = regOff[i];
  uint32_t v = ((uint32_t)raw[o] << 16) | raw[o + 1];
  float f;
  memcpy(&f, &v, sizeof(f));
  return f;
}

String Meter::regValue(uint8_t i, uint8_t decimals) const {
  const RegDef &r = prof->regs[i];
  if (r.fmt == F_HEX8) {
    char buf[32];
    uint16_t o = regOff[i];
    snprintf(buf, sizeof(buf), "%02x %02x %02x %02x %02x %02x %02x %02x",
             raw[o] >> 8, raw[o] & 0xFF, raw[o + 1] >> 8, raw[o + 1] & 0xFF,
             raw[o + 2] >> 8, raw[o + 2] & 0xFF, raw[o + 3] >> 8, raw[o + 3] & 0xFF);
    return String(buf);
  }
  float f = regFloat(i);
  if (isnan(f) || isinf(f)) return String(F("nan"));
  return String(f, (unsigned int)decimals);
}

// Groups the enabled registers into as few Modbus read requests as possible.
static void buildBlocks(Meter &m) {
  const Profile *p = m.prof;
  m.regBlock.assign(p->count, -1);
  m.regOff.assign(p->count, 0);

  std::vector<uint8_t> order;
  for (uint8_t i = 0; i < p->count; i++)
    if (m.cfg.regEnabled(i, p->count)) order.push_back(i);
  std::sort(order.begin(), order.end(), [p](uint8_t a, uint8_t b) {
    if (p->regs[a].fc != p->regs[b].fc) return p->regs[a].fc > p->regs[b].fc;   // input regs (4) first
    return p->regs[a].addr < p->regs[b].addr;
  });

  uint16_t rawSize = 0;
  for (uint8_t idx : order) {
    const RegDef &r = p->regs[idx];
    uint16_t end = r.addr + regWords(r);
    bool merged = false;
    if (!m.blocks.empty()) {
      Block &b = m.blocks.back();
      uint16_t bEnd = b.start + b.len;
      if (b.fc == r.fc && r.addr >= b.start && r.addr <= bEnd + MAX_GAP_WORDS && (uint16_t)(std::max(end, bEnd) - b.start) <= MAX_BLOCK_WORDS) {
        if (end > bEnd) b.len = end - b.start;
        merged = true;
      }
    }
    if (!merged) {
      Block b;
      b.fc = r.fc;
      b.start = r.addr;
      b.len = regWords(r);
      m.blocks.push_back(b);
    }
    m.regBlock[idx] = m.blocks.size() - 1;
  }
  for (auto &b : m.blocks) {
    b.rawOff = rawSize;
    rawSize += b.len;
  }
  for (uint8_t idx : order) {
    const Block &b = m.blocks[m.regBlock[idx]];
    m.regOff[idx] = b.rawOff + (p->regs[idx].addr - b.start);
  }
  m.raw.assign(rawSize, 0);
}

#if defined(ESP8266)
static SerialConfig serialConfig(const String &f) {
  if (f == "8E1") return SERIAL_8E1;
  if (f == "8O1") return SERIAL_8O1;
  if (f == "8N2") return SERIAL_8N2;
  return SERIAL_8N1;
}
#else
static uint32_t serialConfig(const String &f) {
  if (f == "8E1") return SERIAL_8E1;
  if (f == "8O1") return SERIAL_8O1;
  if (f == "8N2") return SERIAL_8N2;
  return SERIAL_8N1;
}
#endif

void metersBegin() {
  s_meters.clear();
  s_meters.reserve(cfg.meters.size());
  for (const auto &mc : cfg.meters) {
    Meter m;
    m.cfg = mc;
    m.prof = findProfile(mc.type);
    if (!m.prof) continue;
    buildBlocks(m);
    logf("Meter addr %u type %s: %u requests, %u registers", mc.addr, m.prof->id, (unsigned)m.blocks.size(), (unsigned)m.raw.size());
    s_meters.push_back(m);
  }

#if defined(ESP8266)
  MODBUS_SERIAL.begin(cfg.baud, serialConfig(cfg.serialFmt));
  if (cfg.swapSerial) MODBUS_SERIAL.swap();   // RX=GPIO13 (D7), TX=GPIO15 (D8)
#else
  MODBUS_SERIAL.begin(cfg.baud, serialConfig(cfg.serialFmt), cfg.rxPin, cfg.txPin);
#endif
  bus.begin(&MODBUS_SERIAL, cfg.dePin, cfg.timeoutMs);
  logf("Modbus: %lu baud %s timeout %u ms poll %lu ms", (unsigned long)cfg.baud, cfg.serialFmt.c_str(), cfg.timeoutMs, (unsigned long)cfg.pollMs);
}

static void startMeter(Meter &m) {
  m.abortCycle = false;
  m.anyOkThisCycle = false;
  m.cycleStart = millis();
  for (auto &b : m.blocks) b.okThisCycle = false;
}

static void finishMeter(size_t idx) {
  Meter &m = s_meters[idx];
  if (!m.cfg.enabled) return;
  m.lastCycleMs = millis() - m.cycleStart;
  if (m.anyOkThisCycle) {
    m.consecFail = 0;
  } else if (m.consecFail < 255) {
    m.consecFail++;
  }
  m.online = m.lastOkMs != 0 && m.consecFail < 3;
  if (m.anyOkThisCycle && s_onDone) s_onDone(idx);
}

// ------------------------------------------------------------------ bus scan
static const uint16_t SCAN_TIMEOUT_MS = 250;
static bool scanActive = false, scanStopReq = false, sWaiting = false;
static uint8_t sFrom = 1, sTo = 1, sAddr = 1, sStep = 0;
static ScanHit sHit;
static std::vector<ScanHit> sResults;

bool scanRunning() { return scanActive; }
uint8_t scanFrom() { return sFrom; }
uint8_t scanTo() { return sTo; }
uint8_t scanCurrent() { return sAddr; }
const std::vector<ScanHit> &scanResults() { return sResults; }

bool scanStart(uint8_t from, uint8_t to) {
  if (scanActive) return false;
  if (from < 1) from = 1;
  if (to > 247) to = 247;
  if (from > to) return false;
  sResults.clear();
  sFrom = from;
  sTo = to;
  sAddr = from;
  sStep = 0;
  sWaiting = false;
  scanStopReq = false;
  scanActive = true;
  logf("Scan: searching meters on addresses %u..%u", from, to);
  return true;
}

void scanStop() {
  if (scanActive) scanStopReq = true;   // finishes after the request in flight
}

static float beFloat(const uint8_t *d) {
  uint32_t v = ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) | ((uint32_t)d[2] << 8) | d[3];
  float f;
  memcpy(&f, &v, sizeof(f));
  return (isnan(f) || isinf(f)) ? 0 : f;
}

static void scanRecord() {
  sResults.push_back(sHit);
  logf("Scan: meter at address %u -> %s (%d / %d / %d V)", sHit.addr, sHit.type, (int)sHit.v1, (int)sHit.v2, (int)sHit.v3);
  sStep = 0;
  sAddr++;
}

// Per address: 1) read voltages (0x00, any SDM answers)
//              2) read line-to-line voltage 0xC8 (only 3-phase meters have it)
//              3) single phase: read resettable total 0x180 (SDM230 full profile) else safe small profile
static void scanLoop() {
  if (!sWaiting) {
    if (scanStopReq || sAddr > sTo) {
      scanActive = false;
      scanStopReq = false;
      bus.setTimeout(cfg.timeoutMs);
      logf("Scan: done, %u meter(s) found", (unsigned)sResults.size());
      return;
    }
    if (millis() - tLastIo < (cfg.gapMs > 20 ? cfg.gapMs : 20)) return;
    bus.setTimeout(SCAN_TIMEOUT_MS);
    bool ok = false;
    if (sStep == 0) ok = bus.request(sAddr, 4, 0x00, 6);
    else if (sStep == 1) ok = bus.request(sAddr, 4, 0xC8, 2);
    else ok = bus.request(sAddr, 4, 0x180, 2);
    sWaiting = ok;
    return;
  }
  int r = bus.poll();
  if (r == MB_PENDING) return;
  tLastIo = millis();
  sWaiting = false;
  if (sStep == 0) {
    if (r == MB_OK) {
      const uint8_t *d = bus.data();
      sHit = ScanHit();
      sHit.addr = sAddr;
      sHit.v1 = beFloat(d);
      sHit.v2 = beFloat(d + 4);
      sHit.v3 = beFloat(d + 8);
      sStep = 1;
    } else {
      if (r != MB_TIMEOUT) logf("Scan: address %u answered: %s (not an SDM?)", sAddr, ModbusRtu::errorStr(r));
      sAddr++;
    }
    return;
  }
  if (sStep == 1) {
    float ll = (r == MB_OK) ? beFloat(bus.data()) : 0;
    sHit.threePhase = sHit.v2 > 50 || sHit.v3 > 50 || ll > 50;
    if (sHit.threePhase) {
      sHit.type = "630";
      scanRecord();
    } else {
      sStep = 2;
    }
    return;
  }
  sHit.type = (r == MB_OK) ? "230" : "230s";
  scanRecord();
}

void metersLoop() {
  if (scanActive && st != P_WAIT) {   // let a running meter request finish first
    scanLoop();
    return;
  }
  uint32_t now = millis();
  switch (st) {
    case P_IDLE:
      if (s_meters.empty()) return;
      if (!firstCycle && now - tCycle < cfg.pollMs) return;
      tCycle = now;
      holdingDue = firstCycle || (now - tHolding >= cfg.holdingSec * 1000UL);
      if (holdingDue) tHolding = now;
      firstCycle = false;
      cycles++;
      if (s_onCycle) s_onCycle();
      mi = 0;
      bi = 0;
      startMeter(s_meters[0]);
      st = P_NEXT;
      return;

    case P_NEXT:
      if (now - tLastIo < cfg.gapMs) return;
      while (mi < s_meters.size()) {
        Meter &m = s_meters[mi];
        if (m.cfg.enabled && !m.abortCycle) {
          while (bi < m.blocks.size() && !(m.blocks[bi].fc == 4 || holdingDue)) bi++;
          if (bi < m.blocks.size()) {
            const Block &b = m.blocks[bi];
            if (!bus.request(m.cfg.addr, b.fc, b.start, b.len)) return;
            st = P_WAIT;
            return;
          }
        }
        finishMeter(mi);
        mi++;
        bi = 0;
        if (mi < s_meters.size()) startMeter(s_meters[mi]);
        return;   // one meter per loop pass keeps loop() snappy
      }
      st = P_IDLE;
      return;

    case P_WAIT: {
      int r = bus.poll();
      if (r == MB_PENDING) return;
      tLastIo = millis();
      Meter &m = s_meters[mi];
      Block &b = m.blocks[bi];
      if (r == MB_OK) {
        const uint8_t *d = bus.data();
        for (uint16_t k = 0; k < b.len; k++) m.raw[b.rawOff + k] = ((uint16_t)d[2 * k] << 8) | d[2 * k + 1];
        b.okThisCycle = true;
        b.lastOkMs = tLastIo;
        m.anyOkThisCycle = true;
        m.lastOkMs = tLastIo;
        m.okCount++;
        lastAnyOk = tLastIo;
      } else {
        m.errCount++;
        if (m.lastError != r || m.errCount % 50 == 1)
          logf("Meter %u: fc%u 0x%04X[%u] -> %s", m.cfg.addr, b.fc, b.start, b.len, ModbusRtu::errorStr(r));
        m.lastError = r;
        // a meter that does not answer at all must not stall the bus for every block
        if (r == MB_TIMEOUT && !m.anyOkThisCycle) m.abortCycle = true;
      }
      if (r == MB_OK) m.lastError = 0;
      bi++;
      st = P_NEXT;
      return;
    }
  }
}
