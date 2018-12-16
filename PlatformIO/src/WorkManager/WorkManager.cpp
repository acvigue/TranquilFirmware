// RBotFirmware
// Rob Dobson 2018

#include "WorkManager.h"
#include "ConfigBase.h"
#include "RobotMotion/RobotController.h"
#include "RestAPISystem.h"
#include "Evaluators/EvaluatorGCode.h"
#include "RobotConfigurations.h"

static const char* MODULE_PREFIX = "WorkManager: ";

WorkManager::WorkManager(ConfigBase& mainConfig,
            ConfigBase &robotConfig, 
            RobotController &robotController,
            RestAPISystem &restAPISystem,
            FileManager& fileManager) :
            _systemConfig(mainConfig),
            _robotConfig(robotConfig),
            _robotController(robotController),
            _restAPISystem(restAPISystem),
            _fileManager(fileManager),
            _evaluatorSequences(fileManager),
            _evaluatorFiles(fileManager),
            _evaluatorThetaRhoLine()
{
}

void WorkManager::queryStatus(String &respStr)
{
    String innerJsonStr;
    int hashUsedBits = 0;
    // System health
    String healthStrSystem;
    hashUsedBits += _restAPISystem.reportHealth(hashUsedBits, NULL, &healthStrSystem);
    if (innerJsonStr.length() > 0)
        innerJsonStr += ",";
    innerJsonStr += healthStrSystem;
    // Robot info
    RobotCommandArgs cmdArgs;
    _robotController.getCurStatus(cmdArgs);
    String healthStrRobot = cmdArgs.toJSON(false);
    if (innerJsonStr.length() > 0)
        innerJsonStr += ",";
    innerJsonStr += healthStrRobot;
    // System information
    respStr = "{" + innerJsonStr + "}";
}

bool WorkManager::canAcceptWorkItem()
{
    return !_workItemQueue.isFull();
}

bool WorkManager::queueIsEmpty()
{
    return _workItemQueue.isEmpty();
}

void WorkManager::getRobotConfig(String &respStr)
{
    respStr = _robotConfig.getConfigString();
}

bool WorkManager::setRobotConfig(const uint8_t *pData, int len)
{
    Log.trace("%ssetRobotConfig len %d\n", MODULE_PREFIX, len);
    // Check sensible length
    if (len + 10 > _robotConfig.getMaxLen())
        return false;
    char* pTmp = new char[len + 1];
    if (!pTmp)
        return false;
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

void WorkManager::processSingle(const char *pCmdStr, String &retStr)
{
    const char *okRslt = "{\"rslt\":\"ok\"}";
    retStr = "{\"rslt\":\"none\"}";

    // Check if this is an immediate command
    if (strcasecmp(pCmdStr, "pause") == 0)
    {
        _robotController.pause(true);
        retStr = okRslt;
    }
    else if (strcasecmp(pCmdStr, "resume") == 0)
    {
        _robotController.pause(false);
        retStr = okRslt;
    }
    else if (strcasecmp(pCmdStr, "stop") == 0)
    {
        _robotController.stop();
        _workItemQueue.clear();
        evaluatorsStop();
        retStr = okRslt;
    }
    else
    {
        // Send the line to the workflow manager
        if (strlen(pCmdStr) != 0)
        {
            bool rslt = _workItemQueue.add(pCmdStr);
            if (!rslt)
                retStr = "{\"rslt\":\"busy\"}";
            else
                retStr = okRslt;
        }
    }
    // Log.verbose("%sprocSingle rslt %s\n", MODULE_PREFIX, retStr.c_str());
}

void WorkManager::addWorkItem(WorkItem& workItem, String &retStr, int cmdIdx)
{
    // Handle the case of a single string
    if (strstr(workItem.getCString(), ";") == NULL)
    {
        return processSingle(workItem.getCString(), retStr);
    }

    // Handle multiple commands (semicolon delimited)
    //Log.trace("%s addWorkItem %s\n", MODULE_PREFIX, pCmdStr);
    const int MAX_TEMP_CMD_STR_LEN = 1000;
    const char *pCurStr = workItem.getCString();
    const char *pCurStrEnd = pCurStr;
    int curCmdIdx = 0;
    while (true)
    {
        // Find line end
        if ((*pCurStrEnd == ';') || (*pCurStrEnd == '\0'))
        {
            // Extract the line
            int stLen = pCurStrEnd - pCurStr;
            if ((stLen == 0) || (stLen > MAX_TEMP_CMD_STR_LEN))
                break;

            // Alloc
            char *pCurCmd = new char[stLen + 1];
            if (!pCurCmd)
                break;
            for (int i = 0; i < stLen; i++)
                pCurCmd[i] = *pCurStr++;
            pCurCmd[stLen] = 0;

            // process
            if (cmdIdx == -1 || cmdIdx == curCmdIdx)
            {
                //Log.trace("%ssingle %d %s\n", MODULE_PREFIX, stLen, pCurCmd);
                processSingle(pCurCmd, retStr);
            }
            delete[] pCurCmd;

            // Move on
            curCmdIdx++;
            if (*pCurStrEnd == '\0')
                break;
            pCurStr = pCurStrEnd + 1;
        }
        pCurStrEnd++;
    }
}

bool WorkManager::execWorkItem(WorkItem& workItem)
{
    // See if the command is a pattern generator
    bool handledOk = false;
    // See if it is a pattern evaluator
    handledOk = _evaluatorPatterns.execWorkItem(workItem, _fileManager);
    if (handledOk)
        return handledOk;
    // See if it is a theta-rho line
    if (_evaluatorThetaRhoLine.isValid(workItem))
    {
        handledOk = _evaluatorThetaRhoLine.execWorkItem(workItem);
        if (handledOk)
            return handledOk;
    }
    // See if it is a file to process
    if (_evaluatorFiles.isValid(workItem))
    {
        handledOk = _evaluatorFiles.execWorkItem(workItem);
        if (handledOk)
            return handledOk;
    }
    // See if it is a command sequencer
    if (_evaluatorSequences.isValid(workItem))
    {
        handledOk = _evaluatorSequences.execWorkItem(workItem);
        if (handledOk)
            return handledOk;
    }
    // Not handled
    return false;
}

void WorkManager::service()
{
    // Pump the workflow here
    // Check if the RobotController can accept more
    if (_robotController.canAcceptCommand())
    {
        // Get a new work item
        WorkItem workItem;
        bool rslt = _workItemQueue.get(workItem);
        if (rslt)
        {
            Log.verbose("%sgetWorkflow rlst=%d (waiting %d), %s\n", MODULE_PREFIX, rslt,
                    _workItemQueue.size(),
                    workItem.getString().c_str());

            // Check for extended commands
            rslt = execWorkItem(workItem);

            // Check for GCode
            if (!rslt)
                EvaluatorGCode::interpretGcode(workItem, &_robotController, true);
        }
    }

    // Service command extender (which pumps the state machines associated with extended commands)
    evaluatorsService();
}

void WorkManager::reconfigure()
{
    // Get the config data
    String configData = _robotConfig.getConfigString();

    // See if robotConfig is present
    String robotConfigStr = RdJson::getString("/robotConfig", "", configData.c_str());
    if (robotConfigStr.length() <= 0)
    {
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
    }

    // Init robot controller and workflow manager
    _robotController.init(robotConfigStr.c_str());
    _workItemQueue.init(robotConfigStr.c_str(), "workItemQueue");
    // Set config into evaluators
    evaluatorsSetConfig(robotConfigStr.c_str());
}

void WorkManager::handleStartupCommands()
{
    // Check for cmdsAtStart in the robot config
    String cmdsAtStart = RdJson::getString("/robotConfig/cmdsAtStart", "", _robotConfig.getConfigCStrPtr());
    Log.notice("%scmdsAtStart <%s>\n", MODULE_PREFIX, cmdsAtStart.c_str());
    if (cmdsAtStart.length() > 0)
    {
        String retStr;
        WorkItem workItem(cmdsAtStart);
        addWorkItem(workItem, retStr);
    }

    // Check for startup commands in the main config
    String runAtStart = RdJson::getString("startup", "", _robotConfig.getConfigCStrPtr());
    RdJson::unescapeString(runAtStart);
    Log.notice("%sstartup commands <%s>\n", MODULE_PREFIX, runAtStart.c_str());
    if (runAtStart.length() > 0)
    {
        String retStr;
        WorkItem workItem(runAtStart);
        addWorkItem(workItem, retStr);
    }
}

void WorkManager::evaluatorsStop()
{
    _evaluatorPatterns.stop();
    _evaluatorSequences.stop();
    _evaluatorFiles.stop();
    _evaluatorThetaRhoLine.stop();
}

void WorkManager::evaluatorsService()
{
    _evaluatorPatterns.service(this);
    _evaluatorFiles.service(this);
    _evaluatorThetaRhoLine.service(this);
    if (!evaluatorsBusy())
        _evaluatorSequences.service(this);
}

bool WorkManager::evaluatorsBusy()
{
    // Check if we're creating a pattern or handling a file, etc
    if (_evaluatorPatterns.isBusy())
        return true;
    if (_evaluatorThetaRhoLine.isBusy())
        return true;
    // Evaluator files must be after any other evaluators that might be in the process
    // of handling a line from a file already
    if (_evaluatorFiles.isBusy())
        return true;
    // Note that evaluatorSequences is not included here. That's because sequences operate
    // at a higher level than other evaluators and only gets services when the workitem
    // queue is completely empty and nothing else is busy
    return false;
}

void WorkManager::evaluatorsSetConfig(const char* configJson)
{
    _evaluatorPatterns.setConfig(configJson);
    _evaluatorSequences.setConfig(configJson);
    _evaluatorFiles.setConfig(configJson);
    _evaluatorThetaRhoLine.setConfig(configJson);
}
