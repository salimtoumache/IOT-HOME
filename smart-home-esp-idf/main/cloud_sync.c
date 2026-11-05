#include "cloud_sync.h"

#include <math.h>
#include <stddef.h>

#include "actuators.h"
#include "app_state.h"
#include "board.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "firebase_rtdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "wifi_sta.h"

static const char *TAG = "cloud";

#define CLOUD_TASK_STACK     10240 /* TLS handshake + JSON need generous stack */
#define CLOUD_TASK_PRIORITY  3
#define CLOUD_LOOP_MS        250

/* Database layout (unchanged from the Arduino firmware). */
#define FB_PATH_SENSOR       "Sensor"
#define FB_PATH_ACTUATORS    "actuators"

#define FB_KEY_TEMPERATURE   "temp"
#define FB_KEY_HUMIDITY      "huy"
#define FB_KEY_PIR           "pir"
#define FB_KEY_FIRE          "flam"
#define FB_KEY_GAS           "gaz"
#define FB_KEY_WATER         "water lavel"

static const struct {
    const char *key;
    actuator_id_t id;
} k_commands[] = {
    {"LAMP1",      ACT_LAMP1},
    {"LAMP2",      ACT_LAMP2},
    {"VENTILATOR", ACT_FAN},
    {"DOOR",       ACT_DOOR},
    {"WINDOW",     ACT_WINDOW},
};

/* Round to one decimal so the database holds "23.4" rather than float noise. */
static double round1(float v)
{
    return (double)lroundf(v * 10.0f) / 10.0;
}

static void send_sensor_data(void)
{
    ESP_LOGI(TAG, "Sending sensor data...");

    app_state_t s;
    app_state_snapshot(&s);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return;
    }

    if (s.environment_valid) {
        cJSON_AddNumberToObject(root, FB_KEY_TEMPERATURE, round1(s.temperature_c));
        cJSON_AddNumberToObject(root, FB_KEY_HUMIDITY, round1(s.humidity_pct));
    }
    cJSON_AddBoolToObject(root, FB_KEY_PIR, s.pir);
    cJSON_AddBoolToObject(root, FB_KEY_FIRE, s.fire);
    cJSON_AddBoolToObject(root, FB_KEY_GAS, s.gas);
    if (s.water_valid) {
        cJSON_AddNumberToObject(root, FB_KEY_WATER, round1(s.water_level_pct));
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    if (json == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        return;
    }

    if (firebase_rtdb_patch(FB_PATH_SENSOR, json) == ESP_OK) {
        ESP_LOGI(TAG, "Transmission completed.");
    } else {
        ESP_LOGE(TAG, "Sensor upload failed.");
    }

    cJSON_free(json);
}

static void read_actuator_commands(void)
{
    char buf[512];

    if (firebase_rtdb_get(FB_PATH_ACTUATORS, buf, sizeof(buf)) != ESP_OK) {
        ESP_LOGE(TAG, "Actuator read failed.");
        return;
    }

    cJSON *root = cJSON_Parse(buf);
    if (!cJSON_IsObject(root)) {
        ESP_LOGW(TAG, "No actuator commands in database.");
        cJSON_Delete(root);
        return;
    }

    for (size_t i = 0; i < sizeof(k_commands) / sizeof(k_commands[0]); i++) {
        const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, k_commands[i].key);

        /* Only well-formed boolean values are applied. */
        if (cJSON_IsBool(item)) {
            actuators_set(k_commands[i].id, cJSON_IsTrue(item));
        } else {
            ESP_LOGW(TAG, "/%s/%s is missing or not a boolean.", FB_PATH_ACTUATORS,
                     k_commands[i].key);
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Actuator synchronization completed.");
}

static void cloud_task(void *arg)
{
    (void)arg;

    int64_t last_auth_ms = -BOARD_AUTH_RETRY_INTERVAL_MS;
    int64_t last_sync_ms = 0;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(CLOUD_LOOP_MS));

        if (!wifi_sta_is_connected()) {
            continue;
        }

        const int64_t now_ms = esp_timer_get_time() / 1000;

        /* Anonymous sign-up, and token refresh before expiry. */
        if (firebase_rtdb_auth_due() && now_ms - last_auth_ms >= BOARD_AUTH_RETRY_INTERVAL_MS) {
            last_auth_ms = now_ms;
            firebase_rtdb_authenticate();
        }

        if (!firebase_rtdb_is_ready()) {
            continue;
        }

        if (now_ms - last_sync_ms >= BOARD_FIREBASE_INTERVAL_MS) {
            last_sync_ms = now_ms;

            send_sensor_data();
            read_actuator_commands();
        }
    }
}

esp_err_t cloud_sync_start(void)
{
    ESP_RETURN_ON_ERROR(firebase_rtdb_init(CONFIG_SH_FIREBASE_API_KEY,
                                           CONFIG_SH_FIREBASE_DATABASE_URL),
                        TAG, "Firebase init failed");

    ESP_RETURN_ON_FALSE(xTaskCreate(cloud_task, "cloud_task", CLOUD_TASK_STACK, NULL,
                                    CLOUD_TASK_PRIORITY, NULL) == pdPASS,
                        ESP_ERR_NO_MEM, TAG, "cloud task create failed");
    return ESP_OK;
}
