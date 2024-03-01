#include "DataManager.h"
#include <Firebase_ESP_Client.h>
#include <Arduino.h>
#include <Preferences.h>
#include <tuple>
//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// #define FIREBASE_URL "https://skripsie-1e23a-default-rtdb.firebaseio.com/"
// #define API_KEY ""

DataManager::DataManager() {
  //init funct
}

// Connect to firebase. 0 - no credentials | 1 - successfull connecion | 2 - incorrect credentials
uint8_t DataManager::connectToFirebase() {
  config.api_key = API_KEY;
  config.database_url = FIREBASE_URL;

  if (Firebase.signUp(&config, &auth, "", "")) {
    //config.token_status_callback = TokenStatusCallback;
    config.token_status_callback = tokenStatusCallback;
    fbdo.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
    fbdo.setResponseSize(2048);
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    if (Firebase.ready()) {
      return 1;
    } else {
      return 0;
    }
  } else {
    return 2;
  }
}

int DataManager::getSoilTarget() {
  dat_preferences.begin("f_base", false);
  int target = dat_preferences.getInt("soil_target", 0);  
  dat_preferences.end();
  return target;
}

// check for any setting updates
bool DataManager::checkForUpdates() {
  bool update = false;
  // Update soil moisture target & record number
  if (Firebase.RTDB.getBool(&fbdo, "/status/update")) {
    update = true;

    Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", "settings/esp32");
    FirebaseJson result;
    result.setJsonData(fbdo.payload());
    FirebaseJsonData data;

    result.get(data, "fields/soil_target/integerValue");
    uint8_t soil_target = data.to<int>();

    result.get(data, "fields/rec_no/integerValue");
    uint8_t rec_no = data.to<int>();

    dat_preferences.begin("f_base", false);
    dat_preferences.putInt("rec_no", rec_no);
    dat_preferences.putInt("soil_target", soil_target);
    dat_preferences.end();

    Firebase.RTDB.setBool(&fbdo, "/status/update", false);
  }
  return update;
}

bool DataManager::getFirebaseStatus() {
  return Firebase.ready();
}

void DataManager::uploadData(int soil_data, int uv_data, String timestamp) {
  dat_preferences.begin("f_base", false);
  int rec_no = dat_preferences.getInt("rec_no", 0);
  FirebaseJson record;
  String documentPath = "data/rec" + String(rec_no);

  record.set("fields/soil/integerValue", soil_data);
  record.set("fields/uv/integerValue", uv_data);
  record.set("fields/timestamp/stringValue", timestamp);

  if (Firebase.Firestore.createDocument(&fbdo, FIREBASE_PROJECT_ID, "", documentPath.c_str(), record.raw())) {
    Serial.println("uploaded");
    rec_no = rec_no + 1;
    dat_preferences.putInt("rec_no", rec_no);
  } else {
    Serial.println(fbdo.errorReason());
  }
  dat_preferences.end();
}

void DataManager::uploadLiveData(int soil_data, int uv_data) {
  Firebase.RTDB.setInt(&fbdo, "data/soil_moisture", soil_data);
  Firebase.RTDB.setInt(&fbdo, "data/uv_lvl", uv_data);
}
