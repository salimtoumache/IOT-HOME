#include "servo.h"

#include <stdbool.h>

#include "esp_check.h"

static const char *TAG = "servo";

#define SERVO_LEDC_MODE   LEDC_LOW_SPEED_MODE
#define SERVO_LEDC_TIMER  LEDC_TIMER_0
#define SERVO_LEDC_RES    LEDC_TIMER_14_BIT
#define SERVO_FREQ_HZ     50
#define SERVO_PERIOD_US   (1000000UL / SERVO_FREQ_HZ)

static bool s_timer_ready;

static esp_err_t ensure_timer(void)
{
    if (s_timer_ready) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer = {
        .speed_mode = SERVO_LEDC_MODE,
        .duty_resolution = SERVO_LEDC_RES,
        .timer_num = SERVO_LEDC_TIMER,
        .freq_hz = SERVO_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "LEDC timer config failed");

    s_timer_ready = true;
    return ESP_OK;
}

esp_err_t servo_init(servo_t *servo, gpio_num_t pin, ledc_channel_t channel,
                     uint32_t min_pulse_us, uint32_t max_pulse_us)
{
    ESP_RETURN_ON_FALSE(servo != NULL, ESP_ERR_INVALID_ARG, TAG, "servo is NULL");
    ESP_RETURN_ON_FALSE(max_pulse_us > min_pulse_us && max_pulse_us < SERVO_PERIOD_US,
                        ESP_ERR_INVALID_ARG, TAG, "invalid pulse range");

    ESP_RETURN_ON_ERROR(ensure_timer(), TAG, "timer setup failed");

    const ledc_channel_config_t ch = {
        .gpio_num = pin,
        .speed_mode = SERVO_LEDC_MODE,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = SERVO_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_RETURN_ON_ERROR(ledc_channel_config(&ch), TAG, "LEDC channel config failed");

    servo->channel = channel;
    servo->min_pulse_us = min_pulse_us;
    servo->max_pulse_us = max_pulse_us;

    return ESP_OK;
}

esp_err_t servo_write(servo_t *servo, int angle_deg)
{
    ESP_RETURN_ON_FALSE(servo != NULL, ESP_ERR_INVALID_ARG, TAG, "servo is NULL");

    if (angle_deg < 0) {
        angle_deg = 0;
    } else if (angle_deg > 180) {
        angle_deg = 180;
    }

    const uint32_t span = servo->max_pulse_us - servo->min_pulse_us;
    const uint32_t pulse_us = servo->min_pulse_us + (uint32_t)(((uint64_t)span * (uint32_t)angle_deg) / 180U);
    const uint32_t duty = (uint32_t)(((uint64_t)pulse_us << SERVO_LEDC_RES) / SERVO_PERIOD_US);

    ESP_RETURN_ON_ERROR(ledc_set_duty(SERVO_LEDC_MODE, servo->channel, duty), TAG,
                        "set duty failed");
    return ledc_update_duty(SERVO_LEDC_MODE, servo->channel);
}
