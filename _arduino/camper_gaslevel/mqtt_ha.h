#ifndef MQTT_HA_H
#define MQTT_HA_H

#include <Arduino.h>
#include "config.h"

void mqttInit();
void mqttLoop();
bool mqttIsConnected();
void mqttPublishDiscovery();
void mqttPublishState();

// Re-send the discovery on the next loop (e.g. the content unit changed with the sensor type)
void mqttRequestDiscovery();

#endif // MQTT_HA_H
