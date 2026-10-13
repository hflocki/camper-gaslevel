#ifndef BLE_SENSOR_H
#define BLE_SENSOR_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "config.h"

bool sensorInit();
void sensorLoop();

// Call after the sensor source / MAC / mapping was changed in the web UI
void sensorReset();

// Call after tank size or fill stop changed (keeps the last reading)
void sensorRecalculate();

// Nearby BLE devices for the web UI (address, name, rssi, company ID, service data UUID)
void sensorFillSeenList(JsonArray arr);
void sensorPrintSeenList();

#endif // BLE_SENSOR_H
