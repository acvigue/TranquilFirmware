# TranquilFirmware

[![MIT License](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/)
[![CodeFactor](https://www.codefactor.io/repository/github/acvigue/tranquilfirmware/badge)](https://www.codefactor.io/repository/github/acvigue/tranquilfirmware)

Firmware for the ESP32-based Tranquil kinetic sand drawing robots.

# This project has been Archived. Please see [here](https://github.com/koiosdigital/Tranquil-fw) for the latest firmware.

## Requirements

This is *not* a drop-in replacement for Rob Dobson's original RBotFirmware! Many things have been changed/moved around.

Hardware:
- Stepper drivers (preferably TMC2208/2209, generics can be used)
- Endstops
- ESP32, must have at least 4MB flash for proper OTA operation
- SD Card - can either use MMC mode or SPI mode, SPI mode can use any pins, albeit is much slower.

> This project requires an operational [TranquilAPI](https://github.com/acvigue/TranquilAPI) server! Run your own on CloudFlare Workers!

## Notes

The firmware is setup to automatically check for updates from a central server. The firmware binaries are built upon commit to this repository and are automatically uploaded to the OTA server.

See [cloudflare-ota-server](https://github.com/acvigue/cloudflare-ota-server) for more information. 

## Robot Configuration Reference

Robot configuration is stored in NVRAM and can be viewed by sending GET request to `/settings/robot` and can be changed by POSTing JSON to `/settings/robot`

Similarly, wifi/ntp/scheduler/light settings can be changed at

- /settings/wifi < Network settings (supports WPA2-Enterprise)
- /settings/ntp < time zone and ntp server
- /settings/scheduler < stores scheduled commands
- /settings/lights < LED lights
- /settings/security < used to store PIN code
- /settings/tranquil < only used for storing TranquilAPI encrypted JWT

> LED strip _must_ be attached to GPIO5

```js
//See RobotConfig.example.json for an uncommented version.
{
  "robotConfig": {
    "robotType": "TranquilSmall", //custom robot config name, please change if building your own and submitting PR.
    "cmdsAtStart": "", //commands to run on startup, seperated by ';' ex. "G28" to home.
    "evaluators": {
      "thrContinue": 0, //must be 0
      "thrThetaMirrored": 1, //to mirror theta axis or not (flip drawings)
      "thrThetaOffsetAngle": 0.5 //rotate drawings around the bed (DEGREES)
    },
    "robotGeom": {
      "model": "SandBotRotary", //keep SandBotRotary
      "motionController": {
        "chip": "TMC2209", //tmc2208 or tmc2209, omit whole object if not using Trinamics, if you are, rest of params are required.
        "TX1": 32, //UART for drv1
        "TX2": 33, //UART for drv2, rest of params are self explanatory.
        "driver_TOFF": 4, //hysterisis TOFF time
        "run_current": 600, //motor run current (mA)
        "microsteps": 16, //motor microsteps
        "stealthChop": 1 //stealthchop, sets stealthchop2 for 2209, stealthchop1 for 2208,2130
      },
      "homing": {
        //homing string, axis A is rotary, B linear.
        "homingSeq": "FR3;A+38400n;B+3200;#;A+38400N;B+3200;#;A+200;#B+400;#;B+30000n;#;B-30000N;#;B-340;#;A=h;B=h;$",
        "maxHomingSecs": 120
      },
      "blockDistanceMM": 1, //movement resolution in mm (keep at 1, lower stalls bot)
      "allowOutOfBounds": 0, //keep 0
      "stepEnablePin": "25", //motor enable GPIO pin
      "stepEnLev": 0, //motor active logic level
      "stepDisableSecs": 30, //seconds after last move to turn motors off
      "axis0": {
        "maxSpeed": 15, //no idea
        "maxAcc": 25, //no idea
        "maxRPM": 4, //max RPM for rotary axis
        "stepsPerRot": 38400, //steps (including microsteps) for one full rotation of the primary rotary axis
        "stepPin": "19", //step pin for this axis
        "dirnPin": "21", //dir pin for this axis
        "dirnRev": "1", //is direction reversed?
        "endStop0": {
            "sensePin": "22", //endstop GPIO pin
            "actLvl": 0, //triggered at what level?
            "inputType": "INPUT"
        }
      },
      "axis1": { //same as above, changes highlighted
        "maxSpeed": 15,
        "maxAcc": 25,
        "maxRPM": 30,
        "stepsPerRot": 3200,
        "unitsPerRot": 40.5, //for each rotation of the upper central gear, how much does the linear arm move in MM
        "maxVal": 145, //maximum MM the linear arm can go out from center
        "stepPin": "27",
        "dirnRev": "1",
        "dirnPin": "3",
        "endStop0": { "sensePin": "23", "actLvl": 0, "inputType": "INPUT" }
      }
    },
    "fileManager": {
      "spiffsEnabled": 1, //must be 1
      "spiffsFormatIfCorrupt": 1, //self explanatory, i advise against it.
      "sdEnabled": 1, //must be 1
      "sdSPI": 0, //OPTIONAL, set to 1 for sdspi, if omitted, SDMMC is used.
      "sdMISO": 0, //REQUIRED FOR SDSPI, omit for SDMMC, miso pin
      "sdMOSI": 0, //REQUIRED FOR SDSPI, omit for SDMMC, mosi pin
      "sdCLK": 0, //REQUIRED FOR SDSPI, omit for SDMMC, clock pin
      "sdCS": 0, //REQUIRED FOR SDSPI, omit for SDMMC, CS pin
      "sdLanes": 1 //1 or 4, sets bus width for SDMMC, not used for SDSPI
    },
    "ledStrip": {
      "ledRGBW": 1, //1 for SK6812, 0 for ws2812
      "ledCount": "143", //led count
      "tslEnabled": "1", //does robot have TSL2561
      "tslSDA": "16", //if so, i2c pins.
      "tslSCL": "17" // ^^^
    }
  },
  "cmdSched": { "jobs": [] }, //don't edit, doesn't do anything but fw still relies on it being here
  "name": "Tranquil" //robot name, not hostname, that is set in wifi settings.
}
```
