// WiFi Manager
// Rob Dobson 2018

#define NO_OTA_PORT

#include "WiFiManager.h"

#include <ArduinoLog.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <WiFi.h>

#include "ConfigNVS.h"
#include "esp_wpa2.h"
#include "Utils.h"

static const char *MODULE_PREFIX = "WiFiManager: ";

String WiFiManager::_hostname;

bool WiFiManager::isEnabled() { return _wifiEnabled; }

String WiFiManager::getHostname() { return _hostname; }

void WiFiManager::setup(ConfigBase &hwConfig, ConfigBase *pSysConfig, const char *defaultHostname) {
    _wifiEnabled = hwConfig.getLong("wifiEnabled", 0) != 0;
    _pConfigBase = pSysConfig;
    _defaultHostname = defaultHostname;
    _connectionMode = pSysConfig->getLong("WiFiMode", 3);

    // Get the SSID, password and hostname if available
    _ssid = pSysConfig->getString("WiFiSSID", "");
    _password = pSysConfig->getString("WiFiPW", "");
    _peapIdentity = pSysConfig->getString("WiFiPEAPIdentity", "");
    _peapUsername = pSysConfig->getString("WiFiPEAPUsername", "");
    _peapPassword = pSysConfig->getString("WiFiPEAPPassword", "");

    _hostname = pSysConfig->getString("WiFiHostname", _defaultHostname.c_str());
    // Set an event handler for WiFi events
    if (_wifiEnabled) {
        WiFi.onEvent(wiFiEventHandler);
        // Set the mode to STA
        WiFi.mode(WIFI_STA);

        //Erase pre-existing credentials
        WiFi.disconnect(false, true);
    }
}

void WiFiManager::service() {
    // Check enabled
    if (!_wifiEnabled) return;

    // Check restart pending
    if (_deviceRestartPending) {
        if (Utils::isTimeout(millis(), _deviceRestartMs, DEVICE_RESTART_DELAY_MS)) {
            _deviceRestartPending = false;
            ESP.restart();
        }
    }

    if(_softAPStarted) {
        _dnsServer.processNextRequest();
    }

    // Check for reconnect required
    if (WiFi.status() != WL_CONNECTED) {
        if (Utils::isTimeout(millis(), _lastWifiBeginAttemptMs,
                             _wifiFirstBeginDone ? TIME_BETWEEN_WIFI_BEGIN_ATTEMPTS_MS : TIME_BEFORE_FIRST_BEGIN_MS)) {
            Log.notice("%snotConn WiFi.begin SSID %s attempt %d\n", MODULE_PREFIX, _ssid.c_str(), _connectAttempts);
            _connectAttempts++;

            if(_connectAttempts > 2) {
                //Force the softAP to start whenever we can't connect for over 2 minutes
                _connectionMode = connectionType::none;
            }

            if(_connectionMode != connectionType::none && _softAPStarted) {
                Log.notice("%sstopping softap (%s)\n", MODULE_PREFIX, _hostname.c_str());
                WiFi.mode(WIFI_STA);
                _dnsServer.stop();
            }

            if (_connectionMode == connectionType::psk) {
                WiFi.begin(_ssid.c_str(), _password.c_str());
            } else if (_connectionMode == connectionType::open) {
                WiFi.begin(_ssid.c_str());
            } else if (_connectionMode == connectionType::peap) {
                // EAP was initialized earlier!
                WiFi.begin(_ssid.c_str(), WPA2_AUTH_PEAP, _peapIdentity.c_str(), _peapUsername.c_str(), _peapPassword.c_str());
            } else if (_connectionMode == connectionType::none && !_softAPStarted) {
                //Start SoftAP
                Log.notice("%sstarting softap (%s)\n", MODULE_PREFIX, _hostname.c_str());
                WiFi.disconnect(false, true);
                WiFi.mode(WIFI_AP);
                WiFi.softAP(_hostname.c_str());
                WiFi.softAPConfig(_apIP, _apIP, IPAddress(255, 255, 255, 0));
                _dnsServer.start(53, "*", _apIP);
                Log.notice("%s softap ip: %s\n", MODULE_PREFIX, WiFi.softAPIP().toString());
                _softAPStarted = true;
            }
            
            WiFi.setHostname(_hostname.c_str());
            _lastWifiBeginAttemptMs = millis();
            _wifiFirstBeginDone = true;
        }
    } else {
        _connectAttempts = 0;
    }
}

bool WiFiManager::isConnected() { return (WiFi.status() == WL_CONNECTED || _softAPStarted); }

String WiFiManager::formConfigStr() {
    return "{\"WiFiMode\":" + String(_connectionMode) + ",\"WiFiSSID\":\"" + _ssid + "\",\"WiFiPW\":\"" + _password + "\",\"WiFiPEAPIdentity\":\"" +
           _peapIdentity + "\",\"WiFiPEAPUsername\":\"" + _peapUsername + "\",\"WiFiPEAPPassword\":\"" + _peapPassword + "\",\"WiFiHostname\":\"" +
           _hostname + "\"}";
}

void WiFiManager::setConfig(const char *configJson) {
    // Save config
    if (_pConfigBase) {
        _pConfigBase->setConfigData(configJson);
        _pConfigBase->writeConfig();
    }

    _deviceRestartPending = true;
}

void WiFiManager::setConfig(uint8_t *pData, int len) {
    if (!_pConfigBase) return;
    if (len >= _pConfigBase->getMaxLen()) return;
    char *pTmp = new char[len + 1];
    if (!pTmp) return;
    memcpy(pTmp, pData, len);
    pTmp[len] = 0;

    setConfig(pTmp);
    delete[] pTmp;
}

void WiFiManager::getConfig(String& config) {
    config = "{}";
    if (_pConfigBase) config = _pConfigBase->getConfigString();
}

void WiFiManager::wiFiEventHandler(WiFiEvent_t event) {
    switch (event) {
        case SYSTEM_EVENT_STA_GOT_IP:
            Log.notice("%sGotIP %s\n", MODULE_PREFIX, WiFi.localIP().toString().c_str());
            //
            // Set up mDNS responder:
            // - first argument is the domain name, in this example
            //   the fully-qualified domain name is "esp8266.local"
            // - second argument is the IP address to advertise
            //   we send our IP address on the WiFi network
            if (MDNS.begin(_hostname.c_str())) {
                Log.notice("%smDNS responder started with hostname %s\n", MODULE_PREFIX, _hostname.c_str());
            } else {
                Log.notice("%smDNS responder failed to start (hostname %s)\n", MODULE_PREFIX, _hostname.c_str());
                break;
            }
            // Add service to MDNS-SD
            MDNS.addService("http", "tcp", 80);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            Log.notice("%sDisconnected\n", MODULE_PREFIX);
            WiFi.reconnect();
            break;
        default:
            // INFO: Default = do nothing
            // Log.notice("WiFiManager: unknown event %d\n", event);
            break;
    }
}

const char *WiFiManager::getEventName(WiFiEvent_t event) {
    static const char *sysEventNames[]{"SYSTEM_EVENT_WIFI_READY",
                                       "SYSTEM_EVENT_SCAN_DONE",
                                       "SYSTEM_EVENT_STA_START",
                                       "SYSTEM_EVENT_STA_STOP",
                                       "SYSTEM_EVENT_STA_CONNECTED",
                                       "SYSTEM_EVENT_STA_DISCONNECTED",
                                       "SYSTEM_EVENT_STA_AUTHMODE_CHANGE",
                                       "SYSTEM_EVENT_STA_GOT_IP",
                                       "SYSTEM_EVENT_STA_LOST_IP",
                                       "SYSTEM_EVENT_STA_WPS_ER_SUCCESS",
                                       "SYSTEM_EVENT_STA_WPS_ER_FAILED",
                                       "SYSTEM_EVENT_STA_WPS_ER_TIMEOUT",
                                       "SYSTEM_EVENT_STA_WPS_ER_PIN",
                                       "SYSTEM_EVENT_AP_START",
                                       "SYSTEM_EVENT_AP_STOP",
                                       "SYSTEM_EVENT_AP_STACONNECTED",
                                       "SYSTEM_EVENT_AP_STADISCONNECTED",
                                       "SYSTEM_EVENT_AP_STAIPASSIGNED",
                                       "SYSTEM_EVENT_AP_PROBEREQRECVED",
                                       "SYSTEM_EVENT_GOT_IP6",
                                       "SYSTEM_EVENT_ETH_START",
                                       "SYSTEM_EVENT_ETH_STOP",
                                       "SYSTEM_EVENT_ETH_CONNECTED",
                                       "SYSTEM_EVENT_ETH_DISCONNECTED",
                                       "SYSTEM_EVENT_ETH_GOT_IP"};

    if ((int)event < 0 || (int)event > (int)SYSTEM_EVENT_MAX) {
        return "UNKNOWN";
    }
    return sysEventNames[event];
}
