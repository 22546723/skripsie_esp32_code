#include <cstdlib>
#include <string>
#include "HardwareSerial.h"
#include "ConnectionManager.h"
#include <Arduino.h>
#include <Preferences.h>
#include "WiFi.h"
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

/**
 * Callback that restarts the BLE advertising after an existing connection is disconnected
 */
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
    };

    void onDisconnect(BLEServer* pServer) {
      if (BLEDevice::getAdvertising() != NULL) {
        BLEDevice::startAdvertising();
      }
      
    }
};

/**
 * Class that manages the bluetooth and wifi connections
 */
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
void ConnectionManager::setupBluetooth() {
  // Initialize the bluetooth device and server
  con_preferences.begin("wifi_creds", false);
  String name = con_preferences.getString("name", "Plant Helper");
  con_preferences.end();

  BLEDevice::init(name);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  
  // Setup connected service
  pServiceConnect = pServer->createService(SERVICE_UUID_CONNECT);

  pCharacteristicDeviceName = pServiceConnect->createCharacteristic(
    CHARACTERISTIC_UUID_DEVICE,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicConnectedSsid = pServiceConnect->createCharacteristic(
    CHARACTERISTIC_UUID_SSID,
    BLECharacteristic::PROPERTY_READ);


  // Setup scan service
  pServiceScan = pServer->createService(SERVICE_UUID_SCAN);

  pCharacteristicScan = pServiceScan->createCharacteristic(
    CHARACTERISTIC_UUID_SCAN,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

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

    pCharacteristicWifiName2 = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_NAME_2,
    BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicWifiSet = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_SET,
    BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);

  pCharacteristicWifiPassword = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_PASSWORD,
    BLECharacteristic::PROPERTY_WRITE);  

  pCharacteristicWifiPassword2 = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_PASSWORD_2,
    BLECharacteristic::PROPERTY_WRITE);  

  pCharacteristicWifiConnected = pServiceSelect->createCharacteristic(
    CHARACTERISTIC_UUID_WIFI_CONNECTED,
    BLECharacteristic::PROPERTY_READ);   
  
  // Set the board name
  pCharacteristicDeviceName->setValue(name);
  pCharacteristicScanNew->setValue("0");
  pCharacteristicScan->setValue("0");
  pCharacteristicWifiSet->setValue("0");
  pCharacteristicConnectedSsid->setValue("404");


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

  if (scan == "1") {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

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
 * @return connection result: 0 - no credentials | 1 - successful connecion | 2 - timeout | 3 - network not found
 */
uint8_t ConnectionManager::connectToWifi() {
  con_preferences.begin("wifi_creds", false);
  String ssid = con_preferences.getString("ssid", "");
  String password = con_preferences.getString("password", "");
  con_preferences.end();


  // No ssid/password saved
  if (ssid == "" || password == "") {
    pCharacteristicConnectedSsid->setValue("No WiFi network selected");
    return 0;
  } else {
    

    int numNetworks = WiFi.scanNetworks();

    bool flag = false;
    uint8_t count = 0;

    // check if the saved network is available
    while (!flag && (count < numNetworks)) {
      if (WiFi.SSID(count) == ssid) {
        flag = true;
      }
      count++;
    }

    if (!flag) {
      pCharacteristicConnectedSsid->setValue("WiFi network not found");
      return 3;
    }


    // Try to connect to WiFi with 5 sec timeout
    WiFi.begin(ssid.c_str(), password.c_str());
    uint8_t counter = 0;
    while ((WiFi.status() != WL_CONNECTED) && (counter < 50)) {
      delay(100);
      counter += 1;
    }


    // Check if the connection was successful
    if (WiFi.status() == WL_CONNECTED) {
      pCharacteristicConnectedSsid->setValue(ssid);
      return 1;
    } else {
      WiFi.disconnect();
      pCharacteristicConnectedSsid->setValue("Connection unsuccessful");
      return 2;
    }
  }
}

/**
 * Read the ssid and password from BLE, save to preferences and connect to wifi
 *
 * @return true if conneted to wifi, false if not
 */
bool ConnectionManager::setupWifi() {
  bool gotInfo = false;
  String set = pCharacteristicWifiSet->getValue();

  if (set == "1") {
    // Read ssid and password from BTLE and set them in preferences
    String ssid = pCharacteristicWifiName->getValue() + pCharacteristicWifiName2->getValue();
    String password = pCharacteristicWifiPassword->getValue() + pCharacteristicWifiPassword2->getValue();
    con_preferences.begin("wifi_creds", false);
    con_preferences.putString("ssid", ssid);
    con_preferences.putString("password", password);
    con_preferences.end();

    // Connect to wifi
    int connected = connectToWifi();
    pCharacteristicWifiConnected->setValue(String(connected));

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
 * Update the device name from the BLE characteristic
 */
void ConnectionManager::updateName() {
  String name = pCharacteristicDeviceName->getValue();
  con_preferences.begin("wifi_creds", false);
  con_preferences.putString("name", name);
  con_preferences.end();
}


/**
 * Clear the saved wifi preferences
 */
void ConnectionManager::clearWifi() {
  con_preferences.begin("wifi_creds", false);
  con_preferences.clear();
  con_preferences.end();
}



