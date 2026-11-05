#include "dht11.h"

#include <string.h>

#include "esp_check.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "dht11";

#define DHT11_START_LOW_MS        20   /* host start signal, >= 18 ms           */
#define DHT11_RELEASE_SETTLE_US   10   /* let the pull-up raise the line        */
#define DHT11_RESP_LOW_TIMEOUT_US 60   /* sensor pulls low 20-40 us after start */
#define DHT11_RESP_HIGH_TIMEOUT_US 100 /* response: 80 us low, 80 us high       */
#define DHT11_BIT_LOW_TIMEOUT_US  70   /* every bit starts with ~50 us low      */
#define DHT11_BIT_HIGH_TIMEOUT_US 100  /* 26-28 us => 0, ~70 us => 1            */
#define DHT11_BIT_THRESHOLD_US    40

static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;

/* Busy-wait until the pin reaches `level`; report how long it took. */
static esp_err_t await_level(gpio_num_t pin, int level, uint32_t timeout_us,
                             uint32_t *elapsed_us)
{
    for (uint32_t t = 0; t <= timeout_us; t++) {
        if (gpio_get_level(pin) == level) {
            if (elapsed_us) {
                *elapsed_us = t;
            }
            return ESP_OK;
        }
        esp_rom_delay_us(1);
    }
    return ESP_ERR_TIMEOUT;
}

/* Time-critical part of the transfer: runs with interrupts disabled. */
static esp_err_t read_frame(gpio_num_t pin, uint8_t data[5])
{
    esp_err_t err;

    memset(data, 0, 5);

    portENTER_CRITICAL(&s_mux);

    gpio_set_level(pin, 1); /* release the bus */
    esp_rom_delay_us(DHT11_RELEASE_SETTLE_US);

    /* Sensor response: low (80 us) -> high (80 us) -> low (start of bit 0). */
    err = await_level(pin, 0, DHT11_RESP_LOW_TIMEOUT_US, NULL);
    if (err == ESP_OK) {
        err = await_level(pin, 1, DHT11_RESP_HIGH_TIMEOUT_US, NULL);
    }
    if (err == ESP_OK) {
        err = await_level(pin, 0, DHT11_RESP_HIGH_TIMEOUT_US, NULL);
    }

    for (int i = 0; err == ESP_OK && i < 40; i++) {
        uint32_t high_us = 0;

        err = await_level(pin, 1, DHT11_BIT_LOW_TIMEOUT_US, NULL);
        if (err != ESP_OK) {
            break;
        }
        err = await_level(pin, 0, DHT11_BIT_HIGH_TIMEOUT_US, &high_us);
        if (err != ESP_OK) {
            break;
        }

        data[i / 8] <<= 1;
        if (high_us > DHT11_BIT_THRESHOLD_US) {
            data[i / 8] |= 1;
        }
    }

    portEXIT_CRITICAL(&s_mux);

    return err;
}

esp_err_t dht11_init(dht11_t *dev, gpio_num_t pin)
{
    ESP_RETURN_ON_FALSE(dev != NULL, ESP_ERR_INVALID_ARG, TAG, "dev is NULL");

    const gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << pin,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&cfg), TAG, "gpio_config failed");

    gpio_set_level(pin, 1); /* idle high */
    dev->pin = pin;

    return ESP_OK;
}

esp_err_t dht11_read(dht11_t *dev, float *temperature_c, float *humidity_pct)
{
    ESP_RETURN_ON_FALSE(dev && temperature_c && humidity_pct, ESP_ERR_INVALID_ARG,
                        TAG, "invalid argument");

    uint8_t data[5];

    /* Host start signal: pull the line low for at least 18 ms. The extra tick
     * guarantees the minimum even with a coarse FreeRTOS tick rate. */
    gpio_set_level(dev->pin, 0);
    vTaskDelay(pdMS_TO_TICKS(DHT11_START_LOW_MS) + 1);

    esp_err_t err = read_frame(dev->pin, data);

    gpio_set_level(dev->pin, 1); /* back to idle */

    if (err != ESP_OK) {
        return err;
    }

    const uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) {
        return ESP_ERR_INVALID_CRC;
    }

    /* Same decoding as the Adafruit DHT library used by the Arduino firmware. */
    float humidity = (float)data[0] + (float)data[1] * 0.1f;

    float temperature = (float)data[2];
    if (data[3] & 0x80) {
        temperature = -1.0f - temperature;
    }
    temperature += (float)(data[3] & 0x0F) * 0.1f;

    *humidity_pct = humidity;
    *temperature_c = temperature;

    return ESP_OK;
}
