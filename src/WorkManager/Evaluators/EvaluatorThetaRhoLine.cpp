// RBotFirmware
// Rob Dobson 2018

#include <Arduino.h>
#include <ArduinoLog.h>
#include "EvaluatorThetaRhoLine.h"
#include "RdJson.h"
#include "Utils.h"
#include "../WorkManager.h"

// #define THETA_RHO_DEBUG 1

static const char *MODULE_PREFIX = "EvaluatorThetaRhoLine: ";

EvaluatorThetaRhoLine::EvaluatorThetaRhoLine(WorkManager& workManager) :
                            _workManager(workManager)
{
    _inProgress = false;
    _curStep = 0;
    _stepAngle = AxisUtils::r2d(DEFAULT_STEP_ANGLE);
    _stepAdaptation = true;
    _curTheta = 0;
    _curRho = 0;
    _continueFromPrevious = true;
    _prevTheta = 0;
    _prevRho = 0;
    _bedRadiusMM = 0;
    _centreOffsetX = 0;
    _centreOffsetY = 0;
    _isInterpolating = false;
}

void EvaluatorThetaRhoLine::setConfig(const char *configStr, const char* robotAttributes)
{
    // Set the theta-rho angle step
    _stepAngle = AxisUtils::d2r(RdJson::getDouble("thrStepDegs", AxisUtils::r2d(DEFAULT_STEP_ANGLE), configStr));
    _stepAdaptation = RdJson::getLong("thrStepAdaptation", 1, configStr) != 0;
    _continueFromPrevious = RdJson::getLong("thrContinue", 1, configStr) != 0;
    // Set the size of the max radius
    double sizeX = RdJson::getDouble("sizeX", 0, robotAttributes);
    double sizeY = RdJson::getDouble("sizeY", 0, robotAttributes);
    double originX = RdJson::getDouble("originX", 0, robotAttributes);
    double originY = RdJson::getDouble("originY", 0, robotAttributes);
    
    _thetaMirrored = RdJson::getLong("thrThetaMirrored", 1, configStr) != 0;
    _thetaOffsetAngle = RdJson::getLong("thrThetaOffsetAngle", 1, configStr);

    _bedRadiusMM = std::min(sizeX, sizeY) / 2;
    _centreOffsetX = sizeX / 2 - originX;
    _centreOffsetY = sizeY / 2 - originY;
}

// Is Busy
bool EvaluatorThetaRhoLine::isBusy()
{
    return _inProgress;
}

const char *EvaluatorThetaRhoLine::getConfig()
{
    return "";
}

// Check if valid
bool EvaluatorThetaRhoLine::isValid(WorkItem &workItem)
{
    // Check if theta-rho
    String cmdStr = workItem.getString();
    cmdStr.trim();
    return cmdStr.startsWith("_THRLINE");
}

double EvaluatorThetaRhoLine::getLineProgress() {
    if(!_isInterpolating) {
        return 0.50;
    }

    return (double) (1.00 / (double) _interpolateSteps) * (double) _curStep;
}

// Process WorkItem
bool EvaluatorThetaRhoLine::execWorkItem(WorkItem &workItem)
{
    // Extract the details
    String thetaStr = Utils::getNthField(workItem.getCString(), 1, '/');
    String rhoStr = Utils::getNthField(workItem.getCString(), 2, '/');
    double mirrored = _thetaMirrored ? -1.00 : 1.00;
    double newTheta = atof(thetaStr.c_str()) * mirrored + (M_PI * _thetaOffsetAngle);
    double newRho = atof(rhoStr.c_str());

    // Check for an uninterpolated line
    if (workItem.getString().startsWith("_THRLINE_"))
    {
        _isInterpolating = false;
        // Next iteration
        char lineBuf[100];
        // Calculate coords
        double x,y;
        calcXYPos(newTheta, newRho, x, y);
        sprintf(lineBuf, "G0 X%0.3f Y%0.3f", x, y);
        String retStr;
        WorkItem workItem(lineBuf);
        _workManager.addWorkItem(workItem, retStr);
        return true;
    }

    // Check for first line of interpolated file
    if (workItem.getString().startsWith("_THRLINE0_"))
    {
        if (_continueFromPrevious)
        {
            _thetaStartOffset = newTheta - _prevTheta;
        }
        else
        {
            _thetaStartOffset = 0;
        }
        _prevTheta = newTheta;
        _prevRho = newRho;
        _isInterpolating = false;
        return true;
    }

    // Must be a _THRLINEN_ then
    double deltaTheta = newTheta - _thetaStartOffset - _prevTheta;
    double absDeltaTheta = abs(deltaTheta);
    double adaptedStepAngle = _stepAngle;
    if (_stepAdaptation)
    {
        double avgRho = std::max(fabs(newRho), fabs(_prevRho));
        if (avgRho > 1)
            avgRho = 1;
        double maxStepAngle = _stepAngle * 16;
        if (maxStepAngle > M_PI / 2)
            maxStepAngle = M_PI / 2;
        double minStepAngle = _stepAngle / 4;
        if (avgRho > RHO_AT_DEFAULT_STEP_ANGLE)
        {
            adaptedStepAngle = ((avgRho - RHO_AT_DEFAULT_STEP_ANGLE) / (1 - RHO_AT_DEFAULT_STEP_ANGLE)) * 
                    (minStepAngle - _stepAngle) + _stepAngle;
        }
        else
        {
            adaptedStepAngle = (avgRho / RHO_AT_DEFAULT_STEP_ANGLE) * 
                    (_stepAngle - maxStepAngle) + maxStepAngle;
        }
    }
    _thetaInc = deltaTheta >= 0 ? adaptedStepAngle : -adaptedStepAngle;
    double deltaRho = newRho - _prevRho;
    if (absDeltaTheta < adaptedStepAngle)
    {
        _thetaInc = deltaTheta;
        _interpolateSteps = 1;
        _rhoInc = deltaRho;
    }
    else
    {
        _interpolateSteps = int(floor(absDeltaTheta / adaptedStepAngle));
        if (_interpolateSteps < 1)
            return true;
        _rhoInc = deltaRho * adaptedStepAngle / absDeltaTheta;
    }
    _curTheta = _prevTheta;
    _curRho = _prevRho;
    _prevTheta = newTheta;
    _prevRho = newRho;
    _curStep = 0;
    _inProgress = true;
    _isInterpolating = true;
    return true;
}

void EvaluatorThetaRhoLine::service()
{
    // Check in progress
    if (!_inProgress)
        return;

    // Check if interpolating
    if (!_isInterpolating)
        return;

    // Process multiple if possible
    for (int i = 0; i < PROCESS_STEPS_PER_SERVICE; i++)
    {
        if (_curStep >= _interpolateSteps)
        {
            _inProgress = false;
            return;
        }

        // See if we can add to the queue
        if (!_workManager.canAcceptWorkItem())
            return;

        // Step
        _curStep++;

        // Inc
        _curTheta += _thetaInc;
        _curRho += _rhoInc;

        // Next iteration
        char lineBuf[100];
        // Calculate coords
        double x,y;
        calcXYPos(_curTheta, _curRho, x, y);
        sprintf(lineBuf, "G0 X%0.3f Y%0.3f", x, y);
        String retStr;
        WorkItem workItem(lineBuf);
        _workManager.addWorkItem(workItem, retStr);
    }
}

void EvaluatorThetaRhoLine::stop()
{
    _inProgress = false;
}

void EvaluatorThetaRhoLine::calcXYPos(double theta, double rho, double& x, double& y)
{
    x = sin(theta) * rho * _bedRadiusMM + _centreOffsetX;
    y = cos(theta) * rho * _bedRadiusMM + _centreOffsetY;
}