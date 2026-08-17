// pins.h — Elecrow CrowPanel Advanced 9" ESP32-P4 HMI (SKU DHE04209D)
//
// Pin reference for porting GhostESP to this board. See ../../docs/PORTING.md.
//
// PROVENANCE / TRUST LEVEL
// ------------------------
// Values here were extracted from Elecrow's published board materials, not from
// a verified build. Before relying on any of them, cross-check against the
// schematic in Elecrow's repo (folder "Eagle_SCH&PCB"):
//   https://github.com/Elecrow-RD/CrowPanel-Advanced-9inch-ESP32-P4-HMI-AI-Display-1024x600-IPS-Touch-Screen
//
// Confidence legend:
//   [OK?]   plausible, but confirm against schematic before trusting
//   [TODO]  NOT known yet — must be read off the schematic
//   [FIXED] fixed-function on ESP32-P4 silicon (not a remappable GPIO)
//
// NOTE on the MIPI-DSI lanes: on the ESP32-P4 the DSI D-PHY uses dedicated
// fixed-function pads, NOT general-purpose GPIOs. The "IOxx" names below come
// from schematic net labels and should be treated as identifiers, not as GPIO
// numbers you program. Configure DSI through esp_lcd_mipi_dsi (lane count +
// timings), not by setting these as GPIOs. The values also showed an internal
// conflict on extraction (a touch-reset net overlapping a DSI net), which is
// exactly why they are marked unverified.

#pragma once

// ---------------------------------------------------------------------------
// Display — 1024x600 IPS, MIPI-DSI, ILI9881C-class, 2 data lanes
// ---------------------------------------------------------------------------
#define LCD_H_RES              1024
#define LCD_V_RES              600
#define LCD_DSI_LANES          2          // [OK?] typical for this panel; confirm
// DSI PHY pads (schematic net names, NOT programmable GPIOs):        [FIXED]
//   DATA0-  IO35   DATA0+  IO36
//   DATA1-  IO39   DATA1+  IO40      <-- overlaps touch-reset net below; VERIFY
//   CLK-    IO37   CLK+    IO38
//   REXT    IO34
// -> Do not program these. Set up DSI via esp_lcd_mipi_dsi + ILI9881C init.
// -> Copy exact porch/timing values from Elecrow factory_sourcecode.  [TODO]
#define LCD_BL_PIN             (-1)       // [TODO] backlight enable/PWM GPIO

// ---------------------------------------------------------------------------
// Touch — GT911 capacitive, I2C
// ---------------------------------------------------------------------------
#define TOUCH_I2C_SCL_PIN      46         // [OK?]
#define TOUCH_I2C_SDA_PIN      45         // [OK?]
#define TOUCH_INT_PIN          42         // [OK?]
#define TOUCH_RST_PIN          40         // [OK?] conflicts w/ DSI net above; VERIFY
// GT911 7-bit address is chosen by INT level during reset:
#define TOUCH_GT911_ADDR_LOW   0x5D       // INT held low at reset
#define TOUCH_GT911_ADDR_HIGH  0x14       // INT held high at reset

// ---------------------------------------------------------------------------
// ESP32-C6 wireless co-processor link (SDIO)  --  THE MOST IMPORTANT PINS
// These drive ALL WiFi/BLE via esp_wifi_remote / esp-hosted. Not yet known.
// Read them off the schematic before anything wireless can work.
// ---------------------------------------------------------------------------
#define C6_SDIO_CLK_PIN        (-1)       // [TODO]
#define C6_SDIO_CMD_PIN        (-1)       // [TODO]
#define C6_SDIO_D0_PIN         (-1)       // [TODO]
#define C6_SDIO_D1_PIN         (-1)       // [TODO]
#define C6_SDIO_D2_PIN         (-1)       // [TODO]
#define C6_SDIO_D3_PIN         (-1)       // [TODO]
#define C6_RESET_PIN           (-1)       // [TODO] host-driven reset to the C6
#define C6_BOOT_PIN            (-1)       // [TODO] strap for flashing C6 slave fw

// ---------------------------------------------------------------------------
// Replaceable wireless-module bay (shared SPI): SX1262 / nRF24L01 / etc.
// ---------------------------------------------------------------------------
#define MOD_SPI_SCK_PIN        8          // [OK?]
#define MOD_SPI_MISO_PIN       7          // [OK?]
#define MOD_SPI_MOSI_PIN       6          // [OK?]
// SX1262 (LoRa) variant:
#define SX1262_NSS_PIN         10         // [OK?]
#define SX1262_BUSY_PIN        9          // [OK?]
#define SX1262_IRQ_PIN         53         // [OK?]
#define SX1262_NRST_PIN        54         // [OK?]
// nRF24L01 variant (shares the bus; pins reused):
#define NRF24_CE_PIN           53         // [OK?]
#define NRF24_CS_PIN           54         // [OK?]
#define NRF24_IRQ_PIN          9          // [OK?]

// ---------------------------------------------------------------------------
// Audio codec (I2S)
// ---------------------------------------------------------------------------
#define AUDIO_LRCLK_PIN        21         // [OK?]
#define AUDIO_BCLK_PIN         22         // [OK?]
#define AUDIO_SDATA_PIN        23         // [OK?]
#define AUDIO_CTRL_PIN         30         // [OK?]

// ---------------------------------------------------------------------------
// SD card (SDMMC) — [TODO] confirm bus width and pins from schematic
// ---------------------------------------------------------------------------
#define SD_CLK_PIN             (-1)       // [TODO]
#define SD_CMD_PIN             (-1)       // [TODO]
#define SD_D0_PIN              (-1)       // [TODO]
