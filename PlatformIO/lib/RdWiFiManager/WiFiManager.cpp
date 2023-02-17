// WiFi Manager
// Rob Dobson 2018

#include "WiFiManager.h"
#include <ArduinoLog.h>
#include <WiFi.h>
#include "ConfigNVS.h"
#include "StatusIndicator.h"
#include <ESPmDNS.h>
#include "esp_wpa2.h"

static const char* MODULE_PREFIX = "WiFiManager: ";

StatusIndicator *WiFiManager::_pStatusLed = NULL;

String WiFiManager::_hostname;

bool WiFiManager::isEnabled()
{
    return _wifiEnabled;
}

String WiFiManager::getHostname()
{
    return _hostname;
}

void WiFiManager::setup(ConfigBase& hwConfig, ConfigBase *pSysConfig, 
            const char *defaultHostname, StatusIndicator *pStatusLed)
{
    _wifiEnabled = hwConfig.getLong("wifiEnabled", 0) != 0;
    _pConfigBase = pSysConfig;
    _defaultHostname = defaultHostname;
    _pStatusLed = pStatusLed;
    _connectionMode = pSysConfig->getLong("WiFiMode", 1);

    // Get the SSID, password and hostname if available
    _ssid = pSysConfig->getString("WiFiSSID", "");
    _password = pSysConfig->getString("WiFiPW", "");
    _peapIdentity = pSysConfig->getString("WiFiPEAPIdentity", "");
    _peapUsername = pSysConfig->getString("WiFiPEAPUsername", "");
    _peapPassword = pSysConfig->getString("WiFiPEAPPassword", "");

    _hostname = pSysConfig->getString("WiFiHostname", _defaultHostname.c_str());
    // Set an event handler for WiFi events
    if (_wifiEnabled)
    {
        WiFi.onEvent(wiFiEventHandler);
        // Set the mode to STA
        WiFi.mode(WIFI_STA);

        //If using PEAP, initialize.
        if(_connectionMode == connectionType::peap) {
            if(_peapIdentity.length() > 0) {
                esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)_peapIdentity.c_str(), strlen(_peapIdentity.c_str()));
            }
            if(_peapUsername.length() > 0) {
                esp_wifi_sta_wpa2_ent_set_username((uint8_t *)_peapUsername.c_str(), strlen(_peapUsername.c_str()));
            }
            if(_peapPassword.length() > 0) {
                esp_wifi_sta_wpa2_ent_set_password((uint8_t *)_peapPassword.c_str(), strlen(_peapPassword.c_str()));
            }
            esp_wpa2_config_t config = WPA2_CONFIG_INIT_DEFAULT();
            esp_wifi_sta_wpa2_ent_enable(&config);
        }
    }
}

void WiFiManager::service()
{
    // Check enabled
    if (!_wifiEnabled)
        return;

    // Check restart pending
    if (_deviceRestartPending)
    {
        if (Utils::isTimeout(millis(), _deviceRestartMs, DEVICE_RESTART_DELAY_MS))
        {
            _deviceRestartPending = false;
            ESP.restart();
        }
    }

    // Check for reconnect required
    if (WiFi.status() != WL_CONNECTED)
    {
        if (Utils::isTimeout(millis(), _lastWifiBeginAttemptMs, 
                    _wifiFirstBeginDone ? TIME_BETWEEN_WIFI_BEGIN_ATTEMPTS_MS : TIME_BEFORE_FIRST_BEGIN_MS))
        {
            Log.notice("%snotConn WiFi.begin SSID %s\n", MODULE_PREFIX, _ssid.c_str());
            if(_connectionMode == connectionType::psk) {
                WiFi.begin(_ssid.c_str(), _password.c_str());
            } else if(_connectionMode == connectionType::open) {
                WiFi.begin(_ssid.c_str());
            } else if(_connectionMode == connectionType::peap) {
                //EAP was initialized earlier!
                WiFi.begin(_ssid.c_str());
            }
            WiFi.setHostname(_hostname.c_str());
            _lastWifiBeginAttemptMs = millis();
            _wifiFirstBeginDone = true;
        }
    }
}

bool WiFiManager::isConnected()
{
    return (WiFi.status() == WL_CONNECTED);
}

String WiFiManager::formConfigStr()
{
    return "{\"WiFiMode\":" + String(_connectionMode) + ",\"WiFiSSID\":\"" + _ssid + "\",\"WiFiPW\":\"" + _password + "\",\"WiFiPEAPIdentity\":\"" + _peapIdentity + "\",\"WiFiPEAPUsername\":\"" + _peapUsername + "\",\"WiFiPEAPPassword\":\"" + _peapPassword + "\",\"WiFiHostname\":\"" + _hostname + "\"}";
}

void WiFiManager::setCredentialsPSK(String &ssid, String &pw, String &hostname, bool resetToImplement)
{
    // Set credentials
    _ssid = ssid;
    _password = pw;
    _connectionMode = connectionType::psk;
    if (hostname.length() == 0)
        Log.trace("%shostname not set, staying with %s\n", MODULE_PREFIX, _hostname.c_str());
    else
        _hostname = hostname;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }

    // Check if reset required
    if (resetToImplement)
    {
        Log.trace("%ssetCredentials ... Reset pending\n", MODULE_PREFIX);
        _deviceRestartPending = true;
        _deviceRestartMs = millis();
    }
}

void WiFiManager::setCredentialsPEAP(String &ssid, String &identity, String &username, String &password, String &hostname, bool resetToImplement)
{
    // Set credentials
    _ssid = ssid;
    _peapIdentity = identity;
    _peapUsername = username;
    _peapPassword = password;
    _connectionMode = connectionType::peap;
    if (hostname.length() == 0)
        Log.trace("%shostname not set, staying with %s\n", MODULE_PREFIX, _hostname.c_str());
    else
        _hostname = hostname;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }

    // Check if reset required
    if (resetToImplement)
    {
        Log.trace("%ssetCredentials ... Reset pending\n", MODULE_PREFIX);
        _deviceRestartPending = true;
        _deviceRestartMs = millis();
    }
}

void WiFiManager::setCredentialsOPEN(String &ssid, String &hostname, bool resetToImplement)
{
    // Set credentials
    _ssid = ssid;
    _connectionMode = connectionType::open;
    if (hostname.length() == 0)
        Log.trace("%shostname not set, staying with %s\n", MODULE_PREFIX, _hostname.c_str());
    else
        _hostname = hostname;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }

    // Check if reset required
    if (resetToImplement)
    {
        Log.trace("%ssetCredentials ... Reset pending\n", MODULE_PREFIX);
        _deviceRestartPending = true;
        _deviceRestartMs = millis();
    }

}

void WiFiManager::clearCredentials()
{
    _ssid = "";
    _password = "";
    _hostname = _defaultHostname;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }
}

void WiFiManager::wiFiEventHandler(WiFiEvent_t event)
{
    Log.trace("%sEvent %s\n", MODULE_PREFIX, getEventName(event));
    switch (event)
    {
    case SYSTEM_EVENT_STA_GOT_IP:
        Log.notice("%sGotIP %s\n", MODULE_PREFIX, WiFi.localIP().toString().c_str());
        if (_pStatusLed)
            _pStatusLed->setCode(1);
        //
        // Set up mDNS responder:
        // - first argument is the domain name, in this example
        //   the fully-qualified domain name is "esp8266.local"
        // - second argument is the IP address to advertise
        //   we send our IP address on the WiFi network
        if (MDNS.begin(_hostname.c_str()))
        {
            Log.notice("%smDNS responder started with hostname %s\n", MODULE_PREFIX, _hostname.c_str());
        }
        else
        {
            Log.notice("%smDNS responder failed to start (hostname %s)\n", MODULE_PREFIX, _hostname.c_str());
            break;
        }
        // Add service to MDNS-SD
        MDNS.addService("http", "tcp", 80);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        Log.notice("%sDisconnected\n", MODULE_PREFIX);
        WiFi.reconnect();
        if (_pStatusLed)
            _pStatusLed->setCode(0);
        break;
    default:
        // INFO: Default = do nothing
        // Log.notice("WiFiManager: unknown event %d\n", event);
        break;
    }
}

const char* WiFiManager::getEventName(WiFiEvent_t event)
{
    static const char* sysEventNames [] {
        "SYSTEM_EVENT_WIFI_READY",           
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
        "SYSTEM_EVENT_ETH_GOT_IP"
        };

    if (event < 0 || event > SYSTEM_EVENT_MAX)
    {
        return "UNKNOWN";
    }
    return sysEventNames[event];
}

