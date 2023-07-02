// WireGuard Manager
// Aiden Vigue 2023

#pragma once

#include <Arduino.h>
#include <WireGuard-ESP32.h>

#include "WiFi.h"

class ConfigBase;

class WireGuardManager {
   private:
    bool _wireGuardEnabled;
    ConfigBase *_pConfigBase;

    // Wireguard
    WireGuard _wireGuard;
    String _wireGuardPrivateKey;
    String _wireGuardLocalIP;
    String _wireGuardPublicKey;
    String _wireGuardPSK;
    String _wireGuardEndpointAddress;
    int _wireGuardEndpointPort;

    void setConfig(const char *configJson);
    void applyConfig();

   public:
    WireGuardManager() {
        _wireGuardEnabled = false;
        _pConfigBase = NULL;
    }
    void setup(ConfigBase *pSysConfig);
    void service();
    bool isConnected();
    String formConfigStr();
    void setConfig(uint8_t *pData, int len);
    void getConfig(String &config);
};
