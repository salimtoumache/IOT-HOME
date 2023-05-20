/*
     __________________________   SMART HOME SYSTEMS V1.0   __________________________

     IOT BASED SMART HOME AUTOMATION CONNECTED WITH FIREBASE AND ANDROID APP,
     ALSO INTEGRATED  WITH ARTIFICIAL NEURAL NETWORKS MODEL FOR FACE RECOGNITION
     USING TENSORFLOW WITH CNN ALGORITHM
     ESP32 IS MAIN CONTROLLER IN IOT
     _________________________________________________________________________________
     Sensor And Devices Are Use
           Esp32 board
           3 set of Relays
           DHT11 sensor
           Flame sensor
           Pir   sensor
           3 set of Servo Motor
*/
// 1) wifi -------------------------------------------------------------------------
#include <WiFi.h>
#define WIFI_SSID   "Galaxy A0s1655"
#define WIFI_PASSWORD "pekg7142"
// 2) BD -------------------------------------------------------------------------
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#define API_KEY "AIzaSyBskALg-FiZSSDl1P75pq-S4dNbeFFFvXI"
#define DATABASE_URL "https://smarthome-20565-default-rtdb.firebaseio.com/"
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
unsigned long sendDataPrevMillis = 0;
bool signupOK = false;
// 3) DHT -------------------------------------------------------------------------
#include <DHT.h>
DHT dht(13, DHT11);
float t ;
float h ;
// 4) PIR -------------------------------------------------------------------------
int dataPIR ;
// 5) FIRE -------------------------------------------------------------------------
int dataFIRE ;
//  6) GAZ -------------------------------------------------------------------------
int dataGAZ ;
//  7) LAMP1 -------------------------------------------------------------------------
int LAMP1_PIN = 2;
bool LAMP1status;
//  8) LAMP2 -------------------------------------------------------------------------
int LAMP2_PIN = 19;
bool LAMP2status;
//  9) VENTILATOR -------------------------------------------------------------------------
int VENTILATOR_PIN = 4;
bool VENTILATORstatus;
// 10) DOOR AND WINDOW -------------------------------------------------------------------------
#include <ESP32Servo.h>
Servo DOOR;
Servo WINDOW;
bool DOORstatus;
bool WINDOWstatus;
// 11) buzzer-------------------------------------------------------------------------
int BUZZER_PIN = 32;
// 12) SERIAL PYTHON -------------------------------------------------------------------------
Servo MAIN_DOOR;
String myCMD;
// 12) water lavel -------------------------------------------------------------------------
const int trigPin = 27;
const int echoPin = 14;
float valper;
float SOUND_SPEED= 0.034;
long duration;
float distanceCm;
void setup() {
  // 1) DHT -------------------------------------------------------------------------
  dht.begin();
  // 2) PIR -------------------------------------------------------------------------
  pinMode(12, INPUT);
  // 3) FIRE -------------------------------------------------------------------------
  pinMode(26, INPUT);
  // 5) GAZ -------------------------------------------------------------------------
  pinMode(33, INPUT);
  // 6) LAMP1 -------------------------------------------------------------------------
  pinMode(LAMP1_PIN, OUTPUT);
  // 7) LAMP2 -------------------------------------------------------------------------
  pinMode(LAMP2_PIN, OUTPUT);
  // 8) VENTILATOR -------------------------------------------------------------------------
  pinMode(VENTILATOR_PIN, OUTPUT);
  // 9) DOOR -------------------------------------------------------------------------
  DOOR.attach(16);
  // 10) WINDOW -------------------------------------------------------------------------
  WINDOW.attach(18);
  // 11) BUZZER -------------------------------------------------------------------------
  pinMode(BUZZER_PIN, OUTPUT);
  // 12) SERIAL PYTHON -------------------------------------------------------------------------
  MAIN_DOOR.attach(15);
  //13) water lave control
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(25, OUTPUT);
  // 13) serial -------------------------------------------------------------------------
  Serial.begin(9600);
  // 14) wifi -------------------------------------------------------------------------
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {}
  // 15) DB -------------------------------------------------------------------------
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  if (Firebase.signUp(&config, &auth, "", "")) {
    signupOK = true;
  }
  else {
  }
  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}
void loop() {
  SENSOR_DATA();
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();
    // SEND -------------------------------------------------------------------------
    SEND_DATA();
    // READ -------------------------------------------------------------------------
    READ_DATA();
  }
  SERIAL_PYTHON();
}
void SENSOR_DATA() {
  t = dht.readTemperature();
  h = dht.readHumidity();
  dataPIR = digitalRead(12);
  dataFIRE = !digitalRead(26);
  dataGAZ = !digitalRead(33);
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH);
  distanceCm = duration * SOUND_SPEED/2;
  if (distanceCm>40) {distanceCm=40;}
  if (distanceCm<5) {distanceCm=5;}
  valper=map(distanceCm,5,40,100,0);  
  ALARM();
}
void SEND_DATA() {
  if (Firebase.RTDB.setFloat(&fbdo, "/Sensor/huy", h)) {}else {}  
  if (Firebase.RTDB.setFloat(&fbdo, "/Sensor/temp", t)) {}else {}
  if (Firebase.RTDB.setBool(&fbdo, "/Sensor/pir", dataPIR)) {}else {}
  if (Firebase.RTDB.setBool(&fbdo, "/Sensor/flam", dataFIRE)){}else {}
  if (Firebase.RTDB.setBool(&fbdo, "/Sensor/gaz", dataGAZ)){}else {}
  if (Firebase.RTDB.setFloat(&fbdo, "/Sensor/water lavel", valper)) {}else {}
}
void READ_DATA() {
  //  LAMP1
  if (Firebase.RTDB.getBool(&fbdo, "/actuators/LAMP1")) {
    if (fbdo.dataType() == "boolean") 
    {
      LAMP1status = fbdo.boolData();
      digitalWrite(LAMP1_PIN, LAMP1status);
    }
  }
  else {}
  //  LAMP2
  if (Firebase.RTDB.getBool(&fbdo, "/actuators/LAMP2")) {
    if (fbdo.dataType() == "boolean") 
    {
      LAMP2status = fbdo.boolData();
      digitalWrite(LAMP2_PIN, LAMP2status);
    }
  }
  else {}
  //  VENTILATOR
  if (Firebase.RTDB.getBool(&fbdo, "/actuators/VENTILATOR")) {
    if (fbdo.dataType() == "boolean") 
    {
      VENTILATORstatus = fbdo.boolData();
      digitalWrite(VENTILATOR_PIN, VENTILATORstatus);
    }
  }
  else {}
  //  DOOR
  if (Firebase.RTDB.getBool(&fbdo, "/actuators/DOOR")) {
    if (fbdo.dataType() == "boolean") {
      DOORstatus = fbdo.boolData();
      if (DOORstatus) {
        DOOR.write(170);
      }
      else {
        DOOR.write(0);
      }
    }
  }
  else {}
  //  WINDOW
  if (Firebase.RTDB.getBool(&fbdo, "/actuators/WINDOW")) {
    if (fbdo.dataType() == "boolean") {
      WINDOWstatus = fbdo.boolData();
      if (WINDOWstatus) {
        WINDOW.write(170);
      }
      else {
        WINDOW.write(0);
      }
    }
  }
  else {}
}
// func alarm
void ALARM() {
  if (dataGAZ ==1 or dataFIRE == 1 or dataPIR == 1 ) {digitalWrite(BUZZER_PIN, 1);}
  else {digitalWrite(BUZZER_PIN, 0);}
  if (valper<50){digitalWrite(25,1);}
  else{digitalWrite(25,0);}
}
// SERIAL PYTHON
void SERIAL_PYTHON() {
  myCMD = Serial.readStringUntil('\r');
  if (myCMD == "ON") {
    MAIN_DOOR.write(180);
  }
  else if (myCMD == "OFF") {
    MAIN_DOOR.write(0);
  }
}
