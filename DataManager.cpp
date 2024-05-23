#include "WString.h"
#include "DataManager.h"
#include <Firebase_ESP_Client.h>
#include <Arduino.h>
#include <Preferences.h>
#include <tuple>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"


DataManager::DataManager() {
  //init funct
}


/**
 * Connect to firebase.
 *
 * @return 0 - no credentials | 1 - successfull connecion | 2 - incorrect credentials
 */
uint8_t DataManager::connectToFirebase() {
  config.api_key = API_KEY;
  config.database_url = FIREBASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    config.token_status_callback = tokenStatusCallback;
    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
    fbdo.setResponseSize(2048);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Firebase.RTDB.setReadTimeout(&fbdo, 2000); // sets a 2 sec read timeout
    Firebase.RTDB.setwriteSizeLimit(&fbdo, "small"); // sets a 10 sec write timeout

    if (Firebase.ready()) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 2;
  }
}


/**
 * Return the soil moisture target
 *
 * @return soil moisture target
 */
int DataManager::getSoilTarget() {
  dat_preferences.begin("f_base", false);
  int target = dat_preferences.getInt("soil_target", 0);  
  dat_preferences.end();
  return target;
}


/**
 * Check for updated settings and set parameters to new values
 *
 * @return tue if settings were updated
 */
bool DataManager::checkForUpdates() {
  bool update = false;
  
  Firebase.RTDB.getBool(&fbdo, "/status/update");
  if (fbdo.to<bool>()) {
    update = true;

    // Read updated values
    Firebase.RTDB.getDouble(&fbdo, "/status/soil_target");
    double soil_target = fbdo.to<double>();

    // Save to preferences
    dat_preferences.begin("f_base", false);
    dat_preferences.putInt("soil_target", soil_target);
    dat_preferences.end();

    Firebase.RTDB.setBool(&fbdo, "/status/update", false);
  }
  return update;
}


/**
 * Check firebase connection
 *
 * @return tue if connected to firebase
 */
bool DataManager::getFirebaseStatus() {
  
  return Firebase.ready();
}


/**
 * Upload data to firestore
 *
 * @param soil_data avg soil moisture sensor reading
 * @param uv_data avg uv light sensor reading
 * @param timestamp epoch timestamp
 */
void DataManager::uploadData(int soil_data, int uv_data, String timestamp) {
  // Get record number
  dat_preferences.begin("f_base", false);
  int rec_no = dat_preferences.getInt("rec_no", 0);

  // Record setup
  FirebaseJson record;
  String documentPath = "data/rec" + String(rec_no);

  // Set record values
  record.set("fields/soil/integerValue", soil_data);
  record.set("fields/uv/integerValue", uv_data);
  record.set("fields/timestamp/stringValue", timestamp);

  // Upload data
  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), record.raw())) {
    rec_no = rec_no + 1;
    dat_preferences.putInt("rec_no", rec_no);  // Update record number
  } else {
    Serial.println(fbdo.errorReason());
  }
  dat_preferences.end();
}


/**
 * Upload data to realtime database
 *
 * @param soil_data soil moisture sensor reading
 * @param uv_data uv light sensor reading
 */
void DataManager::uploadLiveData(int soil_data, int uv_data) {
  // The update field is read to check if the esp can connect to firebase.
  // This is done because the get timeout is much shorter than the set timeout, 
  // causing a much smaller delay if Firebase can't be accessed
  if (Firebase.RTDB.getBool(&fbdo, "/status/update")) {
    Firebase.RTDB.setInt(&fbdo, "data/soil_moisture", soil_data);
    Firebase.RTDB.setInt(&fbdo, "data/uv_lvl", uv_data);
  }
}
