// CommandScheduler
// Rob Dobson 2018

#include "CommandScheduler.h"

#include "RestAPIEndpoints.h"

// #define DEBUG_DISPLAY_TIME_CHECK 1

static const char* MODULE_PREFIX = "CommandScheduler: ";

CommandScheduler::CommandScheduler() {
    _pConfig = NULL;
    _lastCheckForJobsMs = 0;
    _pEndpoints = NULL;
}

void CommandScheduler::setup(ConfigBase* pConfig, RestAPIEndpoints& endpoints) {
    // Save config info
    _pEndpoints = &endpoints;
    _pConfig = pConfig;
    applySetup();
}

// Service
void CommandScheduler::service() {
    // Check for a command to schedule
    if (Utils::isTimeout(millis(), _lastCheckForJobsMs, TIME_BETWEEN_CHECKS_MS)) {
        _lastCheckForJobsMs = millis();
        // Get time
        struct tm timeinfo;
        if (!getLocalTime(&timeinfo, 0)) {
            Log.verbose("%sservice failed to get time\n", MODULE_PREFIX);
            return;
        }

        //Reset job execution states at midnight
        if(timeinfo.tm_hour == 0 && timeinfo.tm_min == 0) {
            for(int i = 0; i < _jobs.size(); i++) {
                Log.notice("%sresetting execution states", MODULE_PREFIX);
                _jobs[i]._executed = false;
            }
        }

        // Check each job
        for (int i = 0; i < _jobs.size(); i++) {
            if (_jobs[i]._executed) continue;

            if ((_jobs[i]._hour == timeinfo.tm_hour) && (_jobs[i]._min == timeinfo.tm_min) && (_jobs[i]._dow & (1<<timeinfo.tm_wday))) {
                Log.trace("%sservice performing job at hour %d min %d dow %d cmd %s\n", MODULE_PREFIX, _jobs[i]._hour, _jobs[i]._min, timeinfo.tm_wday,
                          _jobs[i]._command.c_str());
                _jobs[i]._executed = true;
                if (_pEndpoints) {
                    String retStr;
                    _pEndpoints->handleApiRequest(_jobs[i]._command.c_str(), retStr);
                }
            }
        }
    }
}

void CommandScheduler::configChanged() {
    // Reset config
    Log.trace("%sconfigChanged\n", MODULE_PREFIX);
    applySetup();
}

void CommandScheduler::applySetup() {
    if ((!_pConfig)) return;

    // Get config
    Log.notice("%sconfig %s\n", MODULE_PREFIX, _pConfig->getConfigCStrPtr());

    // Clear schedule
    _jobs.clear();

    // Extract jobs list
    String jobsListJson = _pConfig->getString("jobs", "[]");
    int numJobs = 0;
    if (RdJson::getType(numJobs, jobsListJson.c_str()) != JSMNR_ARRAY) return;

    // Iterate array
    for (int i = 0; i < numJobs; i++) {
        // Extract job details
        String jobJson = RdJson::getString(("[" + String(i) + "]").c_str(), "{}", jobsListJson.c_str());
        int hour = RdJson::getLong("hour", 0, jobJson.c_str());
        int min = RdJson::getLong("minute", 0, jobJson.c_str());
        uint8_t dow = RdJson::getLong("dow", 0, jobJson.c_str());
        ;
        String cmd = RdJson::getString("cmd", "", jobJson.c_str());
        CommandSchedulerJob newjob(hour, min, dow, cmd.c_str());

        // Add to list
        _jobs.push_back(newjob);
        Log.notice("%sapplySetup hour:%d min:%d dow:%d, cmd:%s\n", MODULE_PREFIX, hour, min, dow, cmd.c_str());
    }
}

void CommandScheduler::setConfig(const char *configJson) {
    // Save config
    if (_pConfig) {
        _pConfig->setConfigData(configJson);
        _pConfig->writeConfig();
    }

    applySetup();
}

void CommandScheduler::setConfig(uint8_t *pData, int len) {
    if (!_pConfig) return;
    if (len >= _pConfig->getMaxLen()) return;
    char *pTmp = new char[len + 1];
    if (!pTmp) return;
    memcpy(pTmp, pData, len);
    pTmp[len] = 0;

    setConfig(pTmp);
    delete[] pTmp;
}

void CommandScheduler::getConfig(String& config) {
    config = "{}";
    if (_pConfig) config = _pConfig->getConfigString();
}
