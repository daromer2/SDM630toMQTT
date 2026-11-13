#include "platform.h"
#include "log.h"

#if defined(ESP8266)
ADC_MODE(ADC_VCC);   // A0 measures the module supply voltage (same as old firmware)
#else
  #include <esp_system.h>
#endif

const String &hardwareId() {
  static String id;
  if (id.length() == 0) {
    char buf[8];
#if defined(ESP8266)
    snprintf(buf, sizeof(buf), "%06X", (unsigned)ESP.getChipId());
#else
    uint64_t mac = ESP.getEfuseMac();   // bytes are little endian, last 3 bytes of the MAC are unique
    snprintf(buf, sizeof(buf), "%02X%02X%02X",
             (unsigned)((mac >> 24) & 0xFF), (unsigned)((mac >> 32) & 0xFF), (unsigned)((mac >> 40) & 0xFF));
#endif
    id = buf;
  }
  return id;
}

String resetReasonStr() {
#if defined(ESP8266)
  return ESP.getResetReason();
#else
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return F("Power on");
    case ESP_RST_EXT:       return F("External pin");
    case ESP_RST_SW:        return F("Software restart");
    case ESP_RST_PANIC:     return F("Exception/panic");
    case ESP_RST_INT_WDT:   return F("Interrupt watchdog");
    case ESP_RST_TASK_WDT:  return F("Task watchdog");
    case ESP_RST_WDT:       return F("Other watchdog");
    case ESP_RST_DEEPSLEEP: return F("Deep sleep");
    case ESP_RST_BROWNOUT:  return F("Brownout");
    case ESP_RST_SDIO:      return F("SDIO");
    default:                return F("Unknown");
  }
#endif
}

uint32_t freeHeap() { return ESP.getFreeHeap(); }

uint32_t vccMilliVolt() {
#if defined(ESP8266)
  return ESP.getVcc();
#else
  return 0;
#endif
}

static uint32_t rebootAt = 0;
static bool rebootPending = false;

void requestReboot(uint32_t delayMs) {
  rebootPending = true;
  rebootAt = millis() + delayMs;
  logf("Reboot requested (in %lu ms)", (unsigned long)delayMs);
}

void handleRebootRequest() {
  if (rebootPending && (int32_t)(millis() - rebootAt) >= 0) {
    logf("Rebooting now");
    delay(100);
    ESP.restart();
  }
}
