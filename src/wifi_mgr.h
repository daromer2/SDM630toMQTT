// WiFi station with access point fallback.
//
//  * No SSID configured           -> AP only (setup mode).
//  * SSID configured              -> connect as station.
//  * Station down > apFallbackSec -> start AP "SDM-Gateway-XXXXXX" (captive portal) and
//                                    retry the station every staRetrySec in the background.
//  * Station back                 -> AP is switched off again (as soon as nobody uses it).
#pragma once
#include <Arduino.h>

void wifiBegin();
void wifiLoop();

bool wifiConnected();
bool wifiApActive();
String wifiApSsid();
uint8_t wifiApClients();
uint32_t wifiDownSinceMs();   // 0 when connected
const char *wifiStateStr();
