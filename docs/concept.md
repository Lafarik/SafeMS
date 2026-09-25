# Notfall-Datenkiosk

## Ziel

Ein lokaler WLAN-Zugang stellt vorbereitete und aktualisierte Notfallhinweise
bereit. Ein Krisenstab verteilt kurze, standortbezogene Meldungen ueber MeshCore.
Die Webseiten und statischen Informationen liegen auf dem Kiosk, nicht im Funknetz.

## Geplanter Ablauf

1. Physischer Schalter aktiviert den Notfallmodus.
2. Smartphone verbindet sich mit dem lokalen WLAN.
3. Lokale Webseite zeigt Standort, Meldungen, Quelle und Aktualisierungszeit.
4. Neue Funkmeldungen werden geprueft, gespeichert und auf der Seite angezeigt.
5. Gespeicherte Informationen bleiben bei Funkunterbrechung verfuegbar, mit Altersangabe.

WLAN-Beitritt und Aufruf einer Webseite sind getrennte Schritte; ein WLAN-QR-Code
garantiert keinen automatischen Seitenaufruf. Captive Portal und eine feste lokale
Adresse sind zu testen. Ein gedruckter QR-Code kann ein kleines Display ergaenzen.

## Noch zu implementieren

- Offline-Seite und WLAN-Access-Point.
- Format fuer Meldungs-ID, Standort, Zeit, Ablaufzeit und Inhalt.
- Authentifizierung des Absenders und Schutz vor wiederholten/veralteten Meldungen.
- Knappe Funkpayloads, Deduplizierung und persistente Speicherung.
- Energieversorgung, Lasttest mit mehreren Smartphones und Verhalten nach Neustart.
- Entscheidung zwischen integriertem MeshCore-Port und separatem Funkknoten.
- Separater Repeater oder gepruefte kombinierte Rolle: USB-Companion bedeutet
  nicht automatisch, dass fremde Pakete als Repeater weitergeleitet werden.

Der aktuelle Stand ist ein Hackathon-Prototyp, kein einsatzfertiges Warnsystem.
