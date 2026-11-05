#include "serial_cmd.h"

#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "serial_cmd";

#define SERIAL_RX_BUFFER_SIZE 256
#define SERIAL_TASK_STACK     3072
#define SERIAL_TASK_PRIORITY  4
#define SERIAL_READ_TIMEOUT   pdMS_TO_TICKS(20)

static uart_port_t s_port;
static serial_cmd_handler_t s_handler;
static void *s_ctx;

static void serial_task(void *arg)
{
    (void)arg;

    char line[SERIAL_CMD_MAX_LEN];
    size_t index = 0;
    uint8_t chunk[32];

    for (;;) {
        const int n = uart_read_bytes(s_port, chunk, sizeof(chunk), SERIAL_READ_TIMEOUT);

        for (int i = 0; i < n; i++) {
            const char c = (char)chunk[i];

            /* Accept both CR and LF as terminators. */
            if (c == '\r' || c == '\n') {
                if (index > 0) {
                    line[index] = '\0';
                    s_handler(line, s_ctx);
                    index = 0;
                }
            } else if (index < sizeof(line) - 1) {
                line[index++] = c;
            } else {
                /* Overflow protection: drop the partial command. */
                index = 0;
                ESP_LOGW(TAG, "Command too long.");
            }
        }
    }
}

esp_err_t serial_cmd_start(uart_port_t port, serial_cmd_handler_t handler, void *ctx)
{
    ESP_RETURN_ON_FALSE(handler != NULL, ESP_ERR_INVALID_ARG, TAG, "handler is NULL");

    esp_err_t err = uart_driver_install(port, SERIAL_RX_BUFFER_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { /* already installed */
        ESP_RETURN_ON_ERROR(err, TAG, "uart_driver_install failed");
    }

    s_port = port;
    s_handler = handler;
    s_ctx = ctx;

    ESP_RETURN_ON_FALSE(xTaskCreate(serial_task, "serial_cmd", SERIAL_TASK_STACK, NULL,
                                    SERIAL_TASK_PRIORITY, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "task create failed");

    return ESP_OK;
}
