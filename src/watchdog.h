// Watchdogs:
//  1. Loop watchdog (hardware/task level): if loop() does not run for LOOP_WDT_SEC the chip resets.
//     ESP8266: SDK soft+hard WDT plus a timer based check; ESP32: task watchdog on the loop task.
//  2. Health watchdog: reboots when WiFi / MQTT / Modbus stay down longer than configured,
//     or when free heap gets critically low.
#pragma once
#include <Arduino.h>

void watchdogBegin();
void watchdogFeed();     // call every loop()
void watchdogLoop();     // health checks
