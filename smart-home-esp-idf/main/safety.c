#include "safety.h"

#include "actuators.h"
#include "app_state.h"
#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sensors.h"

static const char *TAG = "safety";

#define SAFETY_TASK_STACK    3072
#define SAFETY_TASK_PRIORITY 5
#define SAFETY_TASK_CORE     (portNUM_PROCESSORS > 1 ? 1 : 0)

static void safety_task(void *arg)
{
    (void)arg;

    bool prev_pir = false;
    bool prev_fire = false;
    bool prev_gas = false;

    TickType_t last_wake = xTaskGetTickCount();

    for (;;) {
        bool pir, fire, gas;
        sensors_read_digital(&pir, &fire, &gas);
        app_state_set_digital(pir, fire, gas);

        /* Log transitions only, so a persistent alarm does not flood the console. */
        if (fire != prev_fire) {
            ESP_LOGW(TAG, "Fire %s", fire ? "DETECTED" : "cleared");
        }
        if (gas != prev_gas) {
            ESP_LOGW(TAG, "Gas %s", gas ? "DETECTED" : "cleared");
        }
        if (pir != prev_pir) {
            ESP_LOGI(TAG, "Motion %s", pir ? "detected" : "ended");
        }
        prev_pir = pir;
        prev_fire = fire;
        prev_gas = gas;

        bool alarm = fire || gas;
#if BOARD_PIR_TRIGGERS_BUZZER
        alarm = alarm || pir;
#endif
        actuators_set(ACT_ALARM, alarm);

        /* Low water warning (only while the reading is valid). */
        app_state_t state;
        app_state_snapshot(&state);
        if (state.water_valid) {
            actuators_set(ACT_WATER_LED, state.water_level_pct < BOARD_WATER_LOW_THRESHOLD_PCT);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(BOARD_SAFETY_PERIOD_MS));
    }
}

esp_err_t safety_start(void)
{
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(safety_task, "safety_task", SAFETY_TASK_STACK,
                                                NULL, SAFETY_TASK_PRIORITY, NULL,
                                                SAFETY_TASK_CORE) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "safety task create failed");
    return ESP_OK;
}
