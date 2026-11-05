#include "app_state.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static app_state_t s_state;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

void app_state_init(void)
{
    portENTER_CRITICAL(&s_lock);
    memset(&s_state, 0, sizeof(s_state));
    portEXIT_CRITICAL(&s_lock);
}

void app_state_snapshot(app_state_t *out)
{
    portENTER_CRITICAL(&s_lock);
    *out = s_state;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_environment(float temperature_c, float humidity_pct)
{
    portENTER_CRITICAL(&s_lock);
    s_state.temperature_c = temperature_c;
    s_state.humidity_pct = humidity_pct;
    s_state.environment_valid = true;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_water(float distance_cm, float level_pct)
{
    portENTER_CRITICAL(&s_lock);
    s_state.distance_cm = distance_cm;
    s_state.water_level_pct = level_pct;
    s_state.water_valid = true;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_invalidate_water(void)
{
    portENTER_CRITICAL(&s_lock);
    s_state.water_valid = false;
    portEXIT_CRITICAL(&s_lock);
}

void app_state_set_digital(bool pir, bool fire, bool gas)
{
    portENTER_CRITICAL(&s_lock);
    s_state.pir = pir;
    s_state.fire = fire;
    s_state.gas = gas;
    portEXIT_CRITICAL(&s_lock);
}
