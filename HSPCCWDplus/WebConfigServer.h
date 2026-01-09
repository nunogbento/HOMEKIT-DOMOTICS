#ifndef WEB_CONFIG_SERVER_H
#define WEB_CONFIG_SERVER_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "HomeSpan.h"
#include "ConfigManager.h"
#include "MCP23017Handler.h"

// Forward declaration
class WebConfigServer;
WebConfigServer* _webServerInstance = nullptr;

class WebConfigServer {
public:
    WebConfigServer(uint16_t httpPort = 80, uint16_t wsPort = 81)
        : _server(httpPort), _wsServer(wsPort), _config(nullptr), _mcp(nullptr) {
        _webServerInstance = this;
    }

    void begin(ConfigManager& config, MCP23017Handler& mcp) {
        _config = &config;
        _mcp = &mcp;
        setupRoutes();
        setupWebSocket();
        _server.begin();
        _wsServer.begin();
        Serial.printf("Web server started on port 80, WebSocket on port 81\n");
    }

    void loop() {
        _server.handleClient();
        _wsServer.loop();
    }

private:
    WebServer _server;
    WebSocketsServer _wsServer;
    ConfigManager* _config;
    MCP23017Handler* _mcp;
    String _cmdBuffer;  // Buffer for multi-char commands

    void setupWebSocket() {
        _wsServer.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
            handleWebSocketEvent(num, type, payload, length);
        });
    }

    void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
            case WStype_DISCONNECTED:
                Serial.printf("[WS] Client #%u disconnected\n", num);
                break;

            case WStype_CONNECTED: {
                IPAddress ip = _wsServer.remoteIP(num);
                Serial.printf("[WS] Client #%u connected from %s\n", num, ip.toString().c_str());
                _wsServer.sendTXT(num, "\r\n*** HomeSpan CLI (Web Terminal) ***\r\n");
                _wsServer.sendTXT(num, "Type '?' for available commands\r\n\r\n");
                break;
            }

            case WStype_TEXT: {
                String input = String((char*)payload).substring(0, length);

                // Echo input
                _wsServer.sendTXT(num, input + "\r\n");

                // Process command
                for (size_t i = 0; i < length; i++) {
                    char c = (char)payload[i];
                    if (c == '\r' || c == '\n') {
                        if (_cmdBuffer.length() > 0) {
                            processCommand(num, _cmdBuffer);
                            _cmdBuffer = "";
                        }
                    } else {
                        _cmdBuffer += c;
                        // Single char commands (HomeSpan style)
                        if (_cmdBuffer.length() == 1 && !isDigit(c)) {
                            processCommand(num, _cmdBuffer);
                            _cmdBuffer = "";
                        }
                    }
                }
                break;
            }

            default:
                break;
        }
    }

    void processCommand(uint8_t clientNum, const String& cmd) {
        String response = "";
        char c = cmd.charAt(0);

        // Also send to HomeSpan for commands it handles internally
        homeSpan.processSerialCommand(cmd.c_str());

        // Generate our own response for display
        switch (c) {
            case '?':
                response = "\r\n*** HomeSpan Commands ***\r\n\r\n";
                response += "  D - disconnect/reconnect to WiFi\r\n";
                response += "  Z - scan for available WiFi networks\r\n";
                response += "\r\n";
                response += "  R - restart device\r\n";
                response += "  F - factory reset and restart\r\n";
                response += "  E - erase ALL stored data and restart\r\n";
                response += "\r\n";
                response += "  L <level> - change Log Level (0/1/2)\r\n";
                response += "\r\n";
                response += "  i - device/service info\r\n";
                response += "  d - accessories database (JSON)\r\n";
                response += "\r\n";
                response += "  ? - print this list of commands\r\n";
                response += "\r\n*** End Commands ***\r\n\r\n";
                break;

            case 'i': {
                response = "\r\n*** Device Info ***\r\n\r\n";
                response += "Service                             UUID         AID  IID  Update  Loop  Button  Linked\r\n";
                response += "------------------------------  --------  ----------  ---  ------  ----  ------  ------\r\n";
                // Note: Full service table requires HomeSpan internals access
                // Show summary info instead
                response += "(Service table - see serial output for full details)\r\n\r\n";
                response += "Configured as Bridge: YES\r\n\r\n";
                response += "WiFi SSID: " + WiFi.SSID() + "\r\n";
                response += "IP Address: " + WiFi.localIP().toString() + "\r\n";
                response += "MAC Address: " + WiFi.macAddress() + "\r\n";
                response += "RSSI: " + String(WiFi.RSSI()) + " dBm\r\n";
                response += "Free Heap: " + String(ESP.getFreeHeap()) + " bytes\r\n";
                response += "Uptime: " + String(millis() / 1000) + " seconds\r\n";
                response += "\r\n*** End Info ***\r\n\r\n";
                break;
            }

            case 'd':
                response = "\r\n*** Accessories Database ***\r\n\r\n";
                response += "(Full JSON database - see serial output)\r\n";
                response += "Configured accessories: " + String(_config ? _config->countAccessories() : 0) + "\r\n";
                response += "MCP23017: " + String(_mcp && _mcp->isConnected() ? "Connected" : "Not found") + "\r\n";
                response += "\r\n*** End Database ***\r\n\r\n";
                break;

            case 'D':
                response = "\r\nDisconnecting/reconnecting WiFi...\r\n";
                response += "(Command sent to HomeSpan)\r\n\r\n";
                break;

            case 'Z':
                response = "\r\nScanning for WiFi networks...\r\n";
                response += "(See serial output for scan results)\r\n\r\n";
                break;

            case 'R':
                response = "\r\n*** Restarting device... ***\r\n\r\n";
                _wsServer.sendTXT(clientNum, response);
                delay(500);
                ESP.restart();
                return;

            case 'F':
                response = "\r\n*** Factory Reset ***\r\n";
                response += "WARNING: This will erase HomeKit pairing data!\r\n";
                response += "(Command sent to HomeSpan - confirm via serial if needed)\r\n\r\n";
                break;

            case 'E':
                response = "\r\n*** Erase ALL Data ***\r\n";
                response += "WARNING: This erases WiFi credentials and all settings!\r\n";
                response += "(Command sent to HomeSpan - device will restart)\r\n\r\n";
                break;

            case 'L':
                response = "\r\nLog level command sent to HomeSpan\r\n";
                response += "Use L0 (errors), L1 (warnings), or L2 (all)\r\n\r\n";
                break;

            case 'W':
                response = "\r\n*** WiFi Setup ***\r\n";
                response += "This is an interactive command.\r\n";
                response += "Please use serial terminal for WiFi configuration.\r\n\r\n";
                response += "Current WiFi Status:\r\n";
                response += "  SSID: " + WiFi.SSID() + "\r\n";
                response += "  IP: " + WiFi.localIP().toString() + "\r\n";
                response += "  RSSI: " + String(WiFi.RSSI()) + " dBm\r\n\r\n";
                break;

            default:
                if (c >= '0' && c <= '9') {
                    response = "\r\nNumeric command '" + cmd + "' sent to HomeSpan\r\n\r\n";
                } else {
                    response = "\r\nUnknown command '" + cmd + "'\r\n";
                    response += "Type '?' for list of commands\r\n\r\n";
                }
                break;
        }

        _wsServer.sendTXT(clientNum, response);
    }

    void setupRoutes() {
        // Enable CORS
        _server.enableCORS(true);

        // Serve static files from LittleFS
        _server.on("/", HTTP_GET, [this]() { serveFile("/index.html", "text/html"); });
        _server.on("/index.html", HTTP_GET, [this]() { serveFile("/index.html", "text/html"); });
        _server.on("/app.js", HTTP_GET, [this]() { serveFile("/app.js", "application/javascript"); });
        _server.on("/style.css", HTTP_GET, [this]() { serveFile("/style.css", "text/css"); });

        // API: Get full configuration
        _server.on("/api/config", HTTP_GET, [this]() {
            if (!_config) {
                sendError(500, "Config not initialized");
                return;
            }
            _server.send(200, "application/json", _config->toJson());
        });

        // API: Save full configuration (PUT)
        _server.on("/api/config", HTTP_PUT, [this]() {
            if (!_config) {
                sendError(500, "Config not initialized");
                return;
            }

            String body = _server.arg("plain");
            if (_config->fromJson(body)) {
                if (_config->save()) {
                    _server.send(200, "application/json", "{\"success\":true,\"message\":\"Config saved. Restart to apply.\"}");
                } else {
                    sendError(500, "Failed to save config");
                }
            } else {
                sendError(400, "Invalid JSON");
            }
        });

        // API: Get system info
        _server.on("/api/system/info", HTTP_GET, [this]() {
            JsonDocument doc;
            doc["ip"] = WiFi.localIP().toString();
            doc["mac"] = WiFi.macAddress();
            doc["uptime"] = millis() / 1000;
            doc["freeHeap"] = ESP.getFreeHeap();
            doc["mcpConnected"] = _mcp ? _mcp->isConnected() : false;
            doc["mcpAddress"] = _mcp ? String("0x") + String(_mcp->getAddress(), HEX) : "N/A";
            doc["accessoryCount"] = _config ? _config->countAccessories() : 0;
            doc["wsPort"] = 81;

            String output;
            serializeJson(doc, output);
            _server.send(200, "application/json", output);
        });

        // API: Restart device
        _server.on("/api/system/restart", HTTP_POST, [this]() {
            _server.send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
            delay(500);
            ESP.restart();
        });

        // API: Get available pin types
        _server.on("/api/pin-types", HTTP_GET, [this]() {
            JsonDocument doc;
            JsonArray types = doc.to<JsonArray>();

            JsonObject unused = types.add<JsonObject>();
            unused["id"] = "unused";
            unused["name"] = "Unused";
            unused["direction"] = "none";

            JsonObject motion = types.add<JsonObject>();
            motion["id"] = "motion";
            motion["name"] = "Motion Sensor";
            motion["direction"] = "input";

            JsonObject leak = types.add<JsonObject>();
            leak["id"] = "leak";
            leak["name"] = "Leak Sensor";
            leak["direction"] = "input";

            JsonObject smoke = types.add<JsonObject>();
            smoke["id"] = "smoke";
            smoke["name"] = "Smoke Sensor";
            smoke["direction"] = "input";

            JsonObject co2 = types.add<JsonObject>();
            co2["id"] = "co2";
            co2["name"] = "CO2 Sensor";
            co2["direction"] = "input";

            JsonObject valve = types.add<JsonObject>();
            valve["id"] = "valve";
            valve["name"] = "Valve";
            valve["direction"] = "output";

            String output;
            serializeJson(doc, output);
            _server.send(200, "application/json", output);
        });

        // API: Get current pin states
        _server.on("/api/pins/state", HTTP_GET, [this]() {
            if (!_mcp) {
                sendError(500, "MCP23017 not initialized");
                return;
            }

            uint16_t state = _mcp->readAll();
            JsonDocument doc;
            JsonArray pins = doc.to<JsonArray>();

            for (int i = 0; i < NUM_PINS; i++) {
                JsonObject pinObj = pins.add<JsonObject>();
                pinObj["pin"] = i;
                pinObj["state"] = (state & (1 << i)) ? true : false;
            }

            String output;
            serializeJson(doc, output);
            _server.send(200, "application/json", output);
        });

        // Handle 404
        _server.onNotFound([this]() {
            _server.send(404, "application/json", "{\"error\":\"Not found\"}");
        });
    }

    void serveFile(const char* path, const char* contentType) {
        if (LittleFS.exists(path)) {
            File file = LittleFS.open(path, "r");
            _server.streamFile(file, contentType);
            file.close();
        } else {
            _server.send(404, "text/plain", "File not found");
        }
    }

    void sendError(int code, const char* message) {
        JsonDocument doc;
        doc["error"] = message;
        String output;
        serializeJson(doc, output);
        _server.send(code, "application/json", output);
    }
};

#endif // WEB_CONFIG_SERVER_H
