#include "cli_parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "wifi_manager.h"
#include "node_manager.h"
#include "node_ingest.h"

static const char *TAG = "cli";
static bool s_logger_enabled = false;

static int cmd_ping(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("pong\n");
    return 0;
}

static int cmd_echo(int argc, char **argv)
{
    struct {
        struct arg_str *text;
        struct arg_end *end;
    } args;

    args.text = arg_str1(NULL, NULL, "<text>", "text to echo");
    args.end = arg_end(2);

    void *argtable[] = { args.text, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    printf("%s\n", args.text->sval[0]);
    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 0;
}

static int cmd_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    const int64_t uptime_us = esp_timer_get_time();
    printf("uptime_ms=%lld\n", uptime_us / 1000);
    printf("logger_enabled=%s\n", s_logger_enabled ? "true" : "false");
    return 0;
}

static int cmd_logger(int argc, char **argv)
{
    struct {
        struct arg_str *action;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, NULL, "<start|stop|status>", "logger control action");
    args.end = arg_end(2);

    void *argtable[] = { args.action, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    const char *action = args.action->sval[0];
    if (strcmp(action, "start") == 0) {
        s_logger_enabled = true;
        printf("logger=started\n");
    } else if (strcmp(action, "stop") == 0) {
        s_logger_enabled = false;
        printf("logger=stopped\n");
    } else if (strcmp(action, "status") == 0) {
        printf("logger=%s\n", s_logger_enabled ? "running" : "stopped");
    } else {
        printf("invalid action '%s' (use: start|stop|status)\n", action);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return 0;
}

/* ============================================================
 * WiFi Commands
 * ============================================================ */

static int cmd_wifi_connect(int argc, char **argv)
{
    struct {
        struct arg_str *ssid;
        struct arg_str *password;
        struct arg_end *end;
    } args;

    args.ssid = arg_str1(NULL, NULL, "<ssid>", "WiFi network SSID");
    args.password = arg_str1(NULL, NULL, "<password>", "WiFi password");
    args.end = arg_end(3);

    void *argtable[] = { args.ssid, args.password, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    const char *ssid = args.ssid->sval[0];
    const char *password = args.password->sval[0];

    esp_err_t ret = wifi_manager_connect(ssid, password);
    if (ret == ESP_OK) {
        wifi_manager_save_credentials(ssid, password);
        printf("wifi=connecting to %s\n", ssid);
    } else {
        printf("wifi=error connecting: %s\n", esp_err_to_name(ret));
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_wifi_status(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    wifi_state_t state = wifi_manager_get_state();
    const char *state_str;

    switch (state) {
        case WIFI_STATE_IDLE:           state_str = "idle"; break;
        case WIFI_STATE_SCANNING:       state_str = "scanning"; break;
        case WIFI_STATE_CONNECTING:     state_str = "connecting"; break;
        case WIFI_STATE_CONNECTED:      state_str = "connected"; break;
        case WIFI_STATE_DISCONNECTED:   state_str = "disconnected"; break;
        case WIFI_STATE_ERROR:          state_str = "error"; break;
        default:                        state_str = "unknown"; break;
    }

    printf("wifi_state=%s\n", state_str);
    if (state == WIFI_STATE_CONNECTED) {
        printf("wifi_ip=%s\n", wifi_manager_get_ip());
    }

    return 0;
}

static int cmd_wifi_scan(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    wifi_ap_info_t ap_list[20];
    int count = wifi_manager_scan(ap_list, 20);

    if (count < 0) {
        printf("wifi_scan=error\n");
        return 1;
    }

    printf("wifi_scan_found=%d networks\n", count);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s (RSSI=%d dBm)\n", i, ap_list[i].ssid, ap_list[i].rssi);
    }

    return 0;
}

static int cmd_wifi_disconnect(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_err_t ret = wifi_manager_disconnect();
    if (ret == ESP_OK) {
        printf("wifi=disconnected\n");
    } else {
        printf("wifi_disconnect_error=%s\n", esp_err_to_name(ret));
    }

    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_wifi_reconnect(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    char ssid[32] = {0};
    char password[64] = {0};

    esp_err_t ret = wifi_manager_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if (ret != ESP_OK) {
        printf("wifi_reconnect_error=no_saved_credentials\n");
        return 1;
    }

    ret = wifi_manager_connect(ssid, password);
    if (ret == ESP_OK) {
        printf("wifi=reconnecting to %s\n", ssid);
    } else {
        printf("wifi_reconnect_error=%s\n", esp_err_to_name(ret));
    }

    return (ret == ESP_OK) ? 0 : 1;
}

/* ============================================================
 * Node Commands
 * ============================================================ */

static int cmd_node_add(int argc, char **argv)
{
    struct {
        struct arg_str *node_id;
        struct arg_str *ip_addr;
        struct arg_int *port;
        struct arg_end *end;
    } args;

    args.node_id = arg_str1(NULL, NULL, "<id>", "Node ID");
    args.ip_addr = arg_str1(NULL, NULL, "<ip>", "Node IP address");
    args.port = arg_int0(NULL, NULL, "<port>", "Node port (default 8080)");
    args.end = arg_end(4);

    void *argtable[] = { args.node_id, args.ip_addr, args.port, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    const char *node_id = args.node_id->sval[0];
    const char *ip_addr = args.ip_addr->sval[0];
    uint16_t port = (args.port->count > 0) ? args.port->ival[0] : 8080;

    esp_err_t ret = node_manager_add(node_id, ip_addr, port);
    if (ret == ESP_OK) {
        printf("node_added=%s@%s:%u\n", node_id, ip_addr, port);
    } else {
        printf("node_add_error=%s\n", esp_err_to_name(ret));
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_node_list(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    node_info_t nodes[MAX_NODES];
    int count = node_manager_get_all(nodes, MAX_NODES);

    printf("nodes_count=%d\n", count);
    for (int i = 0; i < count; i++) {
        printf("  [%d] %s @ %s:%u (active=%s)\n",
               i, nodes[i].node_id, nodes[i].ip_addr, nodes[i].port,
               nodes[i].active ? "true" : "false");
    }

    return 0;
}

static int cmd_node_remove(int argc, char **argv)
{
    struct {
        struct arg_str *node_id;
        struct arg_end *end;
    } args;

    args.node_id = arg_str1(NULL, NULL, "<id>", "Node ID to remove");
    args.end = arg_end(2);

    void *argtable[] = { args.node_id, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    const char *node_id = args.node_id->sval[0];
    esp_err_t ret = node_manager_remove(node_id);

    if (ret == ESP_OK) {
        printf("node_removed=%s\n", node_id);
    } else {
        printf("node_remove_error=%s\n", esp_err_to_name(ret));
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_node_clear(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    esp_err_t ret = node_manager_clear();
    if (ret == ESP_OK) {
        printf("nodes_cleared\n");
    } else {
        printf("node_clear_error=%s\n", esp_err_to_name(ret));
    }

    return (ret == ESP_OK) ? 0 : 1;
}

static int cmd_ingest(int argc, char **argv)
{
    struct {
        struct arg_str *action;
        struct arg_int *port;
        struct arg_end *end;
    } args;

    args.action = arg_str1(NULL, NULL, "<start|stop|status>", "ingest control action");
    args.port = arg_int0(NULL, NULL, "<port>", "UDP port (default 7001)");
    args.end = arg_end(3);

    void *argtable[] = { args.action, args.port, args.end };
    int nerrors = arg_parse(argc, argv, argtable);
    if (nerrors != 0) {
        arg_print_errors(stderr, args.end, argv[0]);
        arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
        return 1;
    }

    const char *action = args.action->sval[0];
    esp_err_t ret = ESP_OK;

    if (strcmp(action, "start") == 0) {
        uint16_t port = (args.port->count > 0) ? (uint16_t)args.port->ival[0] : 7001;
        ret = node_ingest_start(port);
        if (ret == ESP_OK) {
            printf("ingest=started port=%u\n", port);
        } else {
            printf("ingest_start_error=%s\n", esp_err_to_name(ret));
        }
    } else if (strcmp(action, "stop") == 0) {
        ret = node_ingest_stop();
        if (ret == ESP_OK) {
            printf("ingest=stopped\n");
        } else {
            printf("ingest_stop_error=%s\n", esp_err_to_name(ret));
        }
    } else if (strcmp(action, "status") == 0) {
        printf("ingest_running=%s\n", node_ingest_is_running() ? "true" : "false");
        printf("ingest_port=%u\n", node_ingest_port());
        printf("ingest_packets_rx=%lu\n", (unsigned long)node_ingest_packets_rx());
    } else {
        printf("invalid action '%s' (use: start|stop|status)\n", action);
        ret = ESP_ERR_INVALID_ARG;
    }

    arg_freetable(argtable, sizeof(argtable) / sizeof(argtable[0]));
    return (ret == ESP_OK) ? 0 : 1;
}

esp_err_t cli_parser_register_commands(void)
{
    const esp_console_cmd_t ping_cmd = {
        .command = "ping",
        .help = "Connectivity check",
        .hint = NULL,
        .func = &cmd_ping,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ping_cmd));

    const esp_console_cmd_t echo_cmd = {
        .command = "echo",
        .help = "Echo text. Example: echo hello",
        .hint = NULL,
        .func = &cmd_echo,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&echo_cmd));

    const esp_console_cmd_t status_cmd = {
        .command = "status",
        .help = "Show runtime status",
        .hint = NULL,
        .func = &cmd_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&status_cmd));

    const esp_console_cmd_t logger_cmd = {
        .command = "logger",
        .help = "Logger control: logger <start|stop|status>",
        .hint = NULL,
        .func = &cmd_logger,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&logger_cmd));

    /* WiFi commands */
    const esp_console_cmd_t wifi_connect_cmd = {
        .command = "wifi",
        .help = "WiFi control: wifi connect <ssid> <password> | wifi_status | wifi_scan",
        .hint = NULL,
        .func = &cmd_wifi_connect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_connect_cmd));

    const esp_console_cmd_t wifi_status_cmd = {
        .command = "wifi_status",
        .help = "Show WiFi status",
        .hint = NULL,
        .func = &cmd_wifi_status,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_status_cmd));

    const esp_console_cmd_t wifi_scan_cmd = {
        .command = "wifi_scan",
        .help = "Scan WiFi networks",
        .hint = NULL,
        .func = &cmd_wifi_scan,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_scan_cmd));

    const esp_console_cmd_t wifi_disconnect_cmd = {
        .command = "wifi_disconnect",
        .help = "Disconnect from WiFi",
        .hint = NULL,
        .func = &cmd_wifi_disconnect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_disconnect_cmd));

    const esp_console_cmd_t wifi_reconnect_cmd = {
        .command = "wifi_reconnect",
        .help = "Reconnect using saved WiFi credentials",
        .hint = NULL,
        .func = &cmd_wifi_reconnect,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&wifi_reconnect_cmd));

    /* Node commands */
    const esp_console_cmd_t node_add_cmd = {
        .command = "node",
        .help = "Node management: node add <id> <ip> [port] | node_list | node_remove <id> | node_clear",
        .hint = NULL,
        .func = &cmd_node_add,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&node_add_cmd));

    const esp_console_cmd_t node_list_cmd = {
        .command = "node_list",
        .help = "List all stored nodes",
        .hint = NULL,
        .func = &cmd_node_list,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&node_list_cmd));

    const esp_console_cmd_t node_remove_cmd = {
        .command = "node_remove",
        .help = "Remove node: node_remove <id>",
        .hint = NULL,
        .func = &cmd_node_remove,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&node_remove_cmd));

    const esp_console_cmd_t node_clear_cmd = {
        .command = "node_clear",
        .help = "Clear all nodes",
        .hint = NULL,
        .func = &cmd_node_clear,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&node_clear_cmd));

    const esp_console_cmd_t ingest_cmd = {
        .command = "ingest",
        .help = "UDP ingest control: ingest <start|stop|status> [port]",
        .hint = NULL,
        .func = &cmd_ingest,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&ingest_cmd));

    ESP_LOGI(TAG, "CLI commands registered");
    return ESP_OK;
}
