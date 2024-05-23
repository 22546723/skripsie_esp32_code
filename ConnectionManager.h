#include <cstdint>
#include <Arduino.h>
#include <Preferences.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#define SERVICE_UUID_SCAN "e1f6fa49-170d-4629-bd77-ea170a0309dc"
#define CHARACTERISTIC_UUID_SCAN "27ce423b-bcc6-4fe7-aa98-0d502e79f15a"
#define CHARACTERISTIC_UUID_SCAN_NAME "9a61b953-5e81-45f7-b998-4c54d213e710"
#define CHARACTERISTIC_UUID_SCAN_NEW "ffeca8e2-4285-4949-9819-66364ef72d25"

#define SERVICE_UUID_WIFI_SELECT "c3313912-4f44-4c5d-abe4-6a99dbe5274f"
#define CHARACTERISTIC_UUID_WIFI_NAME "62a140cc-0bcf-49da-94e3-fba705938272"
#define CHARACTERISTIC_UUID_WIFI_NAME_2 "2b8787a9-0908-48e5-90ed-18208cf9c197"
#define CHARACTERISTIC_UUID_WIFI_PASSWORD "1603d3a3-7500-4e90-8310-6e7b12e7bfa4"
#define CHARACTERISTIC_UUID_WIFI_PASSWORD_2 "55799be4-bdcf-4b97-ac6f-783dc4de4757"
#define CHARACTERISTIC_UUID_WIFI_SET "1675752a-2a8e-48e8-99ef-ee90ff878d0f"
#define CHARACTERISTIC_UUID_WIFI_CONNECTED "792920e9-f1ff-4587-879a-8cefb09c18d2"

#define SERVICE_UUID_CONNECT "a676b1bc-61f9-4af6-8dae-28831bad2ec6"
#define CHARACTERISTIC_UUID_DEVICE "9fbf707f-c39a-418a-828c-6e291af51ac4"
#define CHARACTERISTIC_UUID_SSID "ee2a9fb2-7f5b-4b84-be0b-b54e4c7e2863"

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H


/**
 * Manage the WiFi and BLE connections
 */
class ConnectionManager {
public:
  explicit ConnectionManager();

  void setupBluetooth();
  void sendWifiNetworks();
  uint8_t connectToWifi();
  bool setupWifi();
  bool haveWifiConfig();
  bool getConnectionStatus();
  void updateName();
  void clearWifi();

protected:

private:
  Preferences con_preferences;
  BLEServer *pServer;

  BLEService *pServiceScan;
  BLECharacteristic *pCharacteristicScan;
  BLECharacteristic *pCharacteristicScanName;
  BLECharacteristic *pCharacteristicScanNew;

  BLEService *pServiceSelect;
  BLECharacteristic *pCharacteristicWifiSet;
  BLECharacteristic *pCharacteristicWifiName;
  BLECharacteristic *pCharacteristicWifiName2;
  BLECharacteristic *pCharacteristicWifiPassword;
  BLECharacteristic *pCharacteristicWifiPassword2;
  BLECharacteristic *pCharacteristicWifiConnected;

  BLEService *pServiceConnect;
  BLECharacteristic *pCharacteristicDeviceName;
  BLECharacteristic *pCharacteristicConnectedSsid;

};


#endif  // CONNECTION_MANAGER_H