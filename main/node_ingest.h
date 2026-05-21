#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t node_ingest_start(uint16_t port);
esp_err_t node_ingest_stop(void);
bool node_ingest_is_running(void);
uint16_t node_ingest_port(void);
uint32_t node_ingest_packets_rx(void);

#ifdef __cplusplus
}
#endif
