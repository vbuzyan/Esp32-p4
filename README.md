# GhostESP on ESP32-P4 — CrowPanel Advanced 9" HMI

Bringing [GhostESP](https://github.com/GhostESP-Revival/GhostESP) to the
**Elecrow CrowPanel Advanced 9-inch ESP32-P4 HMI AI Display**
(SKU `DHE04209D`, board rev **V1.0**, 1024×600 IPS, MIPI-DSI, capacitive touch).

## Status

| Phase | What | State |
| ----- | ---- | ----- |
| **1** | P4 board bring-up: EK79007 MIPI-DSI panel + GT911 touch + LVGL UI | ✅ **built, flashed, verified on hardware** (ESP-IDF v6.0.2, ESP32-P4 rev v1.3) |
| 2 | Port GhostESP itself to `esp32p4`, its UI on this panel, GhostLink to a radio module | ⏳ next |
| 3 | Optional: P4's own WiFi/WebUI via onboard C6 (`esp_wifi_remote`) | later |

Phase 1 lives in [`firmware/`](firmware/) and is a real, flashable app —
see [`firmware/README.md`](firmware/README.md) for build/flash steps.

## The architecture (why a radio module matters)

The **ESP32-P4 has no WiFi or Bluetooth radio.** On this board wireless comes
from a companion **ESP32-C6** over SDIO (Espressif `esp-hosted`). GhostESP is
built on the two low-level radio APIs that hosted transport does **not** forward
(`esp_wifi_80211_tx` raw injection, `esp_wifi_set_promiscuous` sniffing), so
making the *P4 itself* run GhostESP's attacks would need research-grade custom
esp-hosted RPC firmware.

**The tractable plan instead:** run stock GhostESP on a dedicated **ESP32-C6 / C5
/ S3** radio module (all natively supported targets — every feature works), and
use the P4 as the big-screen front-end driving it over a 3-wire **GhostLink**
UART. See [`docs/PORTING.md`](docs/PORTING.md) for the full roadmap.

## Hardware (verified V1.0 values)

| Item | Detail |
| ---- | ------ |
| Host MCU | ESP32-P4 (RISC-V dual-core, no radio), rev v1.3, 32 MB PSRAM, 16 MB flash |
| Display | 1024×600 IPS, **EK79007** MIPI-DSI, 2 lanes @ 900 Mbps, DPI 51 MHz, RGB565 |
| DSI power | LDO ch3 = 2500 mV (PHY), LDO ch4 = 3300 mV (panel) — both required |
| Backlight | GPIO 31 |
| Touch | GT911 I²C: SCL 46, SDA 45, INT 42, RST 40 (addr 0x5D / 0x14) |
| Onboard radio | ESP32-C6 over SDIO (esp-hosted) — for Phase 3 only |
| Expansion | Replaceable wireless-module bay (SPI: SX1262 / nRF24 / …) |

## Repo layout

- [`firmware/`](firmware/) — Phase 1 bring-up (ESP-IDF v6.0.2 project).
- [`docs/PORTING.md`](docs/PORTING.md) — architecture + phased roadmap.
- [`boards/crowpanel_advanced_9inch_p4/`](boards/crowpanel_advanced_9inch_p4/)
  — pin reference (`pins.h`) and a GhostESP board-profile seed
  (`sdkconfig.defaults`).

## Toolchain

ESP-IDF **v6.0.2** (installed at `C:\esp\v6.0.2`). Managed components:
`esp_lcd_ek79007`, `esp_lcd_touch_gt911`, `lvgl` v8. GhostESP targets v6.0, so
Phase 2 uses the same toolchain.

## License

GhostESP is GPL-3.0; firmware derived from it inherits that license.
