# GhostESP on ESP32-P4 (CrowPanel Advanced 9" HMI)

Porting groundwork and roadmap for running [GhostESP](https://github.com/GhostESP-Revival/GhostESP)
on the **Elecrow CrowPanel Advanced 9-inch ESP32-P4 HMI AI Display**
(SKU `DHE04209D`, 1024×600 IPS, MIPI-DSI, capacitive touch).

> **Status: not a flashable build.** This repo contains the analysis, the
> verified board pinout, and a step-by-step porting plan — **not** a working
> GhostESP binary. There is a genuine architectural blocker (described below)
> that must be solved with firmware development before GhostESP's core features
> will run on this board. Read [`docs/PORTING.md`](docs/PORTING.md) first.

---

## TL;DR — why this is hard

The ESP32-P4 has **no WiFi and no Bluetooth radio of its own.** On this board,
all wireless comes from a companion **ESP32-C6** connected over **SDIO**, driven
by Espressif's [`esp-hosted`](https://github.com/espressif/esp-hosted-mcu) /
`esp_wifi_remote` framework. Standard `esp_wifi_*` calls are transparently
forwarded from the P4 to the C6.

GhostESP is built on exactly the two low-level APIs that framework does **not**
forward out of the box:

| GhostESP depends on            | Used for                                   | Works over stock esp-hosted? |
| ------------------------------ | ------------------------------------------ | ---------------------------- |
| `esp_wifi_80211_tx`            | deauth, beacon spam, all injection attacks | ❌ No (stub commented out)    |
| `esp_wifi_set_promiscuous` + RX cb | sniffing, handshake/PMKID capture, monitors | ❌ No (stub commented out)    |

So a naïve "add a board profile" build boots and then fails at runtime on nearly
every headline feature. Making them work requires a **custom RPC extension on
both the P4 host and the C6 slave firmware** — this has been demonstrated as a
proof of concept ([`r4d10n/esp32p4-c6-wifi-test`](https://github.com/r4d10n/esp32p4-c6-wifi-test))
but is not a drop-in.

## What's in this repo

- [`docs/PORTING.md`](docs/PORTING.md) — full technical roadmap: architecture,
  the radio blocker and how to attack it, display/touch bring-up, and a phased
  task list.
- [`boards/crowpanel_advanced_9inch_p4/pins.h`](boards/crowpanel_advanced_9inch_p4/pins.h)
  — board pin reference extracted from Elecrow's materials. **Several pins still
  require schematic verification** — see the header for which ones and why.

## Hardware summary

| Item        | Detail                                                        |
| ----------- | ------------------------------------------------------------ |
| Host MCU    | ESP32-P4 (RISC-V dual-core, no radio)                         |
| Radio       | ESP32-C6 co-processor over SDIO (WiFi 6 + BLE 5), esp-hosted  |
| Display     | 1024×600 IPS, **MIPI-DSI** (ILI9881C-class 2-lane)           |
| Touch       | GT911 capacitive, I²C (addr `0x5D` / `0x14`)                  |
| Expansion   | Replaceable wireless module bay (SX1262 / ESP32-H2 / nRF24 / WiFi-HaLow) |
| Camera      | Optional 2 MP (MIPI-CSI) on the "With Camera" variant         |

## Toolchain

GhostESP targets **ESP-IDF v6.0**. A P4 + C6 build additionally needs the
`esp_wifi_remote` / `esp-hosted-mcu` managed components and matching **C6 slave
firmware** flashed to the co-processor. None of that toolchain is present in
this environment, so nothing here has been compiled — treat every config value
as *proposed and unverified* until it builds on real ESP-IDF v6.0.

## License

GhostESP is GPL-3.0. Any firmware derived from it inherits that license.
