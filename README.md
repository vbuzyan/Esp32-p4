# GhostESP on ESP32-P4 (CrowPanel Advanced 9" HMI)

Porting groundwork and roadmap for running [GhostESP](https://github.com/GhostESP-Revival/GhostESP)
on the **Elecrow CrowPanel Advanced 9-inch ESP32-P4 HMI AI Display**
(SKU `DHE04209D`, 1024×600 IPS, MIPI-DSI, capacitive touch).

> **Status: groundwork, not a flashable build.** This repo holds the analysis,
> the board pinout, and a porting plan — not a compiled GhostESP binary. Read
> [`docs/PORTING.md`](docs/PORTING.md) first. There is no ESP-IDF v6 toolchain
> in this environment, so nothing here has been built or flashed; treat every
> config value as *proposed and unverified*.

---

## The plan (updated): two-node design with a dedicated WiFi module

The ESP32-P4 has **no radio of its own.** Rather than fight that with custom
low-level firmware, the recommended architecture uses a **second ESP32 as the
radio** and lets the P4 be the display/UI brain:

```
   ┌──────────────────────────┐   3-wire UART    ┌───────────────────────────┐
   │        ESP32-P4          │   TX/RX/GND      │   WiFi module (ESP32)     │
   │   CrowPanel 9" display   │ ◄──────────────► │   ESP32-C6 / C5 / S3      │
   │                          │    GhostLink     │                           │
   │  GhostESP UI on 1024×600 │    command relay │  full native GhostESP:    │
   │  touch, WebUI, logs      │                  │  deauth, sniff, capture…  │
   └──────────────────────────┘                  └───────────────────────────┘
```

**Why this works.** GhostESP's [GhostLink](docs/PORTING.md#3-ghostlink-the-clean-way-to-use-the-added-module)
feature relays GhostESP command strings between two boards over a 3-wire UART.
The WiFi module runs **stock GhostESP on a native-radio chip that GhostESP
already fully supports** (C6/C5/S3 are shipping targets), so every offensive
feature — deauth, beacon spam, promiscuous sniffing, handshake/PMKID capture —
works natively **on the module**. The P4 sends commands (`commsend attack -d`)
and shows results on the big screen. No custom radio firmware required.

This is the pivot: **adding a WiFi module removes the hard blocker.** The only
path that needed research-grade `esp-hosted` custom-RPC firmware was making the
*P4 itself* sniff/inject through the onboard C6 — with a dedicated module you
don't do that at all.

### What each node does

| Node | Runs | Provides |
| --- | --- | --- |
| **WiFi module** (C6/C5/S3) | stock GhostESP for that target | all radio features, natively |
| **ESP32-P4** (CrowPanel) | GhostESP ported to esp32p4 | UI on the 9" DSI panel, touch, GhostLink relay, optional own WebUI/AP via onboard C6 |

## What still has to be built for the P4 node

Even in the easy architecture, the P4 side is a real port (see `docs/PORTING.md`):

1. **Board profile for `esp32p4`** — GhostESP has no P4 config, isn't in
   `build.py`, and pulls in no `esp_wifi_remote`.
2. **MIPI-DSI display driver** — the 9" panel is DSI (ILI9881C-class); GhostESP's
   LVGL layer only drives SPI/QSPI/RGB today. GT911 touch already has a driver.
3. **GhostLink UART wiring** — pick two free P4 GPIOs for the link to the module.
4. **Optional: onboard-C6 station mode** via `esp_wifi_remote` so the P4 can also
   host its own WebUI — this is the *normal* hosted path, not the custom-RPC one.

## Hardware summary

| Item        | Detail                                                        |
| ----------- | ------------------------------------------------------------ |
| Host MCU    | ESP32-P4 (RISC-V dual-core, no radio)                         |
| Onboard radio | ESP32-C6 over SDIO (esp-hosted) — optional, for P4's own WiFi |
| Added radio | WiFi module running GhostESP (recommended: ESP32-C6/C5/S3)    |
| Display     | 1024×600 IPS, **MIPI-DSI** (ILI9881C-class 2-lane)           |
| Touch       | GT911 capacitive, I²C (addr `0x5D` / `0x14`)                  |
| Expansion   | Replaceable wireless-module bay (SPI: SX1262 / nRF24 / …)     |

## What's in this repo

- [`docs/PORTING.md`](docs/PORTING.md) — architecture, the GhostLink path,
  display/touch bring-up, board integration, and a phased plan.
- [`boards/crowpanel_advanced_9inch_p4/pins.h`](boards/crowpanel_advanced_9inch_p4/pins.h)
  — board pin reference (several pins still need schematic verification).
- [`boards/crowpanel_advanced_9inch_p4/sdkconfig.defaults`](boards/crowpanel_advanced_9inch_p4/sdkconfig.defaults)
  — starter Kconfig overlay for the P4 board profile (seed, then expand via
  `idf.py menuconfig`).

## Toolchain

GhostESP targets **ESP-IDF v6.0**. The P4 node additionally needs the
`esp_wifi_remote` managed component only if you enable the onboard-C6 WiFi;
the GhostLink relay itself needs only a UART. None of that toolchain is present
here, so nothing has been compiled.

## License

GhostESP is GPL-3.0. Any firmware derived from it inherits that license.
