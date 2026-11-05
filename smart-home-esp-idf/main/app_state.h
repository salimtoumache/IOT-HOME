/**
 * @file app_state.h
 * @brief Thread-safe snapshot of the latest sensor readings.
 *
 * Producers (safety / environment tasks) publish into it, consumers (cloud
 * sync, safety logic) read a consistent copy.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* DHT11 (temperature and humidity are always updated together) */
    float temperature_c;
    float humidity_pct;
    bool environment_valid;

    /* HC-SR04 water level */
    float distance_cm;
    float water_level_pct;
    bool water_valid;

    /* Digital sensors */
    bool pir;
    bool fire;
    bool gas;
} app_state_t;

void app_state_init(void);
void app_state_snapshot(app_state_t *out);

void app_state_set_environment(float temperature_c, float humidity_pct);
void app_state_set_water(float distance_cm, float level_pct);
void app_state_invalidate_water(void);
void app_state_set_digital(bool pir, bool fire, bool gas);

#ifdef __cplusplus
}
#endif
