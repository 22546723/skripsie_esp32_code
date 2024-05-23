#include <cstdint>
#include <Firebase_ESP_Client.h>
#include <Arduino.h>
#include <Preferences.h>
#include <tuple>

#define FIREBASE_URL "https://skripsie-1e23a-default-rtdb.firebaseio.com/"
#define API_KEY "AIzaSyDE2WyVTuhDcH06S6S2GaLgF9BJ6VFEsmY"
#define FIREBASE_PROJECT_ID "skripsie-1e23a"

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

class DataManager
{
    public:
        explicit DataManager();
        uint8_t connectToFirebase();
        void uploadData(int soil_data, int uv_data, String timestamp);
        bool getFirebaseStatus();
        bool checkForUpdates();
        int getSoilTarget();
        void uploadLiveData(int soil_data, int uv_data);
    protected:

    private:
      Preferences dat_preferences;
      FirebaseData fbdo;
      FirebaseAuth auth;
      FirebaseConfig config;
};

#endif // DATA_MANAGER_H