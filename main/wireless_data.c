#include "wireless_data.h"

#include <stdio.h>
#include <string.h>

static wireless_device_t s_devices[WIRELESS_DEVICE_MAX];
static uint32_t s_count = 0;
static uint32_t s_uptime_s = 0;

void wireless_data_init(void)
{
    s_count = 12;
    for (uint32_t i = 0; i < s_count; i++) {
        snprintf(s_devices[i].node_id, sizeof(s_devices[i].node_id), "ESP32-%02lu", (unsigned long)(i + 1));
        s_devices[i].rssi_dbm = -30 - (int)(i * 3);
        s_devices[i].battery_v = 4.20f - (0.05f * (float)i);
        s_devices[i].samples = 100 + (i * 17);
        s_devices[i].last_seen_s = 0;
    }
}

void wireless_data_mock_tick(void)
{
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
}

bool wireless_data_get_by_index(uint32_t index, wireless_device_t *out)
{
    if (index >= s_count || out == NULL) {
        return false;
    }

    *out = s_devices[index];
    return true;
}

uint32_t wireless_data_count(void)
{
    return s_count;
}
