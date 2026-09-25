# SafeMS

Hackathon-Prototyp fuer dezentrale Notfall-Datenkioske: lokale Informationen per
WLAN, kurze Lage-Updates ueber LoRa/MeshCore und Betrieb ohne Internet.

## Stand

- senseBox Eye mit ESP32-S3, 16 MiB Flash und 8 MiB PSRAM per USB identifiziert.
- Experimentelle MeshCore-USB-Companion-Boardvariante installiert; v1.17.1 antwortet ueber USB auf COM7.
- SPI-Diagnoseprogramm kompiliert und am Geraet getestet: RegVersion 0x12, SX1276/77/78/79-Familie. Keine Aussendungen.
- Original-Firmware vollstaendig lokal gesichert und gegen die Geraete-Pruefsumme verifiziert.
- Kein bestaetigter Funkbetrieb, kein fertiges Notfallportal.
- Hardware-Backup und Diagnoseergebnisse: siehe `docs/hardware.md`.

## Projektaufbau

| Pfad | Inhalt |
| --- | --- |
| `radio-probe/` | Registertest ohne Funk-Aussendungen |
| `meshcore-overlay/` | Eigene Boardvariante fuer den festgelegten MeshCore-Stand |
| `scripts/prepare_meshcore.py` | Laedt MeshCore und kopiert die Boardvariante hinein |
| `backup_flash.py` | Lokale, blockweise gepruefte Flash-Sicherung |
| `docs/` | Hardwarebefunde und Kiosk-Konzept |

## Bauen

Python, Git und PlatformIO werden benoetigt:

```sh
python -m pip install platformio==6.2.0 esptool==4.12.0
python -m platformio run -d radio-probe
python scripts/prepare_meshcore.py
python -m platformio run -d MeshCore -e SenseBox_Eye_companion_radio_usb
```

Das Vorbereitungsskript nutzt MeshCore-Commit
`e94125987ed87497e706a0b54d1e80c709343980`. MeshCore und seine Abhaengigkeiten
behalten ihre jeweiligen Lizenzen; upstream: https://github.com/meshcore-dev/MeshCore.
Builds laden Abhaengigkeiten aus GitHub und der PlatformIO-Registry.

Die Befehle oben bauen nur und flashen kein Geraet. Die Boardvariante ist
experimentell. Funkchip und Interrupt-Verhalten muessen am konkreten Board
geprueft werden. Erst nach vollstaendiger Sicherung einen Diagnosetest flashen.
Backups, Zugangsdaten und lokale Logs werden nicht versioniert.

## Flashen und USB pruefen

Nach Sicherung und Hardwarepruefung, mit dem passenden Board auf COM7:

```sh
python -m platformio run -d MeshCore -e SenseBox_Eye_companion_radio_usb -t upload --upload-port COM7
python scripts/check_meshcore_usb.py --port COM7
```

Der USB-Test liest Firmwareversion, Funkparameter, Laufzeit und Fehlerzaehler.
Er fordert keine Funk-Aussendungen an und liest keine privaten Schluessel aus.
Am 25.09.2026 erfolgreich geprueft: steigende Laufzeit, Fehlerflags 0,
keine gesendeten oder empfangenen Funkpakete. Ein Funk-Link ist damit noch
nicht bestaetigt. Das WLAN-Notfallportal ist noch nicht implementiert.
