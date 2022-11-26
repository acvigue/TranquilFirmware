// Auto Dimming LED Strip
// Matt Grammes 2018

// Modified by Aiden Vigue - 2022
// -> Switch to WS2812-based LED strip.

#include "LedStrip.h"
#include "Arduino.h"
#include "ConfigNVS.h"
#include <WS2812fx.h>

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
    String sensorStr = ledConfig.getString("sensorPin", "");
    int sensorPin = -1;
    if (sensorStr.length() != 0)
        sensorPin = ConfigPinMap::getPinFromName(sensorStr.c_str());

    Log.notice("%sLED pin %d Sensor pin %d count %d\n", MODULE_PREFIX, ledPin, sensorPin, ledCount);
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

        //TODO: initialize ws2812fx
         Log.info("init ws2812fx");
        _ws2812fx = new WS2812FX(_ledCount, _ledPin, NEO_GRB + NEO_KHZ800);
        _ws2812fx->init();
    }

    // Setup the sensor
    _sensorPin = sensorPin;
    if (_sensorPin != -1) {
        pinMode(_sensorPin, INPUT);
        for (int i = 0; i < NUM_SENSOR_VALUES; i++) {
            sensorValues[i] = 0;
        }
    }

    // If there is no LED data stored, set to default
    String ledStripConfigStr = _ledNvValues.getConfigString();
    if (ledStripConfigStr.length() == 0 || ledStripConfigStr.equals("{}")) {
        Log.trace("%sNo LED Data Found in NV Storge, Defaulting\n", MODULE_PREFIX);
        // Default to LED On, Half Brightness
        _ledOn = true;
        _ledBrightness = 0x7f;
        _effectSpeed = 50;
        _autoDim = false;
        _redVal = 0xcc;
        _greenVal = 0xcc;
        _blueVal = 0xcc;
        _effectID = 0;
        updateNv();
    } else {
        _ledOn = _ledNvValues.getLong("ledOn", 0) == 1;
        _ledBrightness = _ledNvValues.getLong("ledBrightness", 50);
        _autoDim = _ledNvValues.getLong("autoDim", 0) == 1;
        _effectSpeed = _ledNvValues.getLong("effectSpeed", 0);
        _effectID = _ledNvValues.getLong("effectID", 0);
        _redVal = _ledNvValues.getLong("redVal", 127);
        _greenVal = _ledNvValues.getLong("greenVal", 127);
        _blueVal = _ledNvValues.getLong("blueVal", 127);

        Log.trace("%sLED Setup from JSON: %s On: %d, Brightness: %d, Auto Dim: %d, Effect: %d, Color R: %d, G: %d, B: %d\n", MODULE_PREFIX, 
                    ledStripConfigStr.c_str(), _ledOn, _ledBrightness, _autoDim, _effectID, _redVal, _greenVal, _blueVal);
    }

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
    int redVal = RdJson::getLong("redVal", 0, pLedJson);
    if (redVal != _redVal) {
        
        _redVal = redVal;
        changed = true;
    }
    int greenVal = RdJson::getLong("greenVal", 0, pLedJson);
    if (greenVal != _greenVal) {
        
        _greenVal = greenVal;
        changed = true;
    }
    int blueVal = RdJson::getLong("blueVal", 0, pLedJson);
    if (blueVal != _blueVal) {
        
        _blueVal = blueVal;
        changed = true;
    }

    if (changed)
        updateNv();
}

const char* LedStrip::getConfigStrPtr() {
    return _ledNvValues.getConfigCStrPtr();
}

void LedStrip::service()
{
    // Check if active
    if (!_isSetup)
        return;

    // If the switch is off or sleeping, turn off the led
    if (!_ledOn || _isSleeping)
    {
        _ledBrightness = 0x0;
    }
    else
    {
        // TODO Auto Dim isn't working as expected - this should never go enabled right now
        // Check if we need to read and evaluate the light sensor
        if (_autoDim) {
            if (_sensorPin != -1) {
                sensorValues[sensorReadingCount++ % NUM_SENSOR_VALUES] = analogRead(_sensorPin);
                uint16_t sensorAvg = LedStrip::getAverageSensorReading();
                // if (count % 100 == 0) {
                // Log.trace("Ambient Light Avg Value: %d, reading count %d\n", sensorAvg, sensorReadingCount % NUM_SENSOR_VALUES);
                
                // Convert ambient light (int) to led value (byte)
                int ledBrightnessInt = sensorAvg / 4;

                // This case shouldn't be hit
                if (ledBrightnessInt > 255) {
                    ledBrightnessInt = 255;
                    Log.error("%sAverage Sensor Value over max!\n", MODULE_PREFIX);
                }
                byte ledBrightness = ledBrightnessInt;
                if (_ledBrightness != ledBrightness) {
                    _ledBrightness = ledBrightness;
                    updateNv();
                }
            }
        }
    }

    if (ledConfigChanged) {
        ledConfigChanged = false;
        Log.info("settingColor, setting Brightness, setMode");
        _ws2812fx->setColor(_ws2812fx->Color(_redVal, _greenVal, _blueVal));
        _ws2812fx->setBrightness(_ledBrightness);
        _ws2812fx->setMode(_effectID);
        _ws2812fx->start();
    }

    _ws2812fx->service();
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
    jsonStr += "\"redVal\":";
    jsonStr += _redVal;
    jsonStr += ",";
    jsonStr += "\"greenVal\":";
    jsonStr += _greenVal;
    jsonStr += ",";
    jsonStr += "\"blueVal\":";
    jsonStr += _blueVal;
    jsonStr += ",";
    jsonStr += "\"autoDim\":";
    jsonStr += _autoDim ? "1" : "0";
    jsonStr += "}";
    _ledNvValues.setConfigData(jsonStr.c_str());
    _ledNvValues.writeConfig();
    Log.trace("%supdateNv() : wrote %s\n", MODULE_PREFIX, _ledNvValues.getConfigCStrPtr());
    ledConfigChanged = true;
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
