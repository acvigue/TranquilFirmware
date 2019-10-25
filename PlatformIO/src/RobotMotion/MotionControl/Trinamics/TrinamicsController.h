// RBotFirmware
// Rob Dobson 2016-19

#pragma once

#include <ArduinoLog.h>
#include <SPI.h>
#include "../../AxesParams.h"
#include "../MotionPipeline.h"

class TrinamicsController
{
public:
    TrinamicsController(AxesParams& axesParams, MotionPipeline& motionPipeline);
    ~TrinamicsController();

    void configure(const char *configJSON);
    bool configureAxis(int axisIdx, const char *axisJSON);

    void deinit();
    void process();

    bool isEnabled()
    {
        return _isEnabled;
    }

    bool isRampGenerator()
    {
        return _isRampGenerator;
    }

    void _timerCallback(void* arg);

    static void _staticTimerCb(void* arg)
    {
        if (_pThisObj)
            _pThisObj->_timerCallback(arg);
    } 

    class tmc5072Status_t
    {
    public:
        // Status
        uint8_t summary;
        uint32_t d1RampStat;
        uint32_t d2RampStat;

        // Current position
        uint32_t d1Steps;
        uint32_t d2Steps;

        // // Move pending
        // bool d1MovePending;
        // bool d2MovePending;
        // uint32_t d1Target;
        // uint32_t d2Target;

        tmc5072Status_t()
        {
            summary = 0;
            d1RampStat = 0;
            d2RampStat = 0;
        }
        void set(uint8_t sumry, uint32_t rampStat1, uint32_t rampStat2, uint32_t steps1, uint32_t steps2)
        {
            summary = sumry;
            d1RampStat = rampStat1;
            d2RampStat = rampStat1;
            d1Steps = steps1;
            d2Steps = steps2;
        }
        bool isReset()
        {
            return summary & 0x01;
        }
        bool isGstatClearNeeded()
        {
            return (summary & 0x07) != 0;
        }
        bool anyAxisMoving()
        {
            // if (d1MovePending || d2MovePending)
            //     return true;
            return !((d1RampStat & 0x200) && (d2RampStat & 0x200));
        }
        uint32_t getSteps(int driverIdx)
        {
            if (driverIdx == 0)
                return d1Steps;
            return d2Steps;
        }
        String getStatusStr()
        {
            String ost;
            ost = (summary & 0x01) ? " RESET " : "";
            ost = (summary & 0x02) ? " DRV1DOWN " : "";
            ost = (summary & 0x04) ? " DRV2DOWN " : "";
            return ost;
        }
        String getDriverStr(int driverIdx)
        {
            String ost;
            int rampStat = d1RampStat;
            if (driverIdx == 1)
                rampStat = d2RampStat;
            ost = (summary & (0x02 << driverIdx)) ? "E" : ".";
            ost += (rampStat & 0x01) ? "L" : ".";
            ost += (rampStat & 0x02) ? "R" : ".";
            ost += (rampStat & 0x04) ? "F" : ".";
            ost += (rampStat & 0x08) ? "G" : ".";
            ost += (rampStat & 0x10) ? "S" : ".";
            ost += (rampStat & 0x20) ? "T" : ".";
            ost += (rampStat & 0x40) ? "X" : ".";
            ost += (rampStat & 0x80) ? "E" : ".";
            ost += (rampStat & 0x100) ? "V" : ".";
            ost += (rampStat & 0x200) ? "P" : ".";
            ost += (rampStat & 0x400) ? "Z" : ".";
            ost += (rampStat & 0x800) ? "W" : ".";
            ost += (rampStat & 0x800) ? "C" : ".";
            ost += (rampStat & 0x1000) ? "O" : ".";
            return ost;
        }
        // bool anyMovePending()
        // {
        //     return d1MovePending || d2MovePending;
        // }
        // bool isMovePending(int driverIdx)
        // {
        //     if (driverIdx == 0)
        //         return d1MovePending;
        //     return d2MovePending;
        // }
        // bool setTarget(int driverIdx, int32_t steps)
        // {
        //     if (isMovePending(driverIdx))
        //         return false;
        //     if (driverIdx == 0)
        //     {
        //         d1Target = steps;
        //         d1MovePending = true;
        //         return true;
        //     }
        //     d2Target = steps;
        //     d2MovePending = true;
        //     return true;
        // }
        // int32_t getTarget(int driverIdx)
        // {
        //     if (driverIdx == 0)
        //         return d1Target;
        //     return d2Target;
        // }
    };

    // bool anyAxisMoving();
    // uint32_t getAxisSteps(int axisIdx);
    // void setAxisTarget(int axisIdx, int32_t steps);
    // void stopMotors();

    void stop()
    {
        // TODO
    }

    void pause(bool pauseIt)
    {
        // TODO
    }

    void resetTotalStepPosition()
    {
        for (int i = 0; i < RobotConsts::MAX_AXES; i++)
        {
            _totalStepsMoved[i] = 0;
        }
    }
    void getTotalStepPosition(AxisInt32s& actuatorPos)
    {
        for (int i = 0; i < RobotConsts::MAX_AXES; i++)
        {
            actuatorPos.setVal(i, _totalStepsMoved[i]);
        }
    }
    void setTotalStepPosition(int axisIdx, int32_t stepPos)
    {
        if ((axisIdx >= 0) && (axisIdx < RobotConsts::MAX_AXES))
            _totalStepsMoved[axisIdx] = stepPos;
    }
    int getLastCompletedNumberedCmdIdx()
    {
        return _lastDoneNumberedCmdIdx;
    }

private:
    // Helpers
    int getPinAndConfigure(const char* configJSON, const char* pinSelector, int direction, int initValue);
    uint64_t tmcWrite(int chipIdx, uint8_t cmd, uint32_t data, bool addWriteFlag=true);
    uint8_t tmcReadLastAndSetCmd(int chipIdx, uint8_t cmd, uint32_t& dataOut);
    void chipSel(int chipIdx, bool en);
    void performSel(int singleCS, int mux1, int mux2, int mux3, int muxCS, bool en);
    uint64_t tmc5072Init(int chipIdxOrNeg1ForAll = -1);
    void updateStatus(int chipIdx);
    void tmc5072SendCmd(int axisIdx, uint8_t baseCmd, uint32_t data);

    // TMC chips
    static const int MAX_TMC2130 = 3;
    static const int MAX_TMC5072 = 2;
    tmc5072Status_t _tmc5072Status[MAX_TMC5072];

    // Singleton pointer (for timer callback)
    static TrinamicsController* _pThisObj;

    // Axes parameters
    AxesParams& _axesParams;

    // Axis settings
    class AxisSettings
    {
    public:
        AxisSettings()
        {
            reversed = false;
        }
        
        bool reversed;
    };
    AxisSettings _axisSettings[RobotConsts::MAX_AXES];

    // Motion pipeline
    MotionPipeline& _motionPipeline;

    bool _isEnabled;
    bool _isRampGenerator;

    // Timer
    esp_timer_handle_t _trinamicsTimerHandle;
    bool _trinamicsTimerStarted;

    // Last completed numbered command
    int _lastDoneNumberedCmdIdx;

    // Steps moved in total
    int32_t _totalStepsMoved[RobotConsts::MAX_AXES];

    // SPI
    int _miso;
    int _mosi;
    int _sck;

    // un-multiplexed chip selects
    int _cs1;
    int _cs2;
    int _cs3;

    // multiplexer pins and chip select mux values
    int _mux1;
    int _mux2;
    int _mux3;
    int _muxCS1;
    int _muxCS2;
    int _muxCS3;

    // SPI controller
    SPIClass* _pVSPI;

    static constexpr uint32_t TRINAMIC_TIMER_PERIOD_US = 500;
    static constexpr double TRINAMIC_CLOCK_FACTOR = 75.0;
    static const int SPI_CLOCK_HZ = 2000000;

    static const int WRITE_FLAG = 1 << 7;
    static const int READ_FLAG = 0;
    static const int REG_GCONF = 0x00;
    static const int REG_GSTAT = 0x01;
    static const int REG_IHOLD_IRUN = 0x10;
    static const int REG_CHOPCONF = 0x6C;
    static const int REG_COOLCONF = 0x6D;
    static const int REG_DCCTRL = 0x6E;
    static const int REG_DRVSTATUS = 0x6F;

    // Debug
    uint32_t _debugTimerLast;
};