#include "actuators.h"

#include "board.h"
#include "esp_check.h"
#include "esp_log.h"
#include "servo.h"

static const char *TAG = "actuators";

static const char *const k_names[ACT_COUNT] = {
    [ACT_LAMP1]     = "LAMP1",
    [ACT_LAMP2]     = "LAMP2",
    [ACT_FAN]       = "VENTILATOR",
    [ACT_DOOR]      = "DOOR",
    [ACT_WINDOW]    = "WINDOW",
    [ACT_MAIN_DOOR] = "MAIN_DOOR",
    [ACT_ALARM]     = "ALARM",
    [ACT_WATER_LED] = "WATER_LED",
};

static servo_t s_door;
static servo_t s_window;
static servo_t s_main_door;
static bool s_state[ACT_COUNT];

static void drive_gpio(gpio_num_t pin, int level)
{
    gpio_set_level(pin, level);
}

static void apply(actuator_id_t id, bool on)
{
    switch (id) {
    case ACT_LAMP1:
        drive_gpio(BOARD_PIN_LAMP1, on ? BOARD_RELAY_ON : BOARD_RELAY_OFF);
        break;
    case ACT_LAMP2:
        drive_gpio(BOARD_PIN_LAMP2, on ? BOARD_RELAY_ON : BOARD_RELAY_OFF);
        break;
    case ACT_FAN:
        drive_gpio(BOARD_PIN_FAN, on ? BOARD_RELAY_ON : BOARD_RELAY_OFF);
        break;
    case ACT_DOOR:
        servo_write(&s_door, on ? BOARD_DOOR_OPEN_ANGLE : BOARD_SERVO_CLOSED_ANGLE);
        break;
    case ACT_WINDOW:
        servo_write(&s_window, on ? BOARD_WINDOW_OPEN_ANGLE : BOARD_SERVO_CLOSED_ANGLE);
        break;
    case ACT_MAIN_DOOR:
        servo_write(&s_main_door, on ? BOARD_MAIN_DOOR_OPEN_ANGLE : BOARD_SERVO_CLOSED_ANGLE);
        break;
    case ACT_ALARM:
        drive_gpio(BOARD_PIN_BUZZER, on ? 1 : 0);
        break;
    case ACT_WATER_LED:
        drive_gpio(BOARD_PIN_WATER_LED, on ? 1 : 0);
        break;
    default:
        break;
    }
}

esp_err_t actuators_init(void)
{
    /* Pre-load the output latches so relays never glitch on during setup,
     * whatever the configured polarity is. */
    gpio_set_level(BOARD_PIN_LAMP1, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_LAMP2, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_FAN, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_BUZZER, 0);
    gpio_set_level(BOARD_PIN_WATER_LED, 0);

    const gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << BOARD_PIN_LAMP1) | (1ULL << BOARD_PIN_LAMP2) |
                        (1ULL << BOARD_PIN_FAN) | (1ULL << BOARD_PIN_BUZZER) |
                        (1ULL << BOARD_PIN_WATER_LED),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&outputs), TAG, "output GPIO config failed");

    /* Re-assert OFF now that the pins really are outputs. */
    gpio_set_level(BOARD_PIN_LAMP1, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_LAMP2, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_FAN, BOARD_RELAY_OFF);
    gpio_set_level(BOARD_PIN_BUZZER, 0);
    gpio_set_level(BOARD_PIN_WATER_LED, 0);

    ESP_LOGI(TAG, "GPIO initialized.");

    ESP_RETURN_ON_ERROR(servo_init(&s_door, BOARD_PIN_DOOR_SERVO, LEDC_CHANNEL_0,
                                   BOARD_SERVO_MIN_PULSE_US, BOARD_SERVO_MAX_PULSE_US),
                        TAG, "door servo init failed");
    ESP_RETURN_ON_ERROR(servo_init(&s_window, BOARD_PIN_WINDOW_SERVO, LEDC_CHANNEL_1,
                                   BOARD_SERVO_MIN_PULSE_US, BOARD_SERVO_MAX_PULSE_US),
                        TAG, "window servo init failed");
    ESP_RETURN_ON_ERROR(servo_init(&s_main_door, BOARD_PIN_MAIN_DOOR_SERVO, LEDC_CHANNEL_2,
                                   BOARD_SERVO_MIN_PULSE_US, BOARD_SERVO_MAX_PULSE_US),
                        TAG, "main door servo init failed");

    /* Initial locked/closed positions. */
    servo_write(&s_door, BOARD_SERVO_CLOSED_ANGLE);
    servo_write(&s_window, BOARD_SERVO_CLOSED_ANGLE);
    servo_write(&s_main_door, BOARD_SERVO_CLOSED_ANGLE);

    for (int i = 0; i < ACT_COUNT; i++) {
        s_state[i] = false;
    }

    ESP_LOGI(TAG, "Servo motors initialized.");
    return ESP_OK;
}

void actuators_set(actuator_id_t id, bool on)
{
    if ((unsigned)id >= ACT_COUNT || s_state[id] == on) {
        return;
    }

    s_state[id] = on;
    apply(id, on);

    ESP_LOGI(TAG, "%s -> %s", k_names[id], on ? "ON" : "OFF");
}

bool actuators_get(actuator_id_t id)
{
    return ((unsigned)id < ACT_COUNT) ? s_state[id] : false;
}
