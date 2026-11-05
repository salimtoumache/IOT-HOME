/**
 * @file dht11.h
 * @brief Bit-banged single-wire driver for the DHT11 temperature/humidity sensor.
 */
#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t pin;
} dht11_t;

/**
 * @brief Configure the data pin (open-drain with internal pull-up).
 */
esp_err_t dht11_init(dht11_t *dev, gpio_num_t pin);

/**
 * @brief Read one temperature/humidity sample.
 *
 * Blocks for roughly 25 ms. The sensor must not be polled faster than once
 * per second.
 *
 * @return ESP_OK, ESP_ERR_TIMEOUT (no/garbled response) or
 *         ESP_ERR_INVALID_CRC (checksum mismatch).
 */
esp_err_t dht11_read(dht11_t *dev, float *temperature_c, float *humidity_pct);

#ifdef __cplusplus
}
#endif
