// Eastron SDM register profiles.
// Register names are kept identical to the old firmware so existing MQTT topics keep working.
#pragma once
#include <Arduino.h>

enum RegFmt : uint8_t { F_FLOAT = 0, F_HEX8 = 1 };

enum Unit : uint8_t {
  U_NONE, U_V, U_A, U_W, U_VA, U_VAR, U_PF, U_DEG, U_HZ,
  U_KWH, U_KVARH, U_KVAH, U_AH, U_PCT, U_MIN, U__COUNT
};

struct UnitInfo {
  const char *unit;        // "" = none
  const char *devClass;    // Home Assistant device_class, "" = none
  const char *stateClass;  // Home Assistant state_class, "" = none
};

struct RegDef {
  const char *name;   // MQTT sub topic, e.g. "Voltage/1"
  uint8_t fc;         // 4 = input register, 3 = holding register
  uint16_t addr;
  uint8_t fmt;        // RegFmt
  uint8_t unit;       // Unit
};

inline uint8_t regWords(const RegDef &r) { return r.fmt == F_HEX8 ? 4 : 2; }

struct Profile {
  const char *id;      // stored in config, same values as the old "device_type" parameter
  const char *label;   // shown in the UI
  const char *model;   // Home Assistant device model
  const RegDef *regs;
  uint8_t count;
};

const UnitInfo &unitInfo(uint8_t u);
const Profile *findProfile(const String &id);
size_t profileCount();
const Profile &profileAt(size_t i);
