# Smart Home Automation System

ESP32 firmware for a connected smart home, written for **ESP-IDF v5.x**.
It monitors the environment, runs a local safety system that works with no
network, syncs with **Firebase Realtime Database**, and accepts door commands
from a Python face-recognition application over USB serial.

Version 2.0 · Author: **Salim Toumache**

## Features

| # | Function | Implementation |
|---|----------|----------------|
| 1 | Environmental monitoring | DHT11 (bit-banged driver) |
| 2 | Motion detection | PIR |
| 3 | Fire detection | Digital flame sensor |
| 4 | Gas detection | Digital gas sensor |
| 5 | Water level monitoring | HC-SR04, interrupt-timestamped echo |
| 6 | Remote lighting control | 2 relays via Firebase |
| 7 | Ventilation control | Relay via Firebase |
| 8 | Door and window automation | Servos (LEDC, 50 Hz) via Firebase |
| 9 | Local safety alarm | Buzzer + low-water LED, independent of WiFi/Firebase |
| 10 | Python-to-ESP32 serial link | UART0, 9600 baud, `ON` / `OFF` main door commands |
| 11 | Firebase synchronisation | REST over TLS, anonymous auth with token refresh |

## Architecture

```
                 +-------------------------------------------+
                 |                  app_main                 |
                 +-------------------------------------------+
                    |         |          |          |
             safety_task  env_task   serial_cmd  cloud_task
             (core 1,     (core 1,   (UART0)     (HTTPS,
              20 ms)       DHT/HCSR)                 5 s)
                    \         |          |          /
                     +--- app_state (snapshot) ----+
                                   |
                              actuators  --> relays / servos / buzzer / LED
```

* `safety_task` samples PIR, fire and gas every 20 ms and drives the buzzer
  directly. It starts before any network code, so the alarm never depends on
  WiFi or Firebase.
* `env_task` reads the DHT11 every 2 s and the water level every 500 ms. It is
  pinned to the application core because the DHT11 read briefly disables
  interrupts.
* `cloud_task` authenticates anonymously, refreshes the token before it
  expires, then every 5 s pushes sensors and pulls actuator commands. One
  persistent TLS connection is reused, so a cycle is two HTTP requests.
* `serial_cmd` parses CR/LF-terminated lines from the PC.

### Project layout

```
.
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/                  application logic
│   ├── main.c             entry point and task start-up
│   ├── app_state.[ch]     thread-safe sensor snapshot
│   ├── sensors.[ch]       digital inputs, DHT11, water level
│   ├── actuators.[ch]     relays, servos, buzzer, LED
│   ├── safety.[ch]        local alarm logic
│   ├── cloud_sync.[ch]    Firebase sync task
│   └── Kconfig.projbuild  credentials and behaviour switches
└── components/            reusable drivers
    ├── board/             pin map, timing, thresholds
    ├── dht11/             DHT11 driver
    ├── hcsr04/            HC-SR04 driver
    ├── servo/             LEDC servo driver
    ├── wifi_sta/          WiFi station with auto-reconnect
    ├── firebase_rtdb/     Firebase REST client
    └── serial_cmd/        UART line command receiver
```

## Hardware

| Signal | GPIO | | Signal | GPIO |
|--------|------|-|--------|------|
| DHT11 data | 13 | | Lamp 1 relay | 2 |
| PIR | 12 | | Lamp 2 relay | 19 |
| Fire sensor | 26 | | Ventilator relay | 4 |
| Gas sensor | 33 | | Door servo | 16 |
| HC-SR04 TRIG | 27 | | Window servo | 18 |
| HC-SR04 ECHO | 14 | | Main door servo | 15 |
| Buzzer | 32 | | Low-water LED | 25 |

All pins are defined in `components/board/include/board.h`.

Hardware notes:

* **HC-SR04 ECHO outputs 5 V.** Put a voltage divider (for example 1 kΩ / 2 kΩ)
  between ECHO and GPIO14.
* GPIO 2, 12 and 15 are ESP32 *strapping pins*. A device pulling GPIO12 high at
  power-up (a PIR can do this) may prevent the board from booting; unplug it
  while flashing if you see boot errors.
* Servos and relays need their own power supply, with a common ground.

## Firebase

Enable **Anonymous** sign-in (Authentication → Sign-in method) and allow
authenticated access in the Realtime Database rules.

Data model (unchanged from the Arduino version):

```
Sensor/
  temp          number   °C
  huy           number   % relative humidity
  pir           boolean
  flam          boolean  fire
  gaz           boolean  gas
  water lavel   number   % (0-100)
actuators/
  LAMP1         boolean
  LAMP2         boolean
  VENTILATOR    boolean
  DOOR          boolean
  WINDOW        boolean
```

Numeric values are rounded to one decimal place.

## Serial protocol

9600 baud, 8N1, commands terminated by `\n` or `\r`.

| Command | Action |
|---------|--------|
| `ON`  | Open the main door (servo 180°) |
| `OFF` | Close the main door (servo 0°) |

Anything else is answered with an `Unknown command` log line.

## Build and flash

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

```bash
idf.py set-target esp32
idf.py menuconfig        # Smart Home Configuration: WiFi + Firebase credentials
idf.py build
idf.py -p /dev/ttyUSB0 flash
idf.py -p /dev/ttyUSB0 monitor -b 9600
```

Credentials are stored in the generated `sdkconfig`, which is git-ignored.
`menuconfig` also exposes the relay polarity, fire/gas sensor polarity and the
PIR-triggers-buzzer switch.

The console runs at 9600 baud (set in `sdkconfig.defaults`) so the existing
Python application keeps working without changes.
