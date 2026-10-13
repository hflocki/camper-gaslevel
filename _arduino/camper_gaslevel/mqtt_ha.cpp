#include "mqtt_ha.h"
#include "ble_sensor.h"
#include "wifi_manager.h"
#include "webserver.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient espClient;
static PubSubClient mqtt(espClient);

static unsigned long lastPublish = 0;
static unsigned long lastReconnect = 0;
static bool discoveryPublished = false;
static uint32_t lastPublishedUpdate = 0;

static void mqttCallback(char* topic, byte* payload, unsigned int length);
static void publishSensor(const char* name, const char* id, const char* unit,
                          const char* devClass, const char* icon, bool measurement, bool keepLastValue);
static void addDeviceBlock(JsonDocument& doc, bool withAvailability = true);
static void publishJson(const String& topic, JsonDocument& doc, const char* id);

void mqttInit() {
    if (strlen(config.mqtt_server) == 0) {
        Serial.println("[MQTT] No server configured");
        return;
    }

    mqtt.setServer(config.mqtt_server, config.mqtt_port);
    mqtt.setCallback(mqttCallback);
    mqtt.setBufferSize(1024);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(5);

    Serial.printf("[MQTT] Configured: %s:%d\n", config.mqtt_server, config.mqtt_port);
}

void mqttLoop() {
    if (!isConnected()) return;
    if (strlen(config.mqtt_server) == 0) return;

    if (!mqtt.connected()) {
        // connect() blocks while the broker is unreachable, so retry only every 5 s
        if (millis() - lastReconnect > 5000) {
            lastReconnect = millis();
            Serial.println("[MQTT] Connection attempt...");

            String clientId = "camper-gaslevel-" + WiFi.macAddress();
            clientId.replace(":", "");

            String willTopic = String(config.mqtt_prefix) + "/status";
            bool connected;

            if (strlen(config.mqtt_user) > 0) {
                connected = mqtt.connect(clientId.c_str(), config.mqtt_user, config.mqtt_pass,
                                         willTopic.c_str(), 0, true, "offline");
            } else {
                connected = mqtt.connect(clientId.c_str(),
                                         NULL, NULL, willTopic.c_str(), 0, true, "offline");
            }

            if (connected) {
                Serial.println("[MQTT] Connected!");

                String cmdBase = String(config.mqtt_prefix) + "/cmd/#";
                mqtt.subscribe(cmdBase.c_str());

                discoveryPublished = false;   // Always re-announce after (re)connect
                mqtt.publish(willTopic.c_str(), "online", true);
                lastPublish = 0;              // Publish the state right away
            } else {
                Serial.printf("[MQTT] Failed, rc=%d\n", mqtt.state());
            }
        }
        return;
    }

    mqtt.loop();

    if (!discoveryPublished) {
        mqttPublishDiscovery();
        discoveryPublished = true;
    }

    // Publish immediately when a new BLE reading arrived, otherwise periodically
    bool newData = (sensorData.updates != lastPublishedUpdate);
    if (newData || lastPublish == 0 || millis() - lastPublish > MQTT_PUBLISH_MS) {
        lastPublish = millis();
        lastPublishedUpdate = sensorData.updates;
        mqttPublishState();
    }
}

bool mqttIsConnected() {
    return mqtt.connected();
}

void mqttRequestDiscovery() {
    discoveryPublished = false;
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    // No commands at the moment (sensor data is received passively)
}

void mqttPublishState() {
    if (!mqtt.connected()) return;

    String prefix = String(config.mqtt_prefix);
    char buf[16];

    // Level values only once a real reading exists, so HA keeps the last known value after a reboot
    if (sensorData.has_data) {
        dtostrf(sensorData.level, 1, 1, buf);
        mqtt.publish((prefix + "/level").c_str(), buf, true);

        dtostrf(sensorData.raw_level, 1, 1, buf);
        mqtt.publish((prefix + "/raw_level").c_str(), buf, true);

        dtostrf(sensorData.content, 1, 1, buf);
        mqtt.publish((prefix + "/content").c_str(), buf, true);

        itoa(sensorData.ble_rssi, buf, 10);
        mqtt.publish((prefix + "/ble_rssi").c_str(), buf, true);
    }
    if (!isnan(sensorData.battery)) {
        dtostrf(sensorData.battery, 1, 0, buf);
        mqtt.publish((prefix + "/battery").c_str(), buf, true);
    }

    mqtt.publish((prefix + "/ble_state").c_str(), sensorData.ble_state, true);

    itoa(sensorData.rssi, buf, 10);
    mqtt.publish((prefix + "/rssi").c_str(), buf, true);

    mqtt.publish((prefix + "/status").c_str(), "online", true);
}

void mqttPublishDiscovery() {
    Serial.println("[MQTT] Publishing HA Discovery...");

    // No device_class for the levels: "battery" would show a battery icon in Home Assistant
    publishSensor("Gas Füllstand",         "level",     "%",   "None",            "mdi:gauge",           true,  true);
    publishSensor("Gas Füllstand Rohwert", "raw_level", "%",   "None",            "mdi:gauge-low",       true,  true);
    publishSensor("Gas Inhalt",            "content",   "L",   "volume_storage",  "mdi:gas-cylinder",    true,  true);
    if (BATTERY_ENABLED) {
        publishSensor("Gassensor Batterie", "battery",   "%",   "battery",         "mdi:battery",         true,  true);
    }
    publishSensor("Gassensor Signal",      "ble_rssi",  "dBm", "signal_strength", "mdi:bluetooth",       true,  false);
    publishSensor("Gassensor Status",      "ble_state", "",    "None",            "mdi:bluetooth-connect", false, false);
    publishSensor("WLAN Signal",           "rssi",      "dBm", "signal_strength", "mdi:wifi",            true,  false);
    publishSensor("Status",                "status",    "",    "None",            "mdi:check-circle",    false, true);


    Serial.println("[MQTT] Discovery complete");
}

static void addDeviceBlock(JsonDocument& doc, bool withAvailability) {
    JsonObject device = doc["device"].to<JsonObject>();
    device["identifiers"][0] = "camper_gaslevel_sensor";
    device["name"] = "Camper Gaslevel Sensor";
    device["model"] = "ESP32 BLE Gateway (" SENSOR_MODEL ")";
    device["manufacturer"] = "DIY";
    device["sw_version"] = FW_VERSION;

    // Level sensors and the status sensor skip availability on purpose, so Home Assistant
    // keeps showing the last known value (and "offline") while the device is switched off
    if (withAvailability) {
        doc["availability_topic"] = String(config.mqtt_prefix) + "/status";
        doc["payload_available"] = "online";
        doc["payload_not_available"] = "offline";
    }
}

static void publishJson(const String& topic, JsonDocument& doc, const char* id) {
    char buffer[768];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0 || len >= sizeof(buffer)) {
        Serial.printf("[MQTT] Discovery payload for '%s' too large\n", id);
        return;
    }
    mqtt.publish(topic.c_str(), buffer, true);
}

static void publishSensor(const char* name, const char* id, const char* unit,
                          const char* devClass, const char* icon, bool measurement, bool keepLastValue) {
    JsonDocument doc;

    doc["name"] = name;
    doc["unique_id"] = String("camper_gaslevel_") + id;
    doc["state_topic"] = String(config.mqtt_prefix) + "/" + id;
    doc["icon"] = icon;

    if (strlen(unit) > 0) doc["unit_of_measurement"] = unit;
    if (strcmp(devClass, "None") != 0) doc["device_class"] = devClass;
    if (measurement) doc["state_class"] = "measurement";

    addDeviceBlock(doc, !keepLastValue);
    publishJson(String("homeassistant/sensor/camper_gaslevel/") + id + "/config", doc, id);
}

