// WiFi Manager
// Rob Dobson 2018

#pragma once

#include <Arduino.h>
#include "WiFi.h"
#include <DNSServer.h>

class ConfigBase;

class WiFiManager
{
private:
    bool _wifiEnabled;
    String _ssid;
    int _connectionMode;
    int _connectAttempts;
    String _password;
    String _peapIdentity;
    String _peapUsername;
    String _peapPassword;
    static String _hostname;
    String _defaultHostname;
    unsigned long _lastWifiBeginAttemptMs;
    bool _wifiFirstBeginDone;
    bool _softAPStarted;
    IPAddress _apIP = IPAddress(8,8,4,4); // The default android DNS
    DNSServer _dnsServer;
    static constexpr unsigned long TIME_BETWEEN_WIFI_BEGIN_ATTEMPTS_MS = 60000;
    static constexpr unsigned long TIME_BEFORE_FIRST_BEGIN_MS = 2000;
    ConfigBase* _pConfigBase;
    // Reset
    bool _deviceRestartPending;
    bool _otaSetup;
    unsigned long _deviceRestartMs;
    static const int DEVICE_RESTART_DELAY_MS = 1000;
    void setConfig(const char *configJson);

public:
    WiFiManager()
    {
        _wifiEnabled = false;
        _lastWifiBeginAttemptMs = 0;
        _wifiFirstBeginDone = false;
        _pConfigBase = NULL;
        _deviceRestartPending = false;
        _deviceRestartMs = 0;
    }
    enum connectionType { open, psk, peap, none };
    bool isEnabled();
    String getHostname();
    void setup(ConfigBase& hwConfig, ConfigBase *pSysConfig, const char *defaultHostname);
    void service();
    bool isConnected();
    String formConfigStr();
    void getConfig(String& config);
    void setConfig(uint8_t *pData, int len);
    static void wiFiEventHandler(WiFiEvent_t event);
    static const char* getEventName(WiFiEvent_t event);
};
