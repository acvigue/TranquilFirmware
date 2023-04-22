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

    /*
    char private_key[45] = "qJjjnTEd7O71Pa5VAn0p1Ce0hV2sxTgosluXJNoeMkc=";  // [Interface] PrivateKey
    IPAddress *local_ip;                                   // [Interface] Address
    char public_key[45] = "a4fH7rM496TPH8gDYgEuO3FV9axheTl2ddy0jEiTw3I=";   // [Peer] PublicKey
    char psk_key[45] = "n+LfrLTdi9QxoOyGgIwAO9twwLDLCkQUdJX2Soxzoj0=";   // [Peer] PublicKey
    char endpoint_address[12] = "192.168.0.5";                            // [Peer] Endpoint
    int endpoint_port = 51820;                                            // [Peer] Endpoint
    */
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
