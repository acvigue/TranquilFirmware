// RBotFirmware
// Rob Dobson 2018

#include "WorkManager.h"

#include "ConfigBase.h"
#include "Evaluators/EvaluatorGCode.h"
#include "RestAPISystem.h"
#include "RobotConfigurations.h"
#include "RobotMotion/RobotController.h"

static const char *MODULE_PREFIX = "WorkManager: ";

WorkManager::WorkManager(ConfigBase &mainConfig, ConfigBase &robotConfig, RobotController &robotController, LedStrip &ledStrip, WireGuardManager &wireGuardManager,
                         RestAPISystem &restAPISystem, FileManager &fileManager)
    : _systemConfig(mainConfig),
      _robotConfig(robotConfig),
      _robotController(robotController),
      _ledStrip(ledStrip),
      _wireGuardManager(wireGuardManager),
      _restAPISystem(restAPISystem),
      _fileManager(fileManager),
      _evaluatorSequences(fileManager, *this),
      _evaluatorFiles(fileManager, *this),
      _evaluatorThetaRhoLine(*this) {
    _statusReportLastCheck = 0;
    _statusLastHashVal = 0;
#ifdef DEBUG_WORK_ITEM_SERVICE
    _debugLastWorkServiceMs = 0;
#endif
}

void WorkManager::queryStatus(String &respStr) {
    String innerJsonStr;
    int hashUsedBits = 0;
    // System health
    String healthStrSystem;
    hashUsedBits += _restAPISystem.reportHealth(hashUsedBits, NULL, &healthStrSystem);
    if ((innerJsonStr.length() > 0) && (healthStrSystem.length() > 0)) innerJsonStr += ",";
    innerJsonStr += healthStrSystem;
    // Robot info
    RobotCommandArgs cmdArgs;
    _robotController.getCurStatus(cmdArgs);
    String healthStrRobot = cmdArgs.toJSON(false);
    if ((innerJsonStr.length() > 0) && (healthStrRobot.length() > 0)) innerJsonStr += ",";
    innerJsonStr += healthStrRobot;

    // Time of Day
    String timeJsonStr;
    struct tm timeinfo;
    const int MAX_LOCAL_TIME_STR_LEN = 40;
    char localTimeString[MAX_LOCAL_TIME_STR_LEN];
    if (getLocalTime(&timeinfo, 0)) {
        strftime(localTimeString, MAX_LOCAL_TIME_STR_LEN, "%Y-%m-%d %H:%M:%S", &timeinfo);
        timeJsonStr += "\"tod\":\"";
        timeJsonStr += localTimeString;
        timeJsonStr += "\"";
    }
    if (timeJsonStr.length() > 0) {
        if (innerJsonStr.length() > 0) innerJsonStr += ",";
        innerJsonStr += timeJsonStr;
    }

    if (_evaluatorSequences.isBusy()) {
        innerJsonStr += ",\"playlist\": true, \"playlistName\": \"";
        innerJsonStr += _evaluatorSequences.fileName();
        innerJsonStr += "\",\"playlistIdx\": ";
        innerJsonStr += String(_evaluatorSequences.currentWorkItemIndex());
        innerJsonStr += ",\"repeatMode\":";
        innerJsonStr += (_evaluatorSequences.getRepeat() == true) ? "true" : "false";
        innerJsonStr += ",\"shuffleMode\":";
        innerJsonStr += (_evaluatorSequences.getShuffle() == true) ? "true" : "false";
    }

    innerJsonStr += ",\"wgConn\":";
    innerJsonStr += (_wireGuardManager.isConnected()) ? "true" : "false";

    if (_evaluatorFiles.isBusy()) {
        innerJsonStr += ",\"file\": \"";
        innerJsonStr += _evaluatorFiles.fileName();

        if (_evaluatorThetaRhoLine.isBusy()) {
            innerJsonStr += "\",\"filePos\": ";
            innerJsonStr += String((_evaluatorFiles.getCurrentFilePosition()) - ((1 - _evaluatorThetaRhoLine.getLineProgress()) * _evaluatorFiles.getCurrentLineLength()));
        } else {
            innerJsonStr += "\",\"filePos\": ";
            innerJsonStr += String(_evaluatorFiles.getCurrentFilePosition());
        }

        innerJsonStr += ",\"fileLen\": ";
        innerJsonStr += String(_evaluatorFiles.getTotalFileLength());
    }

    // System information
    respStr = "{" + innerJsonStr + "}";
}

bool WorkManager::canAcceptWorkItem() { return !_workItemQueue.isFull(); }

bool WorkManager::queueIsEmpty() { return _workItemQueue.isEmpty(); }

void WorkManager::getRobotConfig(String &respStr) { respStr = _robotConfig.getConfigString(); }

void WorkManager::getLedStripConfig(String &respStr) { respStr = _ledStrip.getCurrentConfigStr(); }

bool WorkManager::setLedStripConfig(const uint8_t *pData, int len) {
    char tmpBuf[len + 1];
    memcpy(tmpBuf, pData, len);
    tmpBuf[len] = 0;
    // Make sure string is terminated
    _ledStrip.updateLedFromConfig(tmpBuf);

    return true;
}

bool WorkManager::setRobotConfig(const uint8_t *pData, int len) {
    // Check sensible length
    if (len + 10 > _robotConfig.getMaxLen()) return false;
    char *pTmp = new char[len + 1];
    if (!pTmp) return false;
    memcpy(pTmp, pData, len);
    pTmp[len] = 0;
    // Make sure string is terminated
    _robotConfig.setConfigData(pTmp);
    delete[] pTmp;
    // Reconfigure the robot
    reconfigure();
    // Store the configuration permanently
    _robotConfig.writeConfig();
    return true;
}

void WorkManager::processSingle(const char *pCmdStr, String &retStr) {
    const char *okRslt = "{\"rslt\":\"ok\"}";
    retStr = "{\"rslt\":\"none\"}";

    // Check if this is an immediate command
    if (strcasecmp(pCmdStr, "pause") == 0) {
        _robotController.pause(true);
        retStr = okRslt;
    } else if (strcasecmp(pCmdStr, "sleep") == 0) {
        _robotController.pause(true);
        _ledStrip.setSleepMode(true);
        retStr = okRslt;
    } else if (strcasecmp(pCmdStr, "resume") == 0) {
        _robotController.pause(false);
        _ledStrip.setSleepMode(false);
        retStr = okRslt;
    } else if (strcasecmp(pCmdStr, "playpause") == 0) {
        // Toggle pause state
        _robotController.pause(!_robotController.isPaused());
        retStr = okRslt;
    } else if (strcasecmp(pCmdStr, "stop") == 0) {
        _robotController.stop();
        _workItemQueue.clear();
        evaluatorsStop();
        retStr = okRslt;
    } else if (strcasecmp(pCmdStr, "seq_next") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _robotController.stop();
            _evaluatorThetaRhoLine.stop();
            _evaluatorFiles.stop();
            _workItemQueue.clear();
            retStr = okRslt;
        }
    } else if (strcasecmp(pCmdStr, "seq_prev") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _robotController.stop();
            _evaluatorThetaRhoLine.stop();
            _evaluatorFiles.stop();
            _workItemQueue.clear();
            _evaluatorSequences.loadPrevious();
            retStr = okRslt;
        }
    } else if (strcasecmp(pCmdStr, "seq_shuffle_on") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _evaluatorSequences.setShuffle(true);
            retStr = okRslt;
        }
    } else if (strcasecmp(pCmdStr, "seq_shuffle_off") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _evaluatorSequences.setShuffle(false);
            retStr = okRslt;
        }
    } else if (strcasecmp(pCmdStr, "seq_repeat_on") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _evaluatorSequences.setRepeatMode(true);
            retStr = okRslt;
        }
    } else if (strcasecmp(pCmdStr, "seq_repeat_off") == 0) {
        if (_evaluatorSequences.isBusy()) {
            _evaluatorSequences.setRepeatMode(false);
            retStr = okRslt;
        }
    } else {
        // Send the line to the workflow manager
        if (strlen(pCmdStr) != 0) {
            bool rslt = _workItemQueue.add(pCmdStr);
            if (!rslt) {
                retStr = "{\"rslt\":\"busy\"}";
                Log.verbose("%sprocessSingle failed to add\n", MODULE_PREFIX);
            } else {
                retStr = okRslt;
            }
        }
    }
    // Log.verbose("%sprocSingle rslt %s\n", MODULE_PREFIX, retStr.c_str());
}

void WorkManager::addWorkItem(WorkItem &workItem, String &retStr, int cmdIdx) {
    // Handle the case of a single string
    if (strstr(workItem.getCString(), ";") == NULL) {
        return processSingle(workItem.getCString(), retStr);
    }

    // Handle multiple commands (semicolon delimited)
    const int MAX_TEMP_CMD_STR_LEN = 1000;
    const char *pCurStr = workItem.getCString();
    const char *pCurStrEnd = pCurStr;
    int curCmdIdx = 0;
    while (true) {
        // Find line end
        if ((*pCurStrEnd == ';') || (*pCurStrEnd == '\0')) {
            // Extract the line
            int stLen = pCurStrEnd - pCurStr;
            if ((stLen == 0) || (stLen > MAX_TEMP_CMD_STR_LEN)) break;

            // Alloc
            char *pCurCmd = new char[stLen + 1];
            if (!pCurCmd) break;
            for (int i = 0; i < stLen; i++) pCurCmd[i] = *pCurStr++;
            pCurCmd[stLen] = 0;

            // process
            if (cmdIdx == -1 || cmdIdx == curCmdIdx) {
                processSingle(pCurCmd, retStr);
            }
            delete[] pCurCmd;

            // Move on
            curCmdIdx++;
            if (*pCurStrEnd == '\0') break;
            pCurStr = pCurStrEnd + 1;
        }
        pCurStrEnd++;
    }
}

bool WorkManager::canBeProcessed(WorkItem &workItem) {
    // See if it is a theta-rho evaluator work item
    if (_evaluatorThetaRhoLine.isValid(workItem)) return !_evaluatorThetaRhoLine.isBusy();

    // See if it is a file to process
    if (_evaluatorFiles.isValid(workItem)) return !_evaluatorFiles.isBusy();

    // See if it is command sequence
    if (_evaluatorSequences.isValid(workItem)) return !_evaluatorSequences.isBusy();

    // Assume it is gcode
    return _robotController.canAcceptCommand();
}

bool WorkManager::execWorkItem(WorkItem &workItem) {
    // See if the command is a pattern generator
    bool handledOk = false;

    // See if it is a theta-rho line
    if (_evaluatorThetaRhoLine.isValid(workItem)) {
        handledOk = _evaluatorThetaRhoLine.execWorkItem(workItem);
        if (handledOk) return handledOk;
    }
    // See if it is a file to process
    if (_evaluatorFiles.isValid(workItem)) {
        handledOk = _evaluatorFiles.execWorkItem(workItem);
        if (handledOk) return handledOk;
    }
    // See if it is a command sequence
    if (_evaluatorSequences.isValid(workItem)) {
        handledOk = _evaluatorSequences.execWorkItem(workItem);
        if (handledOk) return handledOk;
    }
    // Not handled
    return false;
}

void WorkManager::service() {
    // Pump the workflow here
    // Check if the RobotController can accept more
    if (_robotController.canAcceptCommand()) {
        // Peek at next work item
        WorkItem workItem;
        bool rslt = _workItemQueue.peek(workItem);
        if (rslt) {
            // Check if this work item can be processed
            if (canBeProcessed(workItem)) {
                rslt = _workItemQueue.get(workItem);
                if (rslt) {
                    // Check for extended commands
                    rslt = execWorkItem(workItem);

                    // Check for GCode
                    if (!rslt) EvaluatorGCode::interpretGcode(workItem, &_robotController, true);
                }
            }
        }
    }

    // Service evaluators
    evaluatorsService();
}

void WorkManager::reconfigure() {
    // Get the config data
    String configData = _robotConfig.getConfigString();

    // See if robotConfig is present
    String robotConfigStr = RdJson::getString("/robotConfig", "", configData.c_str());
    if (robotConfigStr.length() <= 0) {
        Log.notice("%sNo robotConfig found - defaulting\n", MODULE_PREFIX);
        // See if there is a robotType specified in the config
        String robotType = RdJson::getString("/robotType", "", configData.c_str());
        if (robotType.length() <= 0)
            // If not see if there is a default robot type
            robotType = RdJson::getString("/defaultRobotType", "", _systemConfig.getConfigCStrPtr());
        if (robotType.length() <= 0)
            // Just use first type
            RobotConfigurations::getNthRobotTypeName(0, robotType);
        // Set the default robot type
        robotConfigStr = RobotConfigurations::getConfig(robotType.c_str());
        String robotConfig = "{\"robotConfig\":";
        robotConfig += robotConfigStr;
        robotConfig += ",\"name\":\"Tranquil\"}";
        _robotConfig.setConfigData(robotConfig.c_str());
        _robotConfig.writeConfig();
        esp_restart();
    }

    // Init robot controller and workflow manager
    _robotController.init(robotConfigStr.c_str());
    _workItemQueue.init(robotConfigStr.c_str(), "workItemQueue");
    // Set config into evaluators
    String robotAttributes;
    _robotController.getRobotAttributes(robotAttributes);
    evaluatorsSetConfig(robotConfigStr.c_str(), "evaluators", robotAttributes.c_str());
}

void WorkManager::handleStartupCommands() {
    // Check for cmdsAtStart in the robot config
    String cmdsAtStart = RdJson::getString("/robotConfig/cmdsAtStart", "", _robotConfig.getConfigCStrPtr());
    Log.notice("%scmdsAtStart <%s>\n", MODULE_PREFIX, cmdsAtStart.c_str());
    if (cmdsAtStart.length() > 0) {
        String retStr;
        WorkItem workItem(cmdsAtStart);
        addWorkItem(workItem, retStr);
    }

    // Check for startup commands in the main config
    String runAtStart = RdJson::getString("startup", "", _robotConfig.getConfigCStrPtr());
    RdJson::unescapeString(runAtStart);
    Log.notice("%sstartup commands <%s>\n", MODULE_PREFIX, runAtStart.c_str());
    if (runAtStart.length() > 0) {
        String retStr;
        WorkItem workItem(runAtStart);
        addWorkItem(workItem, retStr);
    }
}

void WorkManager::evaluatorsStop() {
    _evaluatorSequences.stop();
    _evaluatorFiles.stop();
    _evaluatorThetaRhoLine.stop();
}

void WorkManager::evaluatorsService() {
    _evaluatorThetaRhoLine.service();
    if (!evaluatorsBusy(false)) _evaluatorFiles.service();
    if (!evaluatorsBusy(true)) _evaluatorSequences.service();
}

bool WorkManager::evaluatorsBusy(bool includeFileEvaluator) {
    // Check if we're creating a pattern or handling a file, etc
    if (_evaluatorThetaRhoLine.isBusy()) return true;
    // Evaluator files must be after any other evaluators that might be in the process
    // of handling a line from a file already
    if (includeFileEvaluator)
        if (_evaluatorFiles.isBusy()) return true;
    // Note that evaluatorSequences is not included here. That's because sequences operate
    // at a higher level than other evaluators and only gets services when the workitem
    // queue is completely empty and nothing else is busy
    return false;
}

void WorkManager::evaluatorsSetConfig(const char *configJson, const char *jsonPath, const char *robotAttributes) {
    String evaluatorConfig = RdJson::getString(jsonPath, "{}", configJson);
    _evaluatorSequences.setConfig(evaluatorConfig.c_str());
    _evaluatorFiles.setConfig(evaluatorConfig.c_str());
    _evaluatorThetaRhoLine.setConfig(evaluatorConfig.c_str(), robotAttributes);
}

bool WorkManager::checkStatusChanged() {
    // Check for status change
    if (!Utils::isTimeout(millis(), _statusReportLastCheck, STATUS_CHECK_MS)) return false;
    _statusReportLastCheck = millis();

    // Check if always update timed out
    bool statusChanged = false;
    if (Utils::isTimeout(millis(), _statusAlwaysLastCheck, STATUS_ALWAYS_UPDATE_MS)) {
        _statusAlwaysLastCheck = millis();
        statusChanged = true;
    }

    // Check for system status changes
    unsigned long statusNewHash = 0;
    _restAPISystem.reportHealth(0, &statusNewHash, NULL);

    // Check for robot status changes
    RobotCommandArgs cmdArgs;
    _robotController.getCurStatus(cmdArgs);

    // Check if anything changed
    statusChanged |= (_statusLastHashVal != statusNewHash) | (_statusLastCmdArgs != cmdArgs);
    if (statusChanged) {
        _statusLastHashVal = statusNewHash;
        _statusLastCmdArgs = cmdArgs;
        Log.verbose("%sstatus changed %d %d\n", MODULE_PREFIX, (_statusLastHashVal != statusNewHash), (_statusLastCmdArgs != cmdArgs));
        return true;
    }
    return false;
}

String WorkManager::getDebugStr() {
    String returnStr = (_workItemQueue.isFull() ? " QFULL:" : " QOK:");
    returnStr += _workItemQueue.size();
    return returnStr;
}
