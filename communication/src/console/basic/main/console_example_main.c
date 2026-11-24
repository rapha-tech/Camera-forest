/* Basic console example (esp_console_repl API)

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"


static const char* TAG = "example";
#define PROMPT_STR CONFIG_IDF_TARGET

static struct {
    struct arg_str *message;        /**< message string (required) */
    struct arg_end *end;
} lora_send_message_args;

static int lora_send_message_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&lora_send_message_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, lora_send_message_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sending : %s over LoRa", lora_send_message_args.message->sval[0]);
    return ESP_OK;
}

static void register_lora_send(void)
{
    lora_send_message_args.message = arg_str1(NULL, NULL, "<message>", "Message to send over LoRa");
    lora_send_message_args.end = arg_end(2);

    const esp_console_cmd_t cmd = {
        .command = "lora_send",
        .help = "Send a message over LoRa using string format",
        .hint = NULL,
        .func = &lora_send_message_handler,
        .argtable = &lora_send_message_args,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cmd));
}

void app_main(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = 1024;

    /* Register commands */
    esp_console_register_help_command();
    register_lora_send();

    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
