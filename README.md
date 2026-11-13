# SDM630toMQTT

Reads one or more **Eastron SDM energy meters** (SDM630, SDM230, SDM220, SDM120) over **Modbus RTU / RS485** and publishes the values to **MQTT**, with **Home Assistant auto-discovery**. Configuration and live values are in a built-in **web UI**.

Runs on:

| Board | PlatformIO env |
|---|---|
| ESP8266, e.g. Wemos D1 mini and clones | `d1_mini` |
| ESP32-S3, e.g. DevKitC-1 or SuperMini | `esp32s3` |

> Version 2 is a full rewrite. The old firmware (and the other old projects in this repository) are in [deprecated/](deprecated/) and are no longer maintained.

## Features

- Several meters on one RS485 bus, each with its own Modbus address, type, name and MQTT sub-topic.
- **Bus scan:** finds meters on a range of Modbus addresses, suggests the right type (3-phase or single phase) and adds them with one click.
- You can turn each register on or off for each meter. Registers are grouped into as few Modbus requests as possible.
- Live values, meter status and error counters in the web UI.
- MQTT topics are the same as in the old firmware by default (see below), and you can change them: base topic, per-meter topic, one topic per value or JSON, retain and number of decimals.
- Home Assistant MQTT discovery: each meter is its own device, with correct units, device classes and `total_increasing` for energy, plus availability via an LWT.
- If the WiFi is not set up or is lost, the gateway opens an access point (captive portal). It keeps trying to reconnect to your WiFi in the background and switches the AP off when the WiFi comes back.
- Watchdogs:
  - Loop watchdog: the hardware/task watchdog resets the chip if `loop()` stops running for 45 s.
  - Health watchdog: reboots the gateway if WiFi, MQTT or Modbus stay down longer than the configured time, or if free heap gets too low.
- Firmware update from the web UI (`/update`).
- Nothing in the main loop blocks. Modbus, MQTT and WiFi all run as non-blocking state machines.

## Hardware

Use a TTL to RS485 module. Modules with automatic direction control need no extra pin. MAX485-style modules need a DE/RE pin, which you set in the web UI.

**D1 mini (default wiring, same as the old firmware)**

| D1 mini | RS485 module |
|---|---|
| TX (GPIO1) | DI / TXD |
| RX (GPIO3) | RO / RXD |
| 3V3 / 5V, GND | VCC, GND |

- Alternatively, tick *"use D7(RX)/D8(TX)"* in the Modbus tab to move the UART to D7/D8. That frees the USB serial port.
- Debug logs go to **D4 (GPIO2)** at 115200 baud (TX only). They are also in the web UI under *System → Log*.
- Disconnect the RS485 module while flashing over USB.

**ESP32-S3:** the default pins are RX = GPIO18 and TX = GPIO17. You can change any of the pins in the web UI. Logs go to the native USB port.

Wire the A/B lines of all meters in parallel, and give every meter its own Modbus address (set on the meter's display).

## Build and flash

You need [PlatformIO](https://platformio.org/), either the VS Code extension or `pip install platformio`.

```sh
pio run -e d1_mini -t upload     # ESP8266
pio run -e esp32s3 -t upload     # ESP32-S3
```

After the first flash you can update from the web UI: *System → Firmware update*, then upload `.pio/build/<env>/firmware.bin`.

> **ESP8266 tip:** the first software reboot right after a *serial* upload can hang in the boot loader. This is a known ESP8266 quirk. Power-cycle the board once after flashing over USB.

## First setup

1. Power the board. With no WiFi configured, it opens the access point **`SDM-Gateway-XXXXXX`** (password **`sdm630mqtt`**).
2. Connect to it. The setup page should open by itself; if it doesn't, browse to http://192.168.4.1.
3. **WiFi tab:** scan for networks, pick yours and enter the password.
4. **MQTT / HA tab:** enter the broker address and credentials.
5. **Meters tab:** click **Scan bus** to find your meters and add them, or add them by hand (address, type, name).
6. Click **Save & reboot**. Afterwards the UI is at `http://sdm-gateway-xxxxxx.local/` or at the IP address your router assigned.

You can set a web UI password (user `admin`) in the *System* tab.

## MQTT

Default topics (`<id>` is the 6-hex-digit hardware ID):

| Topic | Payload |
|---|---|
| `<id>/PowerMeter/<meter>/<value>` | e.g. `1A2B3C/PowerMeter/1/Voltage/1` → `231.45` |
| `<id>/PowerMeter/Uptime`, `VCC`, `RSSI`, `LastSeenDateTime`, `LastBootDateTime`, `LastConnectedDateTime` | system values |
| `<id>/PowerMeter/MAC`, `HardwareId`, `Version`, `DeviceType`, `IP` | published on connect |
| `<id>/PowerMeter/status` | `online` / `offline` (retained, LWT) |
| `<id>/PowerMeter/control` | send `reboot`, `discovery` or `system` |

- `<meter>` defaults to the meter's Modbus address, which is how the old firmware named it.
- Value names are also unchanged. For example, the SDM630 *full* profile uses `Voltage/1`, `PowerActive/Sum` and `Energy/ImportkWh`.
- Other options: JSON mode publishes one object per meter to `<id>/PowerMeter/<meter>`, and *Both* publishes the JSON object as well as the separate topics.

### Home Assistant

- Discovery is on by default (prefix `homeassistant`).
- Every meter appears as its own device, linked to the gateway device.
- If you remove a meter or turn off a value, its entity is removed from Home Assistant when you save.
- The meters are read every 1.5 s (Modbus poll interval, same as the old firmware), so the web UI shows live values. MQTT and Home Assistant get the latest values every **10 s** (publish interval, set in the *MQTT / HA* tab; `0` publishes after every poll like the old firmware).

## Meter profiles

| Type | Values |
|---|---|
| `630` | SDM630 full (~70 values: per phase V/A/W/VA/var/PF/angle, totals, energy, demand, THD, serial number) |
| `630s` | SDM630 small (per phase V/A/W/VA/var, total power, neutral current, frequency) |
| `230` / `230s` | SDM230 full / small |
| `220` | SDM220 |
| `120` | SDM120 |

## What was wrong with the old firmware (v1)

- **Out-of-bounds array write:** the meter array had 4 entries (`eastron[4]`), but the code used indices 1..4. Index 4 wrote past the end of the array and corrupted memory, which caused random crashes and hangs.
- **Undefined behaviour:** `xMQTT::Connect()` and `Disconnect()` are declared `bool` but returned nothing. Modern GCC compiles this into code that can loop forever or crash.
- **Blocking loops:** Modbus waited for answers in a busy loop that did not yield. A slow broker or a missing meter could freeze `loop()` for many seconds at a time.
- **WiFi reconnect only ran while MQTT was connected.** When the WiFi dropped, MQTT dropped too, so the reconnect code never ran.
- **Outdated dependencies:** it relied on a forked WiFiManager, ArduinoJson 5, NtpClientLib, xlogger and symlinked libraries, so it no longer compiles.
