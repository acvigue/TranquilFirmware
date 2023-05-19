// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#pragma once
#include <Arduino.h>
#include <WiFiManager.h>

#include "ConfigNVS.h"
#include "FileManager.h"
#include "NTPClient.h"
#include "RdOTAUpdate.h"
#include "RestAPIEndpoints.h"
#include "WireGuardManager.h"

class RestAPISystem {
   private:
    bool _deviceRestartPending;
    unsigned long _deviceRestartMs;
    static const int DEVICE_RESTART_DELAY_MS = 1000;
    bool _updateCheckPending;
    unsigned long _updateCheckMs;
    uint8_t _tmpReqBodyBuf[600];

    // Delay before starting an update check
    // For some reason TCP connect fails if there is not a sufficient delay
    // But only when initiated from MQTT (web works ok)
    // 3s doesn't work, 5s seems ok
    static const int DEVICE_UPDATE_DELAY_MS = 7000;
    WiFiManager &_wifiManager;
    WireGuardManager &_wireGuardManager;
    RdOTAUpdate &_otaUpdate;
    FileManager &_fileManager;
    NTPClient &_ntpClient;
    ConfigBase &_hwConfig;
    ConfigBase &_***EXPUNGED***Config;
    String _systemType;
    static String _systemVersion;

   public:
    RestAPISystem(WiFiManager &wifiManager, WireGuardManager &wireGuardManager, RdOTAUpdate &otaUpdate, FileManager &fileManager,
                  NTPClient &ntpClient, ConfigBase &hwConfig, ConfigBase &***EXPUNGED***Config, const char *systemType, const char *systemVersion);

    // Setup and status
    void setup(RestAPIEndpoints &endpoints);
    static String getWifiStatusStr();
    static int reportHealth(int bitPosStart, unsigned long *pOutHash, String *pOutStr);

    // Call frequently
    void service();

    // Reset machine
    void apiReset(String &reqStr, String &respStr);

    // WiFi Settings
    void apiGetWiFiConfig(String &reqStr, String &respStr);
    void apiPostWiFiConfig(String &reqStr, String &respStr);
    void apiPostWiFiConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

    // Log settings
    void apiGetLogConfig(String &reqStr, String &respStr);
    void apiPostLogConfig(String &reqStr, String &respStr);
    void apiPostLogConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

    // Wireguard settings
    void apiGetWireGuardConfig(String &reqStr, String &respStr);
    void apiPostWireGuardConfig(String &reqStr, String &respStr);
    void apiPostWireGuardConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

    // ***EXPUNGED*** settings (readonly!)
    void apiGet***EXPUNGED***Config(String &reqStr, String &respStr);
    void apiPost***EXPUNGED***Config(String &reqStr, String &respStr);
    void apiPost***EXPUNGED***ConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

    // NTP settings
    void apiGetNTPConfig(String &reqStr, String &respStr);
    void apiPostNTPConfig(String &reqStr, String &respStr);
    void apiPostNTPConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total);

    // Check for OTA updates
    void apiCheckUpdate(String &reqStr, String &respStr);

    // Get system version
    void apiGetVersion(String &reqStr, String &respStr);

    // Format file system
    void apiReformatFS(String &reqStr, String &respStr);

    // List files on a file system
    // Uses FileManager.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs, can be blank to default)
    // The second part of the path is the folder - note that / must be replaced with ~ in folder
    void apiFileList(String &reqStr, String &respStr);

    // Read file contents
    // Uses FileManager.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
    // The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
    void apiFileRead(String &reqStr, String &respStr);

    // Delete file on the file system
    // Uses FileManager.h
    // In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
    // The second part of the path is the filename - note that / must be replaced with ~ in filename
    void apiDeleteFile(String &reqStr, String &respStr);

    // Upload file to file system - completed
    void apiUploadToFileManComplete(String &reqStr, String &respStr);

    // Upload file to file system - part of file (from HTTP POST file)
    void apiUploadToFileManPart(String &req, String &filename, size_t contentLen, size_t index, uint8_t *data, size_t len, bool finalBlock);
};
