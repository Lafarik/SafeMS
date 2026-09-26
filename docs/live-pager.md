# Krisenstab -> MeshCore -> WLAN-Pager

Die Box verarbeitet empfangene Textnachrichten aus dem bereits eingerichteten,
exakt **Krisenstab** genannten MeshCore-Kanal. Andere Kanaele und Direktnachrichten
werden nicht auf der Website veroeffentlicht. Beispiel zum Senden von einem
anderen MeshCore-Knoten:

```text
title: Stromausfall
message: Hier ist dick Stromausfall, Leute!!
```

Alternativ koennen beide Felder in einer Zeile stehen:

```text
title: Stromausfall message: Hier ist dick Stromausfall, Leute!!
```

Zwischen den Feldern genuegen Leerzeichen oder Tabs; echte Zeilenumbrueche
(LF/CRLF) funktionieren weiterhin. Auch ein als zwei Zeichen geschriebenes `\n`
direkt vor `message:` wird als Feldtrenner akzeptiert; im Nachrichtentext
werden solche Zeichenfolgen nicht ersetzt. MeshCores automatisch vorangestellter
Knotenname wird entfernt. `title:` und `message:` sind weiterhin erforderlich;
der Nachrichtentext darf mehrere Zeilen haben. Leere Felder, doppelte Feldnamen,
ungueltiges UTF-8 und mehr als 160 UTF-8-Bytes einschliesslich des Knotennamen-
Praefixes werden verworfen. MeshCore kann zu lange Texte bereits beim Sender
abschneiden, deshalb kurze Meldungen senden.

Die Kanalverschluesselung ist die Zugangskontrolle: Jeder mit diesem
Kanalschluessel kann Meldungen veroeffentlichen. Ein einzelner Absender wird
damit nicht als Krisenstab authentisiert; ein angezeigter Knotenname ist kein
Identitaetsnachweis. Kanalschluessel werden nicht im Repository gespeichert.

## JSON und WebSocket

Die PWA verbindet sich mit `ws://192.168.4.1/ws`. Nach dem Verbindungsaufbau und
bei jeder neuen Meldung erhaelt sie einen vollstaendigen Snapshot:

```json
{"messages":[{"id":"msg-<32 Hexzeichen>","title":"Stromausfall","message":"Hier ist dick Stromausfall, Leute!!","timestamp":"2026-09-26T10:18:40.123Z"}]}
```

Der Beispielzeitstempel wird nicht fest eingebaut. Nach UTC-Synchronisierung
verwendet die Box die aktuelle Empfangszeit. Nach einem Neustart ohne Zeitsync
verwendet sie den MeshCore-Sendezeitstempel des Kanals (Sekunden, `.000Z`). Dessen
Genauigkeit haengt von der Senderuhr ab. Ungueltige Zeitwerte ausserhalb
2026..2099 werden ohne synchronisierte Empfangsuhr nicht veroeffentlicht.
Die fehlerhafte automatisch erkannte Board-RTC wird fuer den Pager nicht benutzt.

```sh
python scripts/check_kiosk.py --port COM7 --sync-time
```

Dieses Skript synchronisiert die separate Kiosk-Uhr vom Rechner, fuehrt die
Parser/JSON/Verlauf-Pruefungen direkt auf dem ESP32 aus und liest den echten
Meldungsspeicher. Eine erfolgreiche normale MeshCore-Zeitsynchronisierung
synchronisiert ebenfalls die Kiosk-Uhr. USB-Erweiterungen: 112 + uint32 LE UTC
setzt die Zeit; 113 + uint16 LE Offset liest JSON in bis zu 171-Byte-Bloecken
(176-Byte-MeshCore-Frame abzueglich fuenf Headerbytes);
114 liefert eine uint32 LE Test-Fehlerbitmap (0 = alle bestanden).
Die Diagnose veroeffentlicht keine erfundenen Meldungen und sendet keinen Funk.

Die ID ist ein stabiler 128-Bit-Ausschnitt aus SHA-256 ueber MeshCore-Sendezeit
und empfangenen Text einschliesslich Absendername. Wiederholungen werden innerhalb
des Verlaufs nicht doppelt angezeigt. Die letzten acht Meldungen bleiben im RAM,
neueste zuerst; ein Neustart leert diesen Verlauf. Eine begrenzte Queue entkoppelt
den Funkempfang vom HTTP-Server. Ueberlast wird unter `/api/status` gezaehlt.

`GET /api/messages` liefert dasselbe JSON. Der offizielle WebSocket-Client aus
`notfall-ms/pwa` fordert mit `{"type":"get_messages"}` einen neuen Snapshot an.
Der bisherige Textbefehl `refresh` bleibt kompatibel. Meldungen lassen sich
ueber diesen Endpunkt nicht einspeisen.

Nach erfolgreicher lokaler Speicherung sendet der PWA-Client Empfangsbestaetigungen:

```json
{"messageId":"msg-<32 Hexzeichen>","deviceId":"pwa-<UUID>","status":"received","timestamp":"2026-09-26T10:18:40.123Z"}
```

Die Box akzeptiert diese ACKs ohne Antwort und ohne LoRa-Weiterleitung. Sie
speichert damit keine Teilnehmerliste und bestaetigt keine Zustellung an den
Krisenstab. Ein ACK besagt auch nicht, dass eine Person die Meldung gelesen hat.
Eingehende WebSocket-Nutzdaten sind auf 512 Bytes begrenzt und werden geprueft.

Auf der Box nutzt die PWA den offiziellen WebSocket-Client automatisch mit
der Adresse des aktuellen WLAN-Kiosks. Sie verbindet sich nach einem Abbruch
erneut und zeigt bis dahin den letzten empfangenen Stand als nicht aktuell.
Ein leerer neuer Snapshot entfernt alte Meldungen aus der Live-Anzeige. Kann
der Browser nichts lokal speichern, bleibt der Live-Empfang im RAM nutzbar;
es werden dann keine Speicherbestaetigungen gesendet. Auf der Box wird kein
Demo-Feed als Live-Meldung gezeigt. Der normale PWA-Betrieb ausserhalb des
Kiosks behaelt die offizielle Transportauswahl mit Bluetooth und WebSocket.

## WLAN-QR auf dem OLED

Die erste Display-Seite zeigt einen standardmaessigen WLAN-QR fuer das offene
Netz **notfall.ms INFO** (`WIFI:T:nopass;S:notfall.ms INFO;;`). Die Taste wechselt
zu den bisherigen MeshCore-Statusseiten. Der Code wird offline vorgeneriert,
ohne neue QR-Bibliothek auf dem ESP32. Neu erzeugen: `pip install qrcode`, dann
`python scripts/generate_wifi_qr.py`.

Auf dem 128x64-OLED wird Version 2-L mit **2x2 Pixeln pro Modul** dargestellt.
Das eigentliche QR-Datenfeld ist damit 50x50 statt 25x25 Pixel gross, die weisse
Flaeche 66x64 Pixel. SSID und Adresse stehen rechts. Die weisse Ruhezone betraegt
horizontal acht, vertikal sieben Pixel. Fuer normgerechte vier Module waeren
vertikal ebenfalls acht Pixel und damit 66 Displayzeilen erforderlich; diese
zwei fehlenden Zeilen sind der Kompromiss fuer gleichmaessig groessere Module.
Der exakt gerenderte Code wurde bei nativen 128x64 Pixeln und drei vergroesserten
Bildgroessen mit ZXing korrekt dekodiert. Der praktische Scanabstand haengt von
Handy und Display ab. Der WLAN-QR verbindet mit dem Netz;
das automatische Oeffnen des Captive Portals haengt vom Handy ab. Manueller
Einstieg ist weiterhin `http://192.168.4.1/`.
