// RBotFirmware
// Aiden Vigue 2023

#include <Arduino.h>
#include "RobotSandTableRotary.h"
#include "../MotionControl/MotionHelper.h"
#include "Utils.h"
#include "math.h"

// #define DEBUG_SANDTABLERotary_MOTION 1
// #define DEBUG_SANDTABLE_CARTESIAN_TO_POLAR 1

static const char* MODULE_PREFIX = "SandTableRotary: ";

// Notes for SandTableRotary
// Positive stepping direction for axis 0 is clockwise movement of the rotary plate
// Positive stepping direction for axis 1 need to move OUT

//Axis 0 = ROTARY axis
//Axis 1 = LINEAR axis!

RobotSandTableRotary::RobotSandTableRotary(const char* pRobotTypeName, MotionHelper& motionHelper) :
    RobotBase(pRobotTypeName, motionHelper)
{
    // Set transforms
    _motionHelper.setTransforms(ptToActuator, actuatorToPt, correctStepOverflow, convertCoords, setRobotAttributes);
}

RobotSandTableRotary::~RobotSandTableRotary()
{
}

// Convert a cartesian point to actuator coordinates
//MARK: REVIEWED
bool RobotSandTableRotary::ptToActuator(AxisFloats& targetPt, AxisFloats& outActuator, 
            AxisPosition& curAxisPositions, AxesParams& axesParams, bool allowOutOfBounds)
{
    // Convert the current position to polar wrapped 0..360 degrees
    // Val0 is theta
    // Val0 is rho
    AxisFloats curPolar;

    stepsToPolar(curAxisPositions._stepsFromHome, curPolar, axesParams);

    // Best relative polar solution
    AxisFloats relativePolarSolution;

	// Check for points close to the origin
	if (AxisUtils::isApprox(targetPt._pt[0], 0, 1) && (AxisUtils::isApprox(targetPt._pt[1], 0, 1)))
	{
		// Keep the current position for theta, set rho to 0 (current position, negative)
        relativePolarSolution.setVal(0, 0);
        relativePolarSolution.setVal(1, curPolar.getVal(1) * -1);
	}
    else {
        // Convert the target cartesian coords to polar wrapped to 0..360 degrees
        AxisFloats targetPolar;
        bool isValid = cartesianToPolar(targetPt, targetPolar, axesParams);
        if ((!isValid) && (!allowOutOfBounds))
        {
            Log.verbose("%sOut of bounds not allowed\n", MODULE_PREFIX);
            return false;
        }

        // Find the minimum rotation for theta
        float theta1Rel = calcRelativePolar(targetPolar.getVal(0), curPolar.getVal(0));
        float rhoRel = targetPolar.getVal(1) - targetPolar.getVal(0);

        relativePolarSolution.setVal(0, theta1Rel);
        relativePolarSolution.setVal(1, rhoRel);
    }

    // Apply this to calculate required steps
    relativePolarToSteps(relativePolarSolution, curAxisPositions, outActuator, axesParams);

    // Debug
    #ifdef DEBUG_SANDTABLERotary_MOTION
        Log.trace("%sptToAct newX %F newY %F prevX %F prevY %F dist %F abs steps %F, %F minRot1 %F, minRot2 %F\n", MODULE_PREFIX, 
                    targetPt._pt[0], 
                    targetPt._pt[1], 
                    curAxisPositions._axisPositionMM.getVal(0), 
                    curAxisPositions._axisPositionMM.getVal(1), 
                    sqrt(targetPt._pt[0]*targetPt._pt[0]+targetPt._pt[1]*targetPt._pt[1]),
                    outActuator.getVal(0), 
                    outActuator.getVal(1), 
                    relativePolarSolution.getVal(0), 
                    relativePolarSolution.getVal(1));
    #endif

    return true;
}

void RobotSandTableRotary::actuatorToPt(AxisInt32s& actuatorPos, AxisFloats& outPt, AxisPosition& curPos, AxesParams& axesParams)
{
    // Get current polar
    AxisFloats curPolar;
    stepsToPolar(actuatorPos, curPolar, axesParams);

	// Calculate max linear axis
	float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    // Compute axis positions from polar values
    float rho = curPolar.getVal(1);
    float theta = curPolar.getVal(0);

    float x = rho * cos(theta);
    float y = rho * sin(theta);

    outPt.setVal(0, x);
    outPt.setVal(1, y);    

    // Debug
#ifdef DEBUG_SANDTABLERotary_MOTION
    Log.verbose("%sacToPt s1 %d s2 %d alpha %F beta %F x %F y %F shel %F elha %F elx %F ely %F haX %F hay %F\n", MODULE_PREFIX, 
                actuatorPos.getVal(0), actuatorPos.getVal(1),
                curPolar.getVal(0), curPolar.getVal(1),
                elbowX + handXDiff, elbowY + handYDiff,
                shoulderElbowMM, elbowHandMM,
                elbowX, elbowY,
                handXDiff, handYDiff);
#endif

}

void RobotSandTableRotary::correctStepOverflow(AxisPosition& curPos, AxesParams& axesParams)
{
    // Since the robot is polar, axis 0 can be considered to have a value between 0 and the stepsPerRot
    // Axis 1 can have a value between 0 (center) and stepsPerRot * (maxVal / unitsPerRot) (end)

    int32_t stepsPerRot0 = int32_t(roundf(axesParams.getStepsPerRot(0)));
    curPos._stepsFromHome.setVal(0, (curPos._stepsFromHome.getVal(0) + stepsPerRot0) % stepsPerRot0);
    int32_t stepsPerRot1 = int32_t(roundf(axesParams.getStepsPerRot(1)));
    if(curPos._stepsFromHome.getVal(1) < 0) {
        curPos._stepsFromHome.setVal(1, 0);
    }

    float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    int32_t maxStepsAxis1 = stepsPerRot1 * (maxLinear / axesParams.getunitsPerRot(1));
    if(curPos._stepsFromHome.getVal(1) > maxStepsAxis1) {
        curPos._stepsFromHome.setVal(1, maxStepsAxis1);
    }
}

bool RobotSandTableRotary::cartesianToPolar(AxisFloats& targetPt, AxisFloats& targetSoln1, AxesParams& axesParams)
{
	// Calculate arm lengths
	// The maxVal for axis0 and axis1 are used to determine the arm lengths
	// The radius of the machine is the sum of these two lengths
	float linearLength = 0;

	bool axis1MaxValid = axesParams.getMaxVal(1, linearLength);
    // If not valid set to some values to avoid arithmetic errors

	if (!axis1MaxValid)
		linearLength = 100;

	// Calculate distance from origin to pt (forms one side of triangle where arm segments form other sides)
	float distFromOrigin = sqrt(pow(targetPt._pt[0], 2) + pow(targetPt._pt[1], 2));

	// Check validity of position (distance from origin cannot be greater than linear axis max length)
	bool posValid = distFromOrigin <= linearLength;

	// Calculate theta. Will always be POSITIVE (0 -> 2PI)
	float theta = atan2(targetPt._pt[1], targetPt._pt[0]);
	if (theta < 0)
		theta += M_PI * 2;

	// Calculate required radius
	float rho = distFromOrigin / axis1MaxValid;

	//Return theta in DEGREES and rho.
	targetSoln1.setVal(0, AxisUtils::r2d(AxisUtils::wrapRadians(theta)));
	targetSoln1.setVal(1, rho);

#ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
    Log.trace("%scartesianToPolar target X%F Y%F l1 %F, l2 %F\n", MODULE_PREFIX,
            targetPt.getVal(0), targetPt.getVal(1),
            shoulderElbowMM, elbowHandMM);	
    Log.trace("%scartesianToPolar %s 3rdSide %Fmm D1 %Fd D2 %Fd innerAng %Fd\n", MODULE_PREFIX,
            posValid ? "ok" : "OUT_OF_BOUNDS",
            thirdSideL3MM, AxisUtils::r2d(delta1), AxisUtils::r2d(delta2), AxisUtils::r2d(innerAngleOppThirdGamma));
    Log.trace("%scartesianToActuator alpha1 %Fd, beta1 %Fd\n", MODULE_PREFIX,
		    targetSoln1.getVal(0), targetSoln1.getVal(1));
#endif

    return posValid;
}

void RobotSandTableRotary::stepsToPolar(AxisInt32s& actuatorCoords, AxisFloats& rotationDegrees, AxesParams& axesParams)
{
    // Axis 0 positive steps clockwise, axis 1 postive steps are anticlockwise
    // Axis 0 zero steps is at 0 degrees, axis 1 zero steps is at 0 mm
    // All angles returned are in degrees clockwise from North
    double currentTheta = AxisUtils::wrapDegrees(actuatorCoords.getVal(0) * 360 / axesParams.getStepsPerRot(0));

    float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    int32_t maxStepsRho = axesParams.getStepsPerRot(1) * (maxLinear / axesParams.getunitsPerRot(1));

    double currentRho = actuatorCoords.getVal(1) / maxStepsRho;
    rotationDegrees.set(currentTheta, currentRho);
    #ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
        Log.trace("%sstepsToPolar: ax0Steps %d ax1Steps %d a %Fd b %Fd\n", MODULE_PREFIX,
                actuatorCoords.getVal(0), actuatorCoords.getVal(1), rotationDegrees._pt[0], rotationDegrees._pt[1]);
    #endif
}

float RobotSandTableRotary::calcRelativePolar(float targetRotation, float curRotation)
{
    // Calculate the difference angle
    float diffAngle = targetRotation - curRotation;

    // For angles between -180 and +180 just use the diffAngle
    float bestRotation = diffAngle;
    if (diffAngle <= -180)
        bestRotation = 360 + diffAngle;
    else if (diffAngle > 180)
        bestRotation = diffAngle - 360;
#ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
    Log.trace("%scalcRelativePolar: target %F cur %F diff %F best %F\n", MODULE_PREFIX,
            targetRotation, curRotation, diffAngle, bestRotation);
#endif
    return bestRotation;
}

void RobotSandTableRotary::relativePolarToSteps(AxisFloats& relativePolar, AxisPosition& curAxisPositions, 
            AxisFloats& outActuator, AxesParams& axesParams)
{
    float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    // Convert relative polar to steps
    int32_t stepsRelTheta = int32_t(roundf(relativePolar.getVal(0) * axesParams.getStepsPerRot(0) / 360));

    //Rho axis is a special one! Step it to rotate the gear the same degree as the theta.
    //Theta is moving relativePolar.getVal(0) degrees, therefore rho would be moving 
    //relativePolar.getVal(0) * axesParams.getStepsPerRot(1) steps.
    float rhoCounteractSteps = relativePolar.getVal(0) * axesParams.getStepsPerRot(1);

    //Then, rho really NEEDS to move relPolar[1] * maxLinear in mm.
    //which, mm -> steps is (mm / mm/rot) * stepsPerRot
    
    float rhoMM = relativePolar.getVal(1) * maxLinear;
    float rhoActiveSteps = (rhoMM / axesParams.getunitsPerRot(1)) * axesParams.getStepsPerRot(1);
    
    int32_t stepsRelRho = int32_t(roundf(rhoCounteractSteps + rhoActiveSteps));

    // Add to existing
    outActuator.setVal(0, curAxisPositions._stepsFromHome.getVal(0) + stepsRelTheta);
    outActuator.setVal(1, curAxisPositions._stepsFromHome.getVal(1) + stepsRelRho);
    #ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
        Log.trace("%srelativePolarToSteps: stepsRel0 %d stepsRel1 %d curSteps0 %d curSteps1 %d destSteps0 %F destSteps1 %F\n",
                MODULE_PREFIX,
                stepsRel0, stepsRel1, curAxisPositions._stepsFromHome.getVal(0), curAxisPositions._stepsFromHome.getVal(1),
                outActuator.getVal(0), outActuator.getVal(1));
    #endif
}

void RobotSandTableRotary::convertCoords(RobotCommandArgs& cmdArgs, AxesParams& axesParams)
{
    // Coordinates can be converted here if required
}


void RobotSandTableRotary::setRobotAttributes(AxesParams& axesParams, String& robotAttributes)
{
    // Calculate max and min cartesian size of robot
	// Calculate arm lengths
	// The maxVal for axis0 and axis1 are used to determine the arm lengths
	// The radius of the machine is the sum of these two lengths
	float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    // Set attributes
    constexpr int MAX_ATTR_STR_LEN = 400;
    char attrStr[MAX_ATTR_STR_LEN];
    sprintf(attrStr, "{\"sizeX\":%0.2f,\"sizeY\":%0.2f,\"sizeZ\":%0.2f,\"originX\":%0.2f,\"originY\":%0.2f,\"originZ\":%0.2f}",
            maxLinear*2, maxLinear*2, 0.0,
            maxLinear, maxLinear, 0.0);
    robotAttributes = attrStr;
}
