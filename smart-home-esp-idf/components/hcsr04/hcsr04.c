#include "hcsr04.h"

#include "esp_attr.h"
#include "esp_check.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"

static const char *TAG = "hcsr04";

/* Speed of sound at ~20 degC, in centimetres per microsecond. */
#define HCSR04_SOUND_SPEED_CM_PER_US 0.0343f

static void IRAM_ATTR echo_isr(void *arg)
{
    hcsr04_t *dev = (hcsr04_t *)arg;
    const int64_t now = esp_timer_get_time();

    if (gpio_get_level(dev->echo_pin)) {
        dev->rise_us = now;
        return;
    }

    /* Falling edge: ignore it unless we saw the matching rising edge. */
    if (dev->rise_us == 0 || dev->waiter == NULL) {
        return;
    }

    dev->fall_us = now;

    BaseType_t higher_priority_woken = pdFALSE;
    vTaskNotifyGiveFromISR(dev->waiter, &higher_priority_woken);
    if (higher_priority_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t hcsr04_init(hcsr04_t *dev, gpio_num_t trig_pin, gpio_num_t echo_pin)
{
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");

    dev->trig_pin = trig_pin;
    dev->echo_pin = echo_pin;
    dev->rise_us = 0;
    dev->fall_us = 0;
    dev->waiter = NULL;

    const gpio_config_t trig_cfg = {
        .pin_bit_mask = 1ULL << trig_pin,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&trig_cfg), TAG, "trigger pin config failed");
    gpio_set_level(trig_pin, 0);

    const gpio_config_t echo_cfg = {
        .pin_bit_mask = 1ULL << echo_pin,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&echo_cfg), TAG, "echo pin config failed");

    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) { /* already installed */
        ESP_RETURN_ON_ERROR(err, TAG, "ISR service install failed");
    }

    ESP_RETURN_ON_ERROR(gpio_isr_handler_add(echo_pin, echo_isr, dev), TAG,
                        "ISR handler add failed");

    return ESP_OK;
}

esp_err_t hcsr04_measure(hcsr04_t *dev, uint32_t timeout_ms, float *distance_cm)
{
    ESP_RETURN_ON_FALSE(dev && distance_cm, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    dev->rise_us = 0;
    dev->fall_us = 0;
    dev->waiter = xTaskGetCurrentTaskHandle();
    (void)ulTaskNotifyTake(pdTRUE, 0); /* drop any stale notification */

    /* 10 us trigger pulse, preceded by a clean low level. */
    gpio_set_level(dev->trig_pin, 0);
    esp_rom_delay_us(2);
    gpio_set_level(dev->trig_pin, 1);
    esp_rom_delay_us(10);
    gpio_set_level(dev->trig_pin, 0);

    const uint32_t got_echo = ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeout_ms));
    dev->waiter = NULL;

    if (got_echo == 0) {
        return ESP_ERR_TIMEOUT;
    }

    const int64_t pulse_us = dev->fall_us - dev->rise_us;
    if (pulse_us <= 0) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    *distance_cm = ((float)pulse_us * HCSR04_SOUND_SPEED_CM_PER_US) / 2.0f;

    return ESP_OK;
}
