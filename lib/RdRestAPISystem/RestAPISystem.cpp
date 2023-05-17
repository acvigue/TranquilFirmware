// REST API for System, WiFi, etc
// Rob Dobson 2012-2018

#include "RestAPISystem.h"

#include "RestAPIEndpoints.h"

static const char *MODULE_PREFIX = "RestAPISystem: ";

String RestAPISystem::_systemVersion;

RestAPISystem::RestAPISystem(WiFiManager &wifiManager, WireGuardManager &wireGuardManager, RdOTAUpdate &otaUpdate,
                              FileManager &fileManager, NTPClient &ntpClient, ConfigBase &hwConfig,
                             const char *systemType, const char *systemVersion)
    : _wifiManager(wifiManager),
      _wireGuardManager(wireGuardManager),
      _otaUpdate(otaUpdate),
      _fileManager(fileManager),
      _ntpClient(ntpClient),
      _hwConfig(hwConfig) {
    _deviceRestartPending = false;
    _deviceRestartMs = 0;
    _updateCheckPending = false;
    _updateCheckMs = 0;
    _systemType = systemType;
    _systemVersion = systemVersion;
}

void RestAPISystem::setup(RestAPIEndpoints &endpoints) {
    endpoints.addEndpoint("settings/wifi", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiGetWiFiConfig, this, std::placeholders::_1, std::placeholders::_2),
                          "Setup WiFi SSID/password/hostname (PSK)");
    endpoints.addEndpoint("settings/wifi", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                          std::bind(&RestAPISystem::apiPostWiFiConfig, this, std::placeholders::_1, std::placeholders::_2), "Set NTP configuration",
                          "application/json", NULL, true, NULL,
                          std::bind(&RestAPISystem::apiPostWiFiConfigBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                    std::placeholders::_4, std::placeholders::_5));

    endpoints.addEndpoint("reset", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiReset, this, std::placeholders::_1, std::placeholders::_2), "Restart program");
    endpoints.addEndpoint("update", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiCheckUpdate, this, std::placeholders::_1, std::placeholders::_2), "Check for updates");

    // NTP settings
    endpoints.addEndpoint("settings/ntp", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiGetNTPConfig, this, std::placeholders::_1, std::placeholders::_2), "Get ntp configuration");
    endpoints.addEndpoint("settings/ntp", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                          std::bind(&RestAPISystem::apiPostNTPConfig, this, std::placeholders::_1, std::placeholders::_2), "Set NTP configuration",
                          "application/json", NULL, true, NULL,
                          std::bind(&RestAPISystem::apiPostNTPConfigBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                    std::placeholders::_4, std::placeholders::_5));

    endpoints.addEndpoint("fs/format", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiReformatFS, this, std::placeholders::_1, std::placeholders::_2),
                          "Reformat file system e.g. /spiffs");
    endpoints.addEndpoint("fs/delete", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiDeleteFile, this, std::placeholders::_1, std::placeholders::_2),
                          "Delete file e.g. /spiffs/filename ... ~ for / in filename");
    endpoints.addEndpoint("fs/upload", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                          std::bind(&RestAPISystem::apiUploadToFileManComplete, this, std::placeholders::_1, std::placeholders::_2), "Upload file",
                          "application/json", NULL, true, NULL, NULL,
                          std::bind(&RestAPISystem::apiUploadToFileManPart, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                    std::placeholders::_4, std::placeholders::_5, std::placeholders::_6, std::placeholders::_7));

    // Wireguard settings
    endpoints.addEndpoint("settings/wireguard", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiGetWireGuardConfig, this, std::placeholders::_1, std::placeholders::_2),
                          "Get WireGuard configuration");
    endpoints.addEndpoint("settings/wireguard", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_POST,
                          std::bind(&RestAPISystem::apiPostWireGuardConfig, this, std::placeholders::_1, std::placeholders::_2), "Set WireGuard configuration",
                          "application/json", NULL, true, NULL,
                          std::bind(&RestAPISystem::apiPostWireGuardConfigBody, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                                    std::placeholders::_4, std::placeholders::_5));

    // ***EXPUNGED*** settings
    endpoints.addEndpoint("settings/***EXPUNGED***", RestAPIEndpointDef::ENDPOINT_CALLBACK, RestAPIEndpointDef::ENDPOINT_GET,
                          std::bind(&RestAPISystem::apiGet***EXPUNGED***Config, this, std::placeholders::_1, std::placeholders::_2),
                          "Get ***EXPUNGED*** configuration");
}

String RestAPISystem::getWifiStatusStr() {
    if (WiFi.status() == WL_CONNECTED) return "C";
    if (WiFi.status() == WL_NO_SHIELD) return "A";
    if (WiFi.status() == WL_IDLE_STATUS) return "I";
    if (WiFi.status() == WL_NO_SSID_AVAIL) return "N";
    if (WiFi.status() == WL_SCAN_COMPLETED) return "S";
    if (WiFi.status() == WL_CONNECT_FAILED) return "F";
    if (WiFi.status() == WL_CONNECTION_LOST) return "L";
    return "D";
}

int RestAPISystem::reportHealth(int bitPosStart, unsigned long *pOutHash, String *pOutStr) {
    // Generate hash if required
    if (pOutHash) {
        unsigned long hashVal = (WiFi.status() == WL_CONNECTED);
        hashVal = hashVal << bitPosStart;
        *pOutHash += hashVal;
        *pOutHash ^= WiFi.localIP();
    }
    // Generate JSON string if needed
    if (pOutStr) {
        byte mac[6];
        WiFi.macAddress(mac);
        String macStr = String(mac[0], HEX) + ":" + String(mac[1], HEX) + ":" + String(mac[2], HEX) + ":" + String(mac[3], HEX) + ":" +
                        String(mac[4], HEX) + ":" + String(mac[5], HEX);
        String sOut = "\"wifiIP\":\"" + WiFi.localIP().toString() + "\",\"wifiConn\":\"" + getWifiStatusStr() + "\"";
        sOut += ",\"ssid\":\"" + WiFi.SSID() + "\",\"MAC\":\"" + macStr + "\",\"RSSI\":" + String(WiFi.RSSI());
        sOut += ",\"espV\":\"" + _systemVersion + " (built " + __DATE__ + " " + __TIME__ + ")\"";
        *pOutStr = sOut;
    }
    // Return number of bits in hash
    return 8;
}

void RestAPISystem::service() {
    // Check restart pending
    if (_deviceRestartPending) {
        if (Utils::isTimeout(millis(), _deviceRestartMs, DEVICE_RESTART_DELAY_MS)) {
            _deviceRestartPending = false;
            ESP.restart();
        }
    }
    // Check for update pending
    if (_updateCheckPending) {
        if (Utils::isTimeout(millis(), _updateCheckMs, DEVICE_UPDATE_DELAY_MS)) {
            _updateCheckPending = false;
            Log.notice("%sservice start update check\n", MODULE_PREFIX);
            _otaUpdate.requestUpdateCheck();
        }
    }
}

void RestAPISystem::apiReset(String &reqStr, String &respStr) {
    // Register that a restart is required but don't restart immediately
    // as the acknowledgement would not get through
    Utils::setJsonBoolResult(respStr, true);

    // Restart the device
    _deviceRestartPending = true;
    _deviceRestartMs = millis();
}

//MARK: WiFi
void RestAPISystem::apiGetWiFiConfig(String &reqStr, String &respStr) {
    // Get config
    String configStr;
    _wifiManager.getConfig(configStr);
    configStr = "\"wifi\":" + configStr;
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiPostWiFiConfig(String &reqStr, String &respStr) {
    Log.notice("%sPostWiFi %s\n", MODULE_PREFIX, reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiPostWiFiConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total) {
    Log.notice("%sPostWiFiBody len %d\n", MODULE_PREFIX, len);
    // Store the settings
    _wifiManager.setConfig(pData, len);
}

//MARK: Wireguard
void RestAPISystem::apiGetWireGuardConfig(String &reqStr, String &respStr) {
    // Get config
    String configStr;
    _wireGuardManager.getConfig(configStr);
    configStr = "\"wireGuard\":" + configStr;
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiPostWireGuardConfig(String &reqStr, String &respStr) {
    Log.notice("%sPostWireGuard %s\n", MODULE_PREFIX, reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiPostWireGuardConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total) {
    Log.notice("%sPostWireGuardBody len %d\n", MODULE_PREFIX, len);
    // Store the settings
    _wireGuardManager.setConfig(pData, len);
}

//MARK: NTP
void RestAPISystem::apiGetNTPConfig(String &reqStr, String &respStr) {
    // Get NTP config
    String configStr;
    _ntpClient.getConfig(configStr);
    configStr = "\"ntp\":" + configStr;
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiPostNTPConfig(String &reqStr, String &respStr) {
    Log.notice("%sPostNTPConfig %s\n", MODULE_PREFIX, reqStr.c_str());
    // Result
    Utils::setJsonBoolResult(respStr, true);
}

void RestAPISystem::apiPostNTPConfigBody(String &reqStr, uint8_t *pData, size_t len, size_t index, size_t total) {
    Log.notice("%sPostNTPConfigBody len %d\n", MODULE_PREFIX, len);
    // Store the settings
    _ntpClient.setConfig(pData, len);
}

//MARK: ***EXPUNGED***
void RestAPISystem::apiGet***EXPUNGED***Config(String &reqStr, String &respStr) {
    // Get config
    String configStr;
    ConfigBase wcConfig;
    wcConfig.setConfigData(_hwConfig.getString("***EXPUNGED***", "").c_str());
    configStr = "\"***EXPUNGED***\":{\"email\":\"" + wcConfig.getString("email", "") + "\",\"password\":\"" + wcConfig.getString("password", "") +
                "\",\"sisbot_id\":\"" + wcConfig.getString("sisbot_id", "") + "\",\"sisbot_mac\":\"" + wcConfig.getString("sisbot_mac", "") + "\"}";
    Utils::setJsonBoolResult(respStr, true, configStr.c_str());
}

void RestAPISystem::apiCheckUpdate(String &reqStr, String &respStr) {
    // Register that an update check is required but don't start immediately
    // as the TCP stack doesn't seem to connect if the server is the same
    Utils::setJsonBoolResult(respStr, true);

    // Check for updates on update server
    _updateCheckPending = true;
    _updateCheckMs = millis();
}

void RestAPISystem::apiGetVersion(String &reqStr, String &respStr) {
    respStr = "{\"sysType\":\"" + _systemType + "\", \"version\":\"" + _systemVersion + "\"}";
}

// Format file system
void RestAPISystem::apiReformatFS(String &reqStr, String &respStr) {
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    _fileManager.reformat(fileSystemStr, respStr);
}

// List files on a file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs, can be blank to default)
// The second part of the path is the folder - note that / must be replaced with ~ in folder
void RestAPISystem::apiFileList(String &reqStr, String &respStr) {
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Folder
    String folderStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    folderStr.replace("~", "/");
    if (folderStr.length() == 0) folderStr = "/";
    _fileManager.getFilesJSON(fileSystemStr, folderStr, respStr);
}

// Read file contents
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the folder and filename - note that / must be replaced with ~ in folder
void RestAPISystem::apiFileRead(String &reqStr, String &respStr) {
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String fileNameStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    fileNameStr.replace("~", "/");
    respStr = _fileManager.getFileContents(fileSystemStr, fileNameStr);
}

// Delete file on the file system
// Uses FileManager.h
// In the reqStr the first part of the path is the file system name (e.g. sd or spiffs)
// The second part of the path is the filename - note that / must be replaced with ~ in filename
void RestAPISystem::apiDeleteFile(String &reqStr, String &respStr) {
    // File system
    String fileSystemStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 1);
    // Filename
    String filenameStr = RestAPIEndpoints::getNthArgStr(reqStr.c_str(), 2);
    bool rslt = false;
    filenameStr.replace("~", "/");
    if (filenameStr.length() != 0) rslt = _fileManager.deleteFile(fileSystemStr, filenameStr);
    Utils::setJsonBoolResult(respStr, rslt);
    Log.trace("%sdeleteFile fs %s, filename %s rslt %s\n", MODULE_PREFIX, fileSystemStr.c_str(), filenameStr.c_str(), rslt ? "ok" : "fail");
}

// Upload file to file system - completed
void RestAPISystem::apiUploadToFileManComplete(String &reqStr, String &respStr) {
    Log.trace("%sapiUploadToFileManComplete %s\n", MODULE_PREFIX, reqStr.c_str());
    _fileManager.uploadAPIBlocksComplete();
    Utils::setJsonBoolResult(respStr, true);
}

// Upload file to file system - part of file (from HTTP POST file)
void RestAPISystem::apiUploadToFileManPart(String &req, String &filename, size_t contentLen, size_t index, uint8_t *data, size_t len,
                                           bool finalBlock) {
    Log.verbose("%sapiUpToFileMan %d, %d, %d, %d\n", MODULE_PREFIX, contentLen, index, len, finalBlock);
    if (contentLen > 0) _fileManager.uploadAPIBlockHandler("", req, filename, contentLen, index, data, len, finalBlock);
}
