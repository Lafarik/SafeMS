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

## Offizielle PWA und Display-Branch

- Eingebauter PWA-Quellstand `notfall-ms/pwa@08bcd73214b11736ddc4e039d5f7c21ee1933940`. Alle 25 Webrouten einschliesslich drei Dokumenten, Logo, CSS und JavaScript liegen im Programm-Flash. Der Blackout-Flyer hat 1.011.864 Bytes.
- WLAN umbenannt in **notfall.ms INFO**. Webseite weiterhin `http://192.168.4.1/`. Die bisherige SafeMS-Testseite wird durch die offizielle PWA-Oberflaeche ersetzt.
- Lokaler HTTP-Fallback fuer die Dokumentliste: kein Service Worker oder CacheStorage erforderlich, keine falsche Zusage einer Speicherung auf dem Handy. 21 gezielte Jest-Tests bestanden.
- Browserpruefung mit den eingebetteten Dateien als lokalen HTTP-Antworten: drei Dokumente, offizielles Logo und PDF vorhanden; keine JavaScript-Fehler und keine fehlenden Ressourcen. Das ist ein Browser-Fixture-Test, kein Hardware-Netzwerktest.
- Erstes PWA-Image erfolgreich auf COM7 geflasht und Hash-verifiziert. USB antwortete danach mit Laufzeit 41/45 s, Fehlerflags 0 und Warteschlange 0.
- Display-Branch `802d600` integriert: SSD1306, 128x64, I2C `0x3D`, SDA GPIO2 / SCL GPIO1, Reset-Pin -1. Die Anpassung ist im gebauten Firmwareordner und im Overlay enthalten.
- Branch-Korrekturen: ungueltige Adafruit-Aufrufe am MeshCore-Wrapper entfallen zugunsten der zentralen Display-Initialisierung; Companion-UI, Displayquellen und Bibliotheken werden mitgebaut. Rotation wird nach erfolgreichem Start gesetzt. Globale Defaults anderer OLED-/ST7789-Boards werden nicht geaendert.
- Ohne erkanntes OLED arbeitet die Box weiter. Fuer den Kiosktest ist der Display-Autotimer ausgeschaltet. Die sichtbare Ausgabe muss am angeschlossenen Display kontrolliert werden.
- Kombinierter PWA/OLED/BLE/USB/WLAN-Build erfolgreich: 99.076 Bytes statischer RAM, 3.819.429 Bytes Programm-Flash (58,3 % der Anwendungs-Partition).
- Vollstaendiger PWA-Ursprung als Quellarchiv, nachvollziehbarer Anpassungs-Patch, Lizenz, Build- und Verpackungsskripte liegen ebenfalls im Repository.
- Abschliessend auch den kombinierten Display/PWA-Build auf COM7 geschrieben; Flash-Hash geprueft. Danach USB-Laufzeit 81/84 s, Fehlerflags 0, Queue 0, RX 1, TX 0 und RX-Fehler 0.
- Der Nutzer bestaetigt am Handy im WLAN **notfall.ms INFO**: offizielle Oberflaeche und Blackout-PDF funktionieren. Sichtbare OLED-Ausgabe und Bluetooth-Kopplung bleiben gesondert zu pruefen; im aktuellen Scan wurde die Ziel-Box nicht als neues BLE-Geraet gefunden.


## Krisenstab-WebSocket, PWA-Update und WLAN-QR am 26.09.2026

- Der Nutzer hat die funktionierende sichtbare OLED-Ausgabe bestaetigt.
- Offiziellen PWA-Upstream erneut geprueft und auf Version 1.1.2 / Commit
  `369e253aa39fe71de66c89bcb5950fef732ac17b` aktualisiert (Bluetooth-Diagnose).
- Empfang aus dem vorhandenen MeshCore-Kanal `Krisenstab` wird in Pager-JSON
  umgesetzt und per `/ws` an alle verbundenen PWA-Clients verteilt.
  Implementierte Grenzen und Uhrverhalten stehen in `live-pager.md`.
- 48 Jest-Tests bestanden; produktiver Browser-Build mit simulierter WebSocket-
  Gegenstelle zeigt Live-Push ohne Neuladen, drei Dokumente und Blackout-PDF.
  Keine Browserfehler/fehlenden Ressourcen. Kein Ersatz fuer den Funk-End-to-End-Test.
- Vollstaendiger PWA-Quellstand aus Archiv plus Patch rekonstruiert und verglichen.
- WLAN-QR mit explizitem `nopass`, SSID `notfall.ms INFO`, Version 2-L erzeugt.
  Gerendertes OLED-Bild mit unabhaengigem ZXing-Decoder korrekt dekodiert.
  Physischer Handy-Scan auf diesem kleinen Display noch vom Nutzer zu bestaetigen.
- Build: 101.564 Bytes statischer RAM, 3.841.285 Bytes Programm-Flash (58,6 %).
  3.841.696-Byte-Image erfolgreich auf COM7 geschrieben und Hash verifiziert.
- Alle 19 read-only Parser-/JSON-/Verlauf-Selbsttests direkt auf dem ESP32 bestanden.
  Kiosk-UTC vom Rechner synchronisiert. Unterliegende MeshCore-RTC unveraendert,
  da sie beim Auslesen ein falsches Datum lieferte.
- Nach Flash USB-Laufzeit 33/36 s, Fehlerflags 0, Queue 0, RX/TX/RX-Fehler 0.
  Funkparameter unveraendert. BLE, USB, WLAN und OLED bleiben im selben Build.
- Der echte Meldungsspeicher war beim USB-Test leer. Gezielte Funkmeldung von
  zweitem Knoten und Anzeige auf dem Handy sind zur Bestaetigung angefragt.

## Groesserer WLAN-QR

- OLED-Datenfeld auf 50x50 Pixel vergroessert: einheitliche 2x2-Pixel-Module,
  SSID und IP-Adresse rechts daneben. Oben/unten jeweils sieben Pixel weisser
  Rand; die Begrenzung gegenueber der Vier-Modul-Ruhezone ist in `live-pager.md`
  dokumentiert. Hotspotname und QR-Nutzdaten bleiben unveraendert.
- Exakt gerenderte Headerdaten mit schwarzem Displayhintergrund und Beschriftung
  bei nativen 128x64 sowie 2x/4x/8x Bildgroessen erfolgreich mit ZXing dekodiert.
  Synthetischer Test mit leichter Unschaerfe und Drehung ebenfalls erfolgreich;
  ein echter Handy-Scan bleibt gesondert zu bestaetigen.
- Build erfolgreich: 101.564 Bytes RAM, 3.841.253 Bytes Programm-Flash.
  3.841.664-Byte-Image auf COM7 geflasht und Schreib-Hash verifiziert.
- Danach Kiosk-UTC erneut synchronisiert, alle 19 Geraete-Selbsttests bestanden.
  USB-Laufzeit 2/5 s, Fehlerflags 0, Queue 0; Funkparameter unveraendert.
