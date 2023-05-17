// NTPClient - handle NTP server setting
// Rob Dobson 2018

#include <Arduino.h>
#include <ArduinoLog.h>
#include <NTPClient.h>
#include <WiFi.h>

#include "ConfigBase.h"
#include "RdJson.h"
#include "Utils.h"
#include "time.h"

static const char* MODULE_PREFIX = "NTPClient: ";

NTPClient::NTPClient() {
    _pDefaultConfig = NULL;
    _pConfig = NULL;
    _ntpSetup = false;
    _lastNTPCheckTime = 0;
    _betweenNTPChecksSecs = BETWEEN_NTP_CHECKS_SECS_DEFAULT;
}

void NTPClient::setup(ConfigBase* pDefaultConfig, const char* configName, ConfigBase* pConfig) {
    // Save config and register callback on config changed
    _pDefaultConfig = pDefaultConfig;
    _configName = configName;
    _pConfig = pConfig;

    applySetup();
}

void NTPClient::service() {
    if (Utils::isTimeout(millis(), _lastNTPCheckTime, _ntpSetup ? _betweenNTPChecksSecs * 1000 : 1000)) {
        _lastNTPCheckTime = millis();
        if (!_ntpSetup && _ntpServer.length() > 0) {
            if (WiFi.status() == WL_CONNECTED) {
                configTzTime(_timezone.c_str(), _ntpServer.c_str());
                _ntpSetup = true;
                Log.notice("%sservice - configTime set\n", MODULE_PREFIX);
            }
        }
    }
}

void NTPClient::applySetup() {
    if (!_pConfig) return;
    ConfigBase defaultConfig;
    if (_pDefaultConfig) defaultConfig.setConfigData(_pDefaultConfig->getString(_configName.c_str(), "").c_str());
    Log.trace("%ssetup name %s configStr %s\n", MODULE_PREFIX, _configName.c_str(), _pConfig->getConfigCStrPtr());
    // Extract server, etc
    _ntpServer = _pConfig->getString("ntpServer", defaultConfig.getString("ntpServer", "").c_str());
    _timezone = _pConfig->getString("ntpTimezone", defaultConfig.getString("ntpTimezone", "").c_str());
    _lastNTPCheckTime = 0;
    _ntpSetup = false;
    Log.notice("%ssetup server %s, timezone %s\n", MODULE_PREFIX, _ntpServer.c_str(), _timezone.c_str());
}

void NTPClient::setConfig(const char *configJson) {
    // Save config
    if (_pConfig) {
        _pConfig->setConfigData(configJson);
        _pConfig->writeConfig();
        Log.trace("%setConfig %s\n", MODULE_PREFIX, _pConfig->getConfigCStrPtr());
    }

    applySetup();
}

void NTPClient::setConfig(uint8_t *pData, int len) {
    if (!_pConfig) return;
    if (len >= _pConfig->getMaxLen()) return;
    char *pTmp = new char[len + 1];
    if (!pTmp) return;
    memcpy(pTmp, pData, len);
    pTmp[len] = 0;

    setConfig(pTmp);
    delete[] pTmp;
}

void NTPClient::getConfig(String& config) {
    config = "{}";
    if (_pConfig) config = _pConfig->getConfigString();
}
