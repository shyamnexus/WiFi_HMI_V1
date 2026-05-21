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

static void trim_in_place(char *s)
{
    if (!s) {
        return;
    }

    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') {
        start++;
    }

    if (start != s) {
        memmove(s, start, strlen(start) + 1);
    }

    size_t len = strlen(s);
    while (len > 0) {
        char c = s[len - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            s[len - 1] = '\0';
            len--;
        } else {
            break;
        }
    }
}

static bool parse_packet(const char *line,
                         char *device_name,
                         size_t device_name_len,
                         char *data1,
                         size_t data1_len,
                         char *data2,
                         size_t data2_len,
                         char *data3,
                         size_t data3_len,
                         char *data4,
                         size_t data4_len)
{
    if (!line || !device_name || !data1 || !data2 || !data3 || !data4) {
        return false;
    }

    char parsed_device_name[WIRELESS_FIELD_MAX + 1] = {0};
    char parsed_data1[WIRELESS_FIELD_MAX + 1] = {0};
    char parsed_data2[WIRELESS_FIELD_MAX + 1] = {0};
    char parsed_data3[WIRELESS_FIELD_MAX + 1] = {0};
    char parsed_data4[WIRELESS_FIELD_MAX + 1] = {0};

    int matched = sscanf(line,
                         " %16[^,],%16[^,],%16[^,],%16[^,],%16[^\r\n]",
                         parsed_device_name,
                         parsed_data1,
                         parsed_data2,
                         parsed_data3,
                         parsed_data4);
    if (matched != 5) {
        return false;
    }

    trim_in_place(parsed_device_name);
    trim_in_place(parsed_data1);
    trim_in_place(parsed_data2);
    trim_in_place(parsed_data3);
    trim_in_place(parsed_data4);

    if (parsed_device_name[0] == '\0' ||
        parsed_data1[0] == '\0' ||
        parsed_data2[0] == '\0' ||
        parsed_data3[0] == '\0' ||
        parsed_data4[0] == '\0') {
        return false;
    }

    strncpy(device_name, parsed_device_name, device_name_len - 1);
    device_name[device_name_len - 1] = '\0';
    strncpy(data1, parsed_data1, data1_len - 1);
    data1[data1_len - 1] = '\0';
    strncpy(data2, parsed_data2, data2_len - 1);
    data2[data2_len - 1] = '\0';
    strncpy(data3, parsed_data3, data3_len - 1);
    data3[data3_len - 1] = '\0';
    strncpy(data4, parsed_data4, data4_len - 1);
    data4[data4_len - 1] = '\0';

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

        char device_name[WIRELESS_FIELD_MAX + 1] = {0};
        char data1[WIRELESS_FIELD_MAX + 1] = {0};
        char data2[WIRELESS_FIELD_MAX + 1] = {0};
        char data3[WIRELESS_FIELD_MAX + 1] = {0};
        char data4[WIRELESS_FIELD_MAX + 1] = {0};

        if (parse_packet(rx_buf,
                         device_name,
                         sizeof(device_name),
                         data1,
                         sizeof(data1),
                         data2,
                         sizeof(data2),
                         data3,
                         sizeof(data3),
                         data4,
                         sizeof(data4))) {
            wireless_data_upsert(device_name, data1, data2, data3, data4);
            s_packets_rx++;

            char src_ip[16] = {0};
            inet_ntoa_r(src_addr.sin_addr, src_ip, sizeof(src_ip));
            ESP_LOGI(TAG,
                     "RX #%lu from %s:%u -> %s,%s,%s,%s,%s",
                     (unsigned long)s_packets_rx,
                     src_ip,
                     (unsigned)ntohs(src_addr.sin_port),
                     device_name,
                     data1,
                     data2,
                     data3,
                     data4);
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

