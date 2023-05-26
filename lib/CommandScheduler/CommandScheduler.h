// CommandScheduler 
// Rob Dobson 2018
// Aiden Vigue 2023

#pragma once
#include <Arduino.h>
#include "Utils.h"
#include "ConfigBase.h"

class RestAPIEndpoints;

class CommandSchedulerJob
{
public:
    int _hour;
    int _min;
    uint8_t _dow;
    bool _executed;
    String _command;

public:
    CommandSchedulerJob()
    {
        _hour = 0;
        _min = 0;
        _dow = 0;
        _executed = false;
    }
    CommandSchedulerJob(const CommandSchedulerJob& other)
    {
        _hour = other._hour;
        _min = other._min;
        _dow = other._dow;
        _executed = other._executed;
        _command = other._command;
    }
    CommandSchedulerJob(int hour, int min, uint8_t dow, const char* command)
    {
        _hour = hour;
        _min = min;
        _command = command;
        _dow = dow;
        _executed = false;
    }
};

class CommandScheduler
{
private:
    // List of jobs
    std::vector<CommandSchedulerJob> _jobs;

    // Time checking
    uint32_t _lastCheckForJobsMs;
    static const int TIME_BETWEEN_CHECKS_MS = 15000;

    // Config
    ConfigBase* _pConfig;

    // Endpoints
    RestAPIEndpoints* _pEndpoints;

public:
    CommandScheduler();
    void setup(ConfigBase* pConfig,
                RestAPIEndpoints &endpoints);

    // Service 
    void service();
    void getConfig(String& config);
    void setConfig(const char *configJson);
    void setConfig(uint8_t *pData, int len);

private:
    void configChanged();
    void applySetup();
};
