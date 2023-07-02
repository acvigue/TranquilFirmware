// RBotFirmware
// Rob Dobson 2016-2018

#include "RobotConfigurations.h"

const char *RobotConfigurations::_robotConfigs[] = {
    "{\"robotConfig\":{\"robotType\":\"TranquilSmall\",\"cmdsAtStart\":\"\",\"webui\":\"cnc\",\"evaluators\":{\"thrContinue\":0,\"thrThetaMirrored\":"
    "1,\"thrThetaOffsetAngle\":0.5},\"robotGeom\":{\"model\":\"SandBotRotary\",\"motionController\":{\"chip\":\"TMC2209\",\"TX1\":32,\"TX2\":33,"
    "\"driver_TOFF\":4,\"run_current\":600,\"microsteps\":16,\"stealthChop\":1},\"homing\":{\"homingSeq\":\"FR3;A+38400n;B+3200;#;A+38400N;B+3200;#;"
    "A+200;#B+400;#;B+30000n;#;B-30000N;#;B-340;#;A=h;B=h;$\",\"maxHomingSecs\":120},\"blockDistanceMM\":1,\"allowOutOfBounds\":0,\"stepEnablePin\":"
    "\"25\",\"stepEnLev\":0,\"stepDisableSecs\":30,\"axis0\":{\"maxSpeed\":15,\"maxAcc\":25,\"maxRPM\":4,\"stepsPerRot\":38400,\"stepPin\":\"19\","
    "\"dirnPin\":\"21\",\"dirnRev\":\"1\",\"endStop0\":{\"sensePin\":\"22\",\"actLvl\":0,\"inputType\":\"INPUT\"}},\"axis1\":{\"maxSpeed\":15,"
    "\"maxAcc\":25,\"maxRPM\":30,\"stepsPerRot\":3200,\"unitsPerRot\":40.5,\"maxVal\":145,\"stepPin\":\"27\",\"dirnRev\":\"1\",\"dirnPin\":\"3\","
    "\"endStop0\":{\"sensePin\":\"23\",\"actLvl\":0,\"inputType\":\"INPUT\"}}},\"fileManager\":{\"spiffsEnabled\":1,\"spiffsFormatIfCorrupt\":1,"
    "\"sdEnabled\":1,\"sdLanes\":1},\"ledStrip\":{\"ledRGBW\":1,\"ledCount\":\"143\",\"tslEnabled\":\"1\",\"tslSDA\":\"16\",\"tslSCL\":\"17\"}},"
    "\"cmdSched\":{\"jobs\":[]},\"name\":\"Tranquil\"}",

    "{\"robotConfig\":{\"robotType\":\"TranquilLarge\",\"cmdsAtStart\":\"\",\"webui\":\"cnc\",\"evaluators\":{\"thrContinue\":0,\"thrThetaMirrored\":"
    "0,\"thrThetaOffsetAngle\":0.5},\"robotGeom\":{\"model\":\"SandBotRotary\",\"motionController\":{\"chip\":\"TMC2208\",\"TX1\":32,\"TX2\":33,"
    "\"driver_TOFF\":3,\"run_current\":1400,\"microsteps\":16,\"stealthChop\":1},\"homing\":{\"homingSeq\":\"FR3;A+22400n;B+3200;#;A+22400N;B+3200;#;"
    "A+200;#B-400;#;B+30000n;#;B-30000N;#;B-340;#;A=h;B=h;$\",\"maxHomingSecs\":120},\"blockDistanceMM\":1,\"allowOutOfBounds\":0,\"stepEnablePin\":"
    "\"25\",\"stepEnLev\":0,\"stepDisableSecs\":30,\"axis0\":{\"maxSpeed\":15,\"maxAcc\":10,\"maxRPM\":2,\"stepsPerRot\":22400,\"stepPin\":\"19\","
    "\"dirnPin\":\"21\",\"dirnRev\":\"0\",\"endStop0\":{\"sensePin\":\"22\",\"actLvl\":0,\"inputType\":\"INPUT\"}},\"axis1\":{\"maxSpeed\":30,"
    "\"maxAcc\":50,\"maxRPM\":30,\"stepsPerRot\":3200,\"unitsPerRot\":40.5,\"maxVal\":345,\"stepPin\":\"27\",\"dirnRev\":\"0\",\"dirnPin\":\"3\","
    "\"endStop0\":{\"sensePin\":\"23\",\"actLvl\":0,\"inputType\":\"INPUT\"}}},\"fileManager\":{\"spiffsEnabled\":1,\"spiffsFormatIfCorrupt\":1,"
    "\"sdEnabled\":1,\"sdLanes\":1},\"ledStrip\":{\"ledRGBW\":0,\"ledCount\":\"295\",\"tslEnabled\":\"1\",\"tslSDA\":\"16\",\"tslSCL\":\"17\"}},"
    "\"name\":\"Tranquil\"}"};

const int RobotConfigurations::_numRobotConfigurations = sizeof(RobotConfigurations::_robotConfigs) / sizeof(const char *);
