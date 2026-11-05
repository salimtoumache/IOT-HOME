/**
 * @file actuators.h
 * @brief Single access point for every output: relays, servos, buzzer, LED.
 */
#pragma once

#include <stdbool.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACT_LAMP1 = 0,
    ACT_LAMP2,
    ACT_FAN,
    ACT_DOOR,
    ACT_WINDOW,
    ACT_MAIN_DOOR,
    ACT_ALARM,
    ACT_WATER_LED,
    ACT_COUNT
} actuator_id_t;

/** @brief Configure all outputs and drive them to their safe OFF/closed state. */
esp_err_t actuators_init(void);

/** @brief Switch an output. Does nothing (and logs nothing) if already in that state. */
void actuators_set(actuator_id_t id, bool on);

/** @brief Last commanded state of an output. */
bool actuators_get(actuator_id_t id);

#ifdef __cplusplus
}
#endif
