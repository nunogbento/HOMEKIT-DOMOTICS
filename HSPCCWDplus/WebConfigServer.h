#ifndef WEB_CONFIG_SERVER_H
#define WEB_CONFIG_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "HomeSpan.h"
#include "ConfigManager.h"
#include "MCP23017Handler.h"

// Circular buffer for capturing Serial output
#define SERIAL_BUFFER_SIZE 4096

// Forward declaration
class WebConfigServer;

// TeeSerial class - writes to both Serial and web buffer
class TeeSerial : public Print {
public:
    TeeSerial() : _webServer(nullptr) {}

    void begin(unsigned long baud) {
        Serial.begin(baud);
    }

    void setWebServer(WebConfigServer* server) {
        _webServer = server;
    }

    virtual size_t write(uint8_t c) override;
    virtual size_t write(const uint8_t *buffer, size_t size) override;

    // Delegate other Serial methods
    int available() { return Serial.available(); }
    int read() { return Serial.read(); }
    int peek() { return Serial.peek(); }
    void flush() { Serial.flush(); }

private:
    WebConfigServer* _webServer;
};

// Global TeeSerial instance
extern TeeSerial teeSerial;

class WebConfigServer {
public:
    WebConfigServer(uint16_t port = 80)
        : _server(port), _ws("/ws"), _config(nullptr), _mcp(nullptr),
          _bufferHead(0), _bufferTail(0) {}

    void begin(ConfigManager& config, MCP23017Handler& mcp) {
        _config = &config;
        _mcp = &mcp;
        setupRoutes();
        setupWebSocket();
        _server.begin();
        Serial.printf("Web server started on port 80\n");
    }

    // Call this in loop() to send buffered output to WebSocket clients
    void loop() {
        if (_bufferHead != _bufferTail && _ws.count() > 0) {
            String output = "";
            while (_bufferHead != _bufferTail && output.length() < 1024) {
                output += _serialBuffer[_bufferTail];
                _bufferTail = (_bufferTail + 1) % SERIAL_BUFFER_SIZE;
            }
            if (output.length() > 0) {
                _ws.textAll(output);
            }
        }
        _ws.cleanupClients();
    }

    // Write character to buffer (call from custom Serial output)
    void writeToBuffer(char c) {
        size_t nextHead = (_bufferHead + 1) % SERIAL_BUFFER_SIZE;
        if (nextHead != _bufferTail) {  // Don't overwrite unread data
            _serialBuffer[_bufferHead] = c;
            _bufferHead = nextHead;
        }
    }

    // Write string to buffer
    void writeToBuffer(const char* str) {
        while (*str) {
            writeToBuffer(*str++);
        }
    }

private:
    AsyncWebServer _server;
    AsyncWebSocket _ws;
    ConfigManager* _config;
    MCP23017Handler* _mcp;

    // Circular buffer for serial output capture
    char _serialBuffer[SERIAL_BUFFER_SIZE];
    volatile size_t _bufferHead;
    volatile size_t _bufferTail;

    void setupWebSocket() {
        _ws.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                          AwsEventType type, void *arg, uint8_t *data, size_t len) {
            handleWebSocketEvent(server, client, type, arg, data, len);
        });
        _server.addHandler(&_ws);
    }

    void handleWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                              AwsEventType type, void *arg, uint8_t *data, size_t len) {
        switch (type) {
            case WS_EVT_CONNECT:
                Serial.printf("WebSocket client #%u connected\n", client->id());
                client->text("HomeSpan CLI - Type '?' for help\r\n");
                break;

            case WS_EVT_DISCONNECT:
                Serial.printf("WebSocket client #%u disconnected\n", client->id());
                break;

            case WS_EVT_DATA: {
                AwsFrameInfo *info = (AwsFrameInfo*)arg;
                if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
                    // Process each character through HomeSpan CLI
                    for (size_t i = 0; i < len; i++) {
                        char c = (char)data[i];
                        // Echo the character back
                        if (c == '\r' || c == '\n') {
                            writeToBuffer("\r\n");
                        } else {
                            writeToBuffer(c);
                        }
                        // Send to HomeSpan CLI processor
                        char cmd[2] = {c, '\0'};
                        homeSpan.processSerialCommand(cmd);
                    }
                }
                break;
            }

            case WS_EVT_PONG:
            case WS_EVT_ERROR:
                break;
        }
    }

    void setupRoutes() {
        // Serve static files from LittleFS
        _server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

        // API: Get full configuration
        _server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_config) {
                sendError(request, 500, "Config not initialized");
                return;
            }
            sendJson(request, 200, _config->toJson());
        });

        // API: Save full configuration
        _server.on("/api/config", HTTP_PUT,
            [](AsyncWebServerRequest *request) {},
            NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                handlePutConfig(request, data, len, index, total);
            }
        );

        // API: Get single pin config
        _server.on("^\\/api\\/pins\\/(\\d+)$", HTTP_GET, [this](AsyncWebServerRequest *request) {
            int pin = request->pathArg(0).toInt();
            if (pin < 0 || pin >= NUM_PINS) {
                sendError(request, 400, "Invalid pin number");
                return;
            }

            const PinConfig& pinConfig = _config->getPinConfig(pin);
            JsonDocument doc;
            doc["pin"] = pinConfig.pin;
            doc["type"] = ConfigManager::typeToString(pinConfig.type);
            doc["name"] = pinConfig.name;
            doc["inverted"] = pinConfig.inverted;
            doc["cooldown"] = pinConfig.cooldown;
            doc["valveType"] = pinConfig.valveType;

            String output;
            serializeJson(doc, output);
            sendJson(request, 200, output);
        });

        // API: Update single pin config
        _server.on("^\\/api\\/pins\\/(\\d+)$", HTTP_PUT,
            [](AsyncWebServerRequest *request) {},
            NULL,
            [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
                handlePutPin(request, data, len, index, total);
            }
        );

        // API: Get system info
        _server.on("/api/system/info", HTTP_GET, [this](AsyncWebServerRequest *request) {
            JsonDocument doc;
            doc["ip"] = WiFi.localIP().toString();
            doc["mac"] = WiFi.macAddress();
            doc["uptime"] = millis() / 1000;
            doc["freeHeap"] = ESP.getFreeHeap();
            doc["mcpConnected"] = _mcp ? _mcp->isConnected() : false;
            doc["mcpAddress"] = _mcp ? String("0x") + String(_mcp->getAddress(), HEX) : "N/A";
            doc["accessoryCount"] = _config ? _config->countAccessories() : 0;

            String output;
            serializeJson(doc, output);
            sendJson(request, 200, output);
        });

        // API: Restart device
        _server.on("/api/system/restart", HTTP_POST, [](AsyncWebServerRequest *request) {
            request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
            delay(500);
            ESP.restart();
        });

        // API: Get available pin types
        _server.on("/api/pin-types", HTTP_GET, [](AsyncWebServerRequest *request) {
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
            request->send(200, "application/json", output);
        });

        // API: Get current pin states (for debugging)
        _server.on("/api/pins/state", HTTP_GET, [this](AsyncWebServerRequest *request) {
            if (!_mcp) {
                sendError(request, 500, "MCP23017 not initialized");
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
            sendJson(request, 200, output);
        });

        // Handle 404
        _server.onNotFound([](AsyncWebServerRequest *request) {
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
        });

        // Enable CORS for development
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", "*");
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type");
    }

    void handlePutConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        static String body;

        if (index == 0) {
            body = "";
        }

        body += String((char*)data).substring(0, len);

        if (index + len == total) {
            if (!_config) {
                sendError(request, 500, "Config not initialized");
                return;
            }

            if (_config->fromJson(body)) {
                if (_config->save()) {
                    sendJson(request, 200, "{\"success\":true,\"message\":\"Config saved. Restart to apply.\"}");
                } else {
                    sendError(request, 500, "Failed to save config");
                }
            } else {
                sendError(request, 400, "Invalid JSON");
            }
        }
    }

    void handlePutPin(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
        static String body;

        if (index == 0) {
            body = "";
        }

        body += String((char*)data).substring(0, len);

        if (index + len == total) {
            int pin = request->pathArg(0).toInt();
            if (pin < 0 || pin >= NUM_PINS) {
                sendError(request, 400, "Invalid pin number");
                return;
            }

            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);

            if (error) {
                sendError(request, 400, "Invalid JSON");
                return;
            }

            PinConfig pinConfig;
            pinConfig.pin = pin;
            pinConfig.type = ConfigManager::stringToType(doc["type"] | "unused");
            pinConfig.name = doc["name"] | "";
            pinConfig.inverted = doc["inverted"] | true;  // Default active LOW
            pinConfig.cooldown = doc["cooldown"] | 0;
            pinConfig.valveType = doc["valveType"] | 0;

            if (_config->setPinConfig(pin, pinConfig)) {
                if (_config->save()) {
                    sendJson(request, 200, "{\"success\":true,\"message\":\"Pin config saved. Restart to apply.\"}");
                } else {
                    sendError(request, 500, "Failed to save config");
                }
            } else {
                sendError(request, 500, "Failed to update pin config");
            }
        }
    }

    void sendJson(AsyncWebServerRequest *request, int code, const String& json) {
        request->send(code, "application/json", json);
    }

    void sendError(AsyncWebServerRequest *request, int code, const char* message) {
        JsonDocument doc;
        doc["error"] = message;
        String output;
        serializeJson(doc, output);
        request->send(code, "application/json", output);
    }
};

// TeeSerial implementation
inline size_t TeeSerial::write(uint8_t c) {
    Serial.write(c);
    if (_webServer) {
        _webServer->writeToBuffer((char)c);
    }
    return 1;
}

inline size_t TeeSerial::write(const uint8_t *buffer, size_t size) {
    Serial.write(buffer, size);
    if (_webServer) {
        for (size_t i = 0; i < size; i++) {
            _webServer->writeToBuffer((char)buffer[i]);
        }
    }
    return size;
}

// Global TeeSerial instance definition
TeeSerial teeSerial;

#endif // WEB_CONFIG_SERVER_H
