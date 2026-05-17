#include "cli_parser.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "argtable3/argtable3.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_timer.h"

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

    ESP_LOGI(TAG, "CLI commands registered");
    return ESP_OK;
}
