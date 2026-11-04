/*
===============================================================================
                    SMART HOME AUTOMATION SYSTEM
                              VERSION 2.0
===============================================================================

  Platform       : ESP32
  Communication  : WiFi / Firebase RTDB / USB Serial
  PC Interface   : Python / TensorFlow / CNN Face Recognition

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

// ============================================================================
// 01. LIBRARIES
// ============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <DHT.h>
#include <ESP32Servo.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"


// ============================================================================
// 02. USER CONFIGURATION
// ============================================================================

// WiFi credentials
#define WIFI_SSID       "SSID"
#define WIFI_PASSWORD   "PASSWORD"

// Firebase credentials
#define API_KEY         "API_KEY"
#define DATABASE_URL    "DATABASE_URL"

// Sensor configuration
#define DHT_PIN         13
#define DHT_TYPE        DHT11

#define PIR_PIN         12
#define FIRE_PIN        26
#define GAS_PIN         33

// Ultrasonic sensor
#define TRIG_PIN        27
#define ECHO_PIN        14

// Actuator pins
#define LAMP1_PIN       2
#define LAMP2_PIN       19
#define FAN_PIN         4

#define DOOR_PIN        16
#define WINDOW_PIN      18
#define MAIN_DOOR_PIN   15

#define BUZZER_PIN      32
#define WATER_LED_PIN   25


// ============================================================================
// 03. SYSTEM PARAMETERS
// ============================================================================

// Sensor timing
constexpr unsigned long DHT_INTERVAL       = 2000;
constexpr unsigned long ULTRASONIC_INTERVAL = 500;

// Firebase timing
constexpr unsigned long FIREBASE_INTERVAL  = 5000;

// WiFi reconnection timing
constexpr unsigned long WIFI_INTERVAL       = 10000;

// Firebase authentication retry
constexpr unsigned long AUTH_INTERVAL       = 15000;

// Ultrasonic parameters
constexpr float SOUND_SPEED = 0.0343f;
constexpr float MIN_DISTANCE = 5.0f;
constexpr float MAX_DISTANCE = 40.0f;

// Relay logic
// Change these if your relay modules are active LOW.
#define RELAY_ON  HIGH
#define RELAY_OFF LOW

// Original behavior: PIR also activates the buzzer.
// Set to false if you want the buzzer reserved for fire/gas.
#define PIR_TRIGGERS_BUZZER true

// Fire and gas modules were active LOW in your original code.
#define FIRE_ACTIVE_LEVEL LOW
#define GAS_ACTIVE_LEVEL  LOW


// ============================================================================
// 04. OBJECTS
// ============================================================================

DHT dht(DHT_PIN, DHT_TYPE);

Servo doorServo;
Servo windowServo;
Servo mainDoorServo;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;


// ============================================================================
// 05. GLOBAL VARIABLES
// ============================================================================

// Environmental measurements
float temperature = NAN;
float humidity = NAN;

// Water level
float distanceCm = NAN;
float waterLevelPercent = 0.0f;

bool waterLevelValid = false;

// Digital sensors
bool pirDetected = false;
bool fireDetected = false;
bool gasDetected = false;

// Actuator states
bool lamp1State = false;
bool lamp2State = false;
bool fanState = false;

bool doorState = false;
bool windowState = false;

// Firebase
bool signupOK = false;

// Timing
unsigned long lastDHTTime = 0;
unsigned long lastUltrasonicTime = 0;
unsigned long lastFirebaseTime = 0;
unsigned long lastWiFiAttempt = 0;
unsigned long lastAuthAttempt = 0;

// Serial command buffer
char serialBuffer[32];
uint8_t serialIndex = 0;


// ============================================================================
// 06. FUNCTION DECLARATIONS
// ============================================================================

void initializePins();
void initializeServos();
void initializeWiFi();
void initializeFirebase();

void maintainWiFi();
void maintainFirebaseAuth();

void readDigitalSensors();
void readDHTSensor();
void readUltrasonicSensor();

void updateSafetySystem();
void updateActuators();

void sendSensorData();
void readActuatorCommands();

void processSerialCommunication();
void executeSerialCommand(const char* command);

bool firebaseReadBool(const char* path, bool &value);
void reportFirebaseError(const char* operation);


// ============================================================================
// 07. INITIALIZATION
// ============================================================================

void setup() {

    Serial.begin(9600);

    Serial.println();
    Serial.println("======================================");
    Serial.println("       SMART HOME SYSTEM V2.0");
    Serial.println("======================================");

    // Initialize hardware
    initializePins();

    dht.begin();

    initializeServos();

    // Initialize communication
    initializeWiFi();

    initializeFirebase();

    Serial.println("[SYSTEM] Initialization completed.");
}


// ============================================================================
// 08. MAIN LOOP
// ============================================================================

void loop() {

    const unsigned long now = millis();

    // Maintain network connection
    maintainWiFi();

    // Maintain Firebase authentication
    maintainFirebaseAuth();

    // Read digital sensors
    readDigitalSensors();

    // DHT11 acquisition
    if (now - lastDHTTime >= DHT_INTERVAL) {

        lastDHTTime = now;

        readDHTSensor();
    }

    // Ultrasonic acquisition
    if (now - lastUltrasonicTime >= ULTRASONIC_INTERVAL) {

        lastUltrasonicTime = now;

        readUltrasonicSensor();
    }

    // Local safety system
    updateSafetySystem();

    // Serial communication with Python
    processSerialCommunication();

    // Firebase synchronization
    if (signupOK &&
        Firebase.ready() &&
        WiFi.status() == WL_CONNECTED &&
        now - lastFirebaseTime >= FIREBASE_INTERVAL) {

        lastFirebaseTime = now;

        sendSensorData();

        readActuatorCommands();
    }
}


// ============================================================================
// 09. HARDWARE INITIALIZATION
// ============================================================================

void initializePins() {

    // Inputs
    pinMode(PIR_PIN, INPUT);
    pinMode(FIRE_PIN, INPUT);
    pinMode(GAS_PIN, INPUT);

    pinMode(ECHO_PIN, INPUT);
    pinMode(TRIG_PIN, OUTPUT);

    digitalWrite(TRIG_PIN, LOW);

    // Outputs
    pinMode(LAMP1_PIN, OUTPUT);
    pinMode(LAMP2_PIN, OUTPUT);
    pinMode(FAN_PIN, OUTPUT);

    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(WATER_LED_PIN, OUTPUT);

    // Initialize outputs to safe OFF state
    digitalWrite(LAMP1_PIN, RELAY_OFF);
    digitalWrite(LAMP2_PIN, RELAY_OFF);
    digitalWrite(FAN_PIN, RELAY_OFF);

    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(WATER_LED_PIN, LOW);

    Serial.println("[HARDWARE] GPIO initialized.");
}


void initializeServos() {

    doorServo.setPeriodHertz(50);
    windowServo.setPeriodHertz(50);
    mainDoorServo.setPeriodHertz(50);

    doorServo.attach(DOOR_PIN, 500, 2400);
    windowServo.attach(WINDOW_PIN, 500, 2400);
    mainDoorServo.attach(MAIN_DOOR_PIN, 500, 2400);

    // Initial locked/closed positions
    doorServo.write(0);
    windowServo.write(0);
    mainDoorServo.write(0);

    Serial.println("[HARDWARE] Servo motors initialized.");
}


// ============================================================================
// 10. WIFI MANAGEMENT
// ============================================================================

void initializeWiFi() {

    WiFi.mode(WIFI_STA);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("[WIFI] Connecting...");
}


void maintainWiFi() {

    if (WiFi.status() == WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastWiFiAttempt >= WIFI_INTERVAL) {

        lastWiFiAttempt = now;

        Serial.println("[WIFI] Reconnecting...");

        WiFi.disconnect();

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
}


// ============================================================================
// 11. FIREBASE INITIALIZATION
// ============================================================================

void initializeFirebase() {

    config.api_key = API_KEY;

    config.database_url = DATABASE_URL;

    config.token_status_callback = tokenStatusCallback;

    Firebase.begin(&config, &auth);

    Firebase.reconnectWiFi(true);

    Serial.println("[FIREBASE] Client initialized.");
}


void maintainFirebaseAuth() {

    if (signupOK || WiFi.status() != WL_CONNECTED) {
        return;
    }

    const unsigned long now = millis();

    if (now - lastAuthAttempt < AUTH_INTERVAL) {
        return;
    }

    lastAuthAttempt = now;

    Serial.println("[FIREBASE] Attempting anonymous authentication...");

    if (Firebase.signUp(&config, &auth, "", "")) {

        signupOK = true;

        Serial.println("[FIREBASE] Authentication successful.");

    } else {

        Serial.println("[FIREBASE] Authentication failed.");

        Serial.println(fbdo.errorReason());
    }
}


// ============================================================================
// 12. DIGITAL SENSOR ACQUISITION
// ============================================================================

void readDigitalSensors() {

    pirDetected = (digitalRead(PIR_PIN) == HIGH);

    fireDetected =
        (digitalRead(FIRE_PIN) == FIRE_ACTIVE_LEVEL);

    gasDetected =
        (digitalRead(GAS_PIN) == GAS_ACTIVE_LEVEL);
}


// ============================================================================
// 13. DHT11 ACQUISITION
// ============================================================================

void readDHTSensor() {

    float newTemperature = dht.readTemperature();

    float newHumidity = dht.readHumidity();

    if (isnan(newTemperature) || isnan(newHumidity)) {

        Serial.println("[DHT] Sensor reading failed.");

        return;
    }

    temperature = newTemperature;

    humidity = newHumidity;

    Serial.print("[DHT] Temperature: ");
    Serial.print(temperature);

    Serial.print(" C | Humidity: ");
    Serial.print(humidity);

    Serial.println(" %");
}


// ============================================================================
// 14. ULTRASONIC SENSOR ACQUISITION
// ============================================================================

void readUltrasonicSensor() {

    digitalWrite(TRIG_PIN, LOW);

    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);

    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    // Timeout = 30 milliseconds
    unsigned long duration = pulseIn(ECHO_PIN, HIGH, 30000UL);

    if (duration == 0) {

        Serial.println("[WATER] No valid ultrasonic echo.");

        waterLevelValid = false;

        return;
    }

    float measuredDistance =
        (duration * SOUND_SPEED) / 2.0f;

    measuredDistance = constrain(
        measuredDistance,
        MIN_DISTANCE,
        MAX_DISTANCE
    );

    distanceCm = measuredDistance;

    // 5 cm = 100%
    // 40 cm = 0%

    waterLevelPercent =
        ((MAX_DISTANCE - distanceCm) /
         (MAX_DISTANCE - MIN_DISTANCE)) * 100.0f;

    waterLevelPercent = constrain(
        waterLevelPercent,
        0.0f,
        100.0f
    );

    waterLevelValid = true;

    Serial.print("[WATER] Level: ");

    Serial.print(waterLevelPercent);

    Serial.println(" %");
}


// ============================================================================
// 15. LOCAL SAFETY SYSTEM
// ============================================================================

void updateSafetySystem() {

    bool alarmCondition =
        fireDetected || gasDetected;

    if (PIR_TRIGGERS_BUZZER) {

        alarmCondition = alarmCondition || pirDetected;
    }

    // Local alarm: does not depend on Firebase or WiFi
    digitalWrite(
        BUZZER_PIN,
        alarmCondition ? HIGH : LOW
    );

    // Low water warning
    if (waterLevelValid) {

        digitalWrite(
            WATER_LED_PIN,
            waterLevelPercent < 50.0f ? HIGH : LOW
        );
    }
}


// ============================================================================
// 16. FIREBASE ERROR REPORTING
// ============================================================================

void reportFirebaseError(const char* operation) {

    Serial.print("[FIREBASE ERROR] ");

    Serial.print(operation);

    Serial.print(" : ");

    Serial.println(fbdo.errorReason());
}


// ============================================================================
// 17. SENSOR DATA TRANSMISSION
// ============================================================================

void sendSensorData() {

    Serial.println("[FIREBASE] Sending sensor data...");

    // Temperature
    if (!isnan(temperature)) {

        if (!Firebase.RTDB.setFloat(
                &fbdo,
                "/Sensor/temp",
                temperature)) {

            reportFirebaseError("Temperature");
        }
    }

    // Humidity
    if (!isnan(humidity)) {

        if (!Firebase.RTDB.setFloat(
                &fbdo,
                "/Sensor/huy",
                humidity)) {

            reportFirebaseError("Humidity");
        }
    }

    // PIR
    if (!Firebase.RTDB.setBool(
            &fbdo,
            "/Sensor/pir",
            pirDetected)) {

        reportFirebaseError("PIR");
    }

    // Fire
    if (!Firebase.RTDB.setBool(
            &fbdo,
            "/Sensor/flam",
            fireDetected)) {

        reportFirebaseError("Fire");
    }

    // Gas
    if (!Firebase.RTDB.setBool(
            &fbdo,
            "/Sensor/gaz",
            gasDetected)) {

        reportFirebaseError("Gas");
    }

    // Water level
    if (waterLevelValid) {

        if (!Firebase.RTDB.setFloat(
                &fbdo,
                "/Sensor/water lavel",
                waterLevelPercent)) {

            reportFirebaseError("Water level");
        }
    }

    Serial.println("[FIREBASE] Transmission completed.");
}


// ============================================================================
// 18. FIREBASE BOOLEAN READING
// ============================================================================

bool firebaseReadBool(
    const char* path,
    bool &value
) {

    if (Firebase.RTDB.getBool(&fbdo, path)) {

        if (fbdo.dataType() == "boolean") {

            value = fbdo.boolData();

            return true;
        }
    }

    reportFirebaseError(path);

    return false;
}


// ============================================================================
// 19. REMOTE ACTUATOR CONTROL
// ============================================================================

void readActuatorCommands() {

    bool newState;

    // LAMP 1
    if (firebaseReadBool(
            "/actuators/LAMP1",
            newState)) {

        lamp1State = newState;

        digitalWrite(
            LAMP1_PIN,
            lamp1State ? RELAY_ON : RELAY_OFF
        );
    }


    // LAMP 2
    if (firebaseReadBool(
            "/actuators/LAMP2",
            newState)) {

        lamp2State = newState;

        digitalWrite(
            LAMP2_PIN,
            lamp2State ? RELAY_ON : RELAY_OFF
        );
    }


    // VENTILATOR
    if (firebaseReadBool(
            "/actuators/VENTILATOR",
            newState)) {

        fanState = newState;

        digitalWrite(
            FAN_PIN,
            fanState ? RELAY_ON : RELAY_OFF
        );
    }


    // DOOR
    if (firebaseReadBool(
            "/actuators/DOOR",
            newState)) {

        doorState = newState;

        doorServo.write(
            doorState ? 170 : 0
        );
    }


    // WINDOW
    if (firebaseReadBool(
            "/actuators/WINDOW",
            newState)) {

        windowState = newState;

        windowServo.write(
            windowState ? 170 : 0
        );
    }

    Serial.println("[FIREBASE] Actuator synchronization completed.");
}


// ============================================================================
// 20. SERIAL COMMUNICATION WITH PYTHON
// ============================================================================

void executeSerialCommand(const char* command) {

    Serial.print("[SERIAL] Received: ");

    Serial.println(command);

    if (strcmp(command, "ON") == 0) {

        mainDoorServo.write(180);

        Serial.println("[SERIAL] Main door OPEN command executed.");

    }

    else if (strcmp(command, "OFF") == 0) {

        mainDoorServo.write(0);

        Serial.println("[SERIAL] Main door CLOSE command executed.");

    }

    else {

        Serial.println("[SERIAL] Unknown command.");
    }
}


void processSerialCommunication() {

    while (Serial.available() > 0) {

        char incomingChar = Serial.read();

        // Accept both CR and LF
        if (incomingChar == '\r' || incomingChar == '\n') {

            if (serialIndex > 0) {

                serialBuffer[serialIndex] = '\0';

                executeSerialCommand(serialBuffer);

                serialIndex = 0;
            }
        }

        else {

            if (serialIndex < sizeof(serialBuffer) - 1) {

                serialBuffer[serialIndex++] = incomingChar;

            }

            else {

                // Prevent buffer overflow
                serialIndex = 0;

                Serial.println("[SERIAL] Command too long.");
            }
        }
    }
}


// ============================================================================
//                            END OF FIRMWARE
//=============================================================================
