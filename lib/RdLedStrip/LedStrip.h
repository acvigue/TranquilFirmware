// Auto Dimming LED Strip
// Matt Grammes 2018

#pragma once

#include "Utils.h"
#include "ConfigNVS.h"
#include "ConfigPinMap.h"
#include <FastLED.h>
#include "FastLED_RGBW.h"

#include <Wire.h>
#include <SparkFunTSL2561.h>

class LedStrip
{
public:
    LedStrip(ConfigBase &ledNvValues);
    void setup(ConfigBase* pConfig, const char* ledStripName);
    void service(float currentX, float currentY);
    void serviceStrip();
    void updateLedFromConfig(const char* pLedJson);
    const char* getConfigStrPtr();
    float getLuxLevel();
    void setSleepMode(int sleep);

private:
    void configChanged();
    void updateNv();
    void effect_pride();
    void effect_followTheta();
    void solid_color();

private:
    String _name;
    ConfigBase* _pHwConfig;

    bool _isSetup;
    bool _isSleeping;
    int _ledPin;
    int _ledCount;
    int _sensorEnabled;
    bool _currentGain = 0;
    unsigned int _currentIntegrationMS = 402;

    bool _ledOn;
    byte _ledBrightness = -1;
    byte _ledRealBrightness = -1;
    int _effectSpeed = 0;
    bool _autoDim = false;

    int _effectID;
    int _primaryRedVal;
    int _primaryGreenVal;
    int _primaryBlueVal;
    int _primaryWhiteVal;
    int _secRedVal;
    int _secGreenVal;
    int _secBlueVal;
    int _secWhiteVal;
    double _luxLevel;

    float _currentX = 0;
    float _currentY = 0;
    CRGBW *_leds;
    CRGB *_ledsRGB;
    CRGB *_ledsRGBTemp;
    SFE_TSL2561 *_tsl;
    bool ledConfigChanged = false;
    unsigned long integration_start_ms = 0;
    bool _isCurrentlyIntegrating = false;

    // Store the settings for LED in NV Storage
    ConfigBase& _ledNvValues;
    CRGBW getRGBWFromRGB(CRGB color);
    void convertTempRGBToRGBW();
};