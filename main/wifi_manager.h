#pragma once

#include "esp_err.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * WiFi manager — handles provisioning, connection, and event callbacks.
 */

typedef enum {
    WIFI_STATE_IDLE,
    WIFI_STATE_SCANNING,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_ERROR,
} wifi_state_t;

typedef struct {
    char ssid[32];
    char password[64];
    uint8_t bssid[6];
    int8_t rssi;
    wifi_auth_mode_t auth_mode;
} wifi_ap_info_t;

/**
 * Initialize WiFi manager (NVS, event loop, WiFi stack).
 * Does not connect automatically.
 */
esp_err_t wifi_manager_init(void);

/**
 * Start WiFi in station mode and connect to stored SSID.
 * Returns ESP_OK if connection initiated, or error code.
 */
esp_err_t wifi_manager_connect(const char *ssid, const char *password);

/**
 * Disconnect from current network.
 */
esp_err_t wifi_manager_disconnect(void);

/**
 * Scan for available networks synchronously.
 * Returns number of networks found, or negative error.
 */
int wifi_manager_scan(wifi_ap_info_t *out_ap_list, int max_count);

/**
 * Get current WiFi connection state.
 */
wifi_state_t wifi_manager_get_state(void);

/**
 * Get connected IP address (only valid if state == WIFI_STATE_CONNECTED).
 * Returns pointer to static buffer.
 */
const char *wifi_manager_get_ip(void);

/**
 * Save WiFi credentials to NVS for automatic reconnection.
 */
esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password);

/**
 * Load WiFi credentials from NVS.
 * Returns ESP_OK if found, ESP_ERR_NVS_NOT_FOUND if no saved credentials.
 */
esp_err_t wifi_manager_load_credentials(char *out_ssid, int ssid_len,
                                         char *out_password, int password_len);

#ifdef __cplusplus
}
#endif
