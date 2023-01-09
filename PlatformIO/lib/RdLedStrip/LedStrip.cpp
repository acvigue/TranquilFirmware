// Auto Dimming LED Strip
// Matt Grammes 2018

// Modified by Aiden Vigue - 2022
// -> Switch to WS2812-based LED strip.

#include "LedStrip.h"
#include "Arduino.h"
#include "ConfigNVS.h"
#include <WS2812fx.h>
#include "rmt_drv.h"
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_TSL2561_U.h>

static const char* MODULE_PREFIX = "LedStrip: ";

LedStrip::LedStrip(ConfigBase &ledNvValues) : _ledNvValues(ledNvValues)
{
    _isSetup = false;
    _isSleeping = false;
    _ledPin = -1;
}

void LedStrip::setup(ConfigBase* pConfig, const char* ledStripName, void (*showFn)())
{
    _name = ledStripName;
    _showFn = showFn;

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

        _ws2812fx = new WS2812FX(_ledCount, _ledPin, NEO_GRB + NEO_KHZ800);
        _ws2812fx->init();
        _ws2812fx->setCustomShow(showFn);
        rmt_tx_int(RMT_CHANNEL_1, _ws2812fx->getPin()); // assign ws2812fx to RMT channel 0
    }

    // Setup the sensor
    _sensorPin = sensorPin;
    if (_sensorPin != -1) {
        _tsl = new Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

        Wire.begin();
        if(!_tsl->begin())
        {
            /* There was a problem detecting the TSL2561 ... check your connections */
            Log.info("%sOoops, no TSL2561 detected ... Check your wiring or I2C ADDR!\n", MODULE_PREFIX);
            _sensorPin = -1;
        } else {
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
        ledConfigChanged = true;
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
            if (_sensorPin != -1 && (millis() - _last_check_tsl_time > 500)) {

                sensors_event_t event;
                _tsl->getEvent(&event);

                byte ledBrightness;
                if(event.light > 100) {
                    //high brightness
                    ledBrightness = 200;
                } else if(event.light > 50) {
                    //low brightness
                    ledBrightness = 120;
                } else if(event.light > 5) {
                    //low brightness
                    ledBrightness = 30;
                } else {
                    ledBrightness = 5;
                }

                if (_ledBrightness != ledBrightness) {
                    _ledBrightness = ledBrightness;
                    ledConfigChanged = true;
                }

                _last_check_tsl_time = millis();
            }
        }
    }

    if (ledConfigChanged) {
        ledConfigChanged = false;
        updateNv();
        _ws2812fx->setColor(_ws2812fx->Color(_redVal, _greenVal, _blueVal));
        _ws2812fx->setBrightness(_ledBrightness);
        _ws2812fx->setSpeed(_effectSpeed);
        _ws2812fx->setMode(_effectID);
        _ws2812fx->start();
    }
}

void LedStrip::serviceStrip() {

    // Check if active
    if (!_isSetup)
        return;

    _ws2812fx->service();
}

uint8_t * LedStrip::getPixelDataPointer() {
    return _ws2812fx->getPixels();
}

uint16_t LedStrip::getNumBytes() {
    return _ws2812fx->getNumBytes();
}

void LedStrip::configChanged()
{
    // Reset config
    Log.trace("%sconfigChanged\n", MODULE_PREFIX);
    setup(_pHwConfig, _name.c_str(), _showFn);
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
