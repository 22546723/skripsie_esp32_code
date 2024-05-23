#include <tuple>
#include "DataLogger.h"
#include <Arduino.h>
#include <WiFi.h>
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
#define PUMP_T_MAX 1400   // us
#define PUMP_T_MIN 500    // us
#define PPM_PERIOD 20000  // us


////////////////////////////////////////////////////////
/**
* VARIABLES
*/
////////////////////////////////////////////////////////

// General
uint8_t soil_target;

// Timestamp
int timestamp;
const char* ntpServer = "pool.ntp.org";

// Sensors
uint16_t soil_moisture_range[2] = { 1362.0, 3047.0 };
uint16_t uv_range[2] = { 0.0, 3462.0 };

float soil_lvl = 0;
float uv_lvl = 0;


// Classes
DataLogger dataLogger = DataLogger();
ConnectionManager connectionManager = ConnectionManager();
DataManager dataManager = DataManager();

uint16_t upload_period = 30;  // min
uint16_t coreCounter = 0;

//Log timer
hw_timer_t* log_timer = NULL;
uint32_t log_timer_period = 60 * 1000000;  // triger log timer (1MHz) every 60 seconds
portMUX_TYPE logTimerMux = portMUX_INITIALIZER_UNLOCKED;

// Pump timer
hw_timer_t* pump_timer = NULL;
uint32_t pump_timer_period = 1;  // triger pump timer (1MHz) every 1us
portMUX_TYPE pumpTimerMux = portMUX_INITIALIZER_UNLOCKED;
float pump_speed = 0.6;  // % of full speed
bool pumpOn;
uint8_t ppm_state = 0;  // 0 - idle | 1 - pulse | 2 - fill

////////////////////////////////////////////////////////
/**
* AUX FUNCTIONS
*/
////////////////////////////////////////////////////////


/**
 * Log timer interrupt. Runs every minute.
 */
void IRAM_ATTR onLogTimer() {
  portENTER_CRITICAL_ISR(&logTimerMux);
  coreCounter = coreCounter + 1;
  portEXIT_CRITICAL_ISR(&logTimerMux);
}

/**
 * Pump timer interrupt. Runs every 1 us.
 *
 * Manages the PPM signal that controls the pump.
 */
void IRAM_ATTR onPumpTimer() {
  uint16_t pump_delay;
  uint8_t setTo;
  uint16_t pulse_len = (PUMP_T_MAX - (PUMP_T_MAX - PUMP_T_MIN) * pump_speed);

  portENTER_CRITICAL_ISR(&pumpTimerMux);

  if (ppm_state == 0) {  // idle
    setTo = LOW;
    ppm_state = 1;  //pulse
  }

  if (ppm_state == 1) {  // pulse
    setTo = HIGH;
    ppm_state = 2;  // set to fill
    pump_delay = pulse_len;
  } else if (ppm_state == 2) {  // fill
    setTo = LOW;
    ppm_state = 1;                        //set to pulse
    pump_delay = PPM_PERIOD - pulse_len;  // period - pulse
  }


  // restart timer alarm
  timerAlarm(pump_timer, pump_delay, true, 0);

  portEXIT_CRITICAL_ISR(&pumpTimerMux);

  if (pumpOn) {
    digitalWrite(PUMP_PIN, setTo);
  }
}


/**
 * Determine the current epoch time
 *
 * @return Current epoch time in seconds
 */
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;

  if (connectionManager.getConnectionStatus() && getLocalTime(&timeinfo)) {
    time(&now);
    return now;
  } else {
    return (timestamp + upload_period);
  }
}


/**
 * Attempt WiFi connection
 *
 * @see ConnectionManager::haveWifiConfig()
 * @see ConnectionManager::connectToWifi()
 */
void runWifi() {
  uint8_t wifi_error = 0;

  bool conn_check = connectionManager.haveWifiConfig();
  if (conn_check) {
    wifi_error = connectionManager.connectToWifi();
  }

  // Set wifi status LED
  if (wifi_error == 1) {
    digitalWrite(WIFI_PIN, HIGH);
  } else {
    digitalWrite(WIFI_PIN, LOW);
  }
}


/**
 * Attempt Firebase connection
 *
 * @see ConnectionManager::connectToFirebase()
 */
void runFirebase() {
  uint8_t firebase_error = dataManager.connectToFirebase();

  soil_target = dataManager.getSoilTarget();

  // Set firebase status LED
  if (firebase_error == 1) {
    digitalWrite(FIREBASE_PIN, HIGH);
  } else {
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
  digitalWrite(RGB_BUILTIN, HIGH);
  configTime(0, 0, ntpServer);


  // Get soil target
  soil_target = dataManager.getSoilTarget();


  // Set status LED pins
  pinMode(POWER_PIN, OUTPUT);     //power on
  pinMode(WIFI_PIN, OUTPUT);      //wifi connected
  pinMode(FIREBASE_PIN, OUTPUT);  //firebase connected


  // Set power status LED
  digitalWrite(POWER_PIN, HIGH);

  // ADC setup
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);


  // Pump setup
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);
  pumpOn = false;


  // Pump timer setup
  pump_timer = timerBegin(1000000);  // 1MHz timer
  timerAttachInterrupt(pump_timer, &onPumpTimer);
  timerAlarm(pump_timer, PPM_PERIOD, true, 0);

  // Bluetooth setup
  connectionManager.setupBluetooth();
  connectionManager.updateName();


  // WiFi setup
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();


  if (!connectionManager.getConnectionStatus()) {
    runWifi();
  }

  // Log timer setup
  log_timer = timerBegin(1000000);  // 1MHz timer
  timerAttachInterrupt(log_timer, &onLogTimer);
  timerAlarm(log_timer, log_timer_period, true, 0);
  coreCounter = 0;
}

////////////////////////////////////

void loop() {

  /////////
  // SENSOR DATA
  ////////

  // Read sensors
  float soil_lvl = analogRead(MOISTURE_PIN);
  float uv_lvl = analogRead(UV_PIN);

  // Get sensor readings as percentages and add to the log total
  soil_lvl = 100.0 - (((soil_lvl - soil_moisture_range[0]) / (soil_moisture_range[1] - soil_moisture_range[0])) * 100.0);
  uv_lvl = ((uv_lvl - uv_range[0]) / (uv_range[1] - uv_range[0])) * 100;

  dataLogger.logData(soil_lvl, uv_lvl);


  /////////
  // PUMP
  /////////
  if ((soil_lvl <= 0.9 * soil_target)) {
    pumpOn = true;
  }

  if ((soil_lvl >= 1.1 * soil_target) && (pumpOn)) {
    pumpOn = false;
    digitalWrite(PUMP_PIN, LOW); 
  }


  /////////
  // BLUETOOTH
  ////////
  connectionManager.sendWifiNetworks();
  connectionManager.setupWifi();
  connectionManager.updateName();


  /////////
  // FIREBASE
  ////////

  // Connect to firebase if not already connected
  if ((!dataManager.getFirebaseStatus()) && connectionManager.getConnectionStatus()) {
    runFirebase();
  }

  if (dataManager.getFirebaseStatus() && connectionManager.getConnectionStatus()) {

    // update soil target if needed
    if (dataManager.checkForUpdates()) {
      soil_target = dataManager.getSoilTarget();
    }

    // Upload live readings
    dataManager.uploadLiveData(soil_lvl, uv_lvl);

    // Upload the average readings every 30 min
    if (coreCounter >= upload_period) {

      // The upload can take a while if the wifi is slow, so the pump is stopped to prevent over-watering.
      pumpOn = false;
      digitalWrite(PUMP_PIN, LOW);

      timestamp = getTime();

      uint8_t soil_avg, uv_avg;
      std::tie(soil_avg, uv_avg) = dataLogger.getData();

      dataManager.uploadData(soil_avg, uv_avg, String(timestamp));

      coreCounter = 0;
    }
  } else {
    if (coreCounter >= upload_period) {
      dataLogger.resetLogger();
      coreCounter = 0;
    }
  }
}