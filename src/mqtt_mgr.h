// MQTT publishing + Home Assistant discovery.
//
// Default topic layout (same as the old firmware):
//   <hardwareId>/PowerMeter/<meterTopic>/<RegisterName>   e.g. 1A2B3C/PowerMeter/1/Voltage/1
//   <hardwareId>/PowerMeter/Uptime | VCC | RSSI | LastSeenDateTime | ...
//   <hardwareId>/PowerMeter/control      <- commands: reboot, discovery
// New: <base>status (online/offline, retained, LWT) for availability.
#pragma once
#include <Arduino.h>
#include "config.h"

void mqttBegin();
void mqttLoop();
bool mqttConnected();
const char *mqttStateStr();
String mqttBaseTopic();

void mqttPublishMeter(size_t idx);   // after a meter poll
void mqttPublishSystem();            // Uptime, VCC, RSSI ...
void mqttOnCycleStart();

// Removes Home Assistant entities that do not exist in `next` any more (call before saving config).
void mqttCleanupDiscovery(const Config &next);
