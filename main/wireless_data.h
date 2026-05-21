#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WIRELESS_DEVICE_MAX 32
#define WIRELESS_FIELD_MAX  16

typedef struct {
    char device_name[WIRELESS_FIELD_MAX + 1];
    char data1[WIRELESS_FIELD_MAX + 1];
    char data2[WIRELESS_FIELD_MAX + 1];
    char data3[WIRELESS_FIELD_MAX + 1];
    char data4[WIRELESS_FIELD_MAX + 1];
    uint32_t last_seen_s;
} wireless_device_t;

void wireless_data_init(void);
void wireless_data_mock_tick(void);
bool wireless_data_get_by_index(uint32_t index, wireless_device_t *out);
uint32_t wireless_data_count(void);

void wireless_data_set_mock_enabled(bool enabled);
bool wireless_data_mock_enabled(void);
bool wireless_data_upsert(const char *device_name,
                         const char *data1,
                         const char *data2,
                         const char *data3,
                         const char *data4);
