#include "sensors.h"

#include "app_state.h"
#include "board.h"
#include "dht11.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hcsr04.h"

static const char *TAG = "sensors";

#define ENV_TASK_STACK    3072
#define ENV_TASK_PRIORITY 4
#define ENV_TASK_CORE     (portNUM_PROCESSORS > 1 ? 1 : 0)
#define ENV_LOOP_MS       10

static dht11_t s_dht;
static hcsr04_t s_sonar;

static inline float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

esp_err_t sensors_init(void)
{
    const gpio_config_t inputs = {
        .pin_bit_mask = (1ULL << BOARD_PIN_PIR) | (1ULL << BOARD_PIN_FIRE) |
                        (1ULL << BOARD_PIN_GAS),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&inputs), TAG, "digital input config failed");

    ESP_RETURN_ON_ERROR(dht11_init(&s_dht, BOARD_PIN_DHT), TAG, "DHT11 init failed");
    ESP_RETURN_ON_ERROR(hcsr04_init(&s_sonar, BOARD_PIN_TRIG, BOARD_PIN_ECHO), TAG,
                        "HC-SR04 init failed");

    return ESP_OK;
}

void sensors_read_digital(bool *pir, bool *fire, bool *gas)
{
    *pir = (gpio_get_level(BOARD_PIN_PIR) == 1);
    *fire = (gpio_get_level(BOARD_PIN_FIRE) == BOARD_FIRE_ACTIVE_LEVEL);
    *gas = (gpio_get_level(BOARD_PIN_GAS) == BOARD_GAS_ACTIVE_LEVEL);
}

static void read_dht(void)
{
    float temperature = 0.0f;
    float humidity = 0.0f;

    const esp_err_t err = dht11_read(&s_dht, &temperature, &humidity);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[DHT] Sensor reading failed (%s).", esp_err_to_name(err));
        return;
    }

    app_state_set_environment(temperature, humidity);
    ESP_LOGI(TAG, "[DHT] Temperature: %.1f C | Humidity: %.1f %%", temperature, humidity);
}

static void read_water_level(void)
{
    float distance_cm = 0.0f;

    const esp_err_t err = hcsr04_measure(&s_sonar, BOARD_ULTRASONIC_TIMEOUT_MS, &distance_cm);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[WATER] No valid ultrasonic echo.");
        app_state_invalidate_water();
        return;
    }

    distance_cm = clampf(distance_cm, BOARD_WATER_MIN_DISTANCE_CM, BOARD_WATER_MAX_DISTANCE_CM);

    /* 5 cm = 100 %, 40 cm = 0 % */
    float level_pct = ((BOARD_WATER_MAX_DISTANCE_CM - distance_cm) /
                       (BOARD_WATER_MAX_DISTANCE_CM - BOARD_WATER_MIN_DISTANCE_CM)) * 100.0f;
    level_pct = clampf(level_pct, 0.0f, 100.0f);

    app_state_set_water(distance_cm, level_pct);
    ESP_LOGI(TAG, "[WATER] Level: %.1f %%", level_pct);
}

static void env_task(void *arg)
{
    (void)arg;

    /* Both readings fire on the first iteration, then on their own cadence. */
    int64_t last_dht_ms = -BOARD_DHT_INTERVAL_MS;
    int64_t last_sonar_ms = -BOARD_ULTRASONIC_INTERVAL_MS;

    for (;;) {
        const int64_t now_ms = esp_timer_get_time() / 1000;

        if (now_ms - last_dht_ms >= BOARD_DHT_INTERVAL_MS) {
            last_dht_ms = now_ms;
            read_dht();
        }

        if (now_ms - last_sonar_ms >= BOARD_ULTRASONIC_INTERVAL_MS) {
            last_sonar_ms = now_ms;
            read_water_level();
        }

        vTaskDelay(pdMS_TO_TICKS(ENV_LOOP_MS));
    }
}

esp_err_t sensors_start(void)
{
    /* Pinned to the application core: the DHT11 read disables interrupts for a
     * few milliseconds and must not interfere with the WiFi stack on core 0. */
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(env_task, "env_task", ENV_TASK_STACK, NULL,
                                                ENV_TASK_PRIORITY, NULL, ENV_TASK_CORE) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "env task create failed");
    return ESP_OK;
}
