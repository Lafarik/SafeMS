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

- ESP32-S3 mit USB-Companion-Interface, ohne Display oder Kamera.
- Kein PSRAM erforderlich; diese erste Variante nutzt internen RAM.
- SPI-Pins gemaess Schaltplan, RadioLib-Reset `RADIOLIB_NC`.
- Boardeigener CAD-Wrapper: CadDetected/CadDone anhand getrennter Funkregisterbits
  statt der zusammengefuehrten GPIO-Pegel. Nach 2 s Timeout Kanal als belegt behandeln.
- RX/TX-Abschlussinterrupts und CAD muessen mit Gegenstation validiert werden.
- Kein dauerhafter Kiosk-Webserver und keine Repeater-Rolle implementiert.

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
- Diagnose-Firmware geschrieben und Flash-Pruefsummen verifiziert. Sie ist aktuell installiert, MeshCore wurde noch nicht geflasht. Keine Funk-Aussendungen im Testcode.
- Nach dem Upload war ein Watchdog-Reset erforderlich; einfacher RTS-Reset lieferte keine Programmausgabe.
- MeshCore RX/TX/CAD noch nicht am Geraet validiert.
