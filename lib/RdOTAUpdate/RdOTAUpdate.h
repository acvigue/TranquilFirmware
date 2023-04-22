// Over-The-Air (OTA) Firmware Update
// Rob Dobson 2018

#pragma once


#include <ArduinoLog.h>
#include "Utils.h"
#include "ConfigBase.h"


#include <esp32FOTA.hpp>

class RdOTAUpdate
{
private:

    // Enabled
    bool _otaEnabled;

    bool _firmwareCheckRequired;

    unsigned long long _lastUpdateCheckMillis;

    // Update server details
    String _manifestURL;

    // Project name and current version
    String _projectName;
    String _currentVers;

    esp32FOTA _fota;

public:
    RdOTAUpdate();

    // Setup
    void setup(ConfigBase& config, const char *projectName, const char *currentVers);

    // Request update check - actually done in the service loop
    void requestUpdateCheck();

    // Check if update in progress
    bool isInProgress();

    // Call this frequently
    void service(bool isBusy);
};
