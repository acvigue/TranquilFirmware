// WebServer
// Rob Dobson 2012-2018

#include <WebServer.h>
#if defined(ESP8266)
#include "ESPAsyncTCP.h"
#else
#include "AsyncTCP.h"
#endif
#include <ESPAsyncWebServer.h>

#include "AsyncStaticFileHandler.h"
#include "ConfigPinMap.h"
#include "RestAPIEndpoints.h"
#include "WebServerResource.h"

static const char *MODULE_PREFIX = "WebServer: ";

WebServer::WebServer(ConfigBase &tranquilConfig) : _tranquilConfig(tranquilConfig) {
    _pServer = NULL;
    _begun = false;
    _webServerEnabled = false;
    _pAsyncEvents = NULL;
}

WebServer::~WebServer() {
    if (_pServer) delete _pServer;
}

void WebServer::setup(ConfigBase &hwConfig) {
    // Enable
    _webServerEnabled = hwConfig.getLong("webServerEnabled", 0) != 0;

    // Create server
    if (_webServerEnabled) {
        int port = hwConfig.getLong("webServerPort", 80);
        _pServer = new AsyncWebServer(port);
    }
}

String WebServer::recreatedReqUrl(AsyncWebServerRequest *request) {
    String reqUrl = request->url();
    // Add parameters
    int numArgs = request->args();
    for (int i = 0; i < numArgs; i++) {
        if (i == 0)
            reqUrl += "?";
        else
            reqUrl += "&";
        reqUrl = reqUrl + request->argName(i) + "=" + request->arg(i);
    }
    return reqUrl;
}

void WebServer::addEndpoints(RestAPIEndpoints &endpoints) {
    // Check enabled
    if (!_pServer) return;

    // Number of endpoints to add
    int numEndPoints = endpoints.getNumEndpoints();

    // Add each endpoint
    for (int i = 0; i < numEndPoints; i++) {
        // Get endpoint info
        RestAPIEndpointDef *pEndpoint = endpoints.getNthEndpoint(i);
        if (!pEndpoint) continue;

        // Mapping of GET/POST/ETC
        int webMethod = HTTP_GET;
        switch (pEndpoint->_endpointMethod) {
            case RestAPIEndpointDef::ENDPOINT_PUT:
                webMethod = HTTP_PUT;
                break;
            case RestAPIEndpointDef::ENDPOINT_POST:
                webMethod = HTTP_POST;
                break;
            case RestAPIEndpointDef::ENDPOINT_DELETE:
                webMethod = HTTP_DELETE;
                break;
            default:
                webMethod = HTTP_GET;
                break;
        }

        // Handle the endpoint
        _pServer->on(("/" + pEndpoint->_endpointStr).c_str(), webMethod,

                     // Handler for main request URL
                     [pEndpoint, this](AsyncWebServerRequest *request) {
                         // Authenticate (but allow posts to security endpoint to prevent 401 when editing)
                         if (request->method() != HTTP_POST || request->url().indexOf("settings/security") == -1) {
                             if (_tranquilConfig.getLong("pinEnabled", 0) == 1) {
                                 String password = _tranquilConfig.getString("pinCode", "");
                                 if (!request->authenticate("tranquil", password.c_str())) {
                                     AsyncWebServerResponse *response = request->beginResponse(401);
                                     request->send(response);
                                     return;
                                 }
                             }
                         }

                         // Default response
                         String respStr("{ \"rslt\": \"unknown\" }");

                         // Make the required action
                         if (pEndpoint->_endpointType == RestAPIEndpointDef::ENDPOINT_CALLBACK) {
                             String reqUrl = recreatedReqUrl(request);
                             Log.verbose("%sCalling %s url %s\n", MODULE_PREFIX, pEndpoint->_endpointStr.c_str(), request->url().c_str());
                             pEndpoint->callback(reqUrl, respStr);
                         }
                         request->send(200, "application/json", respStr.c_str());
                     },

                     // Handler for upload (as in a file upload)
                     [pEndpoint](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool finalBlock) {
                         String reqUrl = recreatedReqUrl(request);
                         pEndpoint->callbackUpload(reqUrl, filename, request ? request->contentLength() : 0, index, data, len, finalBlock);
                     },

                     // Handler for body
                     [pEndpoint, this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                         if (_tranquilConfig.getLong("pinEnabled", 0) == 1) {
                             String password = _tranquilConfig.getString("pinCode", "");
                             if (!request->authenticate("tranquil", password.c_str())) {
                                 AsyncWebServerResponse *response = request->beginResponse(401);
                                 request->send(response);
                                 return;
                             }
                         }

                         String reqUrl = recreatedReqUrl(request);
                         pEndpoint->callbackBody(reqUrl, data, len, index, total);
                     });

        // Handle preflight requests
        _pServer->on(("/" + pEndpoint->_endpointStr).c_str(), HTTP_OPTIONS, [pEndpoint](AsyncWebServerRequest *request) { request->send(200); });
    }

    // Handle 404 errors
    _pServer->onNotFound([](AsyncWebServerRequest *request) {
        if (WiFi.getMode() == WIFI_AP) {
            Log.verboseln("WAP request 302: host %s, url %s", request->host(), request->url());
            AsyncWebServerResponse *response = request->beginResponse(302);
            response->addHeader("Location", "http://8.8.4.4/");
            response->addHeader("Cache-Control", "no-cache");
            request->send(response);
        } else {
            Log.verboseln("request 404: host %s, url %s", request->host(), request->url());
            AsyncWebServerResponse *response = request->beginResponse(404);
            request->send(response);
        }
    });
}

void WebServer::begin(bool accessControlAllowOriginAll) {
    if (_pServer && !_begun) {
        if (accessControlAllowOriginAll) DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
        _pServer->begin();
        _begun = true;
        Log.notice("%sbegun\n", MODULE_PREFIX);
    }
}

// Add resources to the web server
void WebServer::addStaticResources(const WebServerResource *pResources, int numResources) {
    // Add each resource
    for (int i = 0; i < numResources; i++) {
        String alias = pResources[i]._pResId;
        alias.trim();
        if (!alias.startsWith("/")) alias = "/" + alias;
        addStaticResource(&(pResources[i]), alias.c_str());
        // Check for index.html and alias to "/"
        if (strcasecmp(pResources[i]._pResId, "index.html") == 0) {
            addStaticResource(&(pResources[i]), "/");
        }
    }
}

void WebServer::parseAndAddHeaders(AsyncWebServerResponse *response, const char *pHeaders) {
    String headerStr(pHeaders);
    int strPos = 0;
    while (strPos < headerStr.length()) {
        int sepPos = headerStr.indexOf(":", strPos);
        if (sepPos == -1) break;
        String headerName = headerStr.substring(strPos, sepPos);
        headerName.trim();
        int lineEndPos = std::max(headerStr.indexOf("\r"), headerStr.indexOf("\n"));
        if (lineEndPos == -1) lineEndPos = headerStr.length() - 1;
        String headerVal = headerStr.substring(sepPos + 1, lineEndPos);
        headerVal.trim();
        if ((headerName.length() > 0) && (headerVal.length() > 0)) {
            Log.notice("%sAdding header %s: %s", MODULE_PREFIX, headerName.c_str(), headerVal.c_str());
        }
        // Move to next header
        strPos = lineEndPos + 1;
    }
}

void WebServer::addStaticResource(const WebServerResource *pResource, const char *pAliasPath) {
    // Check enabled
    if (!_pServer) return;

    // Check for alias
    const char *pPath = pResource->_pResId;
    if (pAliasPath != NULL) pPath = pAliasPath;

    // Static pages
    _pServer->on(pPath, HTTP_GET, [pResource](AsyncWebServerRequest *request) {
        AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", pResource->_pData, pResource->_dataLen);
        if ((pResource->_pContentEncoding != NULL) && (strlen(pResource->_pContentEncoding) != 0))
            response->addHeader("Content-Encoding", pResource->_pContentEncoding);
        if ((pResource->_pAccessControlAllowOrigin != NULL) && (strlen(pResource->_pAccessControlAllowOrigin) != 0))
            response->addHeader("Access-Control-Allow-Origin", pResource->_pAccessControlAllowOrigin);
        if (pResource->_noCache) response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        if ((pResource->_pExtraHeaders != NULL) && (strlen(pResource->_pExtraHeaders) != 0)) parseAndAddHeaders(response, pResource->_pExtraHeaders);
        request->send(response);
    });
}

void WebServer::serveStaticFiles(const char *baseUrl, const char *baseFolder, const char *cache_control) {
    // Check enabled
    if (!_pServer) return;

    AsyncStaticFileHandler *handler = new AsyncStaticFileHandler(baseUrl, baseFolder, cache_control);
    _pServer->addHandler(handler);
}

void WebServer::enableAsyncEvents(const String &eventsURL) {
    // Enable events
    if (_pAsyncEvents) return;
    _pAsyncEvents = new AsyncEventSource(eventsURL);
    if (!_pAsyncEvents) return;

    // Add handler for events
    _pServer->addHandler(_pAsyncEvents);
}

void WebServer::sendAsyncEvent(const char *eventContent, const char *eventGroup) {
    if (_pAsyncEvents) _pAsyncEvents->send(eventContent, eventGroup, millis());
}

void WebServer::webSocketOpen(const String &websocketURL) {
    // Check enabled
    if (!_pServer) return;

    // Add
    _pWebSocket = new AsyncWebSocket(websocketURL);
    _pServer->addHandler(_pWebSocket);
}

void WebServer::webSocketSend(const uint8_t *pBuf, uint32_t len) {
    if (_pWebSocket) _pWebSocket->binaryAll(const_cast<uint8_t *>(pBuf), len);
}