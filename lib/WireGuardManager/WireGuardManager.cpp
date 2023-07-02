// WireGuard Manager
// Aiden Vigue 2023

#include "WireGuardManager.h"

#include <ArduinoLog.h>
#include <WiFi.h>
#include <WireGuard-ESP32.h>

#include "ConfigNVS.h"

static const char *MODULE_PREFIX = "WireGuardManager: ";

bool WireGuardManager::isConnected() { return _wireGuard.is_initialized(); }

void WireGuardManager::setup(ConfigBase *pSysConfig) {
    _pConfigBase = pSysConfig;
    applyConfig();
}

void WireGuardManager::service() {
    // Check if we're enabled
    if (!_wireGuardEnabled) {
        return;
    }

    if (!_wireGuard.is_initialized()) {
        if (WiFi.status() == WL_CONNECTED) {
            struct tm timeinfo;
            if (!_wireGuard.is_initialized() && getLocalTime(&timeinfo, 0)) {
                uint8_t ip[4];
                sscanf(_wireGuardLocalIP.c_str(), "%u.%u.%u.%u", &ip[0], &ip[1], &ip[2], &ip[3]);
                IPAddress local_ip = IPAddress(ip[0], ip[1], ip[2], ip[3]);

                if (!_wireGuard.begin(local_ip, _wireGuardPrivateKey.c_str(), _wireGuardEndpointAddress.c_str(), _wireGuardPublicKey.c_str(),
                              _wireGuardEndpointPort, _wireGuardPSK.c_str())) {
                    Log.notice("%sinit failed!\n", MODULE_PREFIX);
                } else {
                    Log.notice("%sinit success!\n", MODULE_PREFIX);
                }
            }
        }
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (_wireGuard.is_initialized()) {
            _wireGuard.end();
        }
    }
}

String WireGuardManager::formConfigStr() {
    return "{\"enabled\":" + String(_wireGuardEnabled) + ",\"privateKey\":\"" + _wireGuardPrivateKey + "\",\"publicKey\":\"" +
           _wireGuardPublicKey + "\",\"psk\":\"" + _wireGuardPSK + "\",\"localIP\":\"" + _wireGuardLocalIP + "\",\"endpointAddress\":\"" +
           _wireGuardEndpointAddress + "\",\"endpointPort\":" + String(_wireGuardEndpointPort) + ",\"connected\":" + String(_wireGuard.is_initialized()) + "}";
}

void WireGuardManager::setConfig(const char *configJson) {
    // Save config
    if (_pConfigBase) {
        _pConfigBase->setConfigData(configJson);
        _pConfigBase->writeConfig();
    }

    applyConfig();
}

void WireGuardManager::setConfig(uint8_t *pData, int len) {
    if (!_pConfigBase) return;
    if (len >= _pConfigBase->getMaxLen()) return;
    char *pTmp = new char[len + 1];
    if (!pTmp) return;
    memcpy(pTmp, pData, len);
    pTmp[len] = 0;
    // Make sure string is terminated
    setConfig(pTmp);
    delete[] pTmp;
}

void WireGuardManager::getConfig(String &config) {
    config = "{}";
    if (_pConfigBase) {
        config = _pConfigBase->getConfigString();
    }
}

void WireGuardManager::applyConfig() {
    _wireGuardEnabled = _pConfigBase->getLong("enabled", 0);

    _wireGuardPrivateKey = _pConfigBase->getString("privateKey", "");
    _wireGuardPublicKey = _pConfigBase->getString("publicKey", "");
    _wireGuardPSK = _pConfigBase->getString("psk", "");

    _wireGuardLocalIP = _pConfigBase->getString("localIP", "");
    _wireGuardEndpointAddress = _pConfigBase->getString("endpointAddress", "");
    _wireGuardEndpointPort = _pConfigBase->getLong("endpointPort", 51820);

    if (_wireGuard.is_initialized()) {
        _wireGuard.end();
    }
}