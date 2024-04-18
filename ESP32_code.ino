#include <tuple>
#include "DataLogger.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "ConnectionManager.h"
#include "DataManager.h"
#include <Preferences.h>
#include "time.h"

#define RGB_BRIGHTNESS 64
#define MOISTURE_PIN 0
#define UV_PIN 1
#define PUMP_PIN 11
#define POWER_PIN 23
#define WIFI_PIN 22
#define FIREBASE_PIN 21
#define PUMP_T_MAX 1400 // us
#define PUMP_T_MIN 500 // us
#define PPM_PERIOD 20000 // us


////////////////////////////////////////////////////////
/**
* VARIABLES
*/
////////////////////////////////////////////////////////

// General
String board_name;
uint8_t soil_target;

// Timestamp
int timestamp;
const char* ntpServer = "pool.ntp.org";

// Sensors
uint16_t soil_moisture_range[2] = { 1362.0, 3047.0 };
uint16_t uv_range[2] = { 0.0, 3462.0 };
uint16_t upload_count_max = 30;  //upload to database every 30 records

// Classes
DataLogger dataLogger(upload_count_max);
ConnectionManager connectionManager;
DataManager dataManager;

// Log timer
hw_timer_t* log_timer = NULL;
uint32_t log_timer_period = 60 * 1000000;  // triger log timer (1MHz) every 60 seconds
volatile SemaphoreHandle_t logTimerSemaphore;
portMUX_TYPE logTimerMux = portMUX_INITIALIZER_UNLOCKED;

// Pump timer
hw_timer_t* pump_timer = NULL;
uint32_t pump_timer_period = 1; // triger pump timer (1MHz) every 1us
volatile SemaphoreHandle_t pumpTimerSemaphore;
portMUX_TYPE pumpTimerMux = portMUX_INITIALIZER_UNLOCKED;
uint16_t current_pump_delay;
 // = PUMP_T_MAX - (PUMP_T_MAX - PUMP_T_MIN)*pump_speed;
float pump_speed = 0.6; // % of full speed
bool pumpOn;
uint8_t ppm_state = 0; // 0 - idle | 1 - pulse | 2 - fill

int temp_count = 0;

////////////////////////////////////////////////////////
/**
* AUX FUNCTIONS
*/
////////////////////////////////////////////////////////

// Log timer interrupt
void IRAM_ATTR onLogTimer() {
  portENTER_CRITICAL_ISR(&logTimerMux);
  portEXIT_CRITICAL_ISR(&logTimerMux);
  xSemaphoreGiveFromISR(logTimerSemaphore, NULL);
}

// Pump timer interrupt
void IRAM_ATTR onPumpTimer() {
  uint16_t pump_delay;
  uint8_t setTo;
  uint16_t pulse_len = (PUMP_T_MAX - (PUMP_T_MAX - PUMP_T_MIN)*pump_speed);
  // Serial.println("at pump interrupt");

  portENTER_CRITICAL_ISR(&pumpTimerMux);

  if (ppm_state == 0) {  // idle
    setTo = LOW;
    ppm_state = 1; //pulse
  }

  if (ppm_state == 1) { // pulse
    setTo = HIGH;
    ppm_state = 2; // set to fill
    pump_delay = pulse_len; 
  }
  else if (ppm_state == 2) { // fill
    // digitalWrite(PUMP_PIN, HIGH);
    setTo = LOW;
    ppm_state = 1; //set to pulse
    pump_delay = PPM_PERIOD - pulse_len; // period - pulse
  }

  // restart timer alarm
  timerAlarm(pump_timer, pump_delay, true, 0);
  // if (pumpOn) {
  //   timerAlarm(pump_timer, pump_delay, true, 0);
  // }
  

  portEXIT_CRITICAL_ISR(&pumpTimerMux);
  digitalWrite(PUMP_PIN, setTo);
  // xSemaphoreGiveFromISR(pumpTimerSemaphore, NULL);
}

// Function that gets current epoch time
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    //Serial.println("Failed to obtain time");
    return (0);
  }
  time(&now);
  return now;
}

// Connect to wifi if possible
void runWifi() {
  uint8_t wifi_error = 0;
  
  bool conn_check = connectionManager.haveWifiConfig();
  if (conn_check) {
    wifi_error = connectionManager.connectToWifi();
  }

  // Set wifi status LED
  if (wifi_error == 1) {
    digitalWrite(WIFI_PIN, HIGH);
  }
  else {
    digitalWrite(WIFI_PIN, LOW);
  }
}


// Connect to firebase 
void runFirebase() {
  uint8_t firebase_error = dataManager.connectToFirebase();

  board_name = dataManager.getName();
  connectionManager.setName(board_name);
  soil_target = dataManager.getSoilTarget();

  // Set firebase status LED
  if (firebase_error == 1) {
    digitalWrite(FIREBASE_PIN, HIGH);
  }
  else {
    digitalWrite(FIREBASE_PIN, LOW);
  }
}

////////////////////////////////////////////////////////
/**
* MAIN FUNCTIONS
*/
////////////////////////////////////////////////////////

void setup() {
 
  // General setup
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("test");
  digitalWrite(RGB_BUILTIN, HIGH);
  configTime(0, 0, ntpServer);

  // Board name
  board_name = dataManager.getName();

  // Set status LED pins
  pinMode(POWER_PIN, OUTPUT); //working
  pinMode(WIFI_PIN, OUTPUT); //wifi connected
  pinMode(FIREBASE_PIN, OUTPUT); //firebase connected

  // Set working status LED
  digitalWrite(POWER_PIN, HIGH);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Serial.println("s1");

  // Pump setup
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  current_pump_delay = 0;
  pumpOn = false;

  // Serial.println("s2");

  // Pump timer setup
  // pumpTimerSemaphore = xSemaphoreCreateBinary();
  // Serial.println("s2.1");
  pump_timer = timerBegin(1000000);  // 1MHz timer
  // Serial.println("s2.2");
  timerAttachInterrupt(pump_timer, &onPumpTimer);
  // Serial.println("s2.3");
  timerAlarm(pump_timer, PPM_PERIOD, true, 0);

  // Serial.println("s3");

  // Log timer setup
  logTimerSemaphore = xSemaphoreCreateBinary();
  log_timer = timerBegin(1000000);  // 1MHz timer
  timerAttachInterrupt(log_timer, &onLogTimer);
  timerAlarm(log_timer, log_timer_period, true, 0);

  // Serial.println("s4");

  // Bluetooth setup
  connectionManager.setupBluetooth(board_name);

  // Serial.println("setup done");

}

void loop() {
  Serial.println("loop");

  /////////
  // BLUETOOTH
  ////////
  connectionManager.sendWifiNetworks();
  connectionManager.setupWifi();


  /////////
  // WIFI
  ////////
  if (!connectionManager.getConnectionStatus()) {
    runWifi();
  }


  /////////
  // FIREBASE
  ////////  
  if ((!dataManager.getFirebaseStatus()) && connectionManager.getConnectionStatus()) {
    runFirebase();
  }

  // Serial.println("here3");

  // update soil target if needed
  if (dataManager.checkForUpdates() && dataManager.getFirebaseStatus()) {
    soil_target = dataManager.getSoilTarget();
    board_name = dataManager.getName();
    connectionManager.setName(board_name);
  }


  /////////
  // SENSOR DATA
  ////////  

  // Read sensors
  float soil_lvl = analogRead(MOISTURE_PIN);
  float uv_lvl = analogRead(UV_PIN);
  timestamp = getTime();

  // Get sensor readings as percentages
  soil_lvl = 100.0 - (((soil_lvl - soil_moisture_range[0]) / (soil_moisture_range[1] - soil_moisture_range[0])) * 100.0);
  uv_lvl = ((uv_lvl - uv_range[0]) / (uv_range[1] - uv_range[0])) * 100;


  // Log timer triggered
  if (xSemaphoreTake(logTimerSemaphore, 0) == pdTRUE) {
    portENTER_CRITICAL(&logTimerMux);
    portEXIT_CRITICAL(&logTimerMux);

    bool max_reached;
    uint8_t soil_avg, uv_avg;

    // Update the average sensor readings
    std::tie(max_reached, soil_avg, uv_avg) = dataLogger.logData(soil_lvl, uv_lvl);

    if (dataManager.getFirebaseStatus() && connectionManager.getConnectionStatus()) {
      // Upload live readings
      dataManager.uploadLiveData(soil_lvl, uv_lvl);
      Serial.println("Uploading live sensor readings");    

      // Upload the average readings
      if (max_reached) {
        dataManager.uploadData(soil_avg, uv_avg, String(timestamp));
        Serial.println("Upload to server");
      }
    }
  }


  /////////
  // PUMP
  ////////  
  // bool pump_temp = true;
  // bool pump_stop = false;
  // temp_count = temp_count + 1;

  // if (pump_temp) { // replace with actual on condition (soil_lvl <= 0.9*soil_taget)
  //   pumpOn = true;
  //   // timerAlarm(pump_timer, PPM_PULSE_LEN, false, 0); // start PPM signal timer
  // }

  // if ((temp_count >= 5) && (pumpOn)) { // replace with actual on condition (soil_lvl >= 1.1*soil_taget)
  //   pumpOn = false;
  //   digitalWrite(PUMP_PIN, LOW);
  // }


  // if (xSemaphoreTake(pumpTimerSemaphore, 0) == pdTRUE) {
  //   portENTER_CRITICAL(&pumpTimerMux);
  //   portEXIT_CRITICAL(&pumpTimerMux);
  //   Serial.println("here");

  //   // End the current pulse if high
  //   if (digitalRead(PUMP_PIN) == HIGH) {
  //     digitalWrite(PUMP_PIN, LOW);
  //   }

  //   if (pumpOn) {
  //     // Update current delay and calculate target delay
  //     current_pump_delay = current_pump_delay + 1; 
  //     uint16_t target_delay = PUMP_T_MAX - (PUMP_T_MAX - PUMP_T_MIN)*pump_speed;

  //     // Start next pulse after delay
  //     if (current_pump_delay >= target_delay) {
  //       digitalWrite(PUMP_PIN, HIGH);
  //       current_pump_delay = 0;
  //     }
  //   } else {
  //     Serial.println("pump off");
  //   }
  // }

}