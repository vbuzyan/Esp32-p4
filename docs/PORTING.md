# Porting GhostESP to the CrowPanel Advanced 9" ESP32-P4 (V1.0)

Roadmap to run [GhostESP](https://github.com/GhostESP-Revival/GhostESP) on the
Elecrow CrowPanel Advanced 9" ESP32-P4 HMI (`DHE04209D`, rev V1.0).

---

## 1. Architecture

The ESP32-P4 has **no WiFi/BLE radio.** On this board wireless comes from an
onboard **ESP32-C6** over SDIO via Espressif `esp-hosted` / `esp_wifi_remote`.

GhostESP's core features are built on `esp_wifi_80211_tx` (raw injection — all
attacks) and `esp_wifi_set_promiscuous` (sniffing — all capture/scan). The
hosted RPC layer **does not forward** either (stubs commented out), so making
the *P4 itself* sniff/inject needs a custom-RPC extension on both the P4 and the
C6 slave firmware (research-grade; only demonstrated as a PoC).

**Recommended plan:** don't fight that. Add a dedicated **ESP32-C6 / C5 / S3**
radio module running **stock GhostESP** (all features work natively), and use
the P4 as the display/UI front-end driving it over a 3-wire **GhostLink** UART
(`commsend <cmd>`). GhostLink relays commands, not raw frames, so the radio work
happens on the module — exactly how GhostLink is used in the field.

---

## 2. Phase 1 — board bring-up ✅ DONE

Proven on hardware (`firmware/`, ESP-IDF v6.0.2, ESP32-P4 rev v1.3):

- **EK79007 MIPI-DSI panel** at 1024×600 — 2 lanes @ 900 Mbps, DPI 51 MHz,
  RGB565, timings hbp/hpw/hfp = 160/70/160, vbp/vpw/vfp = 23/10/12.
- **LDOs:** ch3 = 2500 mV (DSI PHY) + ch4 = 3300 mV (panel), acquired before
  panel init — mandatory or the screen stays dark.
- **GT911 touch** over I²C (SCL 46 / SDA 45 / INT 42 / RST 40).
- **LVGL v8** live touch UI.

### Gotchas found (these bite the GhostESP port too)

1. **DMA2D must be enabled explicitly** in IDF 6.0:
   `esp_lcd_dpi_panel_enable_dma2d(panel)`. The old `.flags.use_dma2d` field was
   removed. Without DMA2D the DPI `on_color_trans_done` event never fires and the
   LVGL flush never completes.
2. **`esp_lvgl_port` deadlocks** on this DSI panel under IDF 6.0.2 (2.8.0 *and*
   2.9.0): it registers the display and starts flushing **before** registering
   the DMA-completion callback, so the LVGL task spins forever (IDLE0 watchdog).
   2.9.0 also references `on_frame_buf_complete`, which is RGB-only in 6.0.2
   (DSI uses `on_refresh_done`). **Fix:** a direct LVGL v8 integration that
   registers `on_color_trans_done` *before* starting the LVGL task. See
   `firmware/main/crowpanel_p4_bringup.c`.
3. Early P4 silicon (rev v1.3): set `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` +
   `CONFIG_ESP32P4_REV_MIN_100=y`.

---

## 3. Phase 2 — GhostESP on the P4 (relay mode)

1. **Board profile** `configs/sdkconfig.crowpanel_p4_9inch` in the GhostESP tree:
   `CONFIG_IDF_TARGET_ESP32P4=y`, PSRAM, 16 MB flash, screen 1024×600, GT911,
   GhostLink UART. Seed from `boards/crowpanel_advanced_9inch_p4/sdkconfig.defaults`.
2. **Display driver:** GhostESP's LVGL layer drives SPI/QSPI/RGB panels, not DSI.
   Add an EK79007 DSI path and wire it into GhostESP's LVGL flush **using the
   direct-callback approach from Phase 1** (do not use esp_lvgl_port). GT911 is
   already supported.
3. **GhostLink:** enable the UART link; pick two free P4 GPIOs (not 6/7 — those
   are the module-bay SPI). Drive the radio module with `commsend`.
4. **Guard local radio calls:** the P4 has no local radio, so `#ifdef`/stub the
   promiscuous/injection call sites so GhostESP links and boots; real radio work
   happens on the module.
5. **build.py / CMakeLists:** add the P4 board and a P4 target branch.

## 4. Phase 3 — optional P4 self-WiFi

Wire `esp_wifi_remote` + the onboard C6 (SDIO pins from Elecrow's `Eagle_SCH&PCB`
schematic — still TODO in `pins.h`) so the P4 can also join WiFi / host its own
WebUI. Normal hosted path, no custom RPC.

---

## 5. References

- GhostESP — https://github.com/GhostESP-Revival/GhostESP
- Elecrow CrowPanel 9" P4 (schematic, factory source) —
  https://github.com/Elecrow-RD/CrowPanel-Advanced-9inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
- esp-hosted-mcu (Phase 3) — https://github.com/espressif/esp-hosted-mcu
- P4+C6 custom-RPC PoC (if ever doing single-chip) — https://github.com/r4d10n/esp32p4-c6-wifi-test
