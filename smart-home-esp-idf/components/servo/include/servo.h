/**
 * @file servo.h
 * @brief Hobby-servo driver on top of the LEDC peripheral (50 Hz PWM).
 */
#pragma once

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    ledc_channel_t channel;
    uint32_t min_pulse_us; /* pulse width at 0 degrees   */
    uint32_t max_pulse_us; /* pulse width at 180 degrees */
} servo_t;

/**
 * @brief Attach a servo to a GPIO using the given LEDC channel.
 *
 * All servos share one LEDC timer (50 Hz, 14-bit), configured on first use.
 */
esp_err_t servo_init(servo_t *servo, gpio_num_t pin, ledc_channel_t channel,
                     uint32_t min_pulse_us, uint32_t max_pulse_us);

/**
 * @brief Move the servo to an angle. Values are clamped to 0..180 degrees.
 */
esp_err_t servo_write(servo_t *servo, int angle_deg);

#ifdef __cplusplus
}
#endif
