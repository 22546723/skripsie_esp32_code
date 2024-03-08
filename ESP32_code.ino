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
#define MOISTURE_PIN 2
#define UV_PIN 3

// REMOVE
#define SSID "Doofenshmirtz Evil Inc."
#define PASSWORD "0768114612"
// #define SSID "Gabrielle"
// #define PASSWORD "Gabs2301"


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
    digitalWrite(22, HIGH);
  }
  else {
    digitalWrite(22, LOW);
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
    digitalWrite(23, HIGH);
  }
  else {
    digitalWrite(23, LOW);
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
  pinMode(21, OUTPUT); //working
  pinMode(22, OUTPUT); //wifi connected
  pinMode(23, OUTPUT); //firebase connected

  // Set working status LED
  digitalWrite(21, HIGH);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Log timer setup
  logTimerSemaphore = xSemaphoreCreateBinary();
  log_timer = timerBegin(1000000);  // 1MHz timer
  timerAttachInterrupt(log_timer, &onLogTimer);
  timerAlarm(log_timer, log_timer_period, true, 0);

  // Bluetooth setup
  connectionManager.setupBluetooth(board_name);

  Serial.println("setup done");

}

void loop() {

  /////////
  // BLUETOOTH
  ////////
  connectionManager.sendWifiNetworks();
  connectionManager.setupWifi();

  // Serial.println("here1");

  /////////
  // WIFI
  ////////
  if (!connectionManager.getConnectionStatus()) {
    runWifi();
  }

  // Serial.println("here2");

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

  // Serial.println("here4");


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

  // Serial.println("here5");

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

  // Serial.println("here6");


  /////////
  // PUMP
  ////////  


}