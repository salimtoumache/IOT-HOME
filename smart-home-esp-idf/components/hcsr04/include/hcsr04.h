/**
 * @file hcsr04.h
 * @brief HC-SR04 ultrasonic distance sensor driver.
 *
 * The echo pulse is timestamped in a GPIO interrupt, so the measurement is
 * accurate regardless of task scheduling and never busy-waits on the echo.
 */
#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t trig_pin;
    gpio_num_t echo_pin;
    volatile int64_t rise_us;
    volatile int64_t fall_us;
    volatile TaskHandle_t waiter;
} hcsr04_t;

/**
 * @brief Configure the trigger/echo pins and install the echo interrupt.
 *
 * @note The HC-SR04 drives ECHO at 5 V. Use a voltage divider (or a level
 *       shifter) before connecting it to an ESP32 GPIO.
 */
esp_err_t hcsr04_init(hcsr04_t *dev, gpio_num_t trig_pin, gpio_num_t echo_pin);

/**
 * @brief Trigger one measurement and wait for the echo.
 *
 * Must be called from a single task at a time.
 *
 * @param timeout_ms  Maximum time to wait for the echo.
 * @param distance_cm Output distance in centimetres.
 * @return ESP_OK, ESP_ERR_TIMEOUT (no echo) or ESP_ERR_INVALID_RESPONSE.
 */
esp_err_t hcsr04_measure(hcsr04_t *dev, uint32_t timeout_ms, float *distance_cm);

#ifdef __cplusplus
}
#endif
