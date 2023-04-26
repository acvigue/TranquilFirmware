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
const char* systemVersion = "3.1.2";

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

// Status LED
#include "StatusIndicator.h"
StatusIndicator wifiStatusLed;

// Config
#include "ConfigNVS.h"
#include "ConfigFile.h"

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

// MQTT
#include "MQTTManager.h"
MQTTManager mqttManager(wifiManager, restAPIEndpoints);

// Firmware update
#include <RdOTAUpdate.h>
RdOTAUpdate otaUpdate;

#include "***EXPUNGED***.h"

// Hardware config
static const char *hwConfigJSON = {
    "{"
    "\"unitName\":" SYSTEM_TYPE_NAME ","
    "\"wifiEnabled\":1,"
    "\"mqttEnabled\":0,"
    "\"webServerEnabled\":1,"
    "\"webServerPort\":80,"
    "\"OTAUpdate\":{\"enabled\":1,\"manifestURL\":\"https://pub-085564545d0642ad824517a59d8b1c10.r2.dev/manifest.json\"},"
    "\"serialConsole\":{\"portNum\":0},"
    "\"***EXPUNGED***\":{\"email\":" WEBCENTER_EMAIL ",\"password\":" WEBCENTER_PASSWORD ",\"sisbot_id\":" SISBOT_PI_ID ",\"sisbot_mac\":" SISBOT_PI_MAC_ADDR "},"
    "\"commandSerial\":{\"portNum\":-1,\"baudRate\":115200},"
    "\"ntpConfig\":{\"ntpServer\":\"pool.ntp.org\",\"ntpTimezone\":\"EST5EDT,M3.2.0,M11.1.0\"},"
    "\"defaultRobotType\":\"SandTableScara\""
    "}"
};

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

// Config for MQTT
ConfigNVS mqttConfig("mqtt", 300);

// Config for network logging
ConfigNVS netLogConfig("netLog", 200);

// Config for CommandScheduler
ConfigNVS cmdSchedulerConfig("cmdSched", 500);

// CommandScheduler - time-based commands
#include "CommandScheduler.h"
CommandScheduler commandScheduler;

// CommandSerial port - used to monitor activity remotely and send commands
#include "CommandSerial.h"
CommandSerial commandSerial(fileManager);

// Serial console - for configuration
#include "SerialConsole.h"
SerialConsole serialConsole;

// NetLog
#include "NetLog.h"
NetLog netLog(Serial, mqttManager, commandSerial);

// REST API System
#include "RestAPISystem.h"
RestAPISystem restAPISystem(wifiManager, wireGuardManager, mqttManager,
                            otaUpdate, netLog, fileManager, ntpClient,
                            commandScheduler, hwConfig,
                            systemType, systemVersion);

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
WorkManager _workManager(hwConfig,
                robotConfig, 
                _robotController,
                ledStrip,
                restAPISystem,
                fileManager,
                commandScheduler);

// REST API Robot
#include "RestAPIRobot.h"
RestAPIRobot restAPIRobot(_workManager, fileManager);

// Debug loop used to time main loop
#include "DebugLoopTimer.h"

// Debug loop timer and callback function
void debugLoopInfoCallback(String &infoStr)
{
    if (wifiManager.isEnabled())
        infoStr = wifiManager.getHostname() + " V" + String(systemVersion) + " SSID " + WiFi.SSID() + " IP " + WiFi.localIP().toString() + " Heap " + String(ESP.getFreeHeap());
    else
        infoStr = "WiFi Disabled, Heap " + String(ESP.getFreeHeap());
    infoStr += _workManager.getDebugStr();
    infoStr += _robotController.getDebugStr();
}
DebugLoopTimer debugLoopTimer(10000, debugLoopInfoCallback);

TaskHandle_t ledTask;

void ledTaskFunc(void * parameter) {
    for(;;) {
        ledStrip.serviceStrip();
        delay(16);
    }
}

// Setup
void setup()
{
    // Logging
    Serial.begin(115200);
    Log.begin(LOG_LEVEL_TRACE, &netLog);

    // Message
    Log.notice("%s %s (built %s %s)\n", systemType, systemVersion, buildDate, buildTime);

    // Robot config
    robotConfig.setup();

    // Status Led
    wifiStatusLed.setup(&robotConfig, "robotConfig/wifiLed");

    // File system
    fileManager.setup(robotConfig, "robotConfig/fileManager");

    // WiFi Config
    wifiConfig.setup();

    // WireGuard Config
    wireGuardConfig.setup();

    //OTA update
    otaUpdate.setup(hwConfig, systemType, systemVersion);

    // NTP Config
    ntpConfig.setup();

    // MQTT Config
    mqttConfig.setup();

    // NetLog Config
    netLogConfig.setup();

    // Command scheduler
    commandScheduler.setup(&robotConfig, "cmdSched", &cmdSchedulerConfig, restAPIEndpoints);

    // Serial console
    serialConsole.setup(hwConfig, restAPIEndpoints);

    // WiFi Manager
    wifiManager.setup(hwConfig, &wifiConfig, systemType, &wifiStatusLed);

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

    // MQTT
    mqttManager.setup(hwConfig, &mqttConfig);

    // Network logging
    netLog.setup(&netLogConfig, wifiManager.getHostname().c_str());

    // Led Strip Config
    ledStripConfig.setup();

    // Led Strip
    ledStrip.setup(&robotConfig, "robotConfig/ledStrip");

    // Add debug blocks
    debugLoopTimer.blockAdd(0, "LoopTimer");
    debugLoopTimer.blockAdd(1, "WiFi");
    debugLoopTimer.blockAdd(2, "WireGuard");
    debugLoopTimer.blockAdd(3, "Web");
    debugLoopTimer.blockAdd(4, "SysAPI");
    debugLoopTimer.blockAdd(5, "Console");
    debugLoopTimer.blockAdd(6, "MQTT");
    debugLoopTimer.blockAdd(7, "OTA");
    debugLoopTimer.blockAdd(8, "NetLog");
    debugLoopTimer.blockAdd(9, "NTP");
    debugLoopTimer.blockAdd(10, "Sched");
    debugLoopTimer.blockAdd(11, "WifiLed");
    debugLoopTimer.blockAdd(12, "Status");
    debugLoopTimer.blockAdd(13, "Flow");
    debugLoopTimer.blockAdd(14, "Robot");
    debugLoopTimer.blockAdd(15, "LedStrip");

    // Reconfigure the robot and other settings
    _workManager.reconfigure();

    // Handle statup commands
    _workManager.handleStartupCommands();


    //Service LED strip.
    xTaskCreatePinnedToCore(
      ledTaskFunc, /* Function to implement the task */
      "Task1", /* Name of the task */
      1500,  /* Stack size in words */
      NULL,  /* Task input parameter */
      1,  /* Priority of the task */
      &ledTask,  /* Task handle. */
      0); /* Core where the task should run */
}

// Loop
void loop()
{

    // Debug loop Timing
    debugLoopTimer.blockStart(0);
    debugLoopTimer.service();
    debugLoopTimer.blockEnd(0);

    // Service WiFi
    debugLoopTimer.blockStart(1);
    wifiManager.service();
    debugLoopTimer.blockEnd(1);

    // Service WireGuard
    debugLoopTimer.blockStart(2);
    wireGuardManager.service();
    debugLoopTimer.blockEnd(2);

    // Service the web server
    if (wifiManager.isConnected())
    {
        // Begin the web server
        debugLoopTimer.blockStart(3);
        webServer.begin(true);
        debugLoopTimer.blockEnd(3);
    }

    // Service the system API (restart)
    debugLoopTimer.blockStart(4);
    restAPISystem.service();
    debugLoopTimer.blockEnd(4);

    // Serial console
    debugLoopTimer.blockStart(5);
    serialConsole.service();
    debugLoopTimer.blockEnd(5);

    // Service MQTT
    debugLoopTimer.blockStart(6);
    //mqttManager.service();
    debugLoopTimer.blockEnd(6);

    // Service OTA Update
    debugLoopTimer.blockStart(7);
    otaUpdate.service(!_workManager.queueIsEmpty());
    debugLoopTimer.blockEnd(7);

    // Service NetLog
    debugLoopTimer.blockStart(8);
    //netLog.service(serialConsole.getXonXoff());
    debugLoopTimer.blockStart(8);

    // Service NTP
    debugLoopTimer.blockStart(9);
    ntpClient.service();
    debugLoopTimer.blockEnd(9);

    // Service command scheduler
    debugLoopTimer.blockStart(10);
    commandScheduler.service();
    debugLoopTimer.blockEnd(10);

    // Service the status LED
    debugLoopTimer.blockStart(11);
    wifiStatusLed.service();
    debugLoopTimer.blockEnd(11);

    // Check for changes to status
    debugLoopTimer.blockStart(12);
    if (_workManager.checkStatusChanged())
    {
        // Send changed status
        String newStatus;
        _workManager.queueIsEmpty();
        _workManager.queryStatus(newStatus);
        webServer.sendAsyncEvent(newStatus.c_str(), "status");
    }
    debugLoopTimer.blockEnd(12);

    // Service the command interface (which pumps the workflow queue)
    debugLoopTimer.blockStart(13);
    _workManager.service();
    debugLoopTimer.blockEnd(13);

    // Service the robot controller
    debugLoopTimer.blockStart(14);
    _robotController.service();
    debugLoopTimer.blockEnd(14);

    //Give the LED strip our current position in x,y
    debugLoopTimer.blockStart(15);
    RobotCommandArgs args;
    _robotController.getCurStatus(args);
    ledStrip.service(args.getPointMM().getVal(0), args.getPointMM().getVal(1));
    debugLoopTimer.blockEnd(15);
}

#endif // UNIT_TEST
