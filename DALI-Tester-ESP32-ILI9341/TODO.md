# DALI Tester - Feature & Aufgabenliste

> **Letzte Aktualisierung:** 2026-04-12

---

## 🤖 AI-Assistent Hinweis

> **WICHTIG FÜR JEDE NEUE SESSION:**
> 
> 1. **TODO.md** - Diese Datei bei jeder Änderung aktualisieren:
>    - Erledigte Features mit `[x]` markieren
>    - Neue Features/Bugs hinzufügen
>    - "Nächste Schritte" Prioritäten anpassen
>    - "Letzte Aktualisierung" Datum aktualisieren
>
> 2. **README.md** - Bei relevanten Änderungen aktualisieren:
>    - Feature-Status-Tabelle pflegen
>    - Changelog erweitern bei neuen Versionen
>    - "Bekannte Einschränkungen" aktuell halten
>    - "Letztes Update" Datum im Header aktualisieren
>
> 3. **Projektstatus** - Immer den aktuellen Stand dokumentieren:
>    - Was funktioniert?
>    - Was ist in Arbeit?
>    - Was sind bekannte Probleme?
>
> So weiß jeder Nutzer (und jede AI-Session) sofort, wie der Projektstatus ist.
>
> 4. **Build & Test** - PlatformIO Befehle (PowerShell):
>    ```powershell
>    # Build
>    & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run
>    
>    # Upload
>    & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run --target upload
>    
>    # Serial Monitor
>    & "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" device monitor -b 115200
>    ```
>    **Hinweis:** `pio` und `python -m platformio` funktionieren NICHT in dieser Umgebung!

---

## Legende

- ✅ Abgeschlossen
- 🔄 In Bearbeitung
- ⏳ Geplant (hohe Priorität)
- 💡 Idee / Nice-to-have
- ❌ Blockiert / Problem

---

## ⚠️ Erinnerung: FW-Version

> **Bei JEDEM Release:** `FW_VERSION` in `include/config.h` hochzählen!
> Auch `README.md` Versionstabelle und Changelog aktualisieren.

---

## Hauptfunktionen

### Home Menu
- [x] Scrollbares Kachel-Menü (3×4 Grid) mit Icons und abgerundeten Kacheln
- [x] 10 Bitmap-Icons (38×38px, 1-bit monochrom via drawBitmap)
- [x] Header mit Titel und Status-Badges
- [x] Footer mit kontext-sensitiven Hints

### Device Parameters
- [x] Geräteparameter auslesen (Levels, Fade, etc.)
- [x] Gruppenzuweisungen anzeigen (bitmask)
- [x] Szenen-Level anzeigen (0-15)
- [x] Parameter bearbeiten und speichern
- [x] Gruppen hinzufügen/entfernen
- [x] Szenen-Level setzen

### Scan Devices
- [x] Scan Short Addresses 0-63
- [x] Device Type Anzeige
- [x] Scrollbare Ergebnisliste
- [x] Device Type Namen statt Nummern anzeigen
- [x] Zusätzliche Geräteinformationen (Actual Level, Status)
- [x] Geräte-Kontextmenü mit Seriennummer und Locate
- [x] Erkennung von nicht-adressierten Geräten auf dem Bus
- [x] Unadressierte Geräte in der Scan-Liste anzeigen (unified list)
- [x] DALI-2 Control Devices erkennen und in Scan-Liste anzeigen
- [x] Busspannungs-Erkennung vor Scan (Error Handling bei fehlendem Bus)

### Broadcast Control
- [x] Click = AN/AUS Toggle
- [x] Drehen = Dimmen (Direct Arc Power)
- [x] Visueller Level-Balken

### Address Device
- [x] Address ALL (alle Geräte neu adressieren)
- [x] Address Unaddressed (nur nicht-adressierte)
- [x] Change Address (einzelne Adresse ändern)
- [ ] Fortschrittsanzeige während Commissioning
- [ ] Konflikt-Erkennung bei doppelten Adressen

### Clone Config
- [x] Lesen der Gerätekonfiguration
- [x] Schreiben auf anderes Gerät
- [x] Source/Target Auswahl
- [ ] Config-Preview vor dem Schreiben
- [ ] Mehrere Geräte gleichzeitig beschreiben

### Reset Device
- [x] DALI RESET Befehl an einzelnes Gerät
- [ ] Reset ALL (Broadcast Reset)
- [ ] Reset-Bestätigung mit Abfrage

### DALI PSU
- [x] EXT/INT Umschaltung
- [x] Confirm-Popup vor Umschaltung
- [ ] PSU Status-Überwachung (Überstrom, etc.)

### Send CMD
- [x] Target-Typ: Broadcast / Group / Short Address
- [x] Kommandos: ON / OFF / MIN / MAX
- [ ] Erweiterte Kommandos (UP, DOWN, STEP_UP, STEP_DOWN)
- [x] Scene-Befehle (GOTO SCENE 0-15)
- [ ] Direct Arc Level senden (0-254)
- [ ] Query-Kommandos mit Antwort-Anzeige

### Locate
- [x] Zyklisches MIN/MAX Blinken
- [x] Einstellbares Intervall (200-3000ms)
- [x] Start/Stop Steuerung
- [ ] Locate für Gruppen
- [ ] Locate mit Fade statt hartem Umschalten

### Bus Monitor
- [x] Live Frame-Anzeige
- [x] Freeze/Live Mode Toggle
- [x] Ring-Buffer (80 Einträge)
- [x] Frame-Dekodierung (BC, Group, Short)
- [x] DALI-2 Event Frames (24-bit) Dekodierung
- [ ] Frame-Filter (nur bestimmte Adressen)
- [ ] Export/Log-Funktion
- [ ] Timestamp-Anzeige (relativ oder absolut)

---

## UI / UX

### Display
- [x] ILI9341 320x240 TFT Support
- [x] Farbschema (Dark Theme)
- [x] LCD Backlight Steuerung (TFT_LED Pin)
- [ ] Helligkeit einstellbar
- [ ] Display-Timeout / Screensaver
- [ ] Alternative Farbschemata

### Navigation
- [x] Rotary Encoder mit Taster
- [x] Long Press (≥700ms) für Zurück
- [x] Debouncing
- [ ] Beschleunigtes Scrollen bei schnellem Drehen
- [ ] Haptic Feedback (optional, falls Hardware vorhanden)

### Status-Anzeige
- [x] BUS Status Badge (IDLE, ACT, SEND, OK, etc.)
- [x] PSU Mode Badge (EXT/INT)
- [x] Batterie-Anzeige im Header (Prozent + Farbe)
- [ ] Verbindungsstatus-Indikator
- [ ] Fehler-Log / History

---

## DALI Protokoll

### Basis-Kommandos
- [x] Direct Arc Power Control
- [x] OFF, RECALL MAX, RECALL MIN
- [x] Query Status, Query Actual Level
- [ ] UP, DOWN, STEP UP, STEP DOWN
- [ ] ON AND STEP UP, STEP DOWN AND OFF

### Konfiguration
- [x] Query/Set Max Level
- [x] Query/Set Min Level
- [x] Query/Set Power On Level
- [x] Query/Set System Failure Level
- [x] Query/Set Fade Time/Rate
- [ ] Query/Set Extended Fade Time
- [ ] Scene Level Konfiguration (0-15)

### Gruppen
- [x] Gruppe zuweisen (ADD TO GROUP) via Device Parameters
- [x] Gruppe entfernen (REMOVE FROM GROUP) via Device Parameters
- [x] Gruppenübersicht anzeigen

### Erweiterte Features
- [ ] DALI-2 Control Device Discovery (Sensoren, Taster)
- [x] DALI-2 Device Type Unterstützung (Control Device Scan + Typ-Namen)
- [ ] Memory Bank Lesen
- [ ] Colour Control (DT8)
- [x] Event-basierte Anzeige (Bus Monitor)

---

## Hardware / System

### ESP32
- [x] Hardware Timer für DALI Timing (104µs)
- [x] Open-Drain TX Pin
- [ ] Deep Sleep Mode bei Inaktivität
- [ ] OTA Updates
- [ ] WiFi-Interface (optional)

### DALI Interface
- [x] TX/RX Invertierung konfigurierbar
- [x] PSU Enable Steuerung
- [ ] Bus-Spannungsüberwachung
- [ ] Kurzschluss-Erkennung

---

## Bekannte Probleme

- [ ] **Bus Monitor**: Forward Frames (16-bit) werden nicht vollständig dekodiert bei passivem Mithören
- [ ] **Display Flackern**: Bei schnellen Änderungen kann leichtes Flackern auftreten
- [ ] **Commissioning**: Komplexe Multi-Device Szenarien erfordern manuelle Behandlung

---

## Nächste Schritte (Priorisiert)

1. ✅ Forward Frame Dekodierung im Bus Monitor (erledigt inkl. DALI-2 Events)
2. ⏳ Erweiterte Send CMD Optionen (UP, DOWN, Direct Arc)
3. ✅ Device Type Namen statt Nummern (erledigt)
4. ⏳ Query-Kommandos mit Antwort-Anzeige
5. ✅ Gruppen-Management (erledigt via Device Parameters)
6. ✅ Scene-Konfiguration (erledigt via Device Parameters)
7. ✅ Erkennung nicht-adressierter Geräte (erledigt im Scan)
8. ✅ DALI-2 Control Device Scan (Sensoren, Taster etc.)
9. 💡 DALI-2 Control Device Discovery (erweitert: Instanzen, Memory Banks)
9. 💡 WiFi-Interface für Remote-Steuerung

---

## Notizen

- 2026-03-19: Clone/Config-Write und Reset-Sequenzen robuster gemacht (inkl. Verifikation und korrektem Adress-Loeschen via SET SHORT ADDRESS + DTR=255).
- 2026-03-19: README Pin-Mapping auf aktuelle Hardware Rev1.0 aus `include/config.h` abgeglichen.
- 2026-03-20: Scan Devices zeigt jetzt adressierte UND unadressierte Geräte in einer gemeinsamen Liste (v1.2.2).
- 2026-03-19: Adress-Löschung korrigiert (INITIALISE fehlte), Navigation von Detailansicht geht zurück zur Scan-Liste (v1.2.3).
- 2026-04-12: Pong Easter Egg hinzugefügt (8s Hold im Home-Menü) (v1.3.8).
- 2026-04-12: Home-Menü Partial Redraw (nur geänderte Tiles), Header-Badges vergrößert (+4px), Batterie-Anzeige beim Start repariert (v1.3.7).
- 2026-04-08: Hardware Rev 1.1 - Pin-Belegung aktualisiert (TFT_CS=5, TFT_DC=4, TFT_RST=17), LCD Backlight Pin (TFT_LED=GPIO2) hinzugefügt.
- 2026-03-24: Bus Monitor Live-Ansicht repariert - Ring-Buffer Count blieb bei 80 stehen, Generation-Counter eingeführt (v1.3.4).
- 2026-03-22: Reset-Adressloeschung repariert (SET SHORT ADDRESS ohne REPEAT Flag war Ursache), PSU-Popup Navigation repariert, FW-Version im Home-Footer (v1.3.3).

<!-- 
Platz für eigene Notizen, Ideen, etc.
-->

---

*Diese Datei wird laufend aktualisiert. Features können nach Bedarf umpriorisiert werden.*
