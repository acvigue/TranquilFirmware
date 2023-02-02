// Auto Dimming LED Strip
// Matt Grammes 2018

#pragma once

#include "Utils.h"
#include "ConfigNVS.h"
#include "ConfigPinMap.h"
#include <FastLED.h>

#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

class LedStrip
{
public:
    LedStrip(ConfigBase &ledNvValues);
    void setup(ConfigBase* pConfig, const char* ledStripName);
    void service();
    void serviceStrip();
    void updateLedFromConfig(const char* pLedJson);
    const char* getConfigStrPtr();
    void setSleepMode(int sleep);

private:
    void configChanged();
    void updateNv();
    uint16_t getAverageSensorReading();
    void effect_pride();

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
    CRGB *_leds;
    Adafruit_TSL2561_Unified *_tsl;
    bool ledConfigChanged = false;
    unsigned long _last_check_tsl_time = 0;
    
    int sensorReadingCount = 0;

    // Store the settings for LED in NV Storage
    ConfigBase& _ledNvValues;

    // Store a number of sensor readings for smoother transitions
    uint16_t sensorValues[NUM_SENSOR_VALUES];
};