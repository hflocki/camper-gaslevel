Wird nur noch hier gepflegt als Kombisensor https://github.com/hflocki/camper-tanklevel


# Camper Gaslevel – ESP32 BLE Gas-Levelsensor

ESP32-Gateway für Bluetooth-Gaslevelsensoren. Standardmäßig für Rotarex DIMES / SRG WAVE, andere BLE-Sensoren lassen sich über einen Definitionsblock in `config.h` anbinden. Der ESP liest die Werte direkt per Bluetooth LE, rechnet Füllstopp und Inhalt um, schickt alles per MQTT an Home Assistant (Auto-Discovery) und bringt eine eigene Web-Oberfläche mit.

Die DIMES-Auswertung entspricht der Home-Assistant-Integration [ble_gastank](https://github.com/hflocki/ble_gastank). Aufbau, WiFi-Manager, Web-UI und MQTT entsprechen [camper-waterlevel](https://github.com/hflocki/camper-waterlevel).

---

## Hardware

- **MCU:** ESP32-C3 Super Mini (oder jedes ESP32 mit Bluetooth LE: ESP32, ESP32-S3, ESP32-C6)
- **Sensor:** keine Verkabelung, der Gassensor wird per Bluetooth gelesen
- **Stromversorgung:** 5V VIN / Buck-Converter oder USB-C

Den ESP möglichst in Funkreichweite des Gaskastens montieren (Signal im Web-UI unter *Bluetooth Signal*, besser als ca. −85 dBm).

---

## Sensor

**Standard: Rotarex DIMES / SRG WAVE.** Rein passiv, der ESP hört die BLE-Advertisements mit (Herstellerdaten, ID `0xFFFF`, Byte 1 = Batterie %, Byte 2 = Füllstand %). Der Sensor wird nie verbunden, das schont seine Batterie.

Im Web-UI werden eingestellt:

* **MAC-Adresse** des Sensors (oder aus der Geräteliste übernehmen)
* **Tankgröße** (Brutto-Flaschenvolumen in Litern)
* **Füllstopp** an/aus und bei wie viel %. Ist die Flasche bis zum Füllstopp gefüllt, zeigt *Gas Füllstand* 100 %. Der unkorrigierte Wert steht in *Gas Füllstand Rohwert*.

Rechnung: `Füllstand = Rohwert / Füllstopp × 100` (max. 100 %), `Inhalt = Füllstand × Tankgröße × Füllstopp`. Ohne Füllstopp gilt Füllstopp = 100 %.

### Andere BLE-Sensoren anbinden

Unterstützt werden nur Sensoren, die ihre Werte frei im Advertising senden. Der ESP verbindet sich nie mit dem Sensor, es gibt also keine PIN und kein Pairing. Sensoren, die nur nach Verbindung und PIN-Eingabe Daten liefern, gehen damit nicht.

Woher die Werte kommen, steht im Block **SENSOR DEFINITION** in `config.h`. Für einen anderen Sensor diesen Block anpassen und neu flashen. Darunter steht ein auskommentiertes Service-Data-Beispiel als Vorlage.

| Einstellung          | Bedeutung |
| -------------------- | --------- |
| `SENSOR_SOURCE`      | `SRC_MANUFACTURER` (Herstellerdaten im Advertisement) oder `SRC_SERVICE_DATA` (Service Data im Advertisement) |
| `SENSOR_COMPANY_ID`  | Hersteller-ID bei `SRC_MANUFACTURER` (`COMPANY_ANY` = egal) |
| `SENSOR_UUID`        | Service-Data-UUID bei `SRC_SERVICE_DATA` (16 oder 128 Bit) |
| `LEVEL_*`            | Füllstand in % (Rohwert): Byte-Offset, Länge (1/2/4), Big-Endian, Faktor, Addition |
| `BATTERY_*`          | Batterie in %, gleiche Felder, mit `BATTERY_ENABLED false` abschaltbar |

Jeder Wert wird so berechnet: `Wert = Ganzzahl(Bytes ab OFFSET, LEN Bytes) × FACTOR + ADD`. Der Inhalt in Litern wird immer aus Füllstand, Tankgröße und Füllstopp gerechnet.

**Offsets finden:** Im Tab **🔵 Sensor** zeigt die Karte *Empfangene Daten* die Bytes vom eingestellten Sensor mit ihrem Offset. Die Bytes für Füllstand und Batterie sind farbig markiert. Offsets zählen bei Herstellerdaten **ohne** die 2 Bytes der Hersteller-ID, genau wie in nRF Connect angezeigt. Seriell gibt `scan` die gleichen Daten aus.

---

## Installation – Arduino IDE oder PlatformIO

Der Code ist für beide Wege derselbe. Such dir aus, womit du lieber arbeitest:

| Ordner | Für wen | Anleitung |
| ------ | ------- | --------- |
| [`_arduino/`](_arduino/) | Arduino IDE 2.x: Sketch öffnen, Board einstellen, hochladen | [_arduino/README.md](_arduino/README.md) |
| [`_platformio/`](_platformio/) | PlatformIO (VS Code oder CLI): Board, Partition und Libraries sind schon fertig eingestellt | [_platformio/README.md](_platformio/README.md) |

Die Quelltexte liegen nur einmal in `_arduino/camper_gaslevel/`. Das PlatformIO-Projekt verweist per `src_dir` darauf, Änderungen am Code gelten also automatisch für beide Wege.

In `config.h` stehen nur AP-Name, die Test-Passwörter für AP und OTA und die Sensor-Definition. WLAN, MQTT, Sensor-MAC, Tankgröße, Füllstopp und das AP-Passwort stellst du im Web-UI ein.

---

## Erstbenutzung

1. **Flashen:** Firmware und den `data/`-Ordner (LittleFS) auf den ESP32 laden, siehe Anleitung für [Arduino IDE](_arduino/README.md) oder [PlatformIO](_platformio/README.md).
2. **Erstverbindung:** Ohne WLAN-Konfiguration startet der Access Point `Camper-Gaslevel` (Test-Passwort siehe `config.h`).
3. **WLAN & MQTT:** Mit dem AP verbinden, `http://192.168.4.1` öffnen, im Tab **📶 WiFi** die WLAN-Daten und unter **⚙️ MQTT** den Broker eintragen. Nach dem Speichern startet das Modul neu.
4. **Sensor:** Im Tab **🔵 Sensor** die MAC-Adresse eintragen oder in der Liste *Bluetooth-Geräte in der Nähe* auf **Übernehmen** tippen. Geräte mit passender Hersteller-ID sind hervorgehoben. Dann Tankgröße und Füllstopp eintragen und speichern. Ein Neustart ist nicht nötig.

> Gespeicherte Passwörter werden nie an den Browser zurückgesendet. Ein leeres Passwortfeld bedeutet „unverändert lassen“.

---

## Web-UI

- **🔥 Live:** Flaschenanzeige mit Füllstand, Inhalt, Rohwert, Sensor-Batterie, Bluetooth-Signal, Alter des letzten Werts.
- **🔵 Sensor:** MAC, Tankgröße, Füllstopp, Bluetooth-Scan-Status (Anzahl empfangener Pakete), BLE-Geräte in der Nähe, empfangene Rohbytes und die aktive Sensor-Definition aus `config.h`. In der Geräteliste stehen der eingestellte Sensor und Geräte mit passender Hersteller-ID oben, Apple-Geräte (`0x004C`, wechselnde Adressen) unten.
- **⚙️ MQTT** und **📶 WiFi:** wie bei camper_waterlevel.

---

## Home Assistant Integration

Das Modul meldet sich per MQTT Discovery als Gerät **Camper Gaslevel Sensor** an:

- **Sensoren:**
  * `Gas Füllstand` (% – bezogen auf den Füllstopp)
  * `Gas Füllstand Rohwert` (% – 1:1 Sensorwert)
  * `Gas Inhalt` (L)
  * `Gassensor Batterie` (% – nur wenn der Sensor eine Batterie meldet)
  * `Gassensor Signal` (dBm)
  * `Gassensor Status` (`OK` / `Warte auf Daten` / `Keine Daten` / `Kein Sensor`)
  * `WLAN Signal` (dBm)
  * `Status` (online / offline)

Füllstand, Inhalt und Batterie behalten in Home Assistant ihren letzten Wert, auch wenn der ESP aus ist.

---

## MQTT Topics (Default Prefix: `camper/gaslevel`)

| Topic             | Typ     | Beschreibung                                        |
| ----------------- | ------- | --------------------------------------------------- |
| `.../level`       | Sensor  | Füllstand in %, auf Füllstopp skaliert              |
| `.../raw_level`   | Sensor  | Unkorrigierter Sensorwert in %                      |
| `.../content`     | Sensor  | Inhalt in Litern                                    |
| `.../battery`     | Sensor  | Batterie des Gassensors in %                        |
| `.../ble_rssi`    | Sensor  | Bluetooth-Signal des Gassensors (dBm)               |
| `.../ble_state`   | Sensor  | Zustand der Bluetooth-Verbindung                    |
| `.../rssi`        | Sensor  | WLAN-Signalstärke (dBm)                             |
| `.../status`      | LWT     | `online` / `offline`                                |

Neue Werte werden sofort gesendet, zusätzlich alle 30 s.

---

## Serielle Befehle (115200 Baud)

| Befehl | Funktion                                     |
| ------ | -------------------------------------------- |
| `read` | Aktuelle Messwerte ausgeben                  |
| `scan` | BLE-Geräte in der Nähe + Rohdaten ausgeben   |
| `help` | Befehlsliste                                 |

---

## Fehlersuche

| Symptom | Ursache / Lösung |
| ------- | ---------------- |
| „Bluetooth-Scan“ zeigt 0 Pakete | Bluetooth bekommt keine Funkzeit. WLAN-Stromsparmodus nicht abschalten: ESP32-C3 teilt sich die Antenne zwischen WLAN und Bluetooth, `WIFI_PS_MIN_MODEM` ist dafür nötig (in `wifi_manager.cpp` bereits so eingestellt). |
| Pakete steigen, Sensor taucht aber nicht auf | Sensor sendet nicht: **Batterien prüfen** (DIMES: 2 × AAA). Bei leeren Batterien sendet der Sensor gar nicht mehr. Sonst Reichweite prüfen (ESP näher an den Gaskasten). |
| Status „Keine Daten“ | Seit 30 Minuten kein Paket vom Sensor. Batterie oder Reichweite. |
| Werte passen nicht zur Hersteller-App | Im Tab Sensor die Rohbytes prüfen und Offsets im Block `SENSOR DEFINITION` in `config.h` anpassen. |

Der Bluetooth Proxy von Home Assistant stört nicht: Advertisements können beliebig viele Empfänger gleichzeitig mithören.

> 💡 In Home Assistant lohnt sich eine Automation, die bei `Gassensor Batterie` unter ca. 20 % oder `Gassensor Status` = „Keine Daten“ benachrichtigt.

---

## Projektstruktur

```
.
├── README.md                    – Diese Übersicht
├── _arduino/
│   ├── README.md                – Anleitung Arduino IDE
│   └── camper_gaslevel/         – Sketch-Ordner (Name = Name der .ino)
│       ├── camper_gaslevel.ino  – Hauptsketch (Setup & Hauptschleife)
│       ├── config.h             – Systemkonfiguration, SENSOR DEFINITION & Datenstrukturen
│       ├── ble_sensor.h / .cpp  – NimBLE-Scanner, generische Byte-Auswertung
│       ├── mqtt_ha.h / .cpp     – MQTT Client & Home Assistant Auto-Discovery
│       ├── webserver.h / .cpp   – Webserver, REST-API, WebSocket & Config-Speicher
│       ├── wifi_manager.h / .cpp – WLAN-Verbindungssteuerung & AP-Fallback
│       └── data/
│           └── index.html       – Responsive Web-Oberfläche (LittleFS)
└── _platformio/
    ├── README.md                – Anleitung PlatformIO
    └── platformio.ini           – Projekt (nutzt die Quellen aus _arduino/camper_gaslevel)
```

---

## Gehäuse


Das passt recht gut https://makerworld.com/de/models/1072508-esp32-c3-supermini-case-options?from=search#profileId-1062641


### Disclaimer / Haftungsausschluss

Dieses Projekt steht in keinerlei Verbindung zur Rotarex S.A. oder deren eingetragenen Marken (wie DIMES oder SRG WAVE). Es handelt sich um ein inoffizielles Community-Projekt.
