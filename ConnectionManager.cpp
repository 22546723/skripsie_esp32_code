#include "HardwareSerial.h"
#include "ConnectionManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include "WiFi.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>



ConnectionManager::ConnectionManager() {
  //init funct
}

/**
 * Setup the bluetooth LE connection 
 *
 * Initialises the BTLE device, server, services, characteristics and advertising.
 *
 * @param board_name Name of the device that will be used to initialise the BTLE device. 
 *                   The device characteristic of the connected service will be set to this value
 */
void ConnectionManager::setupBluetooth(String board_name) {
  // Initialize the bluetooth device and server
  BLEDevice::init(board_name);
  pServer = BLEDevice::createServer();
  
  // Setup connected service
  pServiceConnect = pServer->createService(SERVICE_UUID_CONNECT);

  pCharacteristicDeviceName = pServiceConnect->createCharacteristic(
    CHARACTERISTIC_UUID_DEVICE,
    BLECharacteristic::PROPERTY_READ);


  // Setup scan service
  pServiceScan = pServer->createService(SERVICE_UUID_SCAN);

  pCharacteristicScan = pServiceScan->createCharacteristic(
    CHARACTERISTIC_UUID_SCAN,
    BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicScanName = pServiceScan->createCharacteristic(
    CHARACTERISTIC_UUID_SCAN_NAME,
    BLECharacteristic::PROPERTY_READ);

  pCharacteristicScanNew = pServiceScan->createCharacteristic(
    CHARACTERISTIC_UUID_SCAN_NEW,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE); 


  // Setup wifi select service
  pServiceSelect = pServer->createService(SERVICE_UUID_WIFI_SELECT);  

  pCharacteristicWifiName = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_NAME,
    BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicWifiSet = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_SET,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicWifiPassword = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_PASSWORD,
    BLECharacteristic::PROPERTY_WRITE);  

  pCharacteristicWifiConnected = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_CONNECTED,
    BLECharacteristic::PROPERTY_READ);   
  
  // Set the board name
  pCharacteristicDeviceName->setValue(board_name);
  pCharacteristicScanNew->setValue("0");
  pCharacteristicScan->setValue("0");
  pCharacteristicWifiSet->setValue("0");



  // Start the services
  pServiceScan->start();
  pServiceSelect->start();
  pServiceConnect->start();

  // Setup the advertising & attach services
  BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID_SCAN);
  pAdvertising->addServiceUUID(SERVICE_UUID_WIFI_SELECT);
  pAdvertising->addServiceUUID(SERVICE_UUID_CONNECT);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  
  BLEDevice::startAdvertising();
}

/**
 * Sends detected wifi networks using BTLE
 *
 * Scans for available networks and sends them one by one.
 */
void ConnectionManager::sendWifiNetworks() {
  String scan = pCharacteristicScan->getValue();
  // Serial.println("at send wifi");

  if (scan == "1") {
    // Scan available networks
    int numNetworks = WiFi.scanNetworks();
    int sendCount = 0;

    while (sendCount < numNetworks) {
      String scan_new = pCharacteristicScanNew->getValue();

      // new will be set to false by the app after it reads each name
      if (scan_new == "0") {
        pCharacteristicScanName->setValue(WiFi.SSID(sendCount));
        pCharacteristicScanNew->setValue("1");
        sendCount += 1;
      }
    }

    pCharacteristicScan->setValue("0");
  }

}


/**
 * Connect to wifi using the ssid and password saved in preferences
 *
 * @return connection result: 0 - no credentials | 1 - successful connecion | 2 - timeout
 */
uint8_t ConnectionManager::connectToWifi() {
  con_preferences.begin("wifi_creds", false);
  String ssid = con_preferences.getString("ssid", "");
  String password = con_preferences.getString("password", "");
  con_preferences.end();

  // No ssid/password saved
  if (ssid == "" || password == "") {
    return 0;
  } else {
    // Try to connect to WiFi with 5 sec timeout
    WiFi.begin(ssid.c_str(), password.c_str());
    uint8_t counter = 0;
    while ((WiFi.status() != WL_CONNECTED) && (counter < 10)) {
      delay(500);
      counter += 1;
    }

    // Check if the connection was successful
    if (WiFi.status() == WL_CONNECTED) {
      return 1;
    } else {
      return 2;
    }
  }
}

/**
 * Read the ssid and password from BTLE, save to preferences and connect to wifi
 *
 * @return true if conneted to wifi, false if not
 */
bool ConnectionManager::setupWifi() {
  bool gotInfo = false;
  String set = pCharacteristicWifiSet->getValue();

  if (set == "1") {
    // Read ssid and password from BTLE and set them in preferences
    String ssid = pCharacteristicWifiName->getValue();
    String password = pCharacteristicWifiPassword->getValue();
    con_preferences.begin("wifi_creds", false);
    con_preferences.putString("ssid", ssid);
    con_preferences.putString("password", password);
    con_preferences.end();

    // Connect to wifi
    int connected = connectToWifi();
    pCharacteristicWifiConnected->setValue(connected);

    Serial.println(connected);

    pCharacteristicWifiSet->setValue("0");

    if (connected == 1)
      gotInfo = true;
  }

  return gotInfo;
}


/**
 * Checks if there is a wifi ssid and password saved in preferences
 *
 * @return true if found, false if not
 */
bool ConnectionManager::haveWifiConfig() {
  con_preferences.begin("wifi_creds", false);
  String ssid = con_preferences.getString("ssid", "");
  String password = con_preferences.getString("password", "");
  con_preferences.end();

  // No ssid/password saved
  if (ssid == "" || password == "") {
    return false;
  } else {
    return true;
  }
}


/**
 * Checks the wifi connection
 *
 * @return true if device is connected to wifi, false if not
 */
bool ConnectionManager::getConnectionStatus() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  } else {
    return false;
  }
}


/**
 * Update the device name
 *
 * @param name device name
 */
void ConnectionManager::setName(String name) {
  pCharacteristicDeviceName->setValue(name);
}



///////////////////////////////////////////////////////////////



// /**
//  * Checks if device should scan for wifi networks
//  *
//  * @return true if device is connected to wifi, false if not
//  */
// bool ConnectionManager::checkBt() {
//   String scan_flag = pCharacteristicScan->getValue();
//   bool done = false;

//   if (scan_flag == "1") {
//     int n = WiFi.scanNetworks();

//     for (int i = 0; i < n; i++) {
//       pCharacteristicScan->setValue(WiFi.SSID(i));
//       delay(10);  //Give the app enough time to read the network name
//     }
//   }
// }

// // Set wifi credentials
// void ConnectionManager::setWifi(const char* wifi_ssid, const char* wifi_password) {
//   con_preferences.begin("wifi_creds", false);
//   con_preferences.putString("ssid", wifi_ssid);
//   con_preferences.putString("password", wifi_password);
//   con_preferences.end();
// }






// // Get wifi name and password via bluetooth
// bool ConnectionManager::getWifiInfo() {
//   String wifi_set = pCharacteristicSet->getValue();
//   bool done = false;

//   if (wifi_set == "1") {
//     // String name = pCharacteristicSelect->getValue();
//     // String password = pCharacteristicPassword->getValue();
//     setWifi(pCharacteristicSelect->getValue().c_str(), pCharacteristicPassword->getValue().c_str());
//     done = true;
//   }

//   return done;
// }

// // Connect to wifi. 0 - no credentials | 1 - successfull connecion | 2 - timeout
// uint8_t ConnectionManager::connectToWifi() {
//   con_preferences.begin("wifi_creds", false);
//   String ssid = con_preferences.getString("ssid", "");
//   String password = con_preferences.getString("password", "");
//   con_preferences.end();

//   // No ssid/password saved
//   if (ssid == "" || password == "") {
//     return 0;
//   } else {
//     WiFi.begin(ssid.c_str(), password.c_str());
//     uint8_t counter = 0;
//     // Try to connect to WiFi
//     while ((WiFi.status() != WL_CONNECTED) && (counter <= 10)) {
//       delay(500);
//     }

//     if (WiFi.status() != WL_CONNECTED) {
//       return 2;
//     } else {
//       return 1;
//     }
//   }
// }
