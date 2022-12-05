// Auto Dimming LED Strip
// Matt Grammes 2018

#pragma once

#include "Utils.h"
#include "ConfigNVS.h"
#include "ConfigPinMap.h"
#include <WS2812FX.h>

class LedStrip
{
public:
    LedStrip(ConfigBase &ledNvValues);
    void setup(ConfigBase* pConfig, const char* ledStripName, void (*showFn)());
    void service();
    void serviceStrip();
    uint8_t * getPixelDataPointer();
    uint16_t getNumBytes();
    void updateLedFromConfig(const char* pLedJson);
    const char* getConfigStrPtr();
    void setSleepMode(int sleep);
    

private:
    void configChanged();
    void updateNv();
    uint16_t getAverageSensorReading();

private:
    String _name;
    ConfigBase* _pHwConfig;
    static const int NUM_SENSOR_VALUES = 100;
    bool _isSetup;
    bool _isSleeping;
    int _ledPin;
    int _ledCount;
    int _sensorPin;

    bool _ledOn;
    byte _ledBrightness = -1;
    int _effectSpeed = 0;
    bool _autoDim = false;

    int _effectID;
    int _redVal;
    int _greenVal;
    int _blueVal;
    WS2812FX *_ws2812fx;
    void (*_showFn)(void) = NULL;
    bool ledConfigChanged = false;
    
    int sensorReadingCount = 0;

    // Store the settings for LED in NV Storage
    ConfigBase& _ledNvValues;

    // Store a number of sensor readings for smoother transitions
    uint16_t sensorValues[NUM_SENSOR_VALUES];
};