// Auto Dimming LED Strip
// Matt Grammes 2018

#pragma once

#include <FastLED.h>
#include <SparkFunTSL2561.h>
#include <Wire.h>

#include "ConfigNVS.h"
#include "ConfigPinMap.h"
#include "FastLED_RGBW.h"
#include "Utils.h"

class LedStrip {
   public:
    LedStrip(ConfigBase& ledNvValues);
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
    void show();
    void convertTempRGBToRGBW();

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
    int _secRedVal;
    int _secGreenVal;
    int _secBlueVal;
    double _luxLevel;

    float _currentX = 0;
    float _currentY = 0;

    // WE DO NOT WRITE TO THIS USE TMP INSTEAD!!!!!
    CRGBW* _leds;

    // WE DO NOT WRITE TO THIS USE TMP INSTEAD!!!!!
    CRGB* _ledsRGB;

    // WE WRITE TO THIS!
    CRGB* _ledsRGBTemp;

    bool _ledIsRGBW;
    SFE_TSL2561* _tsl;
    bool ledConfigChanged = false;
    unsigned long integration_start_ms = 0;
    bool _isCurrentlyIntegrating = false;

    // Store the settings for LED in NV Storage
    ConfigBase& _ledNvValues;
    CRGBW getRGBWFromRGB(CRGB color);

    const uint8_t gamma8[256] = {
        //    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,        // Change these two lines from '0' to '1' below
        //    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,

        1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,   1,
        1,   1,   1,   1,

        1,   1,   1,   1,   1,   1,   1,   1,   1,   2,   2,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,
        4,   5,   5,   5,   5,   6,   6,   6,   6,   7,   7,   7,   7,   8,   8,   8,   9,   9,   9,   10,  10,  10,  11,  11,  11,  12,  12,  13,
        13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  20,  21,  21,  22,  22,  23,  24,  24,  25,  25,  26,  27,  27,
        28,  29,  29,  30,  31,  32,  32,  33,  34,  35,  35,  36,  37,  38,  39,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  50,
        51,  52,  54,  55,  56,  57,  58,  59,  60,  61,  62,  63,  64,  66,  67,  68,  69,  70,  72,  73,  74,  75,  77,  78,  79,  81,  82,  83,
        85,  86,  87,  89,  90,  92,  93,  95,  96,  98,  99,  101, 102, 104, 105, 107, 109, 110, 112, 114, 115, 117, 119, 120, 122, 124, 126, 127,
        129, 131, 133, 135, 137, 138, 140, 142, 144, 146, 148, 150, 152, 154, 156, 158, 160, 162, 164, 167, 169, 171, 173, 175, 177, 180, 182, 184,
        186, 189, 191, 193, 196, 198, 200, 203, 205, 208, 210, 213, 215, 218, 220, 223, 225, 228, 231, 233, 236, 239, 241, 244, 247, 249, 252, 255};
};