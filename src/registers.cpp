#include "registers.h"

#define IR(n, a, u)  { n, 4, a, F_FLOAT, u }
#define HR(n, a, u)  { n, 3, a, F_FLOAT, u }

static const UnitInfo UNITS[U__COUNT] = {
  /* U_NONE  */ { "",     "",               ""                 },
  /* U_V     */ { "V",    "voltage",        "measurement"      },
  /* U_A     */ { "A",    "current",        "measurement"      },
  /* U_W     */ { "W",    "power",          "measurement"      },
  /* U_VA    */ { "VA",   "apparent_power", "measurement"      },
  /* U_VAR   */ { "var",  "reactive_power", "measurement"      },
  /* U_PF    */ { "",     "power_factor",   "measurement"      },
  /* U_DEG   */ { "\xC2\xB0", "",           "measurement"      },
  /* U_HZ    */ { "Hz",   "frequency",      "measurement"      },
  /* U_KWH   */ { "kWh",  "energy",         "total_increasing" },
  /* U_KVARH */ { "kvarh", "",              "total_increasing" },
  /* U_KVAH  */ { "kVAh", "",               "total_increasing" },
  /* U_AH    */ { "Ah",   "",               "total_increasing" },
  /* U_PCT   */ { "%",    "",               "measurement"      },
  /* U_MIN   */ { "min",  "",               ""                 },
};

const UnitInfo &unitInfo(uint8_t u) { return UNITS[u < U__COUNT ? u : 0]; }

// ---------------------------------------------------------------- SDM630 full
static const RegDef SDM630[] = {
  IR("Voltage/1", 0x00, U_V),        IR("Voltage/2", 0x02, U_V),        IR("Voltage/3", 0x04, U_V),
  IR("Current/1", 0x06, U_A),        IR("Current/2", 0x08, U_A),        IR("Current/3", 0x0A, U_A),
  IR("PowerActive/1", 0x0C, U_W),    IR("PowerActive/2", 0x0E, U_W),    IR("PowerActive/3", 0x10, U_W),
  IR("PowerVA/1", 0x12, U_VA),       IR("PowerVA/2", 0x14, U_VA),       IR("PowerVA/3", 0x16, U_VA),
  IR("PowerVAR/1", 0x18, U_VAR),     IR("PowerVAR/2", 0x1A, U_VAR),     IR("PowerVAR/3", 0x1C, U_VAR),
  IR("PowerFactor/1", 0x1E, U_PF),   IR("PowerFactor/2", 0x20, U_PF),   IR("PowerFactor/3", 0x22, U_PF),
  IR("PhaseAngle/1", 0x24, U_DEG),   IR("PhaseAngle/2", 0x26, U_DEG),   IR("PhaseAngle/3", 0x28, U_DEG),
  IR("Voltage/Avg", 0x2A, U_V),
  IR("Current/Avg", 0x2E, U_A),
  IR("Current/Sum", 0x30, U_A),
  IR("PowerActive/Sum", 0x34, U_W),
  IR("PowerVA/Sum", 0x38, U_VA),
  IR("PowerVAR/Sum", 0x3C, U_VAR),
  IR("PowerFactor/Sum", 0x3E, U_PF),
  IR("PhaseAngle/Total", 0x42, U_DEG),
  IR("Frequency", 0x46, U_HZ),
  IR("Energy/ImportkWh", 0x48, U_KWH),
  IR("Energy/ExportkWh", 0x4A, U_KWH),
  IR("Energy/ImportkVAh", 0x4C, U_KVARH),   // name kept from old firmware (register is import kvarh)
  IR("Energy/ExportkVAh", 0x4E, U_KVARH),   // name kept from old firmware (register is export kvarh)
  IR("Energy/kVAh", 0x50, U_KVAH),
  IR("Energy/Ah", 0x52, U_AH),
  IR("Dem/PowerTotal", 0x54, U_W),
  IR("Dem/MaxPowerTotal", 0x56, U_W),
  IR("Dem/PowerVATotal", 0x64, U_VA),
  IR("Dem/MaxPowerVATotal", 0x66, U_VA),
  IR("Dem/NeutralCurrent", 0x68, U_A),
  IR("Dem/MaxNeutralCurrent", 0x6A, U_A),
  IR("LineToLineVolts/1-2", 0xC8, U_V),
  IR("LineToLineVolts/2-3", 0xCA, U_V),
  IR("LineToLineVolts/3-1", 0xCC, U_V),
  IR("LineToLineVolts/Avg", 0xCE, U_V),
  IR("NeutralCurrent", 0xE0, U_A),
  IR("THD/Voltage/1", 0xEA, U_PCT),  IR("THD/Voltage/2", 0xEC, U_PCT),  IR("THD/Voltage/3", 0xEE, U_PCT),
  IR("THD/Current/1", 0xF0, U_PCT),  IR("THD/Current/2", 0xF2, U_PCT),  IR("THD/Current/3", 0xF4, U_PCT),
  IR("THD/Voltage/Avg", 0xF8, U_PCT),
  IR("THD/Current/Avg", 0xFA, U_PCT),
  IR("PowerFactor/Total", 0xFE, U_PF),
  IR("Dem/Current/1", 0x102, U_A),   IR("Dem/Current/2", 0x104, U_A),   IR("Dem/Current/3", 0x106, U_A),
  IR("Dem/MaxCurrent/1", 0x108, U_A), IR("Dem/MaxCurrent/2", 0x10A, U_A), IR("Dem/MaxCurrent/3", 0x10C, U_A),
  IR("THD/LineToLineVolts/1-2", 0x14E, U_PCT),
  IR("THD/LineToLineVolts/2-3", 0x150, U_PCT),
  IR("THD/LineToLineVolts/3-1", 0x152, U_PCT),
  IR("THD/LineToLineVolts/Avg", 0x154, U_PCT),
  // holding registers (polled at the "holding interval", default every 15 min like the old firmware)
  HR("Dem/Interval", 0x02, U_MIN),
  HR("System/Voltage", 0x06, U_NONE),
  HR("System/Type", 0x08, U_NONE),
  { "System/SerialNumber", 3, 0x2A, F_HEX8, U_NONE },
};

// ---------------------------------------------------------------- SDM630 small
static const RegDef SDM630S[] = {
  IR("Voltage1", 0x00, U_V),       IR("Voltage2", 0x02, U_V),       IR("Voltage3", 0x04, U_V),
  IR("Current1", 0x06, U_A),       IR("Current2", 0x08, U_A),       IR("Current3", 0x0A, U_A),
  IR("PowerActive1", 0x0C, U_W),   IR("PowerActive2", 0x0E, U_W),   IR("PowerActive3", 0x10, U_W),
  IR("PowerVA1", 0x12, U_VA),      IR("PowerVA2", 0x14, U_VA),      IR("PowerVA3", 0x16, U_VA),
  IR("PowerVAR1", 0x18, U_VAR),    IR("PowerVAR2", 0x1A, U_VAR),    IR("PowerVAR3", 0x1C, U_VAR),
  IR("NeutralCurrent", 0xE0, U_A),
  IR("PowerActiveSum", 0x34, U_W),
  IR("Frequency", 0x46, U_HZ),
};

// ---------------------------------------------------------------- SDM230 / SDM120 (single phase)
// SDM120 uses the first 22 entries (no resettable totals).
static const RegDef SDM230[] = {
  IR("Voltage", 0x00, U_V),
  IR("Current", 0x06, U_A),
  IR("PowerActive", 0x0C, U_W),
  IR("PowerVA", 0x12, U_VA),
  IR("PowerVAR", 0x18, U_VAR),
  IR("PowerFactor", 0x1E, U_PF),
  IR("PhaseAngle", 0x24, U_DEG),
  IR("Frequency", 0x46, U_HZ),
  IR("ImportActive", 0x48, U_KWH),
  IR("ExportActive", 0x4A, U_KWH),
  IR("ImportReactive", 0x4C, U_KVARH),
  IR("ExportRreactive", 0x4E, U_KVARH),     // (sic) name kept from old firmware
  IR("TotalDem", 0x54, U_W),
  IR("MaxTotalDem", 0x56, U_W),
  IR("PositivePowerDem", 0x58, U_W),
  IR("MaxPositivePowerDem", 0x5A, U_W),
  IR("ReversePowerDem", 0x5C, U_W),
  IR("MaxReversePowerDem", 0x5E, U_W),
  IR("CurrentDem", 0x102, U_A),
  IR("MaxCurrentDem", 0x108, U_A),
  IR("TotalActiveEnergy", 0x156, U_KWH),
  IR("TotalReactiveEnergy", 0x158, U_KVARH),
  IR("ResTotalActiveEnergy", 0x180, U_KWH),
  IR("ResTotalReactiveEnergy", 0x182, U_KVARH),
};

// ---------------------------------------------------------------- SDM220
static const RegDef SDM220[] = {
  IR("Voltage", 0x00, U_V),
  IR("Current", 0x06, U_A),
  IR("PowerActive", 0x0C, U_W),
  IR("PowerVA", 0x12, U_VA),
  IR("PowerVAR", 0x18, U_VAR),
  IR("PowerFactor", 0x1E, U_PF),
  IR("PhaseAngle", 0x24, U_DEG),
  IR("Frequency", 0x46, U_HZ),
  IR("ImportActive", 0x48, U_KWH),
  IR("ExportActive", 0x4A, U_KWH),
  IR("ImportReactive", 0x4C, U_KVARH),
  IR("ExportRreactive", 0x4E, U_KVARH),
  IR("TotalActive", 0x156, U_KWH),          // old firmware used 0x56 here (bug, always 0)
  IR("TotalRreactive", 0x158, U_KVARH),
};

#define N(a) (uint8_t)(sizeof(a) / sizeof(a[0]))

static const Profile PROFILES[] = {
  { "630",  "SDM630 (full, ~70 values)",  "SDM630", SDM630,  N(SDM630)  },
  { "630s", "SDM630 (small)",             "SDM630", SDM630S, N(SDM630S) },
  { "230",  "SDM230 (full)",              "SDM230", SDM230,  N(SDM230)  },
  { "230s", "SDM230 (small)",             "SDM230", SDM230,  12         },
  { "220",  "SDM220",                     "SDM220", SDM220,  N(SDM220)  },
  { "120",  "SDM120",                     "SDM120", SDM230,  22         },
};

size_t profileCount() { return N(PROFILES); }
const Profile &profileAt(size_t i) { return PROFILES[i < profileCount() ? i : 0]; }

const Profile *findProfile(const String &id) {
  for (size_t i = 0; i < profileCount(); i++)
    if (id == PROFILES[i].id) return &PROFILES[i];
  return nullptr;
}
