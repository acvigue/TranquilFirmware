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
    void service(float currentX, float currentY);
    void serviceStrip();
    void updateLedFromConfig(const char* pLedJson);
    const char* getConfigStrPtr();
    int getLuxLevel();
    void setSleepMode(int sleep);

private:
    void configChanged();
    void updateNv();
    uint16_t getAverageSensorReading();
    void effect_pride();
    void effect_followTheta();
    void solid_color();

private:
    String _name;
    ConfigBase* _pHwConfig;
    static const int NUM_SENSOR_VALUES = 100;
    bool _isSetup;
    bool _isSleeping;
    int _ledPin;
    int _ledCount;
    int _sensorEnabled;

    bool _ledOn;
    byte _ledBrightness = -1;
    byte _ledRealBrightness = -1;
    int _effectSpeed = 0;
    bool _autoDim = false;

    int _effectID;
    int _primaryRedVal;
    int _primaryGreenVal;
    int _primaryBlueVal;
    int _secRedVal;
    int _secGreenVal;
    int _secBlueVal;
    int _luxLevel;

    float _currentX = 0;
    float _currentY = 0;
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