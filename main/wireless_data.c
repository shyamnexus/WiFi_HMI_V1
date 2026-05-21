#include "wireless_data.h"

#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static wireless_device_t s_devices[WIRELESS_DEVICE_MAX];
static uint32_t s_count = 0;
static uint32_t s_uptime_s = 0;
static bool s_mock_enabled = true;
static bool s_live_started = false;
static SemaphoreHandle_t s_lock = NULL;

static uint32_t now_s(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

static void lock_data(void)
{
    if (s_lock) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
    }
}

static void unlock_data(void)
{
    if (s_lock) {
        xSemaphoreGive(s_lock);
    }
}

void wireless_data_init(void)
{
    if (s_lock == NULL) {
        s_lock = xSemaphoreCreateMutex();
    }

    lock_data();
    s_mock_enabled = true;
    s_live_started = false;
    s_count = 12;
    for (uint32_t i = 0; i < s_count; i++) {
        snprintf(s_devices[i].node_id, sizeof(s_devices[i].node_id), "ESP32-%02lu", (unsigned long)(i + 1));
        s_devices[i].rssi_dbm = -30 - (int)(i * 3);
        s_devices[i].battery_v = 4.20f - (0.05f * (float)i);
        s_devices[i].samples = 100 + (i * 17);
        s_devices[i].last_seen_s = 0;
    }
    unlock_data();
}

void wireless_data_mock_tick(void)
{
    lock_data();
    if (!s_mock_enabled) {
        unlock_data();
        return;
    }

    s_uptime_s++;

    for (uint32_t i = 0; i < s_count; i++) {
        int drift = (int)((s_uptime_s + i) % 5) - 2;
        int next_rssi = s_devices[i].rssi_dbm + drift;
        if (next_rssi < -95) {
            next_rssi = -95;
        }
        if (next_rssi > -25) {
            next_rssi = -25;
        }

        s_devices[i].rssi_dbm = next_rssi;
        s_devices[i].battery_v -= 0.0005f;
        if (s_devices[i].battery_v < 3.20f) {
            s_devices[i].battery_v = 4.20f;
        }

        s_devices[i].samples += 1 + (i % 3);
        s_devices[i].last_seen_s = s_uptime_s;
    }
    unlock_data();
}

bool wireless_data_get_by_index(uint32_t index, wireless_device_t *out)
{
    if (out == NULL) {
        return false;
    }

    lock_data();
    if (index >= s_count) {
        unlock_data();
        return false;
    }

    *out = s_devices[index];
    unlock_data();
    return true;
}

uint32_t wireless_data_count(void)
{
    lock_data();
    uint32_t count = s_count;
    unlock_data();
    return count;
}

void wireless_data_set_mock_enabled(bool enabled)
{
    lock_data();
    s_mock_enabled = enabled;
    unlock_data();
}

bool wireless_data_mock_enabled(void)
{
    lock_data();
    bool enabled = s_mock_enabled;
    unlock_data();
    return enabled;
}

bool wireless_data_upsert(const char *node_id, int rssi_dbm, float battery_v, uint32_t samples)
{
    if (!node_id || node_id[0] == '\0') {
        return false;
    }

    lock_data();

    if (!s_live_started) {
        memset(s_devices, 0, sizeof(s_devices));
        s_count = 0;
        s_live_started = true;
        s_mock_enabled = false;
    }

    for (uint32_t i = 0; i < s_count; i++) {
        if (strncmp(s_devices[i].node_id, node_id, sizeof(s_devices[i].node_id)) == 0) {
            s_devices[i].rssi_dbm = rssi_dbm;
            s_devices[i].battery_v = battery_v;
            s_devices[i].samples = samples;
            s_devices[i].last_seen_s = now_s();
            unlock_data();
            return true;
        }
    }

    if (s_count >= WIRELESS_DEVICE_MAX) {
        unlock_data();
        return false;
    }

    strncpy(s_devices[s_count].node_id, node_id, sizeof(s_devices[s_count].node_id) - 1);
    s_devices[s_count].node_id[sizeof(s_devices[s_count].node_id) - 1] = '\0';
    s_devices[s_count].rssi_dbm = rssi_dbm;
    s_devices[s_count].battery_v = battery_v;
    s_devices[s_count].samples = samples;
    s_devices[s_count].last_seen_s = now_s();
    s_count++;

    unlock_data();
    return true;
}
