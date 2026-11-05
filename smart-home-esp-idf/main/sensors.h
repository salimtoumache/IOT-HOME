/**
 * @file sensors.h
 * @brief Sensor acquisition: digital inputs, DHT11 and HC-SR04 water level.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Configure sensor GPIOs and initialise the DHT11 / HC-SR04 drivers. */
esp_err_t sensors_init(void);

/** @brief Sample PIR, fire and gas inputs (already converted to "active" booleans). */
void sensors_read_digital(bool *pir, bool *fire, bool *gas);

/** @brief Start the task that periodically reads the DHT11 and the ultrasonic sensor. */
esp_err_t sensors_start(void);

#ifdef __cplusplus
}
#endif
