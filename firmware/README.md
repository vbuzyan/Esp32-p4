# CrowPanel 9" ESP32-P4 — board bring-up firmware (Phase 1)

**This is NOT GhostESP.** It is a minimal ESP-IDF bring-up that proves the
front-end stack GhostESP needs on the P4:

1. the **1024×600 MIPI-DSI** panel (**EK79007**) lights up,
2. the **GT911** capacitive touch works,
3. **LVGL v8** renders a live touch UI (title, a tappable button with a counter,
   and a live touch-coordinate readout).

> ✅ **Built, flashed, and verified running** on real hardware: ESP32-P4 rev
> v1.3, CrowPanel Advanced 9" V1.0, with **ESP-IDF v6.0.2**. The boot log
> confirms LDOs, the EK79007 panel at 1024×600, GT911 touch, and LVGL
> ("UI up"), with no watchdog. All hardware values are from Elecrow's V1.0
> factory source.

## LVGL note (important for the GhostESP port too)

This uses a **direct LVGL v8 integration, not `esp_lvgl_port`.** On IDF 6.0.2,
`esp_lvgl_port` (both 2.9.0 and 2.8.0) deadlocks on this DSI panel: it registers
the display and starts flushing *before* registering the DPI DMA-completion
callback, so the first flush never signals `lv_disp_flush_ready` and the LVGL
task spins forever (IDLE0 watchdog). The fix here is to register
`on_color_trans_done` (via `esp_lcd_dpi_panel_register_event_callbacks`, with
`esp_lcd_dpi_panel_enable_dma2d()` on) **before** starting the LVGL task, so
every flush completes. GhostESP has its own LVGL flush path, so wire the EK79007
panel into that the same way.

See [`../docs/PORTING.md`](../docs/PORTING.md) for how this feeds the GhostESP port.

## Verified V1.0 hardware values (baked into `main/crowpanel_p4_bringup.c`)

| Value                    | Setting                                   |
| ------------------------ | ----------------------------------------- |
| Panel controller         | **EK79007** (`esp_lcd_ek79007` 1.0.4)     |
| Resolution / depth       | 1024×600, RGB565                          |
| DSI lanes / bit rate     | 2 lanes @ 900 Mbps                        |
| DPI pixel clock          | 51 MHz                                     |
| H timings (bp/pw/fp)     | 160 / 70 / 160                            |
| V timings (bp/pw/fp)     | 23 / 10 / 12                             |
| LDO (DSI PHY / panel)    | ch3 = 2500 mV / ch4 = 3300 mV            |
| Backlight                | GPIO 31                                    |
| Touch GT911 (I²C0)       | SCL 46 / SDA 45 / INT 42 / RST 40         |
| GhostLink UART (Phase 2) | TX/RX unset — pick free GPIOs             |

## ESP-IDF v6.0 gotchas we hit (already fixed here)

Elecrow's factory code targets IDF 5.4.2; three things changed in v6.0:

1. **Chip revision.** This is early P4 silicon (rev v1.x); IDF 6.0 defaults to
   rev v3.1 and treats <3.0 vs ≥3.0 as mutually exclusive. `sdkconfig.defaults`
   sets `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` + `CONFIG_ESP32P4_REV_MIN_100=y`.
2. **`esp_lcd_dpi_panel_config_t`**: `.pixel_format` / `.virtual_channel` were
   replaced by `.in_color_format` + `.out_color_format` (`LCD_COLOR_FMT_*`).
3. **`.flags.use_dma2d` removed** — call `esp_lcd_dpi_panel_enable_dma2d()` if you
   want DMA2D acceleration (not needed for this smoke test).

## Build & flash (ESP-IDF v6.0.2)

ESP-IDF trips on spaces in the project path, so build from a space-free dir such
as `C:\esp\crowpanel_p4_bringup` (copy this `firmware/` folder there).

```powershell
# Activate the env (adjust to your install path):
. "C:\Espressif\tools\Microsoft.v6.0.2.PowerShell_profile.ps1"

idf.py set-target esp32p4
idf.py build
idf.py -p COM9 flash monitor      # COM9 = this board's CH340 USB-serial port
```

You should see 8 horizontal color bars on the panel; the monitor prints a line
each time you touch the screen. `Ctrl+]` exits the monitor.

## Files

- `main/crowpanel_p4_bringup.c` — the bring-up (LDOs → DSI/EK79007 → color bars →
  GT911 touch → GhostLink UART skeleton).
- `main/idf_component.yml` — pulls `esp_lcd_ek79007` + `esp_lcd_touch_gt911`.
- `sdkconfig.defaults` — P4 target, 16 MB flash, PSRAM, rev-<3 selection.
- `partitions.csv` — single 3 MB app (bring-up only).
