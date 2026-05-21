#include <stdio.h>

#include "cli_parser.h"
#include "hmi_screen.h"
#include "wifi_manager.h"
#include "node_manager.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_usb_serial_jtag.h"
#include "linenoise/linenoise.h"
#include "nvs_flash.h"

static const char *TAG = "main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_vfs_dev_usb_serial_jtag_set_rx_line_endings(ESP_LINE_ENDINGS_CR);
    esp_vfs_dev_usb_serial_jtag_set_tx_line_endings(ESP_LINE_ENDINGS_CRLF);

    linenoiseSetMultiLine(1);
    linenoiseHistorySetMaxLen(50);
    linenoiseAllowEmpty(false);

    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "hmi> ";
    repl_config.max_cmdline_length = 256;

    esp_console_repl_t *repl = NULL;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usb_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usb_config, &repl_config, &repl));
#elif CONFIG_ESP_CONSOLE_UART
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
#else
#error "Enable either USB_SERIAL_JTAG or UART console in menuconfig"
#endif

    // REPL constructor initializes esp_console internally.
    ESP_ERROR_CHECK(esp_console_register_help_command());
    ESP_ERROR_CHECK(cli_parser_register_commands());

    /* Initialize WiFi and node managers */
    ESP_ERROR_CHECK(wifi_manager_init());
    ESP_ERROR_CHECK(node_manager_init());

    if (!hmi_screen_start()) {
        ESP_LOGW(TAG, "HMI screen did not start");
    }

    ESP_LOGI(TAG, "ESP32-S3 Touch LCD 4.3B CLI ready");
    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
