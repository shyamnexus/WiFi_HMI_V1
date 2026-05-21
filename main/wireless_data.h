#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WIRELESS_DEVICE_MAX 32

typedef struct {
    char node_id[16];
    int rssi_dbm;
    float battery_v;
    uint32_t samples;
    uint32_t last_seen_s;
} wireless_device_t;

void wireless_data_init(void);
void wireless_data_mock_tick(void);
bool wireless_data_get_by_index(uint32_t index, wireless_device_t *out);
uint32_t wireless_data_count(void);

void wireless_data_set_mock_enabled(bool enabled);
bool wireless_data_mock_enabled(void);
bool wireless_data_upsert(const char *node_id, int rssi_dbm, float battery_v, uint32_t samples);
