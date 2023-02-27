// RBotFirmware
// Aiden Vigue 2023

#include <Arduino.h>
#include "RobotSandTableRotary.h"
#include "../MotionControl/MotionHelper.h"
#include "Utils.h"
#include "math.h"

//#define DEBUG_SANDTABLE_MOTION 1
//#define DEBUG_SANDTABLE_CARTESIAN_TO_POLAR 1

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
    actuatorToPolar(curAxisPositions._stepsFromHome, curPolar, axesParams);
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
        float rhoRel = targetPolar.getVal(1) - curPolar.getVal(1);

        relativePolarSolution.setVal(0, theta1Rel);
        relativePolarSolution.setVal(1, rhoRel);
    }

    // Apply this to calculate required steps
    relativePolarToSteps(relativePolarSolution, curAxisPositions, outActuator, axesParams);

    // Debug
    #ifdef DEBUG_SANDTABLE_MOTION
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
    actuatorToPolar(actuatorPos, curPolar, axesParams);

	// Calculate max linear axis
	float maxLinear = -1;
    axesParams.getMaxVal(1, maxLinear);
    if(maxLinear == -1)
        maxLinear = 100;

    // Compute axis positions from polar values
    float rho = float(curPolar.getVal(1) * maxLinear);
    float theta = curPolar.getVal(0);

    float x = rho * cos(AxisUtils::d2r(theta));
    float y = rho * sin(AxisUtils::d2r(theta));

    outPt.setVal(0, x);
    outPt.setVal(1, y);    

    // Debug
    #ifdef DEBUG_SANDTABLE_MOTION
    Log.trace("%sacToPt s1 %d s2 %d theta %F rho %F x %F y %F\n", MODULE_PREFIX, 
        actuatorPos.getVal(0), actuatorPos.getVal(1),
        theta, rho, x, y);
    #endif
}

void RobotSandTableRotary::correctStepOverflow(AxisPosition& curPos, AxesParams& axesParams)
{
    int rotationStepsTheta = (int)(axesParams.getStepsPerRot(0));
    int rotationStepsRho = (int)(axesParams.getStepsPerRot(1));
    bool showDebug = false;
        if (curPos._stepsFromHome.getVal(0) > rotationStepsTheta || curPos._stepsFromHome.getVal(0) <= -rotationStepsTheta)
        {
            Log.info("CORRECTING ax0 %d ax1 %d\n",
                            curPos._stepsFromHome.getVal(0), curPos._stepsFromHome.getVal(1));
            showDebug = true;
        }
    // Bring steps from home values back within a single rotation
    while (curPos._stepsFromHome.getVal(0) > rotationStepsTheta) {
        curPos._stepsFromHome.setVal(0, curPos._stepsFromHome.getVal(0) - rotationStepsTheta);
        curPos._stepsFromHome.setVal(1, curPos._stepsFromHome.getVal(1) - rotationStepsRho);
    }
    while (curPos._stepsFromHome.getVal(0) <= -rotationStepsTheta) {
        curPos._stepsFromHome.setVal(0, curPos._stepsFromHome.getVal(0) + rotationStepsTheta);
        curPos._stepsFromHome.setVal(1, curPos._stepsFromHome.getVal(1) + rotationStepsRho);
    }
    
    if (showDebug)
        Log.info("CORRECTED ax0 %d ax1 %d\n", curPos._stepsFromHome.getVal(0), curPos._stepsFromHome.getVal(1));
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
	float rho = float(distFromOrigin / linearLength);

	//Return theta in DEGREES and rho.
	targetSoln1.setVal(0, AxisUtils::r2d(AxisUtils::wrapRadians(theta)));
	targetSoln1.setVal(1, rho);

#ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
    Log.trace("%scartesianToPolar target X%F Y%F\n", MODULE_PREFIX,
            targetPt.getVal(0), targetPt.getVal(1));	
    Log.trace("%scartesianToPolar %s theta %F rho %F\n", MODULE_PREFIX,
            posValid ? "ok" : "OUT_OF_BOUNDS",
            theta, rho);
#endif

    return posValid;
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

    float fractionalRotation = float(relativePolar.getVal(0) / float(360));
    int32_t stepsRelTheta = int32_t(roundf(float(fractionalRotation * float(axesParams.getStepsPerRot(0)))));

    //Rho axis is a special one! Step it to rotate the gear the same degree as the theta.
    //Theta is moving relativePolar.getVal(0) degrees, therefore rho would be moving 
    //relativePolar.getVal(0) * axesParams.getStepsPerRot(1) steps.
    float rhoCounteractSteps = float(fractionalRotation * float(axesParams.getStepsPerRot(1)));

    //Then, rho really NEEDS to move relPolar[1] * maxLinear in mm.
    //which, mm -> steps is (mm / mm/rot) * stepsPerRot
    
    float rhoMM = relativePolar.getVal(1) * maxLinear;
    float rhoActiveSteps = rhoMM * axesParams.getStepsPerUnit(1);

    int32_t stepsRelRho = int32_t(roundf(rhoCounteractSteps + rhoActiveSteps));

    // Add to existing
    outActuator.setVal(0, curAxisPositions._stepsFromHome.getVal(0) + stepsRelTheta);
    outActuator.setVal(1, curAxisPositions._stepsFromHome.getVal(1) + stepsRelRho);

    #ifdef DEBUG_SANDTABLE_CARTESIAN_TO_POLAR
        Log.trace("%srelativePolarToSteps: stepsRel0 %d stepsRel1 %d curSteps0 %d curSteps1 %d destSteps0 %F destSteps1 %F inTheta %F inRho %F\n",
                MODULE_PREFIX,
                stepsRelTheta, stepsRelRho, curAxisPositions._stepsFromHome.getVal(0), curAxisPositions._stepsFromHome.getVal(1),
                outActuator.getVal(0), outActuator.getVal(1), relativePolar.getVal(0), relativePolar.getVal(1));
    #endif
}

void RobotSandTableRotary::actuatorToPolar(AxisInt32s& actuatorCoords, AxisFloats& polarCoords, AxesParams& axesParams)
    {
        // Calculate azimuth
        double currentTheta = AxisUtils::wrapDegrees(actuatorCoords.getVal(0) * 360 / axesParams.getStepsPerRot(0));
        polarCoords.setVal(0, currentTheta);

        float maxLinear = -1;
        axesParams.getMaxVal(1, maxLinear);
        if(maxLinear == -1)
            maxLinear = 100;

        // Calculate linear position (note that this robot has interaction between azimuth and linear motion as the rack moves
        // if the pinion gear remains still and the arm assembly moves around it) - so the required linear calculation uses the
        // difference in linear and arm rotation steps
        long linearStepsFromHome = actuatorCoords.getVal(1) - (float(actuatorCoords.getVal(0)) * float(axesParams.getStepsPerRot(1)/axesParams.getStepsPerRot(0)));
        int32_t maxStepsRho = maxLinear * axesParams.getStepsPerUnit(1);
        float currentRho = float(linearStepsFromHome) / float(maxStepsRho);
        polarCoords.setVal(1, currentRho);

        // Log.trace("actuatorToPolar c0 %F c1 %F alphaSteps %d alphaDegs %F linStpHm %d rotD %F lin %F\n", actuatorCoords[0], actuatorCoords[1],
        //     alphaSteps, alphaDegs, linearStepsFromHome, polarCoordsAzFirst[0] * 180 / M_PI, polarCoordsAzFirst[1]);
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
