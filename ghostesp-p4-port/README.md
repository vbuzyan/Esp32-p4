# GhostESP → ESP32-P4 port (CrowPanel Advanced 9" V1.0)

Changes that make [GhostESP](https://github.com/GhostESP-Revival/GhostESP) build,
flash, and boot on the ESP32-P4. **Verified on hardware** (ESP32-P4 rev v1.3,
ESP-IDF v6.0.2): GhostESP boots stably and its full interactive CLI works over
serial (`help` lists wifi / ble / comm / capture / beacon / attack / … ).

## Status

| Works now | Not yet |
| --------- | ------- |
| Full build + link for `esp32p4` | On-screen UI (EK79007 DSI not wired into GhostESP's LVGL — runs headless) |
| Stable boot, "Ghost ESP INIT complete." | Local WiFi via onboard C6 (needs matching esp-hosted **slave firmware** on the C6) |
| Interactive serial CLI (all command categories) | Local BLE (NimBLE host builds; controller is on the C6) |
| GhostLink `comm` commands (drive an external radio module) | ESP-NOW / GTK-supplicant attacks (stubbed — run on the radio module) |

## The 5 changes (see `ghostesp-esp32p4.patch`)

1. **`main/idf_component.yml`** — add `esp_wifi_remote` + `esp_hosted` for
   `target == esp32p4` so the `esp_wifi_*` API compiles/links (P4 has no native
   radio; WiFi/BLE come from the onboard C6 over SDIO).
2. **`sdkconfig.defaults.p4`** — the P4 board profile: target + rev-v1.3 opt-in,
   16 MB flash / PSRAM, NimBLE (controller external), the full LVGL config
   inherited from the `crowtech7inch` board, a 16 MB OTA partition table,
   FreeRTOS 1000 Hz, and esp-hosted SDIO RX buffers shrunk (streaming mode
   over-allocates internal DMA RAM and asserts at boot).
3. **`main/p4_radio_stubs.c`** (new) — P4-only link stubs for radio internals
   `esp_wifi_remote` doesn't provide (`esp_now_*`, `gWpaSm`,
   `esp_wifi_get_sta_key_internal`). Compiles to nothing on other targets.
4. **`main/managers/ble_manager.c`** — guard `esp_bt.h` / the classic-BT
   controller call (no local BT controller on P4).
5. **`main/managers/wifi_manager.c`** — skip `esp_wifi_init()` on P4 (without a
   responsive C6 slave it drives the hosted SDIO transport into a retry loop that
   resets the chip). Phase 3: gate this on a hosted-link-up check once the C6
   runs matching slave firmware.

## How to build

```powershell
git clone --depth 1 https://github.com/GhostESP-Revival/GhostESP
cd GhostESP
git apply ..\ghostesp-p4-port\ghostesp-esp32p4.patch   # or copy the 5 files in
copy ..\ghostesp-p4-port\sdkconfig.defaults.p4 sdkconfig.defaults
idf.py set-target esp32p4
idf.py build
idf.py -p COM8 flash monitor        # use your board's COM port
```

## Next steps

- **Display:** wire the EK79007 MIPI-DSI panel into GhostESP's display manager /
  LVGL flush path, reusing the approach proven in `../firmware/` (register the
  DPI `on_color_trans_done` callback before starting LVGL; enable DMA2D; don't
  use `esp_lvgl_port`). Then re-enable `CONFIG_WITH_SCREEN`.
- **Radio:** either flash the onboard C6 with matching esp-hosted **slave**
  firmware (unlocks WiFi/BLE on the P4 itself), or wire a GhostLink UART to an
  external ESP32-C6/C5/S3 running stock GhostESP and drive it with `commsend`.

See [`../docs/PORTING.md`](../docs/PORTING.md) for the full roadmap.
