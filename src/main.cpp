// RBotFirmware
// Rob Dobson 2016-2019

// API used for web, MQTT and BLE (future)
//   Get version:    /v                     - returns version info
//   Set WiFi:       /w/ssss/pppp/hhhh      - ssss = ssid, pppp = password - assumes WPA2, hhhh = hostname
//                                          - does not clear previous WiFi so clear first if required
//   Clear WiFi:     /wc                    - clears all stored SSID, etc
//   Set MQTT:       /mq/ss/ii/oo/pp        - ss = server, ii and oo are in/out topics, pp = port
//                                          - in topics / should be replaced by ~
//                                          - (e.g. /devicename/in becomes ~devicename~in)
//   Check updates:  /checkupdate           - check for updates on the update server
//   Reset:          /reset                 - reset device
//   Log level:      /loglevel/lll          - Logging level (for MQTT, HTTP and Papertrail)
//                                          - lll one of v (verbose), t (trace), n (notice), w (warning), e (error), f (fatal)
//   Log to MQTT:    /logmqtt/en/topic      - Control logging to MQTT
//                                          - en = 0 or 1 for off/on, topic is the topic logging messages are sent to
//   Log to HTTP:    /loghttp/en/ip/po/ur   - Control logging to HTTP
//                                          - en = 0 or 1 for off/on
//                                          - ip is the IP address of the computer to log to (or hostname) and po is the port
//                                          - ur is the HTTP url logging messages are POSTed to
//   Log to Papertail:    /logpt/en/ho/po   - Control logging to Papertrail
//                                          - en = 0 or 1 for off/on
//                                          - ho is the papertrail host
//                                          - po is the papertrail port
//   Log to serial:  /logserial/en/port     - Control logging to serial
//                                          - en = 0 or 1 for off/on
//                                          - port is the port number 0 = standard USB port
//   Log to cmd:     /logcmd/en             - Control logging to command port (extra serial if configured)
//                                          - en = 0 or 1 for off/on
//   Set NTP:        /ntp/gmt/dst/s1/s2/s3  - Set NTP server(s)
//                                          - gmt = GMT offset in seconds, dst = Daylight Savings Offset in seconds
//                                          - s1, s2, s3 = NTP servers, e.g. pool.ntp.org

// Don't include this code if unit testing
#ifndef UNIT_TEST

// System type - this is duplicated here to make it easier for automated updater which parses the systemName = "aaaa" line
#define SYSTEM_TYPE_NAME "RBotFirmware"
const char* systemType = "RBotFirmware";

// System version
const char* systemVersion = "3.3.0";

// Build date
const char* buildDate = __DATE__;

// Build date
const char* buildTime = __TIME__;

// Arduino
#include <Arduino.h>

// Logging
#include <ArduinoLog.h>

// WiFi
#include <WiFi.h>

// Utils
#include <Utils.h>

// Config
#include "ConfigFile.h"
#include "ConfigNVS.h"

// WiFi Manager
#include "WiFiManager.h"
WiFiManager wifiManager;

// WireGuard Manager
#include "WireGuardManager.h"
WireGuardManager wireGuardManager;

// NTP Client
#include "NTPClient.h"
NTPClient ntpClient;

// File manager
#include "FileManager.h"
FileManager fileManager;

// API Endpoints
#include "RestAPIEndpoints.h"
RestAPIEndpoints restAPIEndpoints;

// Web server
#include "WebServer.h"
WebServer webServer;

// Firmware update
#include <RdOTAUpdate.h>
RdOTAUpdate otaUpdate;

// Hardware config
static const char* hwConfigJSON = {
    "{"
    "\"unitName\":" SYSTEM_TYPE_NAME
    ","
    "\"wifiEnabled\":1,"
    "\"webServerEnabled\":1,"
    "\"webServerPort\":80,"
    "\"OTAUpdate\":{\"enabled\":1,\"manifestURL\":\"https://pub-085564545d0642ad824517a59d8b1c10.r2.dev/manifest.json\"},"
    "\"ntpConfig\":{\"ntpServer\":\"pool.ntp.org\",\"ntpTimezone\":\"EST5EDT,M3.2.0,M11.1.0\"},"
    "\"defaultRobotType\":\"SandTableScara\""
    "}"};

// Config for hardware
ConfigBase hwConfig(hwConfigJSON);

// Config for robot control
ConfigNVS robotConfig("robot", 2000);

// Config for WiFi
ConfigNVS wifiConfig("wifi", 300);

// Config for WireGuard
ConfigNVS wireGuardConfig("wireguard", 400);

// Config for NTP
ConfigNVS ntpConfig("ntp", 100);

// Config for ***EXPUNGED***
ConfigNVS ***EXPUNGED***Config("***EXPUNGED***", 600);

// REST API System
#include "RestAPISystem.h"
RestAPISystem restAPISystem(wifiManager, wireGuardManager, otaUpdate, fileManager, ntpClient, hwConfig, ***EXPUNGED***Config, systemType, systemVersion);

// Config for LED Strip
ConfigNVS ledStripConfig("ledStrip", 200);

// LED Strip
#include "LedStrip.h"
LedStrip ledStrip(ledStripConfig);

// Robot controller
#include "RobotMotion/RobotController.h"
RobotController _robotController;

// Command interface
#include "WorkManager/WorkManager.h"
WorkManager _workManager(hwConfig, robotConfig, _robotController, ledStrip, restAPISystem, fileManager);

// REST API Robot
#include "RestAPIRobot.h"
RestAPIRobot restAPIRobot(_workManager, fileManager);

TaskHandle_t ledTask;

void ledTaskFunc(void* parameter) {
    for (;;) {
        ledStrip.serviceStrip();
        delay(16);
    }
}

// Setup
void setup() {
    // Logging
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_TRACE, &Serial);

    // Message
    Log.notice("%s %s (built %s %s)\n", systemType, systemVersion, buildDate, buildTime);

    // Robot config
    robotConfig.setup();

    // File system
    fileManager.setup(robotConfig, "robotConfig/fileManager");

    // WiFi Config
    wifiConfig.setup();

    // WireGuard Config
    wireGuardConfig.setup();

    // OTA update
    otaUpdate.setup(hwConfig, systemType, systemVersion);

    // NTP Config
    ntpConfig.setup();

    // ***EXPUNGED*** config
    ***EXPUNGED***Config.setup();

    // WiFi Manager
    wifiManager.setup(hwConfig, &wifiConfig, systemType);

    // WireGuard Manager
    wireGuardManager.setup(&wireGuardConfig);

    // NTP Client
    ntpClient.setup(&hwConfig, "ntpConfig", &ntpConfig);

    // Add API endpoints
    restAPISystem.setup(restAPIEndpoints);
    restAPIRobot.setup(restAPIEndpoints);

    // Web server
    webServer.setup(hwConfig);
    webServer.addEndpoints(restAPIEndpoints);
    webServer.serveStaticFiles("/", "/spiffs/", "public, max-age=86400");
    webServer.serveStaticFiles("/files/sd", "/sd/");
    webServer.enableAsyncEvents("/events");

    // Led Strip Config
    ledStripConfig.setup();

    // Led Strip
    ledStrip.setup(&robotConfig, "robotConfig/ledStrip");

    // Reconfigure the robot and other settings
    _workManager.reconfigure();

    // Handle statup commands
    _workManager.handleStartupCommands();

    // Service LED strip.
    xTaskCreatePinnedToCore(ledTaskFunc, /* Function to implement the task */
                            "Task1",     /* Name of the task */
                            1500,        /* Stack size in words */
                            NULL,        /* Task input parameter */
                            1,           /* Priority of the task */
                            &ledTask,    /* Task handle. */
                            0);          /* Core where the task should run */
}

// Loop
void loop() {
    wifiManager.service();
    wireGuardManager.service();

    // Service the web server
    if (wifiManager.isConnected()) {
        webServer.begin(true);
    }

    restAPISystem.service();
    otaUpdate.service(!_workManager.queueIsEmpty());
    ntpClient.service();

    // Check for changes to status
    if (_workManager.checkStatusChanged()) {
        // Send changed status
        String newStatus;
        _workManager.queueIsEmpty();
        _workManager.queryStatus(newStatus);
        webServer.sendAsyncEvent(newStatus.c_str(), "status");
    }

    _workManager.service();
    _robotController.service();

    // Give the LED strip our current position in x,y
    RobotCommandArgs args;
    _robotController.getCurStatus(args);
    ledStrip.service(args.getPointMM().getVal(0), args.getPointMM().getVal(1));
}

#endif  // UNIT_TEST
