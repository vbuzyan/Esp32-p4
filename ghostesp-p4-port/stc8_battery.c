/*
 * stc8_battery.c — CrowPanel 9" ESP32-P4 (V1.0) battery driver.
 * See stc8_battery.h for the wiring and register map. Standalone/reference
 * implementation; GhostESP integrates the same logic in display_manager.c.
 */
#include "stc8_battery.h"

static i2c_master_dev_handle_t s_dev = NULL;

esp_err_t stc8_battery_init(i2c_master_bus_handle_t bus)
{
    if (!bus) return ESP_ERR_INVALID_ARG;
    if (s_dev) return ESP_OK;
    i2c_device_config_t dc = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = STC8_BATTERY_I2C_ADDR,
        .scl_speed_hz    = 100000,
    };
    return i2c_master_bus_add_device(bus, &dc, &s_dev);
}

/* One write-reg / read-byte transaction (no auto-increment on this firmware). */
static bool rd(uint8_t reg, uint8_t *out)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, out, 1, 100) == ESP_OK;
}

bool stc8_battery_read(uint8_t *percent, uint16_t *millivolts, bool *charging)
{
    if (!s_dev) return false;

    uint8_t v;
    if (!rd(0x08, &v)) return false;          /* percentage anchors the read */
    if (percent) *percent = (v > 100) ? 100 : v;

    if (millivolts) {
        uint8_t lo, hi;
        if (rd(0x04, &lo) && rd(0x05, &hi))
            *millivolts = (uint16_t)lo | ((uint16_t)hi << 8);
    }
    if (charging) {
        uint8_t c = 0;
        rd(0x09, &c);
        *charging = (c != 0);
    }
    return true;
}
