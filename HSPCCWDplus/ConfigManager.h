#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

#define CONFIG_FILE "/config.json"
#define CONFIG_VERSION 3
#define NUM_PINS 16
#define NUM_OF8WD 8
#define NUM_IN10WD 10

// Default PCCWD bus addresses
#define DEFAULT_IN10WD_ADDRESS 75
#define DEFAULT_OF8WD_ADDRESS 167

// Pin type enumeration
enum class PinType : uint8_t {
    UNUSED = 0,
    MOTION = 1,
    LEAK = 2,
    SMOKE = 3,
    CO2 = 4,
    VALVE = 5
};

// Pin configuration structure
struct PinConfig {
    uint8_t pin;
    PinType type;
    String name;
    bool inverted;      // For sensors: true=active LOW (default), false=active HIGH
    uint8_t valveType;  // For valves: 0=Generic, 1=Irrigation, 2=Shower, 3=Faucet
    uint16_t cooldown;  // For sensors: cooldown period in seconds (0=disabled)

    PinConfig() : pin(0), type(PinType::UNUSED), name(""), inverted(true), valveType(0), cooldown(0) {}

    bool isInput() const {
        return type == PinType::MOTION || type == PinType::LEAK ||
               type == PinType::SMOKE || type == PinType::CO2;
    }

    bool isOutput() const {
        return type == PinType::VALVE;
    }
};

// Configuration for hardcoded devices (PCCWD bus and PWM LED)
struct HardcodedDeviceConfig {
    bool enabled;
    String name;

    HardcodedDeviceConfig() : enabled(false), name("") {}
    HardcodedDeviceConfig(bool en, const String& n) : enabled(en), name(n) {}
};

class ConfigManager {
public:
    ConfigManager() : _loaded(false) {
        // Initialize all MCP23017 pins as unused
        for (int i = 0; i < NUM_PINS; i++) {
            _pins[i].pin = i;
            _pins[i].type = PinType::UNUSED;
            _pins[i].name = "";
            _pins[i].inverted = false;
            _pins[i].valveType = 0;
        }

        // Initialize hardcoded devices with defaults (all disabled)
        for (int i = 0; i < NUM_OF8WD; i++) {
            _of8wd[i].enabled = false;
            _of8wd[i].name = String("Output ") + String(i + 1);
        }
        for (int i = 0; i < NUM_IN10WD; i++) {
            _in10wd[i].enabled = false;
            _in10wd[i].name = String("Switch ") + String(i + 1);
        }
        _pwmLed.enabled = false;
        _pwmLed.name = "LED Strip";

        // Initialize PCCWD bus addresses with defaults
        _in10wdAddress = DEFAULT_IN10WD_ADDRESS;
        _of8wdAddress = DEFAULT_OF8WD_ADDRESS;
    }

    // Initialize LittleFS and load config
    bool begin() {
        if (!LittleFS.begin(true)) {  // true = format if failed
            Serial.println("LittleFS mount failed");
            return false;
        }

        if (!load()) {
            Serial.println("Config load failed, creating default");
            return save();  // Save default config
        }

        return true;
    }

    // Load configuration from file
    bool load() {
        if (!LittleFS.exists(CONFIG_FILE)) {
            Serial.println("Config file not found");
            return false;
        }

        File file = LittleFS.open(CONFIG_FILE, "r");
        if (!file) {
            Serial.println("Failed to open config file");
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("JSON parse error: %s\n", error.c_str());
            return false;
        }

        // Check version
        int version = doc["version"] | 0;
        if (version != CONFIG_VERSION) {
            Serial.printf("Config version mismatch: %d vs %d\n", version, CONFIG_VERSION);
            // Could add migration logic here
        }

        // Parse pins array
        JsonArray pins = doc["pins"].as<JsonArray>();
        for (JsonObject pinObj : pins) {
            uint8_t pinNum = pinObj["pin"] | 0;
            if (pinNum >= NUM_PINS) continue;

            _pins[pinNum].pin = pinNum;
            _pins[pinNum].type = stringToType(pinObj["type"] | "unused");
            _pins[pinNum].name = pinObj["name"] | "";
            _pins[pinNum].inverted = pinObj["inverted"] | true;  // Default active LOW
            _pins[pinNum].valveType = pinObj["valveType"] | 0;
            _pins[pinNum].cooldown = pinObj["cooldown"] | 0;
        }

        // Parse hardcoded devices
        if (doc.containsKey("of8wd")) {
            JsonArray of8wdArr = doc["of8wd"].as<JsonArray>();
            int i = 0;
            for (JsonObject devObj : of8wdArr) {
                if (i >= NUM_OF8WD) break;
                _of8wd[i].enabled = devObj["enabled"] | false;
                const char* name = devObj["name"];
                _of8wd[i].name = name ? String(name) : (String("Output ") + String(i + 1));
                i++;
            }
        }

        if (doc.containsKey("in10wd")) {
            JsonArray in10wdArr = doc["in10wd"].as<JsonArray>();
            int i = 0;
            for (JsonObject devObj : in10wdArr) {
                if (i >= NUM_IN10WD) break;
                _in10wd[i].enabled = devObj["enabled"] | false;
                const char* swname = devObj["name"];
                _in10wd[i].name = swname ? String(swname) : (String("Switch ") + String(i + 1));
                i++;
            }
        }

        if (doc.containsKey("pwmLed")) {
            JsonObject ledObj = doc["pwmLed"];
            _pwmLed.enabled = ledObj["enabled"] | false;
            _pwmLed.name = ledObj["name"] | "LED Strip";
        }

        // Parse PCCWD bus addresses
        if (doc.containsKey("pccwdBus")) {
            JsonObject busObj = doc["pccwdBus"];
            _in10wdAddress = busObj["in10wdAddress"] | DEFAULT_IN10WD_ADDRESS;
            _of8wdAddress = busObj["of8wdAddress"] | DEFAULT_OF8WD_ADDRESS;
        }

        _loaded = true;
        Serial.println("Config loaded successfully");
        return true;
    }

    // Save configuration to file
    bool save() {
        // Write to temp file first for atomic save
        File file = LittleFS.open(CONFIG_FILE ".tmp", "w");
        if (!file) {
            Serial.println("Failed to create temp config file");
            return false;
        }

        JsonDocument doc;
        doc["version"] = CONFIG_VERSION;

        JsonArray pins = doc["pins"].to<JsonArray>();
        for (int i = 0; i < NUM_PINS; i++) {
            JsonObject pinObj = pins.add<JsonObject>();
            pinObj["pin"] = _pins[i].pin;
            pinObj["type"] = typeToString(_pins[i].type);
            pinObj["name"] = _pins[i].name;
            if (_pins[i].isInput()) {
                pinObj["inverted"] = _pins[i].inverted;
                pinObj["cooldown"] = _pins[i].cooldown;
            }
            if (_pins[i].type == PinType::VALVE) {
                pinObj["valveType"] = _pins[i].valveType;
            }
        }

        // Save hardcoded devices
        JsonArray of8wdArr = doc["of8wd"].to<JsonArray>();
        for (int i = 0; i < NUM_OF8WD; i++) {
            JsonObject devObj = of8wdArr.add<JsonObject>();
            devObj["enabled"] = _of8wd[i].enabled;
            devObj["name"] = _of8wd[i].name;
        }

        JsonArray in10wdArr = doc["in10wd"].to<JsonArray>();
        for (int i = 0; i < NUM_IN10WD; i++) {
            JsonObject devObj = in10wdArr.add<JsonObject>();
            devObj["enabled"] = _in10wd[i].enabled;
            devObj["name"] = _in10wd[i].name;
        }

        JsonObject ledObj = doc["pwmLed"].to<JsonObject>();
        ledObj["enabled"] = _pwmLed.enabled;
        ledObj["name"] = _pwmLed.name;

        // Save PCCWD bus addresses
        JsonObject busObj = doc["pccwdBus"].to<JsonObject>();
        busObj["in10wdAddress"] = _in10wdAddress;
        busObj["of8wdAddress"] = _of8wdAddress;

        if (serializeJson(doc, file) == 0) {
            file.close();
            LittleFS.remove(CONFIG_FILE ".tmp");
            Serial.println("Failed to write config");
            return false;
        }

        file.close();

        // Atomic rename
        LittleFS.remove(CONFIG_FILE);
        if (!LittleFS.rename(CONFIG_FILE ".tmp", CONFIG_FILE)) {
            Serial.println("Failed to rename config file");
            return false;
        }

        Serial.println("Config saved successfully");
        return true;
    }

    // Get pin configuration
    const PinConfig& getPinConfig(uint8_t pin) const {
        if (pin >= NUM_PINS) {
            static PinConfig empty;
            return empty;
        }
        return _pins[pin];
    }

    // Set pin configuration
    bool setPinConfig(uint8_t pin, const PinConfig& config) {
        if (pin >= NUM_PINS) return false;
        _pins[pin] = config;
        _pins[pin].pin = pin;  // Ensure pin number is correct
        return true;
    }

    // Get all pins
    const PinConfig* getAllPins() const {
        return _pins;
    }

    // Serialize to JSON string
    String toJson() const {
        JsonDocument doc;
        doc["version"] = CONFIG_VERSION;

        JsonArray pins = doc["pins"].to<JsonArray>();
        for (int i = 0; i < NUM_PINS; i++) {
            JsonObject pinObj = pins.add<JsonObject>();
            pinObj["pin"] = _pins[i].pin;
            pinObj["type"] = typeToString(_pins[i].type);
            pinObj["name"] = _pins[i].name;
            pinObj["inverted"] = _pins[i].inverted;
            pinObj["valveType"] = _pins[i].valveType;
            pinObj["cooldown"] = _pins[i].cooldown;
        }

        // Include hardcoded devices
        JsonArray of8wdArr = doc["of8wd"].to<JsonArray>();
        for (int i = 0; i < NUM_OF8WD; i++) {
            JsonObject devObj = of8wdArr.add<JsonObject>();
            devObj["enabled"] = _of8wd[i].enabled;
            devObj["name"] = _of8wd[i].name;
        }

        JsonArray in10wdArr = doc["in10wd"].to<JsonArray>();
        for (int i = 0; i < NUM_IN10WD; i++) {
            JsonObject devObj = in10wdArr.add<JsonObject>();
            devObj["enabled"] = _in10wd[i].enabled;
            devObj["name"] = _in10wd[i].name;
        }

        JsonObject ledObj = doc["pwmLed"].to<JsonObject>();
        ledObj["enabled"] = _pwmLed.enabled;
        ledObj["name"] = _pwmLed.name;

        // Include PCCWD bus addresses
        JsonObject busObj = doc["pccwdBus"].to<JsonObject>();
        busObj["in10wdAddress"] = _in10wdAddress;
        busObj["of8wdAddress"] = _of8wdAddress;

        String output;
        serializeJson(doc, output);
        return output;
    }

    // Parse from JSON string
    bool fromJson(const String& json) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, json);

        if (error) {
            Serial.printf("JSON parse error: %s\n", error.c_str());
            return false;
        }

        JsonArray pins = doc["pins"].as<JsonArray>();
        for (JsonObject pinObj : pins) {
            uint8_t pinNum = pinObj["pin"] | 0;
            if (pinNum >= NUM_PINS) continue;

            _pins[pinNum].pin = pinNum;
            _pins[pinNum].type = stringToType(pinObj["type"] | "unused");
            _pins[pinNum].name = pinObj["name"] | "";
            _pins[pinNum].inverted = pinObj["inverted"] | true;  // Default active LOW
            _pins[pinNum].valveType = pinObj["valveType"] | 0;
            _pins[pinNum].cooldown = pinObj["cooldown"] | 0;
        }

        // Parse hardcoded devices
        if (doc.containsKey("of8wd")) {
            JsonArray of8wdArr = doc["of8wd"].as<JsonArray>();
            int i = 0;
            for (JsonObject devObj : of8wdArr) {
                if (i >= NUM_OF8WD) break;
                _of8wd[i].enabled = devObj["enabled"] | false;
                const char* name = devObj["name"];
                _of8wd[i].name = name ? String(name) : (String("Output ") + String(i + 1));
                i++;
            }
        }

        if (doc.containsKey("in10wd")) {
            JsonArray in10wdArr = doc["in10wd"].as<JsonArray>();
            int i = 0;
            for (JsonObject devObj : in10wdArr) {
                if (i >= NUM_IN10WD) break;
                _in10wd[i].enabled = devObj["enabled"] | false;
                const char* swname = devObj["name"];
                _in10wd[i].name = swname ? String(swname) : (String("Switch ") + String(i + 1));
                i++;
            }
        }

        if (doc.containsKey("pwmLed")) {
            JsonObject ledObj = doc["pwmLed"];
            _pwmLed.enabled = ledObj["enabled"] | false;
            _pwmLed.name = ledObj["name"] | "LED Strip";
        }

        // Parse PCCWD bus addresses
        if (doc.containsKey("pccwdBus")) {
            JsonObject busObj = doc["pccwdBus"];
            _in10wdAddress = busObj["in10wdAddress"] | DEFAULT_IN10WD_ADDRESS;
            _of8wdAddress = busObj["of8wdAddress"] | DEFAULT_OF8WD_ADDRESS;
        }

        return true;
    }

    // Check if config is loaded
    bool isLoaded() const {
        return _loaded;
    }

    // Count configured accessories
    int countAccessories() const {
        int count = 0;
        // Count MCP23017 pin accessories
        for (int i = 0; i < NUM_PINS; i++) {
            if (_pins[i].type != PinType::UNUSED) {
                count++;
            }
        }
        // Count enabled hardcoded devices
        for (int i = 0; i < NUM_OF8WD; i++) {
            if (_of8wd[i].enabled) count++;
        }
        for (int i = 0; i < NUM_IN10WD; i++) {
            if (_in10wd[i].enabled) count++;
        }
        if (_pwmLed.enabled) count++;
        return count;
    }

    // Hardcoded device getters
    const HardcodedDeviceConfig& getOF8WdConfig(uint8_t index) const {
        static HardcodedDeviceConfig empty;
        return (index < NUM_OF8WD) ? _of8wd[index] : empty;
    }

    const HardcodedDeviceConfig& getIN10WdConfig(uint8_t index) const {
        static HardcodedDeviceConfig empty;
        return (index < NUM_IN10WD) ? _in10wd[index] : empty;
    }

    const HardcodedDeviceConfig& getPwmLedConfig() const {
        return _pwmLed;
    }

    // Check if specific hardcoded device is enabled
    bool isOF8WdEnabled(uint8_t index) const {
        return (index < NUM_OF8WD) ? _of8wd[index].enabled : false;
    }

    bool isIN10WdEnabled(uint8_t index) const {
        return (index < NUM_IN10WD) ? _in10wd[index].enabled : false;
    }

    bool isPwmLedEnabled() const {
        return _pwmLed.enabled;
    }

    // PCCWD bus address getters
    uint8_t getIN10WdAddress() const {
        return _in10wdAddress;
    }

    uint8_t getOF8WdAddress() const {
        return _of8wdAddress;
    }

    // Convert type to string
    static const char* typeToString(PinType type) {
        switch (type) {
            case PinType::MOTION: return "motion";
            case PinType::LEAK: return "leak";
            case PinType::SMOKE: return "smoke";
            case PinType::CO2: return "co2";
            case PinType::VALVE: return "valve";
            default: return "unused";
        }
    }

    // Convert string to type
    static PinType stringToType(const char* str) {
        if (strcmp(str, "motion") == 0) return PinType::MOTION;
        if (strcmp(str, "leak") == 0) return PinType::LEAK;
        if (strcmp(str, "smoke") == 0) return PinType::SMOKE;
        if (strcmp(str, "co2") == 0) return PinType::CO2;
        if (strcmp(str, "valve") == 0) return PinType::VALVE;
        return PinType::UNUSED;
    }

private:
    PinConfig _pins[NUM_PINS];
    HardcodedDeviceConfig _of8wd[NUM_OF8WD];
    HardcodedDeviceConfig _in10wd[NUM_IN10WD];
    HardcodedDeviceConfig _pwmLed;
    uint8_t _in10wdAddress;
    uint8_t _of8wdAddress;
    bool _loaded;
};

#endif // CONFIG_MANAGER_H
