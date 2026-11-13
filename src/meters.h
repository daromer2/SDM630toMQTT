// Polls all configured meters on the RS485 bus, step by step without blocking loop().
#pragma once
#include <Arduino.h>
#include <vector>
#include "config.h"
#include "registers.h"

struct Block {
  uint8_t fc;
  uint16_t start, len;
  uint16_t rawOff;         // offset into Meter::raw (words)
  bool okThisCycle = false;
  uint32_t lastOkMs = 0;
};

struct Meter {
  MeterCfg cfg;
  const Profile *prof = nullptr;
  std::vector<Block> blocks;
  std::vector<uint16_t> raw;       // register words
  std::vector<int8_t> regBlock;    // per profile register: block index, -1 = disabled
  std::vector<uint16_t> regOff;    // per profile register: offset in raw

  // statistics
  uint32_t okCount = 0, errCount = 0;
  uint8_t consecFail = 0;
  bool online = false;
  uint32_t lastOkMs = 0;
  int lastError = 0;
  uint32_t lastCycleMs = 0;   // duration of the last poll of this meter

  // cycle state
  bool abortCycle = false;
  bool anyOkThisCycle = false;
  uint32_t cycleStart = 0;

  bool regEnabled(uint8_t i) const { return regBlock[i] >= 0; }
  bool regHasValue(uint8_t i) const { return regBlock[i] >= 0 && blocks[regBlock[i]].lastOkMs != 0; }
  bool regFresh(uint8_t i) const { return regBlock[i] >= 0 && blocks[regBlock[i]].okThisCycle; }
  // read successfully after `ms` (millis timestamp)
  bool regUpdatedSince(uint8_t i, uint32_t ms) const {
    return regBlock[i] >= 0 && blocks[regBlock[i]].lastOkMs != 0 && (int32_t)(blocks[regBlock[i]].lastOkMs - ms) > 0;
  }
  float regFloat(uint8_t i) const;
  // formatted value: float with `decimals`, or hex string
  String regValue(uint8_t i, uint8_t decimals) const;
};

void metersBegin();
void metersLoop();
std::vector<Meter> &meters();

// called after a meter has been polled (at least one block answered)
void metersOnMeterDone(void (*cb)(size_t idx));
// called at the start of each poll cycle
void metersOnCycleStart(void (*cb)());

uint32_t metersLastAnyOkMs();
uint32_t metersCycleCount();

// ---- bus scan: finds meters on Modbus addresses [from..to]. Normal polling pauses meanwhile.
struct ScanHit {
  uint8_t addr;
  bool threePhase;
  const char *type;   // suggested profile id
  float v1, v2, v3;
};
bool scanStart(uint8_t from, uint8_t to);
void scanStop();
bool scanRunning();
uint8_t scanFrom();
uint8_t scanTo();
uint8_t scanCurrent();
const std::vector<ScanHit> &scanResults();
