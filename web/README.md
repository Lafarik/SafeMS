# notfall.ms auf der senseBox

Quelle: https://github.com/notfall-ms/pwa

Eingebundener Stand: `369e253aa39fe71de66c89bcb5950fef732ac17b`.
`official-pwa-source.tar.gz` enthaelt den unveraenderten Git-Quellbaum dieses
Commits, einschliesslich Lockfile und Lizenz. `pwa-http-kiosk.patch` dokumentiert
die lokalen Anpassungen. Die Original-Lizenz steht in `LICENSE-pwa.txt`.

## Betrieb

Offenes WLAN: **notfall.ms INFO**. Adresse: **http://192.168.4.1/**.
Die PWA-Oberflaeche, CSS, JavaScript, Bilder, Nachrichten-Demofeed und alle drei
Dokumente werden von der Box ohne Internet ausgeliefert. Dazu gehoert der
Blackout-Flyer als PDF. Inhalte des eingebundenen Repository-Stands bleiben
erhalten. MeshCore, USB und Bluetooth laufen parallel weiter.

Die Website ersetzt die bisherige SafeMS-Testseite. Auf der Box empfaengt der
Pager Live-Meldungen aus dem MeshCore-Kanal Krisenstab ueber `/ws`;
Details zu Format, Zeitstempeln und Grenzen: `../docs/live-pager.md`.
Der aktuelle Upstream ist Version 1.1.2 einschliesslich Bluetooth-Diagnose. Der Bluetooth-Pager der PWA
benutzt ein eigenes GATT-Protokoll, das nicht dem MeshCore-BLE-Protokoll
entspricht; diese Verbindung ist mit dieser Integration nicht implementiert.

## Lokales HTTP

Browser erlauben Service Worker, eine vollstaendige PWA-Installation und Web
Bluetooth auf dieser HTTP-IP-Adresse nicht wie unter HTTPS. Deshalb zeigt die
angepasste Dokumentliste die von der Box abrufbaren Dateien an, ohne CacheStorage
oder einen Service Worker vorauszusetzen. Sie behauptet nicht, dass diese Dateien
bereits offline auf dem Handy gespeichert sind. PDF/TXT/Markdown koennen direkt
aufgerufen und heruntergeladen werden. Die normale HTTPS-Service-Worker-Logik
bleibt fuer einen HTTPS-Betrieb erhalten. Automatische Captive-Portal-Fenster
sind vom Endgeraet abhaengig; die lokale IP ist der manuelle Einstieg.

## Firmware bauen

Der fertige Webstand liegt mit URL/MIME/SHA-256-Manifest in
`firmware/variants/sensebox_eye/web/`. Die Dateien tragen Hashnamen, damit die
unterschiedlichen Upstream-URLs `LOGO.svg` und `logo.svg` auch unter Windows
getrennt erhalten bleiben. `embed_web.py` prueft diese Dateien und erzeugt beim
PlatformIO-Build Flash-Konstanten; Textressourcen liegen zusaetzlich gzip-komprimiert
vor. Es werden weder MeshCore-Dateien noch Website-Dateien beim Build nachgeladen.
Compiler/Framework-Bibliotheken sind weiterhin PlatformIO-Abhaengigkeiten.

Der Webinhalt wird zusammen mit der Anwendung geflasht. SPIFFS mit den gespeicherten
MeshCore-Einstellungen wird dabei nicht mit einem Web-Dateisystem ueberschrieben.
Der HTTP-Server unterstuetzt MIME-Typen, gzip, ETags, einzelne Byte-Ranges fuer PDF
und Captive-Portal-Probes. `/api/status` bleibt als Diagnose verfuegbar.

## PWA erneut bauen

Node.js, npm, Python und Git werden benoetigt. `package-lock.json` bestimmt die
Abhaengigkeiten; `npm ci` benoetigt beim ersten Lauf Internet.

1. Das Quellarchiv in einen neuen Arbeitsordner entpacken. Unter Windows den
   Archivpfad `src/frontend/assets/logo.svg` direkt als `logo-favicon.svg`
   speichern, damit er nicht mit `LOGO.svg` kollidiert.
2. `git apply /pfad/zu/pwa-http-kiosk.patch` im Arbeitsordner ausfuehren.
3. `npm ci` ausfuehren. `NODE_ENV=production` setzen und `node_modules/.bin`
   dem PATH hinzufuegen. Dann `tsx src/setup/scripts/build.ts` starten.
4. Aus SafeMS `python scripts/package_pwa.py --repo /pfad/zum/arbeitsordner --revision 369e253aa39fe71de66c89bcb5950fef732ac17b`
   aufrufen. Alternativ kann ein Git-Checkout verwendet werden; dann `--revision`
   weglassen und den Packager die Herkunft sowie Original-Logos aus Git lesen lassen.
5. `python scripts/update_source_manifest.py` und anschliessend
   `python -m platformio run -d firmware` ausfuehren.

Pruefung der Anpassungen: 48 Jest-Tests fuer Dokumentliste, HTTP-Fallback,
Registrierung, BLE samt Diagnose, Pager und WebSocket bestanden (`--coverage=false`, da der Upstream-
Coverage-Reporter unter diesem Windows-Setup einen separaten Schreibfehler hatte).
