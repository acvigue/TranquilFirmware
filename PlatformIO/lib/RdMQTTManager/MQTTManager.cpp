// MQTT Manager
// Rob Dobson 2018

// Added SSL with credentials support
// Aiden Vigue 2023

#include "MQTTManager.h"
#include "Utils.h"

static const char* MODULE_PREFIX = "MQTTManager: ";

#ifdef MQTT_USE_ASYNC_MQTT

void MQTTManager::onMqttMessage(char *topic, byte *payload, unsigned int len)
{
    Log.verbose("%srx topic: %s len %d\n", MODULE_PREFIX, topic, len);
    const int MAX_RESTAPI_REQ_LEN = 200;
    if (len < MAX_RESTAPI_REQ_LEN)
    {
        char* pReq = new char[len + 1];
        if (pReq)
        {
            memcpy(pReq, payload, len);
            pReq[len] = 0;
            String reqStr(pReq);
            delete[] pReq;
            // Log.notice("%srx payload: %s\n", MODULE_PREFIX, reqStr);
            String respStr;
            _restAPIEndpoints.handleApiRequest(reqStr.c_str(), respStr);
        }
    }
}
#endif

void MQTTManager::setup(ConfigBase& hwConfig, ConfigBase *pConfig)
{
    // Check enabled
    _mqttEnabled = hwConfig.getLong("mqttEnabled", 0) != 0;
    _pConfigBase = pConfig;
    if ((!_mqttEnabled) || (!pConfig))
        return;

    // Get the server, port and in/out topics if available
    _mqttServer = pConfig->getString("MQTTServer", "");
    String mqttPortStr = pConfig->getString("MQTTPort", String(DEFAULT_MQTT_PORT).c_str());
    _mqttPort = mqttPortStr.toInt();
    _mqttInTopic = pConfig->getString("MQTTInTopic", "");
    _mqttOutTopic = pConfig->getString("MQTTOutTopic", "");
    _mqttUsername = pConfig->getString("MQTTUsername", "");
    _mqttPassword = pConfig->getString("MQTTPassword", "");
    if (_mqttServer.length() == 0)
        return;

    // Debug
    Log.notice("%sserver %s:%d, inTopic %s, outTopic %s, username %s, password %s\n", MODULE_PREFIX, _mqttServer.c_str(), _mqttPort, _mqttInTopic.c_str(), _mqttOutTopic.c_str(), _mqttUsername.c_str(), _mqttPassword.c_str());

    // Setup server and callback on receive
    _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    _mqttClient.setCallback(std::bind(&MQTTManager::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2,
                                        std::placeholders::_3));
}

String MQTTManager::formConfigStr()
{
    // This string is stored in NV ram for configuration on power up
    return "{\"MQTTServer\":\"" + _mqttServer + "\",\"MQTTPort\":\"" + String(_mqttPort) + "\",\"MQTTInTopic\":\"" +
            _mqttInTopic + "\",\"MQTTOutTopic\":\"" + _mqttOutTopic + "\",\"MQTTUsername\":\"" + _mqttUsername + "\",\"MQTTPassword\":\"" + _mqttPassword + "\"}";
}

void MQTTManager::setMQTTServer(String &mqttServer, String &mqttInTopic, 
                String &mqttOutTopic, String &mqttUsername, 
                String &mqttPassword, int mqttPort)
{
    _mqttServer = mqttServer;
    if (mqttInTopic.length() > 0)
        _mqttInTopic = mqttInTopic;
    if (mqttOutTopic.length() > 0)
        _mqttOutTopic = mqttOutTopic;
    if (mqttUsername.length() > 0)
        _mqttUsername = mqttUsername;
    if (mqttOutTopic.length() > 0)
        _mqttPassword = mqttPassword;
    _mqttPort = mqttPort;
    if (_pConfigBase)
    {
        _pConfigBase->setConfigData(formConfigStr().c_str());
        _pConfigBase->writeConfig();
    }

    // Check if enabled
    if (!_mqttEnabled)
        return;
    // Disconnect so re-connection with new credentials occurs
    if (_mqttClient.connected())
    {
        Log.trace("%ssetMQTTServer disconnecting to allow new connection\n", MODULE_PREFIX);
        _mqttClient.disconnect();
        // Indicate that server credentials need to be set when disconnect is complete
        _wasConnected = true;
    }
    else
    {
        Log.notice("%ssetMQTTServer %s:%n\n", MODULE_PREFIX, _mqttServer.c_str(), _mqttPort);
        _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
    }
}

void MQTTManager::reportJson(String& msg)
{
    report(msg.c_str());
}

void MQTTManager::report(const char *reportStr)
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

    // Check if connected
    if (!_mqttClient.connected())
    {
        // Store until connected
        if (_mqttMsgToSendWhenConnected.length() == 0)
            _mqttMsgToSendWhenConnected = reportStr;
        return;
    }

    // Send immediately
    int publishRslt = _mqttClient.publish(_mqttOutTopic.c_str(), reportStr, true);
    Log.verbose("%sPublished to %s at QoS 0, publishRslt %d\n", MODULE_PREFIX, _mqttOutTopic.c_str(), publishRslt);
}

// Note do not put any Log messages in here as MQTT may be used for logging
// and an infinite loop would result
void MQTTManager::reportSilent(const char *reportStr)
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

    // Check if connected
    if (!_mqttClient.connected())
    {
        // Store until connected
        if (_mqttMsgToSendWhenConnected.length() == 0)
            _mqttMsgToSendWhenConnected = reportStr;
        return;
    }

    // Send immediately
    _mqttClient.publish(_mqttOutTopic.c_str(), reportStr, true);
}

void MQTTManager::service()
{
    // Check if enabled
    if (!_mqttEnabled)
        return;

    // See if we are connected
    if (_mqttClient.connected())
    {
        // Service the client
        _mqttClient.loop();
    }
    else
    {

        // Check if we were previously connected
        if (_wasConnected)
        {
            // Set last connected time here so we hold off for a short time after disconnect
            // before trying to reconnect
            _lastConnectRetryMs = millis();
            Log.notice("%sDisconnected, status code %d\n", MODULE_PREFIX, _mqttClient.state());
            // Set server for next connection (in case it was changed)
            _mqttClient.setServer(_mqttServer.c_str(), _mqttPort);
            _wasConnected = false;
            return;
        }

        // Check if ready to reconnect
        if (_wifiManager.isConnected() && _mqttServer.length() > 0)
        {
            if (Utils::isTimeout(millis(), _lastConnectRetryMs, CONNECT_RETRY_MS))
            {
                Log.notice("%sConnecting to MQTT client\n", MODULE_PREFIX);
                if (_mqttClient.connect(_wifiManager.getHostname().c_str(), _mqttUsername.c_str(), _mqttPassword.c_str()))
                {
                    // Connected
                    Log.notice("%sConnected to MQTT\n", MODULE_PREFIX);
                    _wasConnected = true;
                    
                    // Subscribe to in topic
                    if (_mqttInTopic.length() > 0)
                    {
                        bool subscribedOk = _mqttClient.subscribe(_mqttInTopic.c_str(), 0);
                        Log.notice("%sSubscribing to %s at QoS 0, subscribe %s\n", MODULE_PREFIX, _mqttInTopic.c_str(), 
                                        subscribedOk ? "OK" : "Failed");
                    }

                    // Re-send last message sent when disconnected (if any)
                    if (_mqttOutTopic.length() > 0 && _mqttMsgToSendWhenConnected.length() > 0)
                    {
                        // Send message "queued" to be sent
                        bool publishedOk = _mqttClient.publish(_mqttOutTopic.c_str(), _mqttMsgToSendWhenConnected.c_str(), true);
                        Log.verbose("%sPublished to %s at QoS 0, result %s\n", MODULE_PREFIX, _mqttOutTopic.c_str(), 
                                                publishedOk ? "OK" : "Failed");
                        _mqttMsgToSendWhenConnected = "";
                    }

                }
                else
                {
                    Log.notice("%sConnect failed\n", MODULE_PREFIX);
                }
                _lastConnectRetryMs = millis();
            }
        }
    }
}


