/*
 * stc8_battery.h — read the CrowPanel Advanced 9" ESP32-P4 (V1.0) battery.
 *
 * The board's battery/charge co-processor is an STC8H1K08-36I (schematic ref
 * U14). It measures VBAT on its own ADC (via an R164/R186 divider) and reads
 * charge status from the charger IC, then exposes everything as an I2C slave.
 *
 *   Bus:      I2C1, SDA = GPIO45, SCL = GPIO46  (shared with the GT911 touch)
 *   Address:  0x2F (7-bit)
 *
 * Register map (reverse-engineered on hardware; read each register with its own
 * write-reg / read-byte transaction — this firmware does NOT auto-increment on
 * multi-byte reads):
 *
 *   0x08        battery percentage, 0-100            (uint8)
 *   0x04, 0x05  battery voltage in millivolts, LE    (uint16, lo=0x04 hi=0x05)
 *   0x09        charging flag (non-zero = charging)  (uint8)
 *   0x00, 0x01  aux / raw-ADC value, LE              (uint16) [tracks charge]
 *   0x0A        second charge-status flag (CHG/DONE) [semantics unconfirmed]
 *
 * Verified live: 87 %, 4115 mV, charging — values climb together while charging.
 *
 * Notes:
 *  - LDO4 (3.3 V) powers the I2C pull-ups on this board; make sure it's on.
 *  - Reads are cheap; poll at ~1-5 s. The bus is shared with the GT911 touch,
 *    so serialize access (e.g. from one UI thread) if the touch is also polled.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "driver/i2c_master.h"

#define STC8_BATTERY_I2C_ADDR   0x2F

/* Add the STC8 as a device on an already-created i2c_master bus (the same bus
 * you use for the GT911 touch). Returns ESP_OK on success. */
esp_err_t stc8_battery_init(i2c_master_bus_handle_t bus);

/* Read battery state. Any out-pointer may be NULL. Returns true on success.
 *   percent    : 0-100
 *   millivolts : pack voltage in mV (e.g. 4115)
 *   charging   : true while charging */
bool stc8_battery_read(uint8_t *percent, uint16_t *millivolts, bool *charging);
