/*
 * Camper Gaslevel - ESP32 BLE gas level gateway
 * =============================================
 * Hardware: ESP32-C3 / ESP32 (built-in Bluetooth LE, no wiring needed)
 *
 * Reads a BLE gas level sensor (default: Rotarex DIMES / SRG WAVE).
 * Other sensors: edit the SENSOR DEFINITION block in config.h.
 *
 * Values are published via MQTT (Home Assistant discovery) and shown in the web UI.
 */

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include "config.h"
#include "wifi_manager.h"
#include "ble_sensor.h"
#include "mqtt_ha.h"
#include "webserver.h"

AppConfig config;
SensorData sensorData;

static unsigned long lastStatusPrint = 0;

void setupOTA() {
    ArduinoOTA.setHostname(OTA_HOSTNAME);
    ArduinoOTA.setPassword(DEFAULT_OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        String type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.println("[OTA] Start updating " + type);
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\n[OTA] Update complete!");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        unsigned int pct = (total > 0) ? (progress * 100U / total) : 0;
        Serial.printf("[OTA] Progress: %u%%\r", pct);
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", error);
    });

    ArduinoOTA.begin();
    Serial.println("[OTA] Ready");
}

void setup() {
    Serial.begin(115200);
    Serial.setTimeout(50);  // Do not block the loop while waiting for a line end
    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  Camper Gaslevel v" FW_VERSION " (ESP32 BLE)");
    Serial.println("========================================");
    Serial.println();

    memset(&sensorData, 0, sizeof(SensorData));

    // Phase 1: load config & mount LittleFS
    webserverInit();

    // Phase 2: WiFi (blocking connect or AP fallback)
    wifiManagerInit();

    // Phase 3: Bluetooth LE scanner (after WiFi, both share the radio)
    sensorInit();

    // Phase 4: start web server
    webserverStart();

    // Phase 5: OTA & MQTT
    setupOTA();
    mqttInit();

    Serial.printf("[MAIN] Setup done! Heap: %u\n", (unsigned)ESP.getFreeHeap());
}

void loop() {
    wifiManagerLoop();
    ArduinoOTA.handle();

    sensorLoop();
    mqttLoop();
    webserverLoop();

    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "read") {
            Serial.printf("[READ] Level: %.1f%% | Raw: %.1f%% | Content: %.1f%s | Battery: %.0f%% | BLE: %s\n",
                sensorData.level, sensorData.raw_level, sensorData.content, "L",
                sensorData.battery, sensorData.ble_state);
        } else if (cmd == "scan") {
            sensorPrintSeenList();
        } else if (cmd == "help") {
            Serial.println("[CMD] Commands: read, scan, help");
        }
    }

    if (millis() - lastStatusPrint > 10000) {
        lastStatusPrint = millis();
        Serial.printf("[STATUS] Level:%.1f%% Content:%.1f%s BLE:%s Scan:%s Adv:%lu WiFi:%s MQTT:%s Heap:%u\n",
            sensorData.level, sensorData.content, "L", sensorData.ble_state,
            sensorData.scanning ? "on" : "OFF", (unsigned long)sensorData.adv_count,
            getWiFiStateString().c_str(),
            mqttIsConnected() ? "OK" : "---",
            (unsigned)ESP.getFreeHeap());
    }

    delay(5);
    yield();
}
