/**
 * @file safety.h
 * @brief Local safety system: fire / gas / motion alarm and low-water warning.
 *
 * Runs entirely on the device and never depends on WiFi or Firebase.
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t safety_start(void);

#ifdef __cplusplus
}
#endif
