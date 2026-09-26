# senseBox Eye: Hardwarebefund

## Direkt ueber USB festgestellt

ESP32-S3 Revision 0.2, 16 MiB SPI-Flash, 8 MiB eingebautes PSRAM,
USB Serial/JTAG; im Entwicklungsrechner COM7. Keine MAC-Adresse im Repository.
Der USB-Chiptyp belegt keine bestimmte Platinenrevision.

## Schaltplan v1.3

Quelle: https://github.com/sensebox/senseBox_Eye/blob/main/hardware/schematics/v1.3/SenseBox-Eye_v1.3.pdf

Alle fuenf Seiten visuell geprueft. Seite 2: ESP32-S3-WROOM-1-N16R8.
Seite 4: IC4 RFM9XW, direkt ueber SPI angebunden.

| Funktion | GPIO |
| --- | --- |
| MOSI | 38 |
| MISO | 40 |
| SCK | 39 |
| LoRa CS | 43 |
| LoRa IRQ | 44 |
| microSD CS | 41 |
| I2C SDA/SCL | 2/1 |

Funk-DIO0 und DIO1 laufen ueber zwei Dioden auf GPIO44. Funk-Reset besitzt
eine RC-Beschaltung, keinen steuerbaren MCU-Pin. microSD teilt den SPI-Bus.
Der Diagnosecode und die Boardvariante halten microSD CS auf HIGH.

Die Herstellerbeispiele im Ordner `v1.3_and_below_of_Eye` nennen dagegen ein
AT-Modem. Diese Zuordnung widerspricht dem oben verlinkten Schaltplan.
Die tatsaechliche Bestueckung muss deshalb am Geraet geprueft werden.
RegVersion 0x12 wuerde die SX1276/77/78/79-Familie bestaetigen, aber weder den
genauen Typ noch das RF-Matching/Frequenzband eindeutig identifizieren.

## MeshCore-Anpassung

- ESP32-S3 mit USB-Companion-Interface und optionalem Bluetooth LE, ohne Display oder Kamera.
- Kein PSRAM erforderlich; diese erste Variante nutzt internen RAM.
- SPI-Pins gemaess Schaltplan, RadioLib-Reset `RADIOLIB_NC`.
- Boardeigener CAD-Wrapper: CadDetected/CadDone anhand getrennter Funkregisterbits
  statt der zusammengefuehrten GPIO-Pegel. Nach 2 s Timeout Kanal als belegt behandeln.
- RX/TX-Abschlussinterrupts und CAD muessen mit Gegenstation validiert werden.
- Optionaler dauerhaft aktiver WLAN-Testkiosk mit DNS/HTTP-Captive-Portal; keine dedizierte Repeater-Rolle implementiert.

## Sicherung und Wiederherstellung

`python backup_flash.py` liest 16 MiB in 64-KiB-Bloecken. Esptool prueft jeden
Block mit MD5; danach Vergleich des gesamten Images gegen die Geraete-MD5 und
Speicherung einer lokalen SHA-256-Datei. `.partial` ist kein fertiges Backup.
Das Skript verwendet COM7 und setzt das Board in den Bootloader zurueck.
Nur fuer dasselbe Geraet und unveraenderten Flash fortsetzen; fuer ein anderes
Geraet muss ein separater Sicherungspfad verwendet werden.

Ein vollstaendiges Backup kann ueber esptool an Offset 0 zurueckgeschrieben
werden. Vorher Chip, Imagegroesse und Pruefsumme kontrollieren; dies ersetzt
den aktuellen Flashinhalt. Backups koennen Schluessel oder WLAN-Daten enthalten.

## Validierung

- Registertest: PlatformIO-Build erfolgreich.
- MeshCore: abschliessender Build mit CAD-Anpassung erfolgreich (56,8 s).
- Vollstaendiges Backup: 16.777.216 Bytes lokal gesichert, blockweise und als Gesamtimage gegen die Geraete-MD5 verifiziert; SHA-256 separat lokal gespeichert.
- Hardware-Registertest erfolgreich: vier aufeinanderfolgende Lesungen RegVersion=0x12, RegOpMode=0x09, IRQ=0. SPI-Pins bestaetigt, SX1276/77/78/79-Familie erkannt. Exakter RF-Typ und Frequenzband weiterhin offen.
- Diagnose-Firmware zuvor geschrieben und Flash-Pruefsummen verifiziert. Keine Funk-Aussendungen im Diagnosecode.
- Nach dem Diagnose-Upload war ein Watchdog-Reset erforderlich; einfacher RTS-Reset lieferte damals keine Programmausgabe.
- Am 25.09.2026 MeshCore USB Companion v1.17.1 installiert. Bootloader, Partitionstabelle, Boot-App und Firmware beim Schreiben jeweils per Hash verifiziert. Dieser Start gelang nach dem normalen Upload-Reset.
- USB-Protokoll 13 antwortet mit Herstellerkennung `senseBox Eye (experimental)`. Zwei Statusabfragen zeigen steigende Laufzeit (51/54 s), Fehlerflags 0, Sendewarteschlange 0 und RX-Fehler 0. Paketzaehler: RX 0 / TX 0. Kein Funk-Link nachgewiesen.
- Ausgelesene Konfiguration: 869,618 MHz, 62,5 kHz Bandbreite, SF8, CR 4/5, 10 dBm; Client-Repeat deaktiviert. Diese Werte bestaetigen die Softwareeinstellung, nicht RF-Matching oder Reichweite.
- Wiederholbare USB-Pruefung: `python scripts/check_meshcore_usb.py --port COM7`.
- Anschliessend Bluetooth-plus-USB-Variante erfolgreich gebaut und geflasht; Flash-Pruefsummen verifiziert. `BLE_PIN_CODE=123456` aktiviert den BLE-Dienst, `SerialBLEInterface.cpp` wird explizit mitgebaut. Ein gespeicherter PIN hat Vorrang vor dem Build-Standard.
- Nach Bluetooth-Upload: USB-Laufzeit 21/24 s, Fehlerflags 0, Queue 0, RX/TX 0 und RX-Fehler 0. Windows-BLE-Scan erkennt den Knotennamen mit `MeshCore-`-Praefix und UART-Service `6e400001-b5a3-f393-e0a9-e50e24dcca9e` (RSSI im Test -50 dBm). Kopplung mit der Handy-App wurde noch nicht getestet.
- Vor dem Bluetooth-Upload meldete die USB-Variante nach knapp 5 Minuten drei empfangene Pakete und weiterhin TX 0. Gezielte Gegenstellen-, TX- und CAD-Tests stehen noch aus.

## Vollstaendiger Quellstand und WLAN-Test am 26.09.2026

- Alle 874 Dateien des festgelegten MeshCore-Upstreams liegen direkt in `firmware/`, zusammen mit sieben senseBox-Variantendateien, Bauanleitung und Pruefsummenmanifest: insgesamt 883 Firmware-Dateien.
- Standard-Build `SenseBox_Eye_companion_radio_ble_usb_wifi`: Bluetooth LE, USB und WLAN-Kiosk. Erfolgreich gebaut mit PlatformIO 6.2.0; statisch 98.804 Bytes RAM und 1.577.821 Bytes Programm-Flash. Diese Werte ersetzen keine Laufzeitmessung des freien Heaps.
- Angeschlossenes ESP32-S3-Board auf COM7 erneut mit drei Lesungen RegVersion `0x12` identifiziert. Diagnose ohne Funk-Aussendungen.
- Firmware erfolgreich geschrieben; esptool hat die geschriebenen Daten per Hash verifiziert. Bestehende Einstellungen blieben erhalten. Keine neue vollstaendige Flash-Sicherung auf Nutzerwunsch.
- USB-Protokoll 13 / MeshCore v1.17.1 antwortet. Laufzeit stieg nach dem Upload von 23 auf 26 s und spaeter von 262 auf 265 s; Fehlerflags und Sendewarteschlange jeweils 0.
- Spaetere Paketzaehler: RX 8, TX 3, RX-Fehler 1. Das sind vom Board gemeldete Zaehler; ein kontrollierter Gegenstellen-Test inklusive Empfangsbestaetigung steht weiterhin aus.
- Der offene Hotspot `SafeMS-Test` wurde im Windows-WLAN-Scan erkannt. Das Notebook konnte sich verbinden. Der HTTP-Test wurde lokal von Windows mit Socket-Fehler 10013 blockiert; die vorherige WLAN-Verbindung wurde wiederhergestellt.
- Der Nutzer hat anschliessend am Handy erfolgreich `http://192.168.4.1/` aufgerufen und die SafeMS-Testseite bestaetigt. Der manuelle Webseitenabruf ist damit bestaetigt; die automatische Captive-Portal-Erkennung ist noch nicht gesondert geprueft.
- Bluetooth ist im geflashten Build aktiviert. Der anschliessende Scan erkannte die Ziel-Box bisher nicht erneut; bei bestehender App-Verbindung wird die Werbung gestoppt. Eine aktuelle Handy-Bestaetigung steht aus.
- Noch offen: Mesh-Fernaktivierung, authentisierte Meldungsuebernahme, QR-Anzeige, Mehrbenutzer-Dauertest und gezielter Funk-Link.
