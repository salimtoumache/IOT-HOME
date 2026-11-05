/**
 * @file serial_cmd.h
 * @brief Line-based command receiver on a UART (CR or LF terminated).
 */
#pragma once

#include "driver/uart.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Maximum command length including the terminating NUL. */
#define SERIAL_CMD_MAX_LEN 32

/** Called from the serial task for every complete, non-empty line. */
typedef void (*serial_cmd_handler_t)(const char *command, void *ctx);

/**
 * @brief Install the UART driver (if needed) and start the receiver task.
 *
 * The port's baud rate is whatever the console is configured for (see
 * CONFIG_ESP_CONSOLE_UART_BAUDRATE; this project defaults it to 9600 to stay
 * compatible with the existing Python tooling).
 */
esp_err_t serial_cmd_start(uart_port_t port, serial_cmd_handler_t handler, void *ctx);

#ifdef __cplusplus
}
#endif
