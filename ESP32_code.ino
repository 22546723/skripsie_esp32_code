#include <tuple>
#include "DataLogger.h"
#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include "ConnectionManager.h"
#include "DataManager.h"
#include <Preferences.h>
#include "time.h"

//#include <ArduinoWebsockets.h>

#define RGB_BRIGHTNESS 64
#define MOISTURE_PIN 2
#define UV_PIN 3
// #define FIREBASE_URL "https://skripsie-1e23a-default-rtdb.firebaseio.com/"
// #define API_KEY ""

// REMOVE
#define SSID "Doofenshmirtz Evil Inc."
#define PASSWORD "0768114612"

//////////////////////////////////////
// VARIABLES
//////////////////////////////////////
// General
String board_name;
uint8_t soil_target;

int timestamp;
const char* ntpServer = "pool.ntp.org";

// Sensor
uint16_t soil_moisture_range[2] = { 1385, 2885 };
uint16_t uv_range[2] = { 0, 3462 };
uint16_t upload_count_max = 30;  //upload to database every 30 records

DataLogger dataLogger(upload_count_max);
ConnectionManager connectionManager;
DataManager dataManager;

// Log timer
hw_timer_t* log_timer = NULL;
uint32_t log_timer_period = 60 * 1000000;  // triger log timer (1MHz) every 60 seconds
volatile SemaphoreHandle_t logTimerSemaphore;
portMUX_TYPE logTimerMux = portMUX_INITIALIZER_UNLOCKED;



//////////////////////////////////////
// AUX FUNCTIONS
//////////////////////////////////////

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

//////////////////////////////////////
// MAIN FNCTIONS
//////////////////////////////////////

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);
  digitalWrite(RGB_BUILTIN, HIGH);
  configTime(0, 0, ntpServer);

  // preferences.begin("network_prefs", false);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Log timer setup
  logTimerSemaphore = xSemaphoreCreateBinary();
  log_timer = timerBegin(1000000);  // 1MHz timer
  timerAttachInterrupt(log_timer, &onLogTimer);
  timerAlarm(log_timer, log_timer_period, true, 0);

  // Connect to wifi if possible
  bool conn_check = connectionManager.haveWifiConfig();
  if (conn_check) {
    uint8_t wifi_error = connectionManager.connectToWifi();

  } else {
    // !!! CHANGE WHEN APP IS READY
    connectionManager.setWifi(SSID, PASSWORD);
    uint8_t wifi_error = connectionManager.connectToWifi();
  }
  //dataManager.setFirebase(FIREBASE_URL, API_KEY);
  uint8_t firebase_error = dataManager.connectToFirebase();
  Serial.println(firebase_error);

  Serial.println("Wifi status:");
  Serial.println(connectionManager.getConnectionStatus());
  Serial.println("Firebase status: ");
  Serial.println(dataManager.getFirebaseStatus());

  //bool check = dataManager.uploadIntData("test/int", 5);
  //Serial.println(check);

  // Test data upload
  // int test_data[2] = {2, 3};
  // dataManager.uploadData(2, 3);
}

void loop() {
  uint16_t soil_lvl = analogRead(MOISTURE_PIN);
  uint16_t uv_lvl = analogRead(UV_PIN);
  timestamp = getTime();

  // update soil target if needed
  if (dataManager.checkForUpdates()) {
    soil_target = dataManager.getSoilTarget();
  }

  // Get levels as percentages
  soil_lvl = ((soil_lvl - soil_moisture_range[0]) / (soil_moisture_range[1] - soil_moisture_range[0])) * 100;
  uv_lvl = ((uv_lvl - uv_range[0]) / (uv_range[1] - uv_range[0])) * 100;

  // Log timer triggered
  if (xSemaphoreTake(logTimerSemaphore, 0) == pdTRUE) {
    portENTER_CRITICAL(&logTimerMux);
    portEXIT_CRITICAL(&logTimerMux);

    bool max_reached;
    uint8_t soil_avg, uv_avg;

    // Update the average sensor readings
    std::tie(max_reached, soil_avg, uv_avg) = dataLogger.logData(soil_lvl, uv_lvl);

    dataManager.uploadLiveData(soil_avg, uv_avg);

    // Upload the average readings
    if (max_reached) {
      dataManager.uploadData(soil_avg, uv_avg, String(timestamp));
      Serial.println("Upload to server");
    }
  }
}