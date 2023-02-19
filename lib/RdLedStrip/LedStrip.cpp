// Auto Dimming LED Strip
// Matt Grammes 2018

// Modified by Aiden Vigue - 2022
// -> Switch to WS2812-based LED strip.

#include "LedStrip.h"
#include "Arduino.h"
#include "ConfigNVS.h"
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

#define FASTLED_ESP32_I2S true
#define LED_PIN 4

static const char* MODULE_PREFIX = "LedStrip: ";

LedStrip::LedStrip(ConfigBase &ledNvValues) : _ledNvValues(ledNvValues)
{
    _isSetup = false;
    _isSleeping = false;
    _ledPin = -1;
}

void LedStrip::setup(ConfigBase* pConfig, const char* ledStripName)
{
    _name = ledStripName;

    // Save config and register callback on config changed
    if (_pHwConfig == NULL)
    {
        _pHwConfig = pConfig;
        _pHwConfig->registerChangeCallback(std::bind(&LedStrip::configChanged, this));
    }


    // Get LED config
    ConfigBase ledConfig(pConfig->getString(ledStripName, "").c_str());
    Log.trace("%ssetup name %s configStr %s\n", MODULE_PREFIX, _name.c_str(),
                    ledConfig.getConfigCStrPtr());


    // LED Strip Negative PWM Pin
    String pinStr = ledConfig.getString("ledPin", "");
    int ledPin = -1;
    if (pinStr.length() != 0)
        ledPin = ConfigPinMap::getPinFromName(pinStr.c_str());

    int ledCount = ledConfig.getLong("ledCount", 0);

    // Ambient Light Sensor Pin
    int sensorEnabled = ledConfig.getLong("tslEnabled", 0);
    int sensorSDA = ledConfig.getLong("tslSDA", 0);
    int sensorSCL = ledConfig.getLong("tslSCL", 0);

    Log.notice("%sLED pin %d TSL enabled: %d TSL SDA: %d TSL SCL: %d count %d\n", MODULE_PREFIX, ledPin, sensorEnabled, sensorSDA, sensorSCL, ledCount);
    // Sensor pin isn't necessary for operation.
    if (ledPin == -1)
        return;

    // Setup led pin
    if (_isSetup && (ledPin != _ledPin))
    {

    }
    else
    {
        _ledPin = ledPin;
        _ledCount = ledCount;
    }

    // Setup the sensor
    _sensorEnabled = sensorEnabled;
    if (_sensorEnabled == 1) {
        _tsl = new Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

        Wire.begin(sensorSDA, sensorSCL);
        if(!_tsl->begin())
        {
            /* There was a problem detecting the TSL2561 ... check your connections */
            Log.info("%sOoops, no TSL2561 detected ... Check your wiring or I2C ADDR!\n", MODULE_PREFIX);
            _sensorEnabled = 0;
        } else {
            Log.info("%sTSL2561 detected!\n", MODULE_PREFIX);
            _tsl->enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
            _tsl->setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */   
        }
    }

    // If there is no LED data stored, set to default
    String ledStripConfigStr = _ledNvValues.getConfigString();
    if (ledStripConfigStr.length() == 0 || ledStripConfigStr.equals("{}")) {
        Log.trace("%sNo LED Data Found in NV Storge, Defaulting\n", MODULE_PREFIX);
        // Default to LED On, Half Brightness
        _ledOn = true;
        _ledBrightness = 0x7f;
        _ledRealBrightness = 0;
        _effectSpeed = 50;
        _autoDim = false;
        _primaryRedVal = 0xcc;
        _primaryGreenVal = 0xcc;
        _primaryBlueVal = 0xcc;
        _secRedVal = 0xcc;
        _secGreenVal = 0xcc;
        _secBlueVal = 0xcc;
        _effectID = 0;
        updateNv();
    } else {
        _ledOn = _ledNvValues.getLong("ledOn", 0) == 1;
        _ledBrightness = _ledNvValues.getLong("ledBrightness", 50);
        _ledRealBrightness = 0;
        _autoDim = _ledNvValues.getLong("autoDim", 0) == 1;
        _effectSpeed = _ledNvValues.getLong("effectSpeed", 0);
        _effectID = _ledNvValues.getLong("effectID", 0);
        _primaryRedVal = _ledNvValues.getLong("primaryRedVal", 127);
        _primaryGreenVal = _ledNvValues.getLong("primaryGreenVal", 127);
        _primaryBlueVal = _ledNvValues.getLong("primaryBlueVal", 127);
        _secRedVal = _ledNvValues.getLong("secRedVal", 127);
        _secGreenVal = _ledNvValues.getLong("secGreenVal", 127);
        _secBlueVal = _ledNvValues.getLong("secBlueVal", 127);

        Log.trace("%sLED Setup from JSON\n", MODULE_PREFIX);
    }

    _leds = new CRGB[_ledCount];
    FastLED.addLeds<NEOPIXEL, LED_PIN>(_leds,_ledCount);
    FastLED.setMaxPowerInVoltsAndMilliamps(5,1000);
    FastLED.setBrightness(_ledBrightness);
    FastLED.show();
    FastLED.clear(true);
    FastLED.show();
    delay(100);

    _isSetup = true;
    // Trigger initial write
    ledConfigChanged = true;
    Log.trace("%sLED Configured: On: %d, Value: %d, AutoDim: %d\n", MODULE_PREFIX, _ledOn, _ledBrightness, _autoDim);
}

void LedStrip::updateLedFromConfig(const char * pLedJson) {

    boolean changed = false;
    boolean ledOn = RdJson::getLong("ledOn", 0, pLedJson) == 1;
    if (ledOn != _ledOn) {
        _ledOn = ledOn;
        changed = true;
    }
    byte ledBrightness = RdJson::getLong("ledBrightness", 0, pLedJson);
    if (ledBrightness != _ledBrightness) {
        _ledBrightness = ledBrightness;
        changed = true;
    }
    boolean autoDim = RdJson::getLong("autoDim", 0, pLedJson) == 1;
    if (autoDim != _autoDim) {
        _autoDim = autoDim;
        changed = true;
    }
    int effectID = RdJson::getLong("effectID", 0, pLedJson);
    if (effectID != _effectID) {
        
        _effectID = effectID;
        changed = true;
    }
    int effectSpeed = RdJson::getLong("effectSpeed", 0, pLedJson);
    if (effectSpeed != _effectSpeed) {
        
        _effectSpeed = effectSpeed;
        changed = true;
    }

    int primaryRedVal = RdJson::getLong("primaryRedVal", 0, pLedJson);
    if (primaryRedVal != _primaryRedVal) {
        
        _primaryRedVal = primaryRedVal;
        changed = true;
    }
    int primaryGreenVal = RdJson::getLong("primaryGreenVal", 0, pLedJson);
    if (primaryGreenVal != _primaryGreenVal) {
        
        _primaryGreenVal = primaryGreenVal;
        changed = true;
    }
    int primaryBlueVal = RdJson::getLong("primaryBlueVal", 0, pLedJson);
    if (primaryBlueVal != _primaryBlueVal) {
        
        _primaryBlueVal = primaryBlueVal;
        changed = true;
    }

    int secRedVal = RdJson::getLong("secRedVal", 0, pLedJson);
    if (secRedVal != _secRedVal) {
        
        _secRedVal = secRedVal;
        changed = true;
    }
    int secGreenVal = RdJson::getLong("secGreenVal", 0, pLedJson);
    if (secGreenVal != _secGreenVal) {
        
        _secGreenVal = secGreenVal;
        changed = true;
    }
    int secBlueVal = RdJson::getLong("secBlueVal", 0, pLedJson);
    if (secBlueVal != _secBlueVal) {
        
        _secBlueVal = secBlueVal;
        changed = true;
    }

    if (changed)
        ledConfigChanged = true;
}

const char* LedStrip::getConfigStrPtr() {
    return _ledNvValues.getConfigCStrPtr();
}

int LedStrip::getLuxLevel() {
    if(_sensorEnabled == 1) {
        return _luxLevel;
    }
    return 0;
}

void LedStrip::service()
{
    // Check if active
    if (!_isSetup)
        return;

    // If the switch is off or sleeping, turn off the led
    if (_ledOn || !_isSleeping)
    {
        // TODO Auto Dim isn't working as expected - this should never go enabled right now
        // Check if we need to read and evaluate the light sensor
        if (_sensorEnabled == 1 && (millis() - _last_check_tsl_time > 3000)) {

            sensors_event_t event;
            _tsl->getEvent(&event);
            _luxLevel = event.light;
            Log.trace("%s lux: %d", MODULE_PREFIX, _luxLevel);

            if(_autoDim) {
                byte ledBrightness;
                if(_luxLevel > 25) {
                    //high brightness
                    ledBrightness = 100;
                } else if(_luxLevel > 10) {
                    //low brightness
                    ledBrightness = 50;
                } else if(_luxLevel > 1) {
                    //low brightness
                    ledBrightness = 5;
                } else {
                    ledBrightness = 0;
                }

                if (_ledBrightness != ledBrightness) {
                    _ledBrightness = ledBrightness;
                    ledConfigChanged = true;
                }
            }

            _last_check_tsl_time = millis();
        }
    }

    if (ledConfigChanged) {
        ledConfigChanged = false;
        updateNv();
    }
}

void LedStrip::serviceStrip() {

    // Check if active
    if (!_isSetup)
        return;

    if (_ledOn) {
        if (_ledRealBrightness != _ledBrightness) {
            int _ledRealBrightnessInt = _ledRealBrightness;
            int _ledBrightnessInt = _ledBrightness;

            if(_ledRealBrightnessInt < _ledBrightnessInt) {
                _ledRealBrightnessInt++;
            } else {
                _ledRealBrightnessInt--;
            }

            _ledRealBrightness = _ledRealBrightnessInt;

            FastLED.setBrightness(_ledRealBrightness);
        }
    } else {
        if(_ledRealBrightness > 0) {
            int _ledRealBrightnessInt = _ledRealBrightness;
            _ledRealBrightnessInt--;
            _ledRealBrightness = _ledRealBrightnessInt;
            FastLED.setBrightness(_ledRealBrightness);
        }
    }

    switch(_effectID) {
        case 0: solid_color(); break;
        case 1: effect_pride(); break;
        default: break;
    }

    FastLED.show();
}

void LedStrip::configChanged()
{
    // Reset config
    Log.trace("%sconfigChanged\n", MODULE_PREFIX);
    setup(_pHwConfig, _name.c_str());
}

void LedStrip::updateNv()
{
    String jsonStr;
    jsonStr += "{";
    jsonStr += "\"ledOn\":";
    jsonStr += _ledOn ? "1" : "0";
    jsonStr += ",";
    jsonStr += "\"ledBrightness\":";
    jsonStr += _ledBrightness;
    jsonStr += ",";
    jsonStr += "\"effectSpeed\":";
    jsonStr += _effectSpeed;
    jsonStr += ",";
    jsonStr += "\"effectID\":";
    jsonStr += _effectID;
    jsonStr += ",";
    jsonStr += "\"primaryRedVal\":";
    jsonStr += _primaryRedVal;
    jsonStr += ",";
    jsonStr += "\"primaryGreenVal\":";
    jsonStr += _primaryGreenVal;
    jsonStr += ",";
    jsonStr += "\"primaryBlueVal\":";
    jsonStr += _primaryBlueVal;
    jsonStr += ",";
    jsonStr += "\"secRedVal\":";
    jsonStr += _secRedVal;
    jsonStr += ",";
    jsonStr += "\"secGreenVal\":";
    jsonStr += _secGreenVal;
    jsonStr += ",";
    jsonStr += "\"secBlueVal\":";
    jsonStr += _secBlueVal;
    jsonStr += ",";
    jsonStr += "\"autoDim\":";
    jsonStr += _sensorEnabled == 1 ? (_autoDim ? "1" : "0") : "-1";
    jsonStr += "}";
    _ledNvValues.setConfigData(jsonStr.c_str());
    _ledNvValues.writeConfig();
    Log.trace("%supdateNv() : wrote %s\n", MODULE_PREFIX, _ledNvValues.getConfigCStrPtr());
}

// Get the average sensor reading
uint16_t LedStrip::getAverageSensorReading() {
    uint16_t sum = 0;
    for (int i = 0; i < NUM_SENSOR_VALUES; i++) {
        sum += sensorValues[i];
    }
    return sum / NUM_SENSOR_VALUES;
}

// Set sleep mode
void LedStrip::setSleepMode(int sleep)
{
    _isSleeping = sleep;
}

//MARK: EFFECTS

void LedStrip::solid_color() 
{
  for( uint16_t i = 0 ; i < _ledCount; i++) {
    CRGB newcolor = CRGB(_primaryRedVal, _primaryGreenVal, _primaryBlueVal);
    _leds[i] = newcolor;
  }
}

void LedStrip::effect_pride() 
{
  static uint16_t sPseudotime = 0;
  static uint16_t sLastMillis = 0;
  static uint16_t sHue16 = 0;
 
  uint8_t sat8 = beatsin88( 87, 220, 250);
  uint8_t brightdepth = beatsin88( 341, 96, 224);
  uint16_t brightnessthetainc16 = beatsin88( 203, (25 * 256), (40 * 256));
  uint8_t msmultiplier = beatsin88(147, 23, 60);

  uint16_t hue16 = sHue16;//gHue * 256;
  uint16_t hueinc16 = beatsin88(113, 1, 3000);
  
  uint16_t ms = millis();
  uint16_t deltams = ms - sLastMillis ;
  sLastMillis  = ms;
  sPseudotime += deltams * msmultiplier;
  sHue16 += deltams * beatsin88( 400, 5,9);
  uint16_t brightnesstheta16 = sPseudotime;
  
  for( uint16_t i = 0 ; i < _ledCount; i++) {
    hue16 += hueinc16;
    uint8_t hue8 = hue16 / 256;

    brightnesstheta16  += brightnessthetainc16;
    uint16_t b16 = sin16( brightnesstheta16  ) + 32768;

    uint16_t bri16 = (uint32_t)((uint32_t)b16 * (uint32_t)b16) / 65536;
    uint8_t bri8 = (uint32_t)(((uint32_t)bri16) * brightdepth) / 65536;
    bri8 += (255 - brightdepth);
    
    CRGB newcolor = CHSV( hue8, sat8, bri8);
    
    uint16_t pixelnumber = i;
    pixelnumber = (_ledCount-1) - pixelnumber;
    
    nblend( _leds[pixelnumber], newcolor, 64);
  }
}
