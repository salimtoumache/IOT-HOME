/**
 * @file board.h
 * @brief Board-level configuration: GPIO map, timing and tuning constants.
 *
 * Everything that is hardware- or wiring-specific lives here so that the rest
 * of the firmware stays free of magic numbers. Behavioural switches (relay
 * polarity, sensor polarity, PIR/buzzer coupling) come from Kconfig
 * (`idf.py menuconfig` -> "Smart Home Configuration").
 */
#pragma once

#include "driver/gpio.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Sensor pins                                                                */
/* -------------------------------------------------------------------------- */
#define BOARD_PIN_DHT                 GPIO_NUM_13
#define BOARD_PIN_PIR                 GPIO_NUM_12
#define BOARD_PIN_FIRE                GPIO_NUM_26
#define BOARD_PIN_GAS                 GPIO_NUM_33
#define BOARD_PIN_TRIG                GPIO_NUM_27
#define BOARD_PIN_ECHO                GPIO_NUM_14

/* -------------------------------------------------------------------------- */
/* Actuator pins                                                              */
/* -------------------------------------------------------------------------- */
#define BOARD_PIN_LAMP1               GPIO_NUM_2
#define BOARD_PIN_LAMP2               GPIO_NUM_19
#define BOARD_PIN_FAN                 GPIO_NUM_4

#define BOARD_PIN_DOOR_SERVO          GPIO_NUM_16
#define BOARD_PIN_WINDOW_SERVO        GPIO_NUM_18
#define BOARD_PIN_MAIN_DOOR_SERVO     GPIO_NUM_15

#define BOARD_PIN_BUZZER              GPIO_NUM_32
#define BOARD_PIN_WATER_LED           GPIO_NUM_25

/* -------------------------------------------------------------------------- */
/* Timing (milliseconds)                                                      */
/* -------------------------------------------------------------------------- */
#define BOARD_DHT_INTERVAL_MS             2000
#define BOARD_ULTRASONIC_INTERVAL_MS      500
#define BOARD_FIREBASE_INTERVAL_MS        5000
#define BOARD_WIFI_RETRY_INTERVAL_MS      10000
#define BOARD_AUTH_RETRY_INTERVAL_MS      15000
#define BOARD_SAFETY_PERIOD_MS            20

/* -------------------------------------------------------------------------- */
/* Water level (HC-SR04)                                                      */
/* -------------------------------------------------------------------------- */
#define BOARD_ULTRASONIC_TIMEOUT_MS       30
#define BOARD_WATER_MIN_DISTANCE_CM       5.0f   /* 100 % */
#define BOARD_WATER_MAX_DISTANCE_CM       40.0f  /*   0 % */
#define BOARD_WATER_LOW_THRESHOLD_PCT     50.0f

/* -------------------------------------------------------------------------- */
/* Servos                                                                     */
/* -------------------------------------------------------------------------- */
#define BOARD_SERVO_MIN_PULSE_US          500
#define BOARD_SERVO_MAX_PULSE_US          2400

#define BOARD_DOOR_OPEN_ANGLE             170
#define BOARD_WINDOW_OPEN_ANGLE           170
#define BOARD_MAIN_DOOR_OPEN_ANGLE        180
#define BOARD_SERVO_CLOSED_ANGLE          0

/* -------------------------------------------------------------------------- */
/* Polarity / behaviour switches (from Kconfig)                               */
/* -------------------------------------------------------------------------- */
#ifdef CONFIG_SH_RELAY_ACTIVE_HIGH
#define BOARD_RELAY_ON                    1
#define BOARD_RELAY_OFF                   0
#else
#define BOARD_RELAY_ON                    0
#define BOARD_RELAY_OFF                   1
#endif

#ifdef CONFIG_SH_FIRE_SENSOR_ACTIVE_LOW
#define BOARD_FIRE_ACTIVE_LEVEL           0
#else
#define BOARD_FIRE_ACTIVE_LEVEL           1
#endif

#ifdef CONFIG_SH_GAS_SENSOR_ACTIVE_LOW
#define BOARD_GAS_ACTIVE_LEVEL            0
#else
#define BOARD_GAS_ACTIVE_LEVEL            1
#endif

#ifdef CONFIG_SH_PIR_TRIGGERS_BUZZER
#define BOARD_PIR_TRIGGERS_BUZZER         1
#else
#define BOARD_PIR_TRIGGERS_BUZZER         0
#endif

#ifdef __cplusplus
}
#endif
