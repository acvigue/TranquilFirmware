// NTPClient - handle NTP server setting
// Rob Dobson 2018

#pragma once

class ConfigBase;

class NTPClient
{
private:
    ConfigBase* _pDefaultConfig;
    ConfigBase* _pConfig;
    String _configName;
    bool _ntpSetup;
    String _ntpServer;
    String _timezone;
    int _betweenNTPChecksSecs;
    static const uint32_t BETWEEN_NTP_CHECKS_SECS_DEFAULT = 3600;
    uint32_t _lastNTPCheckTime;
    
public:
    NTPClient();
    void setup(ConfigBase* pDefaultConfig, const char* configName, ConfigBase* pConfig);
    void service();
    void getConfig(String& config);
    void setConfig(const char *configJson);
    void setConfig(uint8_t *pData, int len);

private:
    void applySetup();
};
