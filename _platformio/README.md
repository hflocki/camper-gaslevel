# Camper Gaslevel – PlatformIO

Anleitung für PlatformIO (VS Code-Erweiterung oder CLI). Allgemeines zum Projekt, Web-UI, Home Assistant und Fehlersuche: siehe [README im Hauptordner](../README.md).

---

## Wie das Projekt aufgebaut ist

Hier liegt nur die `platformio.ini`. Die Quellen und der `data/`-Ordner liegen in [`../_arduino/camper_gaslevel/`](../_arduino/camper_gaslevel/) und werden per `src_dir` / `data_dir` eingebunden. So gibt es nur einen Code-Stand für Arduino IDE und PlatformIO.

In der `platformio.ini` ist schon alles eingestellt:

- **Plattform:** [pioarduino](https://github.com/pioarduino/platform-espressif32) `55.03.39` = Arduino-Core **3.3.9**. Die offizielle `espressif32`-Plattform von PlatformIO bringt nur Core 2.x mit, der Code braucht aber 3.x.
- **Board:** `esp32-c3-devkitm-1` (passt für den ESP32-C3 Super Mini), USB-CDC für die serielle Ausgabe ist aktiviert.
- **Partition:** `min_spiffs.csv` (1,9 MB App mit OTA, 190 KB Dateisystem). WiFi + Bluetooth passen nicht in die Standard-Partition.
- **Dateisystem:** LittleFS
- **Libraries:** werden beim ersten Build automatisch geladen (PubSubClient, ArduinoJson 7, ESPAsyncWebServer, AsyncTCP, NimBLE-Arduino 2.x).

Getestet: Build mit pioarduino 55.03.39, RAM 17 %, Flash 71 %.

---

## Vorbereitung

1. [VS Code](https://code.visualstudio.com/) mit der Erweiterung **PlatformIO IDE** installieren (oder nur die PlatformIO CLI).
2. In VS Code **den Ordner `_platformio` öffnen** (nicht das ganze Repo), damit PlatformIO die `platformio.ini` findet.
3. In `../_arduino/camper_gaslevel/config.h` das OTA-Passwort (`DEFAULT_OTA_PASSWORD`) ändern und denselben Wert in der `platformio.ini` bei `--auth=` eintragen.

Der erste Build lädt Plattform, Toolchain und Libraries herunter und dauert ein paar Minuten.

---

## Flashen per USB

In VS Code über die PlatformIO-Leiste (Umgebung `esp32-c3`) oder im Terminal im Ordner `_platformio`:

```bash
pio run -e esp32-c3 -t upload        # Firmware bauen und hochladen
pio run -e esp32-c3 -t uploadfs      # data/ als LittleFS hochladen (index.html)
pio device monitor                   # Serieller Monitor, 115200 Baud
```

Danach weiter mit der [Erstbenutzung](../README.md#erstbenutzung).

Schlägt der Upload fehl: **BOOT** gedrückt halten, kurz **RESET** drücken, BOOT loslassen und nochmal hochladen.

## Update per WLAN (OTA)

Ist der ESP im WLAN, über die Umgebung `esp32-c3-ota`:

```bash
pio run -e esp32-c3-ota -t upload    # Firmware per WLAN
pio run -e esp32-c3-ota -t uploadfs  # Dateisystem per WLAN
```

Standardmäßig wird `camper-gaslevel.local` angesprochen. Klappt mDNS in deinem Netz nicht, in der `platformio.ini` bei `upload_port` die IP-Adresse des ESP eintragen.

## Serieller Monitor

```bash
pio device monitor
```

Befehle: `read`, `scan`, `help`.
