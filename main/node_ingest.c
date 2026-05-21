#include "node_ingest.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "wireless_data.h"

#define INGEST_STACK_SIZE 4096
#define INGEST_PRIORITY   5
#define INGEST_BUF_SIZE   192

static const char *TAG = "node_ingest";

static TaskHandle_t s_ingest_task = NULL;
static int s_sock = -1;
static uint16_t s_port = 0;
static uint32_t s_packets_rx = 0;

static bool parse_packet(const char *line,
                         char *node_id,
                         size_t node_id_len,
                         int *rssi,
                         float *battery,
                         uint32_t *samples)
{
    if (!line || !node_id || !rssi || !battery || !samples) {
        return false;
    }

    char id[16] = {0};
    int parsed_rssi = 0;
    float parsed_battery = 0.0f;
    unsigned long parsed_samples_ul = 0;

    int matched = sscanf(line, "%15[^,],%d,%f,%lu", id, &parsed_rssi, &parsed_battery, &parsed_samples_ul);
    if (matched != 4) {
        return false;
    }

    strncpy(node_id, id, node_id_len - 1);
    node_id[node_id_len - 1] = '\0';
    *rssi = parsed_rssi;
    *battery = parsed_battery;
    *samples = (uint32_t)parsed_samples_ul;

    return true;
}

static void node_ingest_task(void *arg)
{
    (void)arg;

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(s_port),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };

    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_sock < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        s_ingest_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    int bind_ret = bind(s_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_ret < 0) {
        ESP_LOGE(TAG, "Failed to bind UDP socket on port %u", s_port);
        close(s_sock);
        s_sock = -1;
        s_ingest_task = NULL;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Node ingest UDP listener started on port %u", s_port);

    while (true) {
        char rx_buf[INGEST_BUF_SIZE] = {0};
        struct sockaddr_in src_addr;
        socklen_t src_len = sizeof(src_addr);

        int len = recvfrom(s_sock, rx_buf, sizeof(rx_buf) - 1, 0,
                           (struct sockaddr *)&src_addr, &src_len);
        if (len <= 0) {
            continue;
        }

        rx_buf[len] = '\0';

        char node_id[16] = {0};
        int rssi = 0;
        float battery = 0.0f;
        uint32_t samples = 0;

        if (parse_packet(rx_buf, node_id, sizeof(node_id), &rssi, &battery, &samples)) {
            wireless_data_upsert(node_id, rssi, battery, samples);
            s_packets_rx++;

            char src_ip[16] = {0};
            inet_ntoa_r(src_addr.sin_addr, src_ip, sizeof(src_ip));
            ESP_LOGI(TAG,
                     "RX #%lu from %s:%u -> id=%s rssi=%d batt=%.2f samples=%lu",
                     (unsigned long)s_packets_rx,
                     src_ip,
                     (unsigned)ntohs(src_addr.sin_port),
                     node_id,
                     rssi,
                     (double)battery,
                     (unsigned long)samples);
        } else {
            ESP_LOGW(TAG, "Invalid packet format: %s", rx_buf);
        }
    }
}

esp_err_t node_ingest_start(uint16_t port)
{
    if (s_ingest_task != NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    s_port = port;
    s_packets_rx = 0;

    BaseType_t ok = xTaskCreate(node_ingest_task,
                                "node_ingest",
                                INGEST_STACK_SIZE,
                                NULL,
                                INGEST_PRIORITY,
                                &s_ingest_task);
    if (ok != pdPASS) {
        s_ingest_task = NULL;
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

esp_err_t node_ingest_stop(void)
{
    if (s_ingest_task == NULL) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_sock >= 0) {
        shutdown(s_sock, 0);
        close(s_sock);
        s_sock = -1;
    }

    vTaskDelete(s_ingest_task);
    s_ingest_task = NULL;
    return ESP_OK;
}

bool node_ingest_is_running(void)
{
    return s_ingest_task != NULL;
}

uint16_t node_ingest_port(void)
{
    return s_port;
}

uint32_t node_ingest_packets_rx(void)
{
    return s_packets_rx;
}

