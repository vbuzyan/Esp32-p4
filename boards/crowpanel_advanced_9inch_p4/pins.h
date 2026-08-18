// pins.h — Elecrow CrowPanel Advanced 9" ESP32-P4 HMI (SKU DHE04209D, rev V1.0)
//
// Pin/parameter reference for the GhostESP port. Display + touch values are
// VERIFIED against Elecrow's V1.0 factory source and confirmed running on real
// hardware (see ../../firmware/). GhostLink and onboard-C6 pins are still TODO.

#pragma once

// ---------------------------------------------------------------------------
// Display — 1024x600 IPS, MIPI-DSI, controller = EK79007  [VERIFIED]
// Driver: esp_lcd_ek79007 / esp_lcd_new_panel_ek79007
// ---------------------------------------------------------------------------
#define LCD_H_RES                 1024
#define LCD_V_RES                 600
#define LCD_BITS_PER_PIXEL        16          // RGB565
#define MIPI_DSI_LANE_NUM         2
#define MIPI_DSI_LANE_BITRATE_MBPS 900
#define DPI_CLOCK_FREQ_MHZ        51
// DPI video timing:
//   hsync: back 160, pulse 70, front 160
//   vsync: back 23,  pulse 10, front 12
#define PIN_LCD_BLIGHT            31          // backlight (Elecrow drives LEDC PWM 30 kHz)
// The MIPI-DSI PHY needs two on-chip LDOs powered BEFORE panel init:
#define LDO_MIPI_PHY_CHAN         3           // 2500 mV
#define LDO_MIPI_PHY_MV           2500
#define LDO_PANEL_CHAN            4           // 3300 mV
#define LDO_PANEL_MV              3300

// ---------------------------------------------------------------------------
// Touch — GT911 capacitive, I2C  [VERIFIED]
// ---------------------------------------------------------------------------
#define TOUCH_I2C_PORT            0
#define TOUCH_I2C_SCL_PIN         46
#define TOUCH_I2C_SDA_PIN         45
#define TOUCH_INT_PIN             42
#define TOUCH_RST_PIN             40
#define TOUCH_GT911_ADDR_LOW      0x5D        // INT low at reset
#define TOUCH_GT911_ADDR_HIGH     0x14        // INT high at reset

// ---------------------------------------------------------------------------
// GhostLink UART to the added radio module (Phase 2)  [TODO — pick free GPIOs]
// 3 wires: P4_TX -> module_RX, P4_RX -> module_TX, GND. 115200 baud.
// Avoid GPIO 6/7 (module-bay SPI) and anything used by touch/audio.
// ---------------------------------------------------------------------------
#define GHOSTLINK_UART_TX_PIN     (-1)        // TODO
#define GHOSTLINK_UART_RX_PIN     (-1)        // TODO

// ---------------------------------------------------------------------------
// Expansion wireless-module bay (shared SPI): SX1262 / nRF24  [from wiki]
// ---------------------------------------------------------------------------
#define MOD_SPI_SCK_PIN           8
#define MOD_SPI_MISO_PIN          7
#define MOD_SPI_MOSI_PIN          6
#define SX1262_NSS_PIN            10
#define SX1262_BUSY_PIN           9
#define SX1262_IRQ_PIN            53
#define SX1262_NRST_PIN           54

// ---------------------------------------------------------------------------
// Onboard ESP32-C6 SDIO link (Phase 3 only, for the P4's own WiFi via
// esp_wifi_remote). NOT needed for the GhostLink radio path.  [TODO — schematic]
// ---------------------------------------------------------------------------
#define C6_SDIO_CLK_PIN           (-1)        // TODO
#define C6_SDIO_CMD_PIN           (-1)        // TODO
#define C6_SDIO_D0_PIN            (-1)        // TODO
#define C6_SDIO_D1_PIN            (-1)        // TODO
#define C6_SDIO_D2_PIN            (-1)        // TODO
#define C6_SDIO_D3_PIN            (-1)        // TODO
