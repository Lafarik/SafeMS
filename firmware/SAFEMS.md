# SafeMS: vollstaendiger MeshCore-Quellstand

Dieses Verzeichnis enthaelt alle 874 Upstream-Dateien von MeshCore-Commit
`e94125987ed87497e706a0b54d1e80c709343980`: Bibliotheksquellen, Beispiele,
Boardvarianten, Buildskripte und Lizenzdateien. Hinzu kommen die sieben Dateien
unserer Boardvariante in `variants/sensebox_eye/`. In `platformio.ini` ist
Bluetooth LE, USB und WLAN als Standard-Build ausgewaehlt. Der Companion-Start
und seine Hauptschleife enthalten die bedingt aktivierten Kiosk-Aufrufe.
Dies ist ein normaler Quellordner, kein Submodul und kein Download-Verweis.

## Bauen

Aus dem Repository-Hauptverzeichnis:

```sh
python scripts/prepare_meshcore.py
python -m platformio run -d firmware -e SenseBox_Eye_companion_radio_ble_usb_wifi
```

Der erste Befehl prueft die SHA-256-Pruefsummen aus
`SAFEMS_SOURCE_MANIFEST.json`, ohne Dateien herunterzuladen oder zu veraendern.
Nach absichtlichen Quellcodeaenderungen meldet die Pruefung Abweichungen;
zum Weiterentwickeln kann PlatformIO direkt aufgerufen werden.
Compiler, Arduino-Framework und externe PlatformIO-Bibliotheken sind
Buildabhaengigkeiten und werden bei Bedarf separat installiert.

## Enthaltene Anpassung

- senseBox Eye / ESP32-S3, SX127x-Anbindung gemaess den geprueften Boardpins.
- Angepasste CAD-Abfrage fuer den gemeinsamen DIO0/DIO1-Interrupt.
- `SenseBox_Eye_companion_radio_ble_usb`: Bluetooth LE und USB gleichzeitig;
  `src/helpers/esp32/SerialBLEInterface.cpp` wird explizit mitgebaut.
- Standard-Kopplungs-PIN `123456`; ein gespeicherter eigener PIN hat Vorrang.
- `SenseBox_Eye_companion_radio_usb`: alternative Variante ohne Bluetooth.
- `SenseBox_Eye_companion_radio_ble_usb_wifi`: zusaetzlich offener WLAN-AP
  `SafeMS-Test`, lokale Testseite unter `http://192.168.4.1/` und DNS/HTTP-Captive-Portal.
  Der HTTP-Server laeuft in einer eigenen Task, die Mesh-Hauptschleife bleibt aktiv.
  Es werden keine administrativen MeshCore-Befehle per WLAN bereitgestellt.

Geraetetests sind in `../docs/hardware.md` dokumentiert.
Der WLAN-Kiosk startet derzeit immer und liefert ausschliesslich Testinformationen.
Fernaktivierung und Hinweise vom Krisenstab ueber Mesh sowie eine QR-Anzeige
sind noch nicht implementiert. Ein WLAN-QR-Code kann zum Verbinden dienen;
das Betriebssystem entscheidet, ob danach ein Captive-Portal-Fenster erscheint.
Funk-Link und CAD-Verhalten mit Gegenstation sind noch zu testen.
Dies ist keine PyPortal-Firmware.

## Herkunft und Lizenz

Upstream: https://github.com/meshcore-dev/MeshCore/tree/e94125987ed87497e706a0b54d1e80c709343980

Vorhandene Upstream-Lizenzen und Copyright-Hinweise bleiben erhalten.
Buildprodukte, lokale Geraete-Backups, Schluessel und Zugangsdaten gehoeren
nicht zu diesem Quellpaket.
