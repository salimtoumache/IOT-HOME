/**
 * @file cloud_sync.h
 * @brief Firebase Realtime Database synchronisation.
 *
 * Every BOARD_FIREBASE_INTERVAL_MS the task pushes the latest sensor readings
 * to "/Sensor" and pulls actuator commands from "/actuators".
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t cloud_sync_start(void);

#ifdef __cplusplus
}
#endif
