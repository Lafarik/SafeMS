# SafeMS

Hackathon-Prototyp fuer dezentrale Notfall-Datenkioske: lokale Informationen per
WLAN, kurze Lage-Updates ueber LoRa/MeshCore und Betrieb ohne Internet.

## Stand

- senseBox Eye mit ESP32-S3, 16 MiB Flash und 8 MiB PSRAM per USB identifiziert.
- Experimentelle MeshCore-Companion-Boardvariante mit Bluetooth LE, USB und offenem WLAN-Kiosk mit der offiziellen notfall.ms-PWA.
- Vollstaendiger MeshCore-Quellcode inklusive senseBox-Anpassung direkt in `firmware/`; kein zusaetzlicher MeshCore-Download noetig.
- SPI-Diagnoseprogramm kompiliert und am Geraet getestet: RegVersion 0x12, SX1276/77/78/79-Familie. Keine Aussendungen.
- Original-Firmware vollstaendig lokal gesichert und gegen die Geraete-Pruefsumme verifiziert.
- Offizielle PWA aus `notfall-ms/pwa` mit Dokumenten und DNS/HTTP-Captive-Portal enthalten. Krisenstab-Meldungen per Mesh, Fernaktivierung und QR-Anzeige sind noch nicht implementiert.
- Ein gezielter Funk-Link mit Gegenstation ist noch nicht bestaetigt.
- Hardware-Backup und Diagnoseergebnisse: siehe `docs/hardware.md`.

## Projektaufbau

| Pfad | Inhalt |
| --- | --- |
| `firmware/` | Vollstaendiger MeshCore-Quellbaum samt senseBox-Anpassung; Standard: Bluetooth LE + USB + WLAN |
| `firmware/SAFEMS.md` | Herkunft und Bauanleitung fuer den vollstaendigen Quellstand |
| `firmware/SAFEMS_SOURCE_MANIFEST.json` | SHA-256-Pruefsummen aller enthaltenen Firmware-Quelldateien |
| `radio-probe/` | Registertest ohne Funk-Aussendungen |
| `meshcore-overlay/` | Eigene Boardvariante fuer den festgelegten MeshCore-Stand |
| `scripts/prepare_meshcore.py` | Prueft die enthaltenen Firmwaredateien; laedt nichts herunter |
| `backup_flash.py` | Lokale, blockweise gepruefte Flash-Sicherung |
| `docs/` | Hardwarebefunde und Kiosk-Konzept |

## Bauen

Python und PlatformIO werden benoetigt. Bereits der normale GitHub-ZIP-Download
enthaelt den vollstaendigen MeshCore-Quellbaum:

```sh
python -m pip install platformio==6.2.0 esptool==4.12.0
python -m platformio run -d radio-probe
python scripts/prepare_meshcore.py
python -m platformio run -d firmware -e SenseBox_Eye_companion_radio_ble_usb_wifi
```

Der enthaltene Quellbaum basiert auf MeshCore-Commit
`e94125987ed87497e706a0b54d1e80c709343980`. MeshCore und seine Abhaengigkeiten
behalten ihre jeweiligen Lizenzen; upstream: https://github.com/meshcore-dev/MeshCore.
MeshCore selbst wird nicht nachgeladen. Compiler, Arduino-Framework und externe
Bibliotheken installiert PlatformIO beim ersten Build bei Bedarf; dafuer kann Git
benoetigt werden. `python -m platformio run -d firmware` baut standardmaessig
Bluetooth LE, USB und WLAN gleichzeitig.

Die Befehle oben bauen nur und flashen kein Geraet. Die Boardvariante ist
experimentell. Funkchip und Interrupt-Verhalten muessen am konkreten Board
geprueft werden. Erst nach vollstaendiger Sicherung einen Diagnosetest flashen.
Backups, Zugangsdaten und lokale Logs werden nicht versioniert.

## Flashen und USB pruefen

Nach Sicherung und Hardwarepruefung, mit dem passenden Board auf COM7:

```sh
python -m platformio run -d firmware -e SenseBox_Eye_companion_radio_ble_usb_wifi -t upload --upload-port COM7
python scripts/check_meshcore_usb.py --port COM7
```

Die Standardvariante aktiviert Bluetooth LE, USB und WLAN gleichzeitig.
In der MeshCore-App erscheint das Board als
`MeshCore-<Knotenname>`; die voreingestellte Kopplungs-PIN ist `123456`.
Ein bereits gespeicherter eigener PIN hat Vorrang. Der USB-Test zeigt den
Knotennamen an. Die Variante `SenseBox_Eye_companion_radio_usb` bleibt fuer
USB-Betrieb ohne Bluetooth verfuegbar. `SenseBox_Eye_companion_radio_ble_usb`
baut Bluetooth und USB ohne WLAN.

Der USB-Test liest Firmwareversion, Funkparameter, Laufzeit und Fehlerzaehler.
Er fordert keine Funk-Aussendungen an und liest keine privaten Schluessel aus.
Am 25.09.2026 erfolgreich geprueft: steigende Laufzeit, Fehlerflags 0,
keine gesendeten oder empfangenen Funkpakete. Ein Funk-Link ist damit noch
nicht bestaetigt. Aktuelle Geraetetests werden in `docs/hardware.md` dokumentiert.

## WLAN-Kiosk mit der offiziellen PWA

Nach dem Start bietet die Standardvariante das offene WLAN **notfall.ms INFO** an.
Mit diesem WLAN verbinden und **http://192.168.4.1/** aufrufen. Internet ist
dafuer nicht erforderlich. DNS-Anfragen werden lokal beantwortet; HTTP-Aufrufe
anderer Pfade werden zur Startseite umgeleitet. Das automatische Oeffnen eines
Anmeldefensters haengt vom Smartphone ab; die lokale Adresse funktioniert als
manueller Einstieg. HTTPS wird nicht umgeleitet.

Die Website stammt aus `notfall-ms/pwa` (Version 1.1.2). Auf der Box zeigt sie
Live-Meldungen aus dem MeshCore-Kanal **Krisenstab** ueber WebSocket an.
Nachrichtenformat, Zeitabgleich und Tests: [Live-Pager](docs/live-pager.md).
Alle drei Dokumente einschliesslich Blackout-PDF werden lokal ausgeliefert.
Quellstand, Anpassungen und HTTP-Einschraenkungen: siehe `web/README.md`.
`/api/status` zeigt
Laufzeit, WLAN-Clients, freien Heap und ob die Mesh-Hauptschleife weiterlaeuft.
Das belegt keinen erfolgreichen Funk-Link. Maximal vier WLAN-Clients sind
konfiguriert. Der HTTP-Server laeuft in einer eigenen Task; MeshCore, USB und
Bluetooth bleiben aktiv. Es gibt keine administrativen HTTP-Endpunkte.

WLAN startet momentan immer beim Booten. Die erste OLED-Seite zeigt einen
WLAN-QR-Code. Eine Taste wechselt zu den MeshCore-Statusseiten. Die
Fernaktivierung des Hotspots ueber Mesh bleibt ein weiterer Ausbauschritt.
Meldungen werden anhand des eingerichteten Kanals gefiltert; alle Teilnehmer
mit dem Krisenstab-Kanalschluessel koennen sie senden.

## OLED aus dem Branch display

Der Display-Branch `802d600` ist mit korrigierter Build-Einbindung integriert.
Unterstuetzt wird ein SSD1306-OLED (128x64) an I2C-Adresse `0x3D`,
SDA GPIO2 / SCL GPIO1. Ohne antwortendes OLED startet die Box weiter.
MeshCores Standard-Oberflaeche bleibt fuer den Kiosk eingeschaltet.
Bei vorhandenem Display und nicht gesetzter eigener Bluetooth-PIN kann
MeshCore eine zufaellige PIN fuer den Start erzeugen und auf dem Display zeigen.

