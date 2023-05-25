// RBotFirmware
// Rob Dobson 2016-2018

#include "RobotConfigurations.h"

const char *RobotConfigurations::_robotConfigs[] = {
    "{\"robotType\":\"TranquilSmall\",\"cmdsAtStart\":\"\",\"webui\":\"cnc\",\"evaluators\":{\"thrContinue\":0},\"robotGeom\":{\"model\":"
    "\"SandBotRotary\",\"motionController\":{\"chip\":\"TMC2209\",\"TX1\":32,\"TX2\":33,\"driver_TOFF\":4,\"run_current\":600,\"microsteps\":16,"
    "\"stealthChop\":1},\"homing\":{\"homingSeq\":\"FR1;A+38400n;B+3200;#;A+38400N;B+3200;#;A+200;#B+400;#;B+30000n;#;B-30000N;#;B-340;#;A=h;B=h;$"
    "\",\"maxHomingSecs\":120},\"blockDistanceMM\":1,\"allowOutOfBounds\":0,\"stepEnablePin\":\"25\",\"stepEnLev\":0,\"stepDisableSecs\":30,"
    "\"axis0\":{\"maxSpeed\":15,\"maxAcc\":25,\"maxRPM\":4,\"stepsPerRot\":38400,\"stepPin\":\"19\",\"dirnPin\":\"21\",\"dirnRev\":\"1\","
    "\"endStop0\":{\"sensePin\":\"22\",\"actLvl\":0,\"inputType\":\"INPUT\"}},\"axis1\":{\"maxSpeed\":15,\"maxAcc\":25,\"maxRPM\":30,\"stepsPerRot\":"
    "3200,\"unitsPerRot\":40.5,\"maxVal\":145,\"stepPin\":\"27\",\"dirnRev\":\"1\",\"dirnPin\":\"3\",\"endStop0\":{\"sensePin\":\"23\",\"actLvl\":0,"
    "\"inputType\":\"INPUT\"}}},\"fileManager\":{\"spiffsEnabled\":1,\"spiffsFormatIfCorrupt\":1,\"sdEnabled\":1,\"sdLanes\":4},\"ledStrip\":{"
    "\"ledRGBW\":1,\"ledCount\":\"143\",\"tslEnabled\":\"1\",\"tslSDA\":\"16\",\"tslSCL\":\"17\"}}"};

const int RobotConfigurations::_numRobotConfigurations = sizeof(RobotConfigurations::_robotConfigs) / sizeof(const char *);
