/**
 * @file wifi_sta.h
 * @brief Non-blocking WiFi station with automatic reconnection.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the WiFi station and connect in the background.
 *
 * Returns immediately; the local safety logic keeps running while the link
 * comes up. If the link drops, a reconnect is attempted periodically
 * (BOARD_WIFI_RETRY_INTERVAL_MS).
 *
 * NVS must be initialised before calling this function.
 */
esp_err_t wifi_sta_start(const char *ssid, const char *password);

/** @brief True while the station has an IP address. */
bool wifi_sta_is_connected(void);

#ifdef __cplusplus
}
#endif
