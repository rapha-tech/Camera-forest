/* The example of ESP-IDF
 *
 * This sample code is in the public domain.
 */

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"

#include "ra01s.h"

static const char *TAG = "MAIN";

static struct {
    struct arg_str *message; /**< message string (required) */
    struct arg_end *end;
} lora_send_message_args;

char message[255];

static int lora_send_message_handler(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&lora_send_message_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, lora_send_message_args.end, argv[0]);
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Sending : %s over LoRa", lora_send_message_args.message->sval[0]);

	memcpy(message, lora_send_message_args.message->sval[0], 255);

	int txLen = strlen(message);
	if (txLen == 0) return ESP_OK;

	if (!LoRaSend((uint8_t *)message, txLen, SX126x_TXMODE_SYNC)) {
		ESP_LOGE(TAG, "LoRaSend failed");
	}

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

void task_rx(void *pvParameters)
{
	ESP_LOGI(pcTaskGetName(NULL), "Start");
	uint8_t buf[255]; // Maximum Payload size of SX1261/62/68 is 255
	while(1) {
		uint8_t rxLen = LoRaReceive(buf, sizeof(buf));
		if ( rxLen > 0 ) { 
			ESP_LOGI(pcTaskGetName(NULL), "%d byte packet received:[%.*s]", rxLen, rxLen, buf);

			int8_t rssi, snr;
			GetPacketStatus(&rssi, &snr);
			ESP_LOGI(pcTaskGetName(NULL), "rssi=%d[dBm] snr=%d[dB]", rssi, snr);
		}
		vTaskDelay(1); // Avoid WatchDog alerts
	} // end while

	// never reach here
	vTaskDelete( NULL );
}

void app_main()
{
	esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = CONFIG_IDF_TARGET ">";
    repl_config.max_cmdline_length = 1024;

    /* Register commands */
    esp_console_register_help_command();
    register_lora_send();

    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));

    ESP_ERROR_CHECK(esp_console_start_repl(repl));

	// Initialize LoRa
	LoRaInit();
	int8_t txPowerInDbm = 22;

	uint32_t frequencyInHz = 0;
#if CONFIG_433MHZ
	frequencyInHz = 433000000;
	ESP_LOGI(TAG, "Frequency is 433MHz");
#elif CONFIG_866MHZ
	frequencyInHz = 866000000;
	ESP_LOGI(TAG, "Frequency is 866MHz");
#elif CONFIG_915MHZ
	frequencyInHz = 915000000;
	ESP_LOGI(TAG, "Frequency is 915MHz");
#elif CONFIG_OTHER
	ESP_LOGI(TAG, "Frequency is %dMHz", CONFIG_OTHER_FREQUENCY);
	frequencyInHz = CONFIG_OTHER_FREQUENCY * 1000000;
#endif

#if CONFIG_USE_TCXO
	ESP_LOGW(TAG, "Enable TCXO");
	float tcxoVoltage = 3.3; // use TCXO
	bool useRegulatorLDO = true; // use DCDC + LDO
#else
	ESP_LOGW(TAG, "Disable TCXO");
	float tcxoVoltage = 0.0; // don't use TCXO
	bool useRegulatorLDO = false; // use only LDO in all modes
#endif

	//LoRaDebugPrint(true);
	if (LoRaBegin(frequencyInHz, txPowerInDbm, tcxoVoltage, useRegulatorLDO) != 0) {
		ESP_LOGE(TAG, "Does not recognize the module");
		while(1) {
			vTaskDelay(1);
		}
	}
	
	uint8_t spreadingFactor = 7;
	uint8_t bandwidth = 4;
	uint8_t codingRate = 1;
	uint16_t preambleLength = 8;
	uint8_t payloadLen = 0;
	bool crcOn = true;
	bool invertIrq = false;
#if CONFIG_ADVANCED
	spreadingFactor = CONFIG_SF_RATE;
	bandwidth = CONFIG_BANDWIDTH;
	codingRate = CONFIG_CODING_RATE;
#endif
	LoRaConfig(spreadingFactor, bandwidth, codingRate, preambleLength, payloadLen, crcOn, invertIrq);

	xTaskCreate(&task_rx, "RX", 1024*4, NULL, 5, NULL);
}

