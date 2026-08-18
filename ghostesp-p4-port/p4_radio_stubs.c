/*
 * p4_radio_stubs.c — ESP32-P4-only link stubs.
 *
 * The ESP32-P4 has no local radio; WiFi/BLE come from the onboard ESP32-C6 over
 * esp-hosted (via esp_wifi_remote). A handful of low-level radio symbols that
 * GhostESP uses are NOT provided by esp_wifi_remote because they need direct
 * radio/supplicant access:
 *
 *   - ESP-NOW (esp_now_*)                — not forwarded over hosted transport
 *   - WPA supplicant internals           — gWpaSm, esp_wifi_get_sta_key_internal
 *
 * In the GhostLink architecture these features run on the radio module, not on
 * the P4, so on the P4 build they are stubbed to fail cleanly at runtime. This
 * lets GhostESP link and boot on the P4 as the display/relay front-end.
 *
 * The whole file is compiled to nothing on other targets (which have the real
 * implementations), so it is safe to leave in the globbed source set.
 */

#include "sdkconfig.h"   /* provides CONFIG_IDF_TARGET_ESP32P4 */

#if defined(CONFIG_IDF_TARGET_ESP32P4)

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* --- ESP-NOW: unsupported over esp-hosted --------------------------------- */
esp_err_t esp_now_init(void)                              { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_deinit(void)                            { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_send(const uint8_t *peer_addr,
                       const uint8_t *data, size_t len)   { (void)peer_addr; (void)data; (void)len; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_add_peer(const void *peer)              { (void)peer; return ESP_ERR_NOT_SUPPORTED; }
bool      esp_now_is_peer_exist(const uint8_t *peer_addr) { (void)peer_addr; return false; }
esp_err_t esp_now_register_recv_cb(void *cb)              { (void)cb; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t esp_now_unregister_recv_cb(void)               { return ESP_ERR_NOT_SUPPORTED; }

/* --- WPA supplicant internals (used only by gtk_abuse.c) ------------------ *
 * gtk_abuse casts &gWpaSm to a local prefix struct; a zeroed buffer makes GTK
 * extraction fail gracefully (no local supplicant on the P4). The extern in
 * gtk_abuse.c is `struct wpa_sm gWpaSm;` — C links this by name regardless of
 * the placeholder type here. */
char gWpaSm[8192];

int esp_wifi_get_sta_key_internal(void *ifx, int *alg, void *addr, int *key_idx,
                                  void *key, size_t key_len, int key_flag)
{
    (void)ifx; (void)alg; (void)addr; (void)key_idx; (void)key; (void)key_len; (void)key_flag;
    return -1;
}

#endif /* CONFIG_IDF_TARGET_ESP32P4 */
