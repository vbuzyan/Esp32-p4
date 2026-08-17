# Porting GhostESP to the CrowPanel Advanced 9" ESP32-P4

Roadmap to run [GhostESP](https://github.com/GhostESP-Revival/GhostESP) on the
Elecrow CrowPanel Advanced 9-inch ESP32-P4 HMI board (`DHE04209D`), using a
**dedicated WiFi module** as the radio.

---

## 1. Architecture: why the ESP32-P4 needs help

The ESP32-P4 is a fast RISC-V applications processor with **no WiFi and no
Bluetooth radio.** Every other board GhostESP supports (ESP32, S2, S3, C3, C5,
C6) has a radio on-die and lets GhostESP drive it directly — including the
privileged monitor-mode and raw-injection paths that GhostESP is built on:

- `esp_wifi_80211_tx` (raw frame injection) — used by every attack
  (deauth, beacon spam, EAPOL logoff, auth flood, GTK abuse, channel-switch,
  probe flood).
- `esp_wifi_set_promiscuous` + RX callback — used by every sniffer/scanner
  (airspace monitor, station scan, handshake/PMKID capture, packet monitor,
  flock/aerial detectors).

The P4 can borrow a radio in two ways. **We recommend the second.**

### Option A — onboard C6 via esp-hosted (the hard way)

The board has an ESP32-C6 wired over SDIO, driven by Espressif's
[`esp-hosted`](https://github.com/espressif/esp-hosted-mcu) / `esp_wifi_remote`.
Standard `esp_wifi_*` calls are forwarded P4→C6. **But** the hosted RPC layer
does **not** forward `esp_wifi_80211_tx` or `esp_wifi_set_promiscuous` — those
stubs are commented out. Making them work means writing a **custom-RPC extension
on both the P4 and the C6 slave firmware** (demonstrated only as a proof of
concept: [`r4d10n/esp32p4-c6-wifi-test`](https://github.com/r4d10n/esp32p4-c6-wifi-test)).
This is research-grade firmware work and is **not** recommended as the primary
path.

### Option B — a dedicated WiFi module over GhostLink (recommended)

Add a second ESP32 (C6/C5/S3) as the radio and connect it to the P4 with a
3-wire UART. The module runs **stock GhostESP for its native target**, so all
radio features work with zero custom firmware. The P4 relays commands to it and
displays results.

```
   ┌──────────────────────────┐   3-wire UART    ┌───────────────────────────┐
   │        ESP32-P4          │   TX/RX/GND      │   WiFi module (ESP32)     │
   │   CrowPanel 9" display   │ ◄──────────────► │   C6 / C5 / S3            │
   │  GhostESP UI + relay      │    GhostLink     │  full native GhostESP     │
   └──────────────────────────┘                  └───────────────────────────┘
```

Trade-off to be honest about: GhostLink relays **commands**, not raw frames, so
the radio work (and any captured data / SD logging of it) happens **on the
module**. The P4 drives it and shows output; live per-packet views that assume a
local promiscuous callback will reflect the module's activity via relayed
status, not a local frame stream. For the vast majority of GhostESP's workflow
(scan, attack, capture-to-SD, wardrive) this is exactly how GhostLink is already
used in the field.

---

## 2. The core radio dependencies (evidence)

Confirmed by grepping the GhostESP tree, so the plan is grounded in fact:

- `esp_wifi_80211_tx` → `main/managers/wifi_manager.c`,
  `main/managers/plugin_api_lowlevel.c`, and `main/attacks/wifi/`:
  `eapol_logoff.c`, `beacon_spam.c`, `bad_msg.c`, `auth_flood.c`, `gtk_abuse.c`,
  `channel_switch_attack.c`, `deauth_attack.c`, `probe_request_flood.c`.
- `esp_wifi_set_promiscuous` → `scans/wifi/airspace_monitor.c`,
  `scans/wifi/station_scan.c`, `scans/wifi/arp_scan.c`, `managers/wifi_manager.c`,
  `managers/views/packet_monitor_screen.c`, `managers/flock_detector_manager.c`,
  `managers/aerial_detector_manager.c`, `attacks/wifi/deauth_attack.c`,
  `attacks/wifi/sae_flood.c`.

On a native-radio module these all Just Work — which is the whole point of
Option B.

---

## 3. GhostLink: the clean way to use the added module

GhostLink (docs: `getting-started/dual-communication.md`, hardware:
`hardware/ghostlink-p1.md`) is GhostESP's two-device link:

- **Transport:** UART, 115200 baud, 3 wires (TX→RX, RX→TX, GND).
- **Model:** each board runs GhostESP; one sends command strings to the other.
  Default GhostLink pins are **TX GPIO6 / RX GPIO7** (17/16 on classic ESP32);
  changeable at runtime with `commsetpins <TX> <RX>` (persisted).
- **Commands:** `commstatus`, `commsend <cmd>`, `commconnect`, `commdiscovery`,
  `commdisconnect`. Example: `commsend attack -d`, `commsend capture -probe`.
- **UI:** when connected, a split-view terminal shows local logs left, remote
  responses right — ideal for the 9" panel.

GhostESP even ships GhostLink board profiles as a reference for a
core+peer split: `configs/sdkconfig.ghostlink_p1_core` (S3, `WITH_SCREEN=y`) and
`configs/sdkconfig.ghostlink_p1_peer` (C3, headless radio). The P4 build is the
analog of *core*; the WiFi module is the analog of *peer*.

**Module choice:** any ESP32 GhostESP supports. Recommended **ESP32-C6** (WiFi 6
+ BLE, matches the board's radio) or **ESP32-C5** (adds 5 GHz). An **ESP32-S3**
peer additionally unlocks USB/BadUSB-class features on the radio node. Flash it
with the matching stock GhostESP release — nothing in this repo needed for the
module itself.

---

## 4. Display & touch bring-up (P4 side)

The 9" panel is **1024×600 MIPI-DSI** (ILI9881C-class, 2 data lanes). GhostESP
renders through **LVGL** on an `esp_lcd` panel handle.

State of the tree:
- GhostESP has **no wired-up MIPI-DSI path** in its display manager. Its
  `esp_lcd` drivers are SPI/QSPI/RGB (e.g. `main/vendor/drivers/ST7262.c`). The
  only DSI code present is inside the unused **M5GFX** C++ component
  (`components/M5GFX/.../esp32p4/Panel_DSI.*`, `Panel_ILI9881C.*`), not hooked
  into GhostESP's flush path.
- **Touch is easy:** `components/lvgl_esp32_drivers/lvgl_touch/gt911.c` exists and
  is reusable once I²C pins + reset/INT are set.

Work required:
1. MIPI-DSI panel init via ESP-IDF v6 `esp_lcd_mipi_dsi` + `esp_lcd_dpi_panel`
   and the ILI9881C init sequence → an `esp_lcd_panel_handle_t`.
2. Point GhostESP's LVGL flush at that handle (double-buffered in PSRAM — the P4
   has room for a full 1024×600 framebuffer).
3. Wire GT911 into LVGL input with the board's I²C pins + reset/INT.
4. Add a display-controller option so the board `sdkconfig` selects the DSI path.

Copy exact panel timings from Elecrow's `factory_sourcecode`/`example` folders in
their board repo — don't guess porches or lane rate.

---

## 5. Board profile & build integration (P4 side)

Nothing in GhostESP knows about a P4 board yet.

1. **`configs/sdkconfig.crowpanel_p4_9inch`** — new profile:
   `CONFIG_IDF_TARGET_ESP32P4=y`, PSRAM on, OTA+SD partition table, screen at
   1024×600, the DSI display option (§4), GT911 touch, GhostLink UART enabled.
   Seed it from `boards/crowpanel_advanced_9inch_p4/sdkconfig.defaults` in this
   repo, then flesh out with `idf.py set-target esp32p4 && idf.py menuconfig`.
2. **`main/idf_component.yml`** — add `espressif/esp_wifi_remote` +
   `espressif/esp_hosted` gated `target in ["esp32p4"]` **only if** you also want
   the onboard-C6 station mode for the P4's own WebUI (Option A connectivity, no
   custom RPC). Skip if the P4 is display-only. `elf_loader` already lists
   `esp32p4`, so native SD apps should compile.
3. **`main/CMakeLists.txt`** — a P4 branch: include the DSI display source,
   exclude S3-only pieces (USB HID host, camera guards), and (if used) the
   `esp_wifi_remote` glue.
4. **`build.py`** — add the board (`idf_target: "esp32p4"`) so it builds in CI.

---

## 6. Phased plan

**Phase 0 — Module first (fastest win).** Flash stock GhostESP onto the chosen
WiFi module (C6/C5/S3). You now have a fully working GhostESP handheld on its
own. Everything below just adds the big screen.

**Phase 1 — P4 board bring-up.** Minimal ESP-IDF v6 P4 app: light the DSI panel
with an LVGL "hello", read GT911 touch. De-risks the display, which is the P4's
hardest unknown. (Elecrow factory source = known-good DSI timings.)

**Phase 2 — GhostESP on the P4 in relay mode.** Add the board profile (§5), get
GhostESP to compile/boot for `esp32p4`, render its UI on the panel, take touch,
and drive the module over GhostLink UART (`commsend …`). Guard/stub the P4's
*local* promiscuous/injection call sites so it links without a local radio.
Result: full GhostESP features (executed on the module) on a 9" touchscreen.

**Phase 3 — Optional P4 self-connectivity.** Wire `esp_wifi_remote` + the
onboard C6 so the P4 can also join WiFi and host its own WebUI/AP (normal hosted
path, no custom RPC). Nice-to-have.

**Phase 4 — Polish.** SD card, battery/RGB, OTA for the P4 image, expansion-bay
modules (SX1262/nRF24 over the SPI bay), peer OTA to update the module from the
P4 (GhostESP already has `peer_ota_manager`).

Each phase is independently useful and shippable.

> Note: Option A's custom-RPC work (making the P4 *itself* sniff/inject through
> the onboard C6) is **deliberately not in this plan.** It's only worth doing if
> you specifically need a single-chip P4 solution with no external module.

---

## 7. Verify on real hardware

- **GhostLink UART pins** on the P4 — pick two free GPIOs not used by DSI, touch,
  SD, or audio; set via `commsetpins`. Recorded as `TODO` in `pins.h`.
- **DSI panel timings / ILI9881C init** — copy from Elecrow factory source.
- **GT911 I²C address at reset** — `0x5D` vs `0x14` depends on INT level during
  reset; the driver must sequence reset accordingly.
- **Onboard-C6 SDIO pins** (only if doing Phase 3) — read off Elecrow's
  `Eagle_SCH&PCB` schematic; `TODO` in `pins.h`.
- Which GPIOs the replaceable-module bay exposes and whether they collide with
  SD/audio.

---

## 8. References

- GhostESP — https://github.com/GhostESP-Revival/GhostESP
- GhostLink (dual communication) — GhostESP docs `getting-started/dual-communication.md`
- Elecrow CrowPanel 9" P4 board repo (schematic, factory source, examples) —
  https://github.com/Elecrow-RD/CrowPanel-Advanced-9inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
- esp-hosted-mcu (only for Option A / Phase 3) — https://github.com/espressif/esp-hosted-mcu
- P4+C6 custom-RPC proof of concept (Option A only) — https://github.com/r4d10n/esp32p4-c6-wifi-test
- ESP-IDF MIPI-DSI LCD docs — https://docs.espressif.com/projects/esp-idf/
