// RBotFirmware
// Rob Dobson 2016-19

// Modified by Aiden Vigue - 2022
// -> Simplify register modification and add CHOPCONF to robot configuration

#include "TrinamicsController.h"
#include "RdJson.h"
#include "ConfigPinMap.h"
#include "Utils.h"
#include <TMCStepper.h>
#include <HardwareSerial.h>

static const char* MODULE_PREFIX = "TrinamicsController: ";

TrinamicsController* TrinamicsController::_pThisObj = NULL;

TrinamicsController::TrinamicsController(AxesParams& axesParams, MotionPipeline& motionPipeline) :
        _axesParams(axesParams), _motionPipeline(motionPipeline)
{
    _pThisObj = this;
    _isEnabled = false;
    _isRampGenerator = false;
    _tx1 = _tx2 =  -1;
}

TrinamicsController::~TrinamicsController()
{
    deinit();
}

void TrinamicsController::deinit()
{
    // Release pins
    if (_tx1 >= 0)
        pinMode(_tx1, INPUT);
    if (_tx2 >= 0)
        pinMode(_tx2, INPUT);
    _tx1 = _tx2 = -1;

    _isEnabled = false;
}

void TrinamicsController::configure(const char *configJSON)
{
    Log.verbose("%sconfigure %s\n", MODULE_PREFIX, configJSON);

    // Check for trinamics controller config JSON
    String motionController = RdJson::getString("motionController", "NONE", configJSON);

    // Get chip
    String mcChip = RdJson::getString("chip", "NONE", motionController.c_str());
    Log.trace("%sconfigure motionController %s chip %s\n", MODULE_PREFIX, motionController.c_str(), mcChip.c_str());

    if ((mcChip == "TMC2208" || mcChip == "TMC2209"))
    {
        // SPI settings
        
        String pinName = RdJson::getString("TX1", "", motionController.c_str());
        int TX1Pin = pinName.toInt();
        pinName = RdJson::getString("TX2", "", motionController.c_str());
        int TX2Pin = pinName.toInt();

        // Check valid
        if (TX1Pin == -1 || TX2Pin == -1)
        {
            Log.warning("%ssetup SPI pins invalid\n", MODULE_PREFIX);
        }
        else
        {
            _tx1 = TX1Pin;
            _tx2 = TX2Pin;
            _isEnabled = true;
        }
    }

    // Handle CS (may be multiplexed)
    if (_isEnabled)
    {
        
        int _toff = RdJson::getDouble("driver_TOFF", 5, motionController.c_str());
        int _irun = RdJson::getDouble("run_current", 600, motionController.c_str());
        int _msteps = RdJson::getDouble("microsteps", 16, motionController.c_str());
        int _stealthChop = RdJson::getDouble("stealthChop", 0, motionController.c_str()); 
        
        // Configure TMC2208s
        if (mcChip == "TMC2208")
        {
            HardwareSerial serial1 = HardwareSerial(1);
            HardwareSerial serial2 = HardwareSerial(2);
            TMC2208Stepper driver_1 = TMC2208Stepper(&serial1, 0.11f);
            TMC2208Stepper driver_2 = TMC2208Stepper(&serial2, 0.11f);

            serial1.begin(115200, SERIAL_8N1, 34, _tx1);
            serial2.begin(115200, SERIAL_8N1, 34, _tx2);
            driver_1.reset();
            driver_2.reset();

            driver_1.begin();
            driver_1.toff(_toff);                 // Enables driver in software
            driver_1.rms_current(_irun);        // Set motor RMS current
            driver_1.microsteps(_msteps);          // Set microsteps to 1/16th
            driver_1.intpol(true);

            if(_stealthChop == 1) {
                driver_1.pwm_autoscale(true);
                driver_1.en_spreadCycle(false);
            } else {
                driver_1.en_spreadCycle(true);
            }

            driver_2.begin();
            driver_2.toff(_toff);                 // Enables driver in software
            driver_2.rms_current(_irun);        // Set motor RMS current
            driver_2.microsteps(_msteps);          // Set microsteps to 1/16th
            driver_2.intpol(true);

            if(_stealthChop == 1) {
                driver_2.pwm_autoscale(true);
                driver_2.en_spreadCycle(false);
            } else {
                driver_2.en_spreadCycle(true);
            }
        }

        // Configure TMC2130s
        if (mcChip == "TMC2209")
        {
            HardwareSerial serial1 = HardwareSerial(1);
            HardwareSerial serial2 = HardwareSerial(2);
            TMC2209Stepper driver_1 = TMC2209Stepper(&serial1, 0.11f, 0);
            TMC2209Stepper driver_2 = TMC2209Stepper(&serial2, 0.11f, 0);

            serial1.begin(115200, SERIAL_8N1, 34, _tx1);
            serial2.begin(115200, SERIAL_8N1, 34, _tx2);
            driver_1.reset();
            driver_2.reset();
            
            driver_1.begin();
            driver_1.toff(_toff);                 // Enables driver in software
            driver_1.rms_current(_irun);        // Set motor RMS current
            driver_1.microsteps(_msteps);          // Set microsteps to 1/16th
            driver_1.intpol(true);

            if(_stealthChop == 1) {
                driver_1.pwm_autoscale(true);
                driver_1.en_spreadCycle(false);
            } else {
                driver_1.en_spreadCycle(true);
            }

            driver_2.begin();
            driver_2.toff(_toff);                 // Enables driver in software
            driver_2.rms_current(_irun);        // Set motor RMS current
            driver_2.microsteps(_msteps);          // Set microsteps to 1/16th
            driver_2.intpol(true);

            if(_stealthChop == 1) {
                driver_2.pwm_autoscale(true);
                driver_2.en_spreadCycle(false);
            } else {
                driver_2.en_spreadCycle(true);
            }
        }
    }
}

uint32_t TrinamicsController::getUint32WithBaseFromConfig(const char* dataPath, uint32_t defaultValue,
                            const char* pSourceStr)
{
    String confStr = RdJson::getString(dataPath, "", pSourceStr);
    if (confStr.length() == 0)
        return defaultValue;
    if ((confStr.startsWith("0x")) || (confStr.startsWith("0X")))
        return strtoul(confStr.substring(2).c_str(), NULL, 16);
    else if ((confStr.startsWith("0b")) || (confStr.startsWith("0B")))
        return strtoul(confStr.substring(2).c_str(), NULL, 2);
    return strtoul(confStr.c_str(), NULL, 10);    
}

int TrinamicsController::getPinAndConfigure(const char* configJSON, const char* pinSelector, int direction, int initValue)
{
    String pinName = RdJson::getString(pinSelector, "", configJSON);
    int pinIdx = ConfigPinMap::getPinFromName(pinName.c_str());
    if (pinIdx >= 0)
    {
        pinMode(pinIdx, direction);
        digitalWrite(pinIdx, initValue);
    }
    return pinIdx;
}