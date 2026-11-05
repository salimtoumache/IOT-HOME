/*
===============================================================================
                    SMART HOME AUTOMATION SYSTEM
                              VERSION 2.0
                           (ESP-IDF edition)
===============================================================================

  Platform       : ESP32 (ESP-IDF v5.x)
  Communication  : WiFi / Firebase RTDB / USB Serial
  PC Interface   : Face Recognition

  FUNCTIONS
  ---------
  1. Environmental monitoring (DHT11)
  2. Motion detection (PIR)
  3. Fire detection
  4. Gas detection
  5. Water level monitoring (HC-SR04)
  6. Remote lighting control
  7. Ventilation control
  8. Door and window automation
  9. Local safety alarm
 10. Python-to-ESP32 serial communication
 11. Firebase real-time data synchronization

  AUTHOR: Salim Toumache
===============================================================================
*/

#include <stdio.h>
#include <string.h>

#include "actuators.h"
#include "app_state.h"
#include "cloud_sync.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "safety.h"
#include "sensors.h"
#include "serial_cmd.h"
#include "wifi_sta.h"

static const char *TAG = "smart_home";

/* Commands sent by the Python face-recognition application. */
static void on_serial_command(const char *command, void *ctx)
{
    (void)ctx;

    ESP_LOGI(TAG, "[SERIAL] Received: %s", command);

    if (strcmp(command, "ON") == 0) {
        actuators_set(ACT_MAIN_DOOR, true);
        ESP_LOGI(TAG, "[SERIAL] Main door OPEN command executed.");
    } else if (strcmp(command, "OFF") == 0) {
        actuators_set(ACT_MAIN_DOOR, false);
        ESP_LOGI(TAG, "[SERIAL] Main door CLOSE command executed.");
    } else {
        ESP_LOGW(TAG, "[SERIAL] Unknown command.");
    }
}

static esp_err_t init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "NVS erase failed");
        err = nvs_flash_init();
    }
    return err;
}

void app_main(void)
{
    printf("\n======================================\n");
    printf("       SMART HOME SYSTEM V2.0\n");
    printf("======================================\n");

    ESP_ERROR_CHECK(init_nvs());

    app_state_init();

    /* Hardware first: outputs in a safe state, then sensors. */
    ESP_ERROR_CHECK(actuators_init());
    ESP_ERROR_CHECK(sensors_init());

    /* Local safety logic starts before any network code, so the alarm works
     * even if WiFi or Firebase never come up. */
    ESP_ERROR_CHECK(safety_start());
    ESP_ERROR_CHECK(sensors_start());

    /* PC link (main door commands). */
    ESP_ERROR_CHECK(serial_cmd_start(UART_NUM_0, on_serial_command, NULL));

    /* Connectivity. */
    ESP_ERROR_CHECK(wifi_sta_start(CONFIG_SH_WIFI_SSID, CONFIG_SH_WIFI_PASSWORD));
    ESP_ERROR_CHECK(cloud_sync_start());

    ESP_LOGI(TAG, "[SYSTEM] Initialization completed.");
}
