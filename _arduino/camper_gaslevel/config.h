#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ============================================================
// GENERAL CONSTANTS
// ============================================================
#define FW_VERSION            "1.1.0"

#define DEFAULT_AP_SSID       "Camper-Gaslevel"
#define DEFAULT_AP_PASSWORD   "gaslevel1234"     // Test placeholder (min. 8 characters) - change in the web UI
#define DEFAULT_OTA_PASSWORD  "gaslevel1234"     // Test placeholder
#define DEFAULT_MQTT_SERVER   ""                // Set in the web UI
#define DEFAULT_MQTT_PORT     1883
#define DEFAULT_MQTT_PREFIX   "camper/gaslevel"

// WiFi TX power in 0.25 dBm steps: 70 = 17.5 dBm.
// Full power (19.5 dBm) tends to be unstable on ESP32-C3 Super Mini boards.
#define WIFI_TX_POWER         ((wifi_power_t)70)

#define MQTT_PUBLISH_MS       30000  // Periodic publish (values are also published immediately on change)
#define WS_UPDATE_MS          1000   // Web UI live update interval

#define CONFIG_FILE           "/config.json"
#define OTA_HOSTNAME          "camper-gaslevel"

// ============================================================
// GENERIC BLE SENSOR
// ============================================================
// Where the sensor data comes from
#define SRC_MANUFACTURER      0      // Advertisement: manufacturer specific data (passive)
#define SRC_SERVICE_DATA      1      // Advertisement: service data of a given UUID (passive)

#define COMPANY_ANY           -1     // Accept manufacturer data of any company ID

// ============================================================
// >>> SENSOR DEFINITION - edit this block for other BLE sensors <<<
//
// Each value is decoded as:
//   value = unsigned integer (LEN bytes at OFFSET, little or big endian) * FACTOR + ADD
// LEVEL must give the raw fill level in % (0-100), BATTERY the battery in %.
// Content (liters) is calculated from LEVEL, tank size and fill stop (web UI).
// Only passive advertisement data is supported (no connection, no PIN, no pairing).
//
// Offsets refer to:
//   SRC_MANUFACTURER : manufacturer data WITHOUT the 2 company ID bytes
//                      (same view as nRF Connect / Home Assistant)
//   SRC_SERVICE_DATA : service data of SENSOR_UUID
// The web UI shows the received bytes with their offsets (tab "Sensor").
// ============================================================

// --- Default: Rotarex DIMES / SRG WAVE ---
// manufacturer data, company ID 0xFFFF, payload: [1] = battery %, [2] = level %
#define SENSOR_MODEL          "DIMES / SRG WAVE"
#define SENSOR_SOURCE         SRC_MANUFACTURER
#define SENSOR_COMPANY_ID     0xFFFF   // SRC_MANUFACTURER only (COMPANY_ANY = any)
#define SENSOR_UUID           ""       // SRC_SERVICE_DATA only: service data UUID

#define LEVEL_OFFSET          2
#define LEVEL_LEN             1        // 1, 2 or 4 bytes
#define LEVEL_BIG_ENDIAN      false
#define LEVEL_FACTOR          1.0f
#define LEVEL_ADD             0.0f

#define BATTERY_ENABLED       true     // false if the sensor does not report a battery level
#define BATTERY_OFFSET        1
#define BATTERY_LEN           1
#define BATTERY_BIG_ENDIAN    false
#define BATTERY_FACTOR        1.0f
#define BATTERY_ADD           0.0f

/* --- Example: sensor sending service data (replace the block above) ---
#define SENSOR_MODEL          "My sensor"
#define SENSOR_SOURCE         SRC_SERVICE_DATA
#define SENSOR_COMPANY_ID     COMPANY_ANY
#define SENSOR_UUID           "fcd2"   // Service data UUID (16 bit or 128 bit)
#define LEVEL_OFFSET          3
#define LEVEL_LEN             2
#define LEVEL_BIG_ENDIAN      false
#define LEVEL_FACTOR          0.1f     // e.g. value in 0.1 %
#define LEVEL_ADD             0.0f
#define BATTERY_ENABLED       true
#define BATTERY_OFFSET        1
#define BATTERY_LEN           1
#define BATTERY_BIG_ENDIAN    false
#define BATTERY_FACTOR        1.0f
#define BATTERY_ADD           0.0f
*/
// ============================================================

#define DEFAULT_CAPACITY      22.0f  // Gross bottle volume in liters (web UI)
#define DEFAULT_FILL_STOP_ON  true   // Bottle has a mechanical fill stop (web UI)
#define DEFAULT_FILL_STOP     80.0f  // Fill stop in % (web UI)

// Passive scan duty cycle (units of 0.625 ms). Window < interval leaves airtime for WiFi.
#define BLE_SCAN_INTERVAL     160    // 100 ms
#define BLE_SCAN_WINDOW       80     // 50 ms (50 % duty cycle)
#define BLE_MAX_SEEN          30     // Devices kept in the "nearby" list of the web UI
#define BLE_SEEN_TIMEOUT_MS   120000 // Drop devices from that list after 2 min silence
#define BLE_RAW_MAX           32     // Max. bytes kept for the raw data view

// Values older than this are reported as "no data"
#define DATA_TIMEOUT_MS       (30UL * 60UL * 1000UL)

// ============================================================
// VALUE MAPPING
// value = (bytes at offset, length, endianness) * factor + add
// ============================================================
struct ValueMap {
    bool enabled;
    uint8_t offset;        // Byte offset in the payload / characteristic value
    uint8_t len;           // 1, 2 or 4 bytes (unsigned)
    bool big_endian;
    float factor;
    float add;
};

// ============================================================
// CONFIGURATION STRUCT
// ============================================================
struct AppConfig {
    char wifi_ssid[64];
    char wifi_pass[64];
    char ap_ssid[32];
    char ap_pass[32];

    char mqtt_server[64];
    uint16_t mqtt_port;
    char mqtt_user[32];
    char mqtt_pass[32];
    char mqtt_prefix[32];

    // Sensor (data mapping is defined at compile time, see SENSOR DEFINITION)
    char sensor_mac[18];   // "AA:BB:CC:DD:EE:FF" (upper case), empty = no sensor

    // Tank
    float capacity;        // Gross capacity in liters
    bool fill_stop_on;     // Bottle has a mechanical fill stop
    float fill_stop;       // Fill stop in %
};

// ============================================================
// SENSOR DATA STRUCT
// ============================================================
struct SensorData {
    bool has_data;         // At least one valid reading since boot
    float battery;         // Sensor battery in % (NAN = unknown)
    float raw_level;       // Raw sensor level in % (1:1 value)
    float level;           // Level scaled to the fill stop in %
    float content;         // Remaining content in liters
    int ble_rssi;          // Signal strength of the gas sensor
    unsigned long last_update;  // millis() of the last valid reading
    uint32_t updates;      // Counter of valid readings (used to detect new data)
    char ble_state[24];    // Human readable BLE state for the web UI / MQTT
    char raw_hex[BLE_RAW_MAX * 3 + 1];  // Last payload of the configured sensor as hex (for mapping)
    uint32_t adv_count;    // Advertisements received since boot (any device, for diagnosis)
    bool scanning;         // BLE scan is running
    int rssi;              // WiFi signal strength (written by wifi_manager)
};

extern AppConfig config;
extern SensorData sensorData;

#endif // CONFIG_H
