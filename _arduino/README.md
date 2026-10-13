# Camper Gaslevel – Arduino IDE

Anleitung für die Arduino IDE 2.x. Allgemeines zum Projekt, Web-UI, Home Assistant und Fehlersuche: siehe [README im Hauptordner](../README.md).

---

## Sketch

In der Arduino IDE die Datei `camper_gaslevel/camper_gaslevel.ino` öffnen. Die `.cpp`/`.h`-Dateien im selben Ordner werden automatisch mitkompiliert. Der Ordner muss `camper_gaslevel` heißen (Name = Name der `.ino`), sonst will die IDE den Sketch verschieben.

Vor dem ersten Flashen in `config.h` das OTA-Passwort (`DEFAULT_OTA_PASSWORD`) ändern. Das geht nur im Code, nicht im Web-UI.

## Board

- **Board-Paket:** `esp32` by Espressif Systems, Version 3.x (Boards Manager)
- **Board Manager URL:** `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
- **Board:** ESP32C3 Dev Module
- **Flash Size:** 4MB
- **Partition Scheme:** ⚠️ **Minimal SPIFFS (1.9MB APP with OTA/190KB SPIFFS)**
  WiFi + Bluetooth zusammen ergeben ca. 1,42 MB Firmware und passen nicht mehr in die Standard-Partition (1,2 MB). Die 190 KB Dateisystem reichen für `index.html` und `config.json`. OTA bleibt möglich.
- **USB CDC On Boot:** Enabled (nötig für die Serial-Ausgabe beim ESP32-C3 Super Mini)

## Benötigte Libraries (Library Manager)

1. **PubSubClient** (by Nick O'Leary)
2. **ArduinoJson** (by Benoit Blanchon) – Version 7.x
3. **ESPAsyncWebServer** (ESP32Async-Fork)
4. **AsyncTCP** (ESP32Async-Fork)
5. **NimBLE-Arduino** (by h2zero) – Version **2.x**

Getestet: esp32 3.3.9, NimBLE-Arduino 2.5.1, ArduinoJson 7.4.2, ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0.

## Dateisystem (`data/`-Ordner) hochladen

Die Arduino IDE 2.x hat keinen eingebauten LittleFS-Upload. Dafür wird das Plugin [arduino-littlefs-upload](https://github.com/earlephilhower/arduino-littlefs-upload) benötigt. Wichtig: vorher das Partition Scheme oben einstellen, sonst landet das Dateisystem an der falschen Stelle.

Installation: die `.vsix`-Datei aus den [Releases](https://github.com/earlephilhower/arduino-littlefs-upload/releases) nach `~/.arduinoIDE/plugins/` legen (Ordner ggf. anlegen) und die IDE neu starten.

Ablauf: Sketch hochladen, den Seriellen Monitor schließen, dann mit ⌘+⇧+P (Windows/Linux: Strg+⇧+P) die Befehlsleiste öffnen und **„Upload LittleFS to Pico/ESP8266/ESP32“** ausführen.

---

## Flashen

1. Board-Einstellungen wie oben prüfen, vor allem das **Partition Scheme**.
2. ESP32-C3 per USB anschließen und den Port wählen.
3. Sketch hochladen.
4. `data/`-Ordner per LittleFS-Upload hochladen.
5. Weiter mit der [Erstbenutzung](../README.md#erstbenutzung).

Schlägt der Upload fehl: **BOOT** gedrückt halten, kurz **RESET** drücken, BOOT loslassen und nochmal hochladen.

## Update per WLAN (OTA)

Ist der ESP im WLAN, erscheint er in der Arduino IDE unter **Werkzeuge → Port → Netzwerk-Ports** als `camper-gaslevel`. Beim Hochladen fragt die IDE nach dem Passwort, das ist `DEFAULT_OTA_PASSWORD` aus `config.h`. Der LittleFS-Upload per Plugin geht nur über USB.

## Serieller Monitor

115200 Baud. Befehle: `read`, `scan`, `help`.
