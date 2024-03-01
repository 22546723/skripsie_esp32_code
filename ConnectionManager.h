#include <cstdint>
#include <Arduino.h>
#include <Preferences.h>

#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

class ConnectionManager
{
    public:
        explicit ConnectionManager();
        bool getConnectionStatus();
        uint8_t connectToWifi();
        void setWifi(char* wifi_ssid, char* wifi_password);
        bool haveWifiConfig();

    protected:

    private:
      Preferences con_preferences;
};

#endif // CONNECTION_MANAGER_H