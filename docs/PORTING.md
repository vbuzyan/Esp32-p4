# Porting GhostESP to the CrowPanel Advanced 9" ESP32-P4

This document scopes the work required to run [GhostESP](https://github.com/GhostESP-Revival/GhostESP)
on the Elecrow CrowPanel Advanced 9-inch ESP32-P4 HMI board (`DHE04209D`). It is
written to be actionable: each section states what exists today, what is
missing, and the concrete steps to close the gap.

It is deliberately honest about feasibility. The headline conclusion:

> Bringing up the **board** (boot, display, touch, SD, basic WiFi-via-C6
> connectivity) is a **normal, achievable port**. Making GhostESP's **offensive
> WiFi features** (deauth, beacon spam, sniffing, handshake capture) work is a
> **research-grade firmware effort** because the ESP32-P4 cannot drive a radio
> directly and the hosted-WiFi transport does not forward the required
> low-level APIs.

---

## 1. Architecture: why the ESP32-P4 is different

Every other board GhostESP supports (ESP32, S2, S3, C3, C5, C6) has a WiFi/BLE
radio **on the same die** as the CPU. GhostESP talks to that radio directly
through `esp_wifi_*`, including the privileged monitor/injection paths.

The ESP32-P4 does **not** have a radio. On this board:

```
  ┌─────────────────┐        SDIO         ┌──────────────────┐
  │   ESP32-P4      │  CLK/CMD/D0..D3     │    ESP32-C6      │
  │  (host / apps)  │ ◄─────────────────► │ (WiFi6 + BLE5)   │
  │                 │                     │  runs esp-hosted │
  │ GhostESP UI,    │   esp_wifi_remote   │  "slave" firmware│
  │ LVGL, logic     │   RPC forwarding    │                  │
  └─────────────────┘                     └──────────────────┘
```

- The P4 runs the application (GhostESP) **and** an `esp_wifi_remote` shim.
- `esp_wifi_remote` serializes each `esp_wifi_*` call and ships it over SDIO to
  the C6, which runs Espressif's **esp-hosted-mcu slave firmware** and executes
  the real radio operation.
- **The C6 must be flashed separately** with compatible slave firmware. This is
  a second firmware image, not part of the P4 app.

Consequences for a port:
1. You maintain **two** firmware images (P4 host + C6 slave), and they must be
   version-matched.
2. Only the subset of `esp_wifi_*` that esp-hosted implements will work.
3. Latency and throughput are bounded by the SDIO link, not the radio.

---

## 2. The core blocker: monitor mode & raw injection

GhostESP's identity is low-level 802.11. Confirmed by grepping the source tree:

**`esp_wifi_80211_tx` (raw frame injection)** is used by:
`main/managers/wifi_manager.c`, `main/managers/plugin_api_lowlevel.c`, and the
attacks `eapol_logoff.c`, `beacon_spam.c`, `bad_msg.c`, `auth_flood.c`,
`gtk_abuse.c`, `channel_switch_attack.c`, `deauth_attack.c`,
`probe_request_flood.c`.

**`esp_wifi_set_promiscuous` + promiscuous RX callback** is used by:
`scans/wifi/airspace_monitor.c`, `scans/wifi/station_scan.c`,
`scans/wifi/arp_scan.c`, `managers/wifi_manager.c`,
`managers/views/packet_monitor_screen.c`, `managers/flock_detector_manager.c`,
`managers/aerial_detector_manager.c`, `attacks/wifi/deauth_attack.c`,
`attacks/wifi/sae_flood.c`.

**Neither is forwarded by stock esp-hosted.** The reference bring-up project
[`r4d10n/esp32p4-c6-wifi-test`](https://github.com/r4d10n/esp32p4-c6-wifi-test)
documents this directly:

| API                              | Direct (esp-hosted RPC) | Via CustomRpc extension |
| -------------------------------- | ----------------------- | ----------------------- |
| `esp_wifi_set_promiscuous`       | **NOT SUPPORTED**       | OK                      |
| `esp_wifi_set_promiscuous_filter`| **NOT SUPPORTED**       | OK                      |
| `esp_wifi_80211_tx`              | **NOT SUPPORTED**       | OK                      |

Their workaround: a **CustomRpc extension** — extra RPC opcodes added to the C6
slave firmware plus matching client stubs on the P4 — that (a) enables
promiscuous mode on the C6 and streams captured frames back over SDIO to a P4
callback, and (b) accepts raw frames from the P4 and calls `esp_wifi_80211_tx`
locally on the C6. Their probe captured ~514 packets in 10 s, proving the path
works, but it is a proof of concept, not a maintained library.

### What this means for the three tiers of GhostESP features

| Feature tier                                              | Feasibility on P4+C6 |
| -------------------------------------------------------- | -------------------- |
| **Connected-mode** (join AP, HTTP, WebUI, OTA, mDNS/NetBIOS/port scans, DNS sinkhole) | ✅ Works over stock esp-hosted — these use normal station-mode sockets. |
| **Sniffing** (scans, handshake/PMKID capture, airspace monitor, packet monitor) | ⚠️ Needs the CustomRpc promiscuous path on the C6. |
| **Injection** (deauth, beacon spam, karma, all attacks) | ⚠️ Needs the CustomRpc raw-TX path on the C6. |
| **BLE** (scans, spam, AirTag, GATT)                      | ⚠️ Possible via HCI-over-hosted (NimBLE host on P4, controller on C6), but GhostESP's BLE manager assumes a local controller — needs rework. |
| **SubGHz / IR / NFC / BadUSB**                           | Independent of the C6; depend only on P4 GPIO + expansion modules. BadUSB uses P4 USB-OTG. |

**Bottom line:** without investing in the C6 CustomRpc firmware, you get a
GhostESP that can join networks and do IP-layer recon but cannot deauth, spam,
or sniff — i.e. not the tool people install GhostESP for.

---

## 3. Display & touch bring-up

The 9" panel is **1024×600 MIPI-DSI** (ILI9881C-class, 2 data lanes). GhostESP
renders its UI through **LVGL** on top of an `esp_lcd` panel handle.

State of the tree:
- GhostESP has **no wired-up MIPI-DSI path** in its own display manager. Its
  `esp_lcd` drivers are SPI/QSPI/RGB (e.g. `main/vendor/drivers/ST7262.c` for
  RGB). The only DSI code present lives inside the unused **M5GFX** C++ component
  (`components/M5GFX/.../esp32p4/Panel_DSI.*`, `Panel_ILI9881C.*`) and is not
  hooked into GhostESP's LVGL flush path.
- **Touch is easier:** a GT911 driver already exists
  (`components/lvgl_esp32_drivers/lvgl_touch/gt911.c`) and is reusable as-is once
  the I²C pins are set.

Work required:
1. Add a MIPI-DSI panel init using ESP-IDF v6's `esp_lcd_mipi_dsi` +
   `esp_lcd_dpi_panel` APIs and the ILI9881C init sequence, producing an
   `esp_lcd_panel_handle_t`.
2. Point GhostESP's LVGL flush callback at that handle (double-buffered in
   PSRAM; the P4 has ample RAM for a 1024×600 framebuffer).
3. Wire GT911 into the LVGL input device with the board's I²C pins and reset/INT.
4. Add a new display-controller branch/option so the board's `sdkconfig`
   selects the DSI path rather than an SPI/RGB controller.

This is standard ESP32-P4 LVGL bring-up; Elecrow's own `factory_sourcecode` and
`example` folders in their board repo give a known-good DSI init sequence to
copy timings from.

---

## 4. Board profile & build integration

None of GhostESP's plumbing knows about a P4 board yet. To add one:

1. **`configs/sdkconfig.crowpanel_p4_9inch`** — new board profile:
   `CONFIG_IDF_TARGET_ESP32P4=y`, PSRAM on, partition table with OTA + SD, screen
   enabled at 1024×600, the DSI display option from §3, GT911 touch, and the
   `esp_wifi_remote` component pulled in.
2. **`main/idf_component.yml`** — add `espressif/esp_wifi_remote` and
   `espressif/esp_hosted` with a `target in ["esp32p4"]` rule (mirroring how
   `elf_loader` is already gated). Note `elf_loader` already lists `esp32p4`, so
   native SD apps are expected to compile on P4.
3. **`main/CMakeLists.txt`** — a P4 branch: exclude S3-only components
   (USB HID host, camera guards that assume local radio), include the DSI
   display source and the `esp_wifi_remote` glue.
4. **`build.py`** — add the board to the board list (`idf_target: "esp32p4"`) so
   it participates in CI/manifest builds.
5. **C6 slave image** — decide how it ships. Options: (a) document a manual
   `esp-hosted` slave flash step, or (b) embed the slave `.bin` and add a P4-side
   updater (GhostESP already has an OTA/updater pattern — see the Banshee C5
   updater in `main/CMakeLists.txt` for how a second image is built and embedded).

---

## 5. Suggested phased plan

**Phase 0 — Prove the hardware path (no GhostESP yet).**
Build a minimal ESP-IDF v6 app for the P4 that: flashes the C6 with esp-hosted
slave firmware, brings up `esp_wifi_remote`, joins a WiFi network, and lights
the DSI panel with an LVGL "hello". This de-risks the two hardest unknowns
(hosted link + DSI timings) before touching GhostESP. Elecrow's factory sources
+ `espressif/esp-hosted-mcu` examples are the references.

**Phase 1 — GhostESP in connected mode.**
Add the board profile (§4), get GhostESP to compile and boot for `esp32p4`,
render its UI on the DSI panel, take touch input, and use **station-mode-only**
features (WebUI, IP scanners, DNS sinkhole). Expect to `#ifdef`-guard or stub the
promiscuous/injection call sites so it links. This yields a real, usable — if
limited — GhostESP.

**Phase 2 — CustomRpc: sniffing.**
Port the `esp32p4-c6-wifi-test` CustomRpc promiscuous path: add RX-streaming
opcodes to the C6 slave, add P4 client stubs that satisfy
`esp_wifi_set_promiscuous` / `..._filter` and deliver frames to GhostESP's
existing promiscuous callback. Unlocks scans, handshake/PMKID capture, monitors.

**Phase 3 — CustomRpc: injection.**
Add the raw-TX opcode (`esp_wifi_80211_tx` executed on the C6). Unlocks the
attack suite. Validate deauth/beacon spam against a lab AP you own.

**Phase 4 — BLE, then polish.**
Route BLE through hosted HCI (NimBLE host on P4, C6 controller) and adapt
`ble_manager.c`. Then expansion modules (SX1262/nRF24 over the SPI bay), SD,
battery, OTA for both images.

Each phase is independently useful and independently shippable.

---

## 6. Things you must verify on real hardware

- **The C6↔P4 SDIO pins** (CLK/CMD/D0–D3) and the C6 reset/boot straps. These
  are the single most important pins for WiFi and are **not** yet confirmed for
  this board — get them from Elecrow's `Eagle_SCH&PCB` schematic. `pins.h` marks
  them `TODO`.
- **The DSI panel timings** (porches, lane rate, ILI9881C init) — copy from
  Elecrow factory source, don't guess.
- **GT911 I²C address at reset** — `0x5D` vs `0x14` depends on the INT pin level
  during reset; the driver must drive the reset sequence accordingly.
- Which GPIOs the replaceable-module bay actually exposes, and whether they
  collide with SD or audio.

---

## 7. References

- GhostESP — https://github.com/GhostESP-Revival/GhostESP
- Elecrow CrowPanel 9" P4 board repo (schematic, factory source, examples) —
  https://github.com/Elecrow-RD/CrowPanel-Advanced-9inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
- esp-hosted-mcu (C6 slave firmware + host RPC) —
  https://github.com/espressif/esp-hosted-mcu
- ESP32-P4 + C6 CustomRpc promiscuous/injection proof of concept —
  https://github.com/r4d10n/esp32p4-c6-wifi-test
- ESP-IDF MIPI-DSI LCD docs — https://docs.espressif.com/projects/esp-idf/
