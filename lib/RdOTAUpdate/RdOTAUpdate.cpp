// Over-The-Air (OTA) Firmware Update
// Rob Dobson 2018

#include <RdOTAUpdate.h>

#include <esp32fota.hpp>

static const char *MODULE_PREFIX = "OTAUpdate: ";

RdOTAUpdate::RdOTAUpdate() {
    _otaEnabled = false;
    _firmwareCheckRequired = false;
    _lastUpdateCheckMillis = 0;
}

void RdOTAUpdate::setup(ConfigBase &config, const char *projectName, const char *currentVers) {
    // Get config
    ConfigBase otaConfig(config.getString("OTAUpdate", "").c_str());
    _otaEnabled = otaConfig.getLong("enabled", 0) != 0;

    // Project name must match the file store naming
    _projectName = projectName;
    _currentVers = currentVers;

    // Update server
    _manifestURL = otaConfig.getString("manifestURL", "");

    auto cfg = _fota.getConfig();
    cfg.name = (char *) malloc(_projectName.length());
    cfg.manifest_url = (char *) malloc(_manifestURL.length());
    strcpy(cfg.name, _projectName.c_str());
    strcpy(cfg.manifest_url, _manifestURL.c_str());
    cfg.sem = SemverClass(_currentVers.c_str());
    cfg.check_sig = false;
    cfg.unsafe = true;
    cfg.use_device_id = true;
    _fota.setConfig(cfg);

    _fota.setProgressCb([](size_t progress, size_t size) {
        if (progress == size || progress == 0) Serial.println();
        Log.noticeln("%s.", MODULE_PREFIX);
    });

    _fota.setUpdateBeginFailCb([](int partition) {
        Log.noticeln("%sUpdate could not begin with %s partition", MODULE_PREFIX, partition == U_SPIFFS ? "spiffs" : "firmware");
    });

    _fota.setUpdateFinishedCb([](int partition, bool restart_after) {
        Log.noticeln("%sUpdate done with %s partition\n", MODULE_PREFIX, partition == U_SPIFFS ? "spiffs" : "firmware");
        if (restart_after) {
            ESP.restart();
        }
    });
}

void RdOTAUpdate::requestUpdateCheck() { _firmwareCheckRequired = true; }

void RdOTAUpdate::service(bool isBusy) {
    // Check if enabled
    if (!_otaEnabled) return;

    // Check for updates every 5 mins (if not running a pattern)
    if (Utils::isTimeout(millis(), _lastUpdateCheckMillis, 5 * 60 * 1000)) {
        _firmwareCheckRequired = true;
    }

    // Time to check for firmware?
    if (_firmwareCheckRequired && !isBusy) {
        _firmwareCheckRequired = false;
        _lastUpdateCheckMillis = millis();
        _fota.handle();
    }
}