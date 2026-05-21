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

static void copy_field(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
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
        snprintf(s_devices[i].device_name, sizeof(s_devices[i].device_name), "NODE-%02lu", (unsigned long)(i + 1));
        snprintf(s_devices[i].data1, sizeof(s_devices[i].data1), "STATE%lu", (unsigned long)((i % 5) + 1));
        snprintf(s_devices[i].data2, sizeof(s_devices[i].data2), "TEMP%02lu", (unsigned long)(20 + i));
        snprintf(s_devices[i].data3, sizeof(s_devices[i].data3), "BATT%02lu", (unsigned long)(90 - (i % 10)));
        snprintf(s_devices[i].data4, sizeof(s_devices[i].data4), "CNT%04lu", (unsigned long)(100 + i));
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
        snprintf(s_devices[i].data1, sizeof(s_devices[i].data1), "STATE%lu", (unsigned long)(((s_uptime_s + i) % 5) + 1));
        snprintf(s_devices[i].data2, sizeof(s_devices[i].data2), "TEMP%02lu", (unsigned long)(20 + ((s_uptime_s + i) % 30)));
        snprintf(s_devices[i].data3, sizeof(s_devices[i].data3), "BATT%02lu", (unsigned long)(70 + ((s_uptime_s + i) % 30)));
        snprintf(s_devices[i].data4, sizeof(s_devices[i].data4), "CNT%04lu", (unsigned long)(s_uptime_s + i));
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

bool wireless_data_upsert(const char *device_name,
                         const char *data1,
                         const char *data2,
                         const char *data3,
                         const char *data4)
{
    if (!device_name || device_name[0] == '\0') {
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
        if (strncmp(s_devices[i].device_name, device_name, sizeof(s_devices[i].device_name)) == 0) {
            copy_field(s_devices[i].data1, sizeof(s_devices[i].data1), data1);
            copy_field(s_devices[i].data2, sizeof(s_devices[i].data2), data2);
            copy_field(s_devices[i].data3, sizeof(s_devices[i].data3), data3);
            copy_field(s_devices[i].data4, sizeof(s_devices[i].data4), data4);
            s_devices[i].last_seen_s = now_s();
            unlock_data();
            return true;
        }
    }

    if (s_count >= WIRELESS_DEVICE_MAX) {
        unlock_data();
        return false;
    }

    copy_field(s_devices[s_count].device_name, sizeof(s_devices[s_count].device_name), device_name);
    copy_field(s_devices[s_count].data1, sizeof(s_devices[s_count].data1), data1);
    copy_field(s_devices[s_count].data2, sizeof(s_devices[s_count].data2), data2);
    copy_field(s_devices[s_count].data3, sizeof(s_devices[s_count].data3), data3);
    copy_field(s_devices[s_count].data4, sizeof(s_devices[s_count].data4), data4);
    s_devices[s_count].last_seen_s = now_s();
    s_count++;

    unlock_data();
    return true;
}
