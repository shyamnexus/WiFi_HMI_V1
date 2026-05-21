#include "wifi_manager.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "wifi_mgr";

/* WiFi state tracking */
static wifi_state_t s_wifi_state = WIFI_STATE_IDLE;
static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_netif_sta;
static char s_ip_addr[16] = {0};

/* Event group bits */
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1

/* NVS namespace */
#define NVS_NAMESPACE "wifi_creds"

/**
 * WiFi event handler — connected, disconnected, scan done, etc.
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi STA started");
                s_wifi_state = WIFI_STATE_IDLE;
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi STA connected to AP");
                break;

            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGW(TAG, "WiFi STA disconnected");
                s_wifi_state = WIFI_STATE_DISCONNECTED;
                xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
                /* Optionally retry: esp_wifi_connect(); */
                break;

            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WiFi scan completed");
                break;

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_ip_addr, sizeof(s_ip_addr));
        ESP_LOGI(TAG, "WiFi got IP: %s", s_ip_addr);
        s_wifi_state = WIFI_STATE_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_init(void)
{
    if (s_wifi_event_group != NULL) {
        ESP_LOGW(TAG, "WiFi manager already initialized");
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* Initialize NVS for WiFi credentials storage */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* Initialize network stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* Create WiFi station interface */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Create netif for STA */
    s_netif_sta = esp_netif_create_default_wifi_sta();

    /* Register event handlers */
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL));

    /* Set WiFi mode to station */
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* Start WiFi stack (but don't connect yet) */
    ESP_ERROR_CHECK(esp_wifi_start());

    s_wifi_state = WIFI_STATE_IDLE;
    ESP_LOGI(TAG, "WiFi manager initialized");
    return ESP_OK;
}

esp_err_t wifi_manager_connect(const char *ssid, const char *password)
{
    if (!ssid || !password || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {
        .sta = {
            .scan_method = WIFI_FAST_SCAN,
            .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
            .threshold.rssi = -127,
            .threshold.authmode = WIFI_AUTH_OPEN,
        },
    };

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid);
    esp_err_t ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        s_wifi_state = WIFI_STATE_ERROR;
        return ret;
    }

    /* Reset connection/event bits before a new connect attempt. */
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT);
    s_ip_addr[0] = '\0';

    /* If already connected/connecting, request a reconnect with new config. */
    esp_wifi_disconnect();

    s_wifi_state = WIFI_STATE_CONNECTING;
    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        s_wifi_state = WIFI_STATE_ERROR;
    }
    return ret;
}

esp_err_t wifi_manager_disconnect(void)
{
    ESP_LOGI(TAG, "WiFi disconnect requested");
    s_wifi_state = WIFI_STATE_IDLE;
    return esp_wifi_disconnect();
}

int wifi_manager_scan(wifi_ap_info_t *out_ap_list, int max_count)
{
    if (!out_ap_list || max_count <= 0) {
        return -1;
    }

    uint16_t ap_count = 0;
    wifi_ap_record_t *ap_records = malloc(max_count * sizeof(wifi_ap_record_t));
    if (!ap_records) {
        ESP_LOGE(TAG, "Failed to allocate AP records");
        return -1;
    }

    ESP_LOGI(TAG, "Starting WiFi scan...");
    s_wifi_state = WIFI_STATE_SCANNING;

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active = {
            .min = 100,
            .max = 200,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    /* Convert to output format */
    int count = (ap_count > max_count) ? max_count : ap_count;
    for (int i = 0; i < count; i++) {
        strncpy(out_ap_list[i].ssid, (char *)ap_records[i].ssid, sizeof(out_ap_list[i].ssid) - 1);
        memcpy(out_ap_list[i].bssid, ap_records[i].bssid, 6);
        out_ap_list[i].rssi = ap_records[i].rssi;
        out_ap_list[i].auth_mode = ap_records[i].authmode;
    }

    free(ap_records);
    s_wifi_state = WIFI_STATE_IDLE;
    ESP_LOGI(TAG, "Scan complete: %d networks found", count);
    return count;
}

wifi_state_t wifi_manager_get_state(void)
{
    return s_wifi_state;
}

const char *wifi_manager_get_ip(void)
{
    return s_ip_addr;
}

esp_err_t wifi_manager_save_credentials(const char *ssid, const char *password)
{
    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "ssid", ssid);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "password", password);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials saved to NVS");
    return ret;
}

esp_err_t wifi_manager_load_credentials(char *out_ssid, int ssid_len,
                                         char *out_password, int password_len)
{
    if (!out_ssid || !out_password || ssid_len <= 0 || password_len <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "No saved WiFi credentials in NVS");
        return ESP_ERR_NVS_NOT_FOUND;
    }

    ret = nvs_get_str(nvs_handle, "ssid", out_ssid, (size_t *)&ssid_len);
    if (ret != ESP_OK) {
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_get_str(nvs_handle, "password", out_password, (size_t *)&password_len);
    nvs_close(nvs_handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi credentials loaded from NVS: %s", out_ssid);
    }
    return ret;
}
