#include "webserver.h"
#include "wifi_manager.h"
#include "ble_sensor.h"
#include "mqtt_ha.h"

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

static AsyncWebServer server(80);
static AsyncWebSocket ws("/ws");
static unsigned long lastWsUpdate = 0;

static bool fsReady = false;
static bool restartPending = false;
static unsigned long restartAt = 0;
static volatile bool sensorChanged = false;   // MAC changed (set by the async web task, handled in loop())
static volatile bool tankChanged = false;     // Tank size / fill stop changed

static const size_t MAX_BODY_SIZE = 2048;

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len);
static void handleApiGetConfig(AsyncWebServerRequest* request);
static void handleApiSaveConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total);
static void handleApiRestart(AsyncWebServerRequest* request);

// ============================================================
// Helpers
// ============================================================

static void setDefaults() {
    strlcpy(config.ap_ssid, DEFAULT_AP_SSID, sizeof(config.ap_ssid));
    strlcpy(config.ap_pass, DEFAULT_AP_PASSWORD, sizeof(config.ap_pass));
    memset(config.wifi_ssid, 0, sizeof(config.wifi_ssid));
    memset(config.wifi_pass, 0, sizeof(config.wifi_pass));
    strlcpy(config.mqtt_server, DEFAULT_MQTT_SERVER, sizeof(config.mqtt_server));
    config.mqtt_port = DEFAULT_MQTT_PORT;
    memset(config.mqtt_user, 0, sizeof(config.mqtt_user));
    memset(config.mqtt_pass, 0, sizeof(config.mqtt_pass));
    strlcpy(config.mqtt_prefix, DEFAULT_MQTT_PREFIX, sizeof(config.mqtt_prefix));

    memset(config.sensor_mac, 0, sizeof(config.sensor_mac));
    config.capacity = DEFAULT_CAPACITY;
    config.fill_stop_on = DEFAULT_FILL_STOP_ON;
    config.fill_stop = DEFAULT_FILL_STOP;
}

// Normalizes "aa-bb-cc-dd-ee-ff" / "aabbccddeeff" / "AA:BB:..." to "AA:BB:CC:DD:EE:FF".
// Empty input is valid (= no sensor). Returns false on invalid input.
static bool normalizeMac(const char* in, char* out, size_t size) {
    char hex[13];
    int n = 0;
    for (const char* p = in; *p; p++) {
        if (*p == ':' || *p == '-' || *p == ' ') continue;
        if (!isxdigit((unsigned char)*p) || n >= 12) return false;
        hex[n++] = toupper((unsigned char)*p);
    }
    if (n == 0) { out[0] = '\0'; return true; }
    if (n != 12 || size < 18) return false;
    for (int i = 0; i < 6; i++) {
        out[i * 3] = hex[i * 2];
        out[i * 3 + 1] = hex[i * 2 + 1];
        out[i * 3 + 2] = (i < 5) ? ':' : '\0';
    }
    return true;
}

static bool isNumber(JsonVariantConst v) {
    return v.is<float>() || v.is<int>();
}

static void copyIfPresent(JsonDocument& doc, const char* key, char* dest, size_t size, bool skipEmpty) {
    if (doc[key].isNull()) return;
    const char* v = doc[key].as<const char*>();
    if (!v) return;
    if (skipEmpty && v[0] == '\0') return;
    strlcpy(dest, v, size);
}

static void sendError(AsyncWebServerRequest* request, const char* msg) {
    request->send(400, "application/json", String("{\"error\":\"") + msg + "\"}");
}

// ============================================================
// Setup / loop
// ============================================================

void webserverInit() {
    fsReady = LittleFS.begin(true);
    if (!fsReady) {
        Serial.println("[Web] LittleFS mount failed - running with defaults");
    }

    // Always load the config (falls back to defaults if the filesystem is unavailable)
    loadConfig();

    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Serve ONLY index.html. Serving the whole filesystem would expose /config.json
    // (WiFi and MQTT credentials) to everybody in the network.
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
        if (fsReady && LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html", "text/html");
        } else {
            request->send(200, "text/html",
                "<h1>Camper Gaslevel</h1><p>Please upload index.html to LittleFS.</p>");
        }
    });

    server.on("/api/config", HTTP_GET, handleApiGetConfig);
    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, handleApiSaveConfig);
    server.on("/api/restart", HTTP_POST, handleApiRestart);

    // Nearby BLE devices (for picking the sensor MAC in the web UI)
    server.on("/api/scan", HTTP_GET, [](AsyncWebServerRequest* request) {
        JsonDocument doc;
        sensorFillSeenList(doc["devices"].to<JsonArray>());
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

}

void webserverStart() {
    server.begin();
    Serial.println("[Web] Server started on port 80");
}

void webserverLoop() {
    ws.cleanupClients(2);

    // Sensor settings changed in the web UI
    if (sensorChanged) {
        sensorChanged = false;
        tankChanged = false;
        sensorReset();          // Other sensor: forget the old readings
    } else if (tankChanged) {
        tankChanged = false;
        sensorRecalculate();    // Same sensor: recalculate level/content from the last raw value
    }

    // Deferred restart: the HTTP response can be sent before the chip resets
    if (restartPending && millis() >= restartAt) {
        ESP.restart();
    }

    if (millis() - lastWsUpdate > WS_UPDATE_MS) {
        lastWsUpdate = millis();
        if (ws.count() > 0) {
            wsSendSensorData();
        }
    }
}

void wsSendSensorData() {
    JsonDocument doc;

    doc["has_data"] = sensorData.has_data;
    doc["level"] = round(sensorData.level * 10.0f) / 10.0f;
    doc["raw"] = round(sensorData.raw_level * 10.0f) / 10.0f;
    doc["content"] = round(sensorData.content * 10.0f) / 10.0f;
    if (isnan(sensorData.battery)) doc["battery"] = nullptr;
    else doc["battery"] = round(sensorData.battery);
    doc["ble_rssi"] = sensorData.ble_rssi;
    doc["ble_state"] = (const char*)sensorData.ble_state;
    doc["age"] = sensorData.has_data ? (long)((millis() - sensorData.last_update) / 1000) : -1;
    doc["raw_hex"] = (const char*)sensorData.raw_hex;
    doc["adv_count"] = sensorData.adv_count;
    doc["scanning"] = sensorData.scanning;
    doc["rssi"] = sensorData.rssi;
    doc["wifi"] = getWiFiStateString();
    doc["mqtt"] = mqttIsConnected();

    char buffer[512];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    ws.textAll(buffer, len);
}

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        wsSendSensorData();
    }
}

// ============================================================
// REST API
// ============================================================

static void handleApiGetConfig(AsyncWebServerRequest* request) {
    JsonDocument doc;

    // Passwords are never sent back to the browser
    doc["wifi_ssid"] = (const char*)config.wifi_ssid;
    doc["ap_ssid"] = (const char*)config.ap_ssid;
    doc["wifi_pass_set"] = strlen(config.wifi_pass) > 0;
    doc["mqtt_server"] = (const char*)config.mqtt_server;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = (const char*)config.mqtt_user;
    doc["mqtt_pass_set"] = strlen(config.mqtt_pass) > 0;
    doc["mqtt_prefix"] = (const char*)config.mqtt_prefix;

    doc["sensor_mac"] = (const char*)config.sensor_mac;
    doc["capacity"] = config.capacity;
    doc["fill_stop_on"] = config.fill_stop_on;
    doc["fill_stop"] = config.fill_stop;
    doc["version"] = FW_VERSION;

    // Compile-time sensor definition (read-only in the web UI)
    JsonObject def = doc["sensor_def"].to<JsonObject>();
    def["model"] = SENSOR_MODEL;
    def["source"] = SENSOR_SOURCE;
    if (SENSOR_COMPANY_ID >= 0) {
        char buf[7];
        snprintf(buf, sizeof(buf), "%04X", (unsigned)SENSOR_COMPANY_ID);
        def["company"] = (const char*)buf;
    }
    def["uuid"] = SENSOR_UUID;
    def["level_offset"] = LEVEL_OFFSET;
    def["level_len"] = LEVEL_LEN;
    def["battery"] = BATTERY_ENABLED;
    def["battery_offset"] = BATTERY_OFFSET;
    def["battery_len"] = BATTERY_LEN;

    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

static void handleApiSaveConfig(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
    static String body;
    static bool rejected = false;

    if (index == 0) {
        body = "";
        rejected = (total > MAX_BODY_SIZE);
    }
    if (rejected) {
        if (index + len == total) {
            request->send(413, "application/json", "{\"error\":\"Body too large\"}");
        }
        return;
    }

    body += String((char*)data, len);

    if (index + len != total) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    body = "";

    if (err) {
        sendError(request, "Invalid JSON");
        return;
    }

    // ---- Validate everything first, before anything is changed ----

    // The access point password needs at least 8 characters (WPA2)
    if (doc["ap_pass"].is<const char*>()) {
        size_t apLen = strlen(doc["ap_pass"].as<const char*>());
        if (apLen > 0 && apLen < 8) { sendError(request, "AP password needs at least 8 characters"); return; }
    }

    bool hasMac = doc["sensor_mac"].is<const char*>();
    char newMac[18] = "";
    if (hasMac && !normalizeMac(doc["sensor_mac"].as<const char*>(), newMac, sizeof(newMac))) {
        sendError(request, "Invalid MAC address"); return;
    }

    bool hasCap = isNumber(doc["capacity"]);
    float newCap = hasCap ? doc["capacity"].as<float>() : 0.0f;
    if (hasCap && (newCap < 0.5f || newCap > 1000.0f)) { sendError(request, "Capacity must be 0.5-1000"); return; }

    bool hasStop = isNumber(doc["fill_stop"]);
    float newStop = hasStop ? doc["fill_stop"].as<float>() : 0.0f;
    if (hasStop && (newStop < 1.0f || newStop > 100.0f)) { sendError(request, "Fill stop must be 1-100 %"); return; }

    bool hasStopOn = doc["fill_stop_on"].is<bool>();
    bool newStopOn = hasStopOn ? doc["fill_stop_on"].as<bool>() : false;

    // ---- Apply ----

    // Empty values clear SSID/server/user; empty passwords are ignored (= keep the stored one)
    copyIfPresent(doc, "wifi_ssid",   config.wifi_ssid,   sizeof(config.wifi_ssid),   false);
    copyIfPresent(doc, "wifi_pass",   config.wifi_pass,   sizeof(config.wifi_pass),   true);
    copyIfPresent(doc, "ap_ssid",     config.ap_ssid,     sizeof(config.ap_ssid),     true);
    copyIfPresent(doc, "ap_pass",     config.ap_pass,     sizeof(config.ap_pass),     true);
    copyIfPresent(doc, "mqtt_server", config.mqtt_server, sizeof(config.mqtt_server), false);
    copyIfPresent(doc, "mqtt_user",   config.mqtt_user,   sizeof(config.mqtt_user),   false);
    copyIfPresent(doc, "mqtt_pass",   config.mqtt_pass,   sizeof(config.mqtt_pass),   true);
    copyIfPresent(doc, "mqtt_prefix", config.mqtt_prefix, sizeof(config.mqtt_prefix), true);

    if (doc["mqtt_port"].is<int>()) {
        int port = doc["mqtt_port"].as<int>();
        config.mqtt_port = (port > 0 && port <= 65535) ? (uint16_t)port : DEFAULT_MQTT_PORT;
    }

    if (hasMac && strcmp(newMac, config.sensor_mac) != 0) {
        strlcpy(config.sensor_mac, newMac, sizeof(config.sensor_mac));
        sensorChanged = true;
    }
    if (hasCap && newCap != config.capacity)          { config.capacity = newCap;       tankChanged = true; }
    if (hasStopOn && newStopOn != config.fill_stop_on) { config.fill_stop_on = newStopOn; tankChanged = true; }
    if (hasStop && newStop != config.fill_stop)        { config.fill_stop = newStop;     tankChanged = true; }

    saveConfig();
    request->send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handleApiRestart(AsyncWebServerRequest* request) {
    request->send(200, "application/json", "{\"status\":\"restarting\"}");
    // Do not block the async web task: restart from webserverLoop() instead
    restartPending = true;
    restartAt = millis() + 500;
}

// ============================================================
// Persistence
// ============================================================

void loadConfig() {
    setDefaults();

    if (!fsReady) return;

    if (!LittleFS.exists(CONFIG_FILE)) {
        saveConfig();
        return;
    }

    File file = LittleFS.open(CONFIG_FILE, "r");
    if (!file) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err) {
        Serial.printf("[Config] Parse error (%s) - using defaults\n", err.c_str());
        return;
    }

    strlcpy(config.wifi_ssid,   doc["wifi_ssid"]   | "",                  sizeof(config.wifi_ssid));
    strlcpy(config.wifi_pass,   doc["wifi_pass"]   | "",                  sizeof(config.wifi_pass));
    strlcpy(config.ap_ssid,     doc["ap_ssid"]     | DEFAULT_AP_SSID,     sizeof(config.ap_ssid));
    strlcpy(config.ap_pass,     doc["ap_pass"]     | DEFAULT_AP_PASSWORD, sizeof(config.ap_pass));
    strlcpy(config.mqtt_server, doc["mqtt_server"] | DEFAULT_MQTT_SERVER, sizeof(config.mqtt_server));
    config.mqtt_port = doc["mqtt_port"] | DEFAULT_MQTT_PORT;
    strlcpy(config.mqtt_user,   doc["mqtt_user"]   | "",                  sizeof(config.mqtt_user));
    strlcpy(config.mqtt_pass,   doc["mqtt_pass"]   | "",                  sizeof(config.mqtt_pass));
    strlcpy(config.mqtt_prefix, doc["mqtt_prefix"] | DEFAULT_MQTT_PREFIX, sizeof(config.mqtt_prefix));

    char mac[18];
    if (normalizeMac(doc["sensor_mac"] | "", mac, sizeof(mac))) {
        strlcpy(config.sensor_mac, mac, sizeof(config.sensor_mac));
    }

    float cap = doc["capacity"] | DEFAULT_CAPACITY;
    config.capacity = (cap >= 0.5f && cap <= 1000.0f) ? cap : DEFAULT_CAPACITY;
    config.fill_stop_on = doc["fill_stop_on"] | DEFAULT_FILL_STOP_ON;
    float stop = doc["fill_stop"] | DEFAULT_FILL_STOP;
    config.fill_stop = (stop >= 1.0f && stop <= 100.0f) ? stop : DEFAULT_FILL_STOP;
}

void saveConfig() {
    if (!fsReady) return;

    JsonDocument doc;
    doc["wifi_ssid"] = (const char*)config.wifi_ssid;
    doc["wifi_pass"] = config.wifi_pass;
    doc["ap_ssid"] = (const char*)config.ap_ssid;
    doc["ap_pass"] = config.ap_pass;
    doc["mqtt_server"] = (const char*)config.mqtt_server;
    doc["mqtt_port"] = config.mqtt_port;
    doc["mqtt_user"] = (const char*)config.mqtt_user;
    doc["mqtt_pass"] = config.mqtt_pass;
    doc["mqtt_prefix"] = (const char*)config.mqtt_prefix;

    doc["sensor_mac"] = (const char*)config.sensor_mac;
    doc["capacity"] = config.capacity;
    doc["fill_stop_on"] = config.fill_stop_on;
    doc["fill_stop"] = config.fill_stop;

    // Write to a temp file first so a power loss cannot leave a half-written config
    const char* tmpPath = "/config.tmp";
    File file = LittleFS.open(tmpPath, "w");
    if (!file) {
        Serial.println("[Config] Cannot open temp file for writing");
        return;
    }
    size_t written = serializeJsonPretty(doc, file);
    file.close();

    if (written == 0) {
        LittleFS.remove(tmpPath);
        Serial.println("[Config] Write failed");
        return;
    }

    LittleFS.remove(CONFIG_FILE);
    if (!LittleFS.rename(tmpPath, CONFIG_FILE)) {
        Serial.println("[Config] Rename failed");
    }
}
