#include "ConnectionManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include "WiFi.h"

ConnectionManager::ConnectionManager(){
  //init funct
}

// Check for wifi connection
bool ConnectionManager::getConnectionStatus() {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
    } else {
      return false;
    }
}

// Set wifi credentials
void ConnectionManager::setWifi(char* wifi_ssid, char* wifi_password) {
  con_preferences.begin("wifi_creds", false);
  con_preferences.putString("ssid", wifi_ssid);
  con_preferences.putString("password", wifi_password);
  con_preferences.end();
}

// Connect to wifi. 0 - no credentials | 1 - successfull connecion | 2 - timeout
uint8_t ConnectionManager::connectToWifi() {
  con_preferences.begin("wifi_creds", false);
  String ssid = con_preferences.getString("ssid", "");
  String password = con_preferences.getString("password", "");
  con_preferences.end();

  // No ssid/password saved
  if (ssid==""||password=="") {
    return 0;
  } else {
    WiFi.begin(ssid.c_str(), password.c_str());
    uint8_t counter = 0;
    // Try to connect to WiFi
    while ((WiFi.status() != WL_CONNECTED) && (counter <= 10)) {
      delay(500);
    }

    if (WiFi.status() != WL_CONNECTED) {
      return 2;
    } else {
      return 1;
    }
  }
}

bool ConnectionManager::haveWifiConfig() {
  con_preferences.begin("wifi_creds", false);
  String ssid = con_preferences.getString("ssid", "");
  String password = con_preferences.getString("password", "");
  con_preferences.end();

  // No ssid/password saved
  if (ssid==""||password=="") {
    return false;
  } else {
    return true;
  }
  
}