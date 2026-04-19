# DALI Tester - ESP32 + ILI9341

Ein DALI (Digital Addressable Lighting Interface) Tester mit ESP32 Mikrocontroller, ILI9341 TFT-Display und Rotary Encoder Bedienung.

---

> **🤖 AI-Assistent:** Diese README.md sowie [TODO.md](TODO.md) sind bei jeder Änderung aktuell zu halten!
> Siehe TODO.md für detaillierte Anweisungen.

---

## Projektübersicht

| Eigenschaft | Wert |
|-------------|------|
| **Plattform** | ESP32 (espressif32) |
| **Framework** | Arduino |
| **Display** | ILI9341 2.8" SPI TFT (320x240) |
| **Eingabe** | Rotary Encoder mit Taster |
| **DALI Library** | qqqDALI (qqqlab) |
| **Version** | 1.3.8 |
| **Letztes Update** | 2026-04-12 |

## Funktionen

### Implementierte Features

| Feature | Status | Beschreibung |
|---------|--------|--------------|
| Home Menu | ✅ | Scrollbares Kachel-Menü (3×4 Grid) mit Icons |
| Scan Devices | ✅ | Scannt Control Gear (SA 0-63), DALI-2 Control Devices und unadressierte Geräte |
| Device Info | ✅ | Kontextmenü mit Seriennummer, Typ und Locate |
| Device Parameters | ✅ | Alle Parameter anzeigen/bearbeiten inkl. Gruppen & Szenen |
| Broadcast Control | ✅ | Drehen=Dimmen, Click=AN/AUS |
| Address Device | ✅ | Address ALL, Unaddressed, Change Address |
| Clone Config | ✅ | Liest/schreibt Gerätekonfiguration |
| Reset Device | ✅ | Setzt Einzelgerät zurück (DALI RESET) |
| DALI PSU | ✅ | EXT/INT Umschaltung mit Confirm-Popup |
| Send CMD | ✅ | Broadcast/Group/Short + ON/OFF/MIN/MAX |
| Locate | ✅ | Zyklisches MIN/MAX Blinken |
| Bus Monitor | ✅ | Frame-Anzeige mit Freeze/Live Mode, DALI-2 Device-Cmd Erkennung |

### UI Features

- **Header**: Titel + Status-Badges (BUS Status, PSU Mode)
- **Footer**: Kontext-sensitiver Hint-Text
- **Navigation**: Rotate=Auswahl, Click=Bestätigen, Long Press (≥700ms)=Zurück

## Hardware-Konfiguration

### Pin-Belegung (Hardware Rev1.1)

| Funktion | Pin | Beschreibung |
|----------|-----|--------------|
| **TFT Display** | | |
| TFT_CS | GPIO 5 | Chip Select |
| TFT_DC | GPIO 4 | Data/Command |
| TFT_RST | GPIO 17 | Reset (-1 wenn an EN) |
| TFT_MOSI | GPIO 23 | SPI MOSI (VSPI) |
| TFT_SCLK | GPIO 18 | SPI Clock (VSPI) |
| TFT_MISO | GPIO 19 | SPI MISO (optional) |
| TFT_LED | GPIO 2 | Backlight (HIGH = an) |
| **Encoder** | | |
| ENC_A | GPIO 33 | Encoder Signal A |
| ENC_B | GPIO 32 | Encoder Signal B |
| ENC_BTN | GPIO 34 | Button (active LOW, ext. Pullup) |
| **DALI Interface** | | |
| DALI_TX | GPIO 26 | Transmit (Open-Drain) |
| DALI_RX | GPIO 27 | Receive |
| DALI_PSU_EN | GPIO 25 | PSU Enable |
| **Battery Monitor** | | |
| BATTERY_ADC | GPIO 35 | Akkuspannung via Spannungsteiler |

### Konfigurierbare Einstellungen

Alle Pins und Settings können in `include/config.h` angepasst werden:

```cpp
// TX/RX Invertierung (abhängig von Hardware-Interface)
#define DALI_TX_INVERT  true    // true für typische Opto/Transistor-Schaltung
#define DALI_RX_INVERT  false   // true wenn RX-Schaltung invertiert

// PSU Enable Logik
#define PSU_EN_ACTIVE_HIGH  true  // HIGH = PSU aktiv

// Encoder Richtung und Steps
#define ENCODER_DIR     -1      // +1 oder -1 für Umkehrung
#define ENC_STEP_DIV    4       // Steps pro Rastung
```

## Projektstruktur

```
DALI-Tester-ESP32-ILI9341/
├── platformio.ini          # PlatformIO Konfiguration
├── README.md               # Diese Datei
├── include/
│   ├── config.h            # Pin-Definitionen und Settings
│   ├── dali/
│   │   └── dali_interface.h    # DALI API Header
│   └── ui/
│       ├── ui.h            # UI Core Header
│       └── screens.h       # Screen Handler Header
├── src/
│   ├── main.cpp            # Hauptprogramm
│   ├── dali/
│   │   └── dali_interface.cpp  # DALI Implementierung
│   └── ui/
│       ├── ui.cpp          # UI Core Implementierung
│       └── screens.cpp     # Alle Screen Handler
├── lib/                    # (leer - Libraries via lib_deps)
└── test/                   # (leer - für Unit Tests)
```

## Build & Flash Anleitung

### Voraussetzungen

- [PlatformIO](https://platformio.org/) (VS Code Extension oder CLI)
- USB-Kabel für ESP32

### Build

```bash
# Projekt bauen
pio run

# Oder mit Verbose Output
pio run -v
```

### Flash

```bash
# Firmware hochladen
pio run -t upload

# Mit spezifischem Port
pio run -t upload --upload-port COM3
```

### Serial Monitor

```bash
# Monitor starten (115200 baud)
pio device monitor -b 115200

# Build + Upload + Monitor
pio run -t upload -t monitor
```

## Dependencies

Die Libraries werden automatisch über `lib_deps` in `platformio.ini` installiert:

```ini
lib_deps = 
    adafruit/Adafruit GFX Library@^1.11.9
    adafruit/Adafruit ILI9341@^1.6.0
    adafruit/Adafruit BusIO@^1.14.5
    https://github.com/qqqlab/DALI-Lighting-Interface.git
```

## DALI Interface Hardware

### Typischer Schaltkreis

```
ESP32 DALI_TX (GPIO26) ──[1k]──┬── Optokoppler/Transistor ── DALI Bus +
                               │
                              GND

DALI Bus + ── Optokoppler RX ──┬── DALI_RX (GPIO27)
                               │
                             [10k]
                               │
                              3.3V
```

**Hinweise:**
- TX Pin als Open-Drain konfiguriert
- TX_INVERT=true für typische Transistor/Opto-Schaltungen
- RX mit Pullup (intern oder extern)
- DALI Bus benötigt 16V DC Versorgung

## Bus Monitor

Der Bus Monitor nutzt **ausschließlich** `dali.rx()` aus der qqqDALI Library - **kein RMT** (wegen Manchester Alignment Problemen).

### Frame-Dekodierung

| Frame Typ | Format | Beispiel |
|-----------|--------|----------|
| Broadcast Cmd | `BC <CMD>` | `BC MAX` |
| Group Cmd | `G<n> <CMD>` | `G5 OFF` |
| Short Addr Cmd | `SA<n> <CMD>` | `SA12 Q_STATUS` |
| Direct Arc | `SA<n> ARC <val>` | `SA0 ARC 128` |
| Reply | `REPLY 0x<hex> (<dec>)` | `REPLY 0x80 (128)` |

## Bedienung

### Encoder

| Aktion | Funktion |
|--------|----------|
| Drehen | Navigation / Wert ändern |
| Kurz drücken (<700ms) | Select / Next / Execute |
| Lang drücken (≥700ms) | Zurück / Abbrechen |

### Status Badges

| Badge | Bedeutung |
|-------|-----------|
| IDLE | Keine Aktivität |
| ACT | RX Aktivität erkannt |
| SEND | Kommando wird gesendet |
| OK | Letztes Kommando erfolgreich |
| NO RPLY | Keine Antwort erhalten |
| BUS ERR | DALI Bus Fehler |
| FAILED | Kommando fehlgeschlagen |
| EXT | Externe PSU |
| INT | Interne PSU aktiv |

## Changelog

### Version 1.3.8 (2026-04-12)

- ✅ **Easter Egg**: Pong-Spiel! Im Hauptmenü Encoder 8 Sekunden gedrückt halten. Wand links, Paddle rechts per Encoder. Long Press zum Verlassen.

### Version 1.3.7 (2026-04-12)

- ✅ **Home-Menü Flicker-Fix**: Partial Redraw zeichnet nur geänderte Tiles neu statt gesamten Content-Bereich
- ✅ **Header-Badges vergrößert**: Badges 4px höher und breiter für bessere Lesbarkeit
- ✅ **Batterie-Anzeige Fix**: Spannung wird jetzt sofort beim Start korrekt angezeigt (war 0.0V)

### Version 1.3.6 (2026-04-08)

- ✅ **Hardware Rev 1.1**: Pin-Belegung aktualisiert (TFT_CS=GPIO5, TFT_DC=GPIO4, TFT_RST=GPIO17)
- ✅ **LCD Backlight**: Neuer TFT_LED Pin (GPIO 2) für Hintergrundbeleuchtung, dauerhaft HIGH
- ✅ **Build-Fix**: `-Wno-error=return-type` Flag für Kompatibilität mit aktualisierter ESP32-Plattform

### Version 1.3.5 (2026-03-25)

- ✅ **GOTO SCENE im Send CMD**: Szenen 0-15 können jetzt direkt über Send CMD aufgerufen werden (neues Kommando "SCENE" mit Szenen-Nummer Auswahl)
- ✅ **HW-Version & Credits im Home-Screen**: Footer zeigt jetzt zweizeilig FW-Version (by Claude Code Opus 4.6) und HW-Version (by Lukas Meissner)
- ✅ **HW_VERSION Define**: Zentrale HW-Versionsverwaltung in config.h

### Version 1.3.4 (2026-03-24)

- ✅ **Bugfix Bus Monitor Live-Ansicht**: Ring-Buffer verwendete nur Count für Änderungserkennung — bei vollem Buffer (80 Einträge) wurde kein Update mehr erkannt. Jetzt mit Generation-Counter der bei jedem neuen Frame zählt

### Version 1.3.3 (2026-03-22)

- ✅ **Bugfix Reset Adresse löschen**: SET SHORT ADDRESS wird jetzt mit REPEAT-Flag (doppelt) gesendet wie vom DALI-Standard gefordert
- ✅ **Bugfix DALI PSU Navigation**: Popup-Rückkehr stellt previousScreen korrekt wieder her, kein doppeltes Zurück mehr nötig
- ✅ **FW-Version im Home-Screen**: Footer zeigt Firmware-Version und "powered by Claude Opus"
- ✅ **FW_VERSION Define**: Zentrale Versionsverwaltung in config.h

### Version 1.3.2 (2026-03-22)

- ✅ **Scrollbares Kachel-Menü**: 3×4 Grid mit größeren Kacheln (102×88px), scrollbar per Encoder
- ✅ **Pixel-Art Icons**: 10 individuelle Icons für jedes Menü-Item (Lupe, Zahnrad, Blitz, Fadenkreuz, etc.)
- ✅ **Visuelle Verbesserung**: Ausgewählte Kachel in Cyan, unselektierte mit dezenter Umrandung und Icon

### Version 1.3.1 (2026-03-21)

- ✅ **Home Menu Kachel-Design**: 3×4 Grid mit abgerundeten Kacheln statt scrollbare Liste
- ✅ **Tile Labels**: Kompakte zweizeilige Beschriftung pro Kachel
- ✅ **Visuelle Verbesserung**: Ausgewählte Kachel in Cyan, unselektierte mit dezenter Umrandung

### Version 1.3.0 (2026-03-20)

- ✅ **DALI-2 Control Device Scan**: Sensoren, Taster und andere Control Devices werden per S=0 Adressierung erkannt
- ✅ **Unified Scan List**: Control Gear (weiss), Control Devices (grün) und Unadressierte (orange) in einer Liste
- ✅ **DALI-2 Device Type Namen**: Neue Typnamen für Device Types 128-140 (Switch, OccSensor, Gateway, etc.)
- ✅ **Bus Monitor DALI-2**: S=0 Frames mit Command-Werten >=0x90 werden als Device-Commands dekodiert

### Version 1.2.3 (2026-03-19)

- ✅ **Bugfix Adresse löschen**: INITIALISE/TERMINATE Sequenz hinzugefügt - SET SHORT ADDRESS erfordert INITIALISE-Status laut DALI-Standard
- ✅ **Bugfix Navigation**: Zurück von Detailansicht kehrt jetzt zur Scan-Liste zurück statt zum Home-Menü

### Version 1.2.2 (2026-03-20)

- ✅ **Scan Devices: Unified List**: Adressierte und unadressierte Geräte werden jetzt in einer gemeinsamen Liste angezeigt
- ✅ **Unadressierte Zählung**: `daliCountUnaddressed()` zählt unadressierte Geräte per Binary-Search (ohne Adressvergabe)
- ✅ **Visuelle Unterscheidung**: Unadressierte Geräte in Orange, adressierte in Weiß

### Version 1.2.1 (2026-03-19)

- ✅ **Bugfix Clone/Write Config**: Robusteres Schreiben mit Verifikation, damit fehlerhafte Write-Vorgaenge nicht mehr als OK angezeigt werden
- ✅ **Bugfix Fade Query**: Korrekte Auswertung von Fade Time/Fade Rate aus dem gemeinsamen DALI-Reply-Byte
- ✅ **Bugfix Reset Device**: Reset + optionales Adress-Loeschen mit korrekter DALI-Sequenz und Rueckpruefung
- ✅ **Hardware Rev1.0 Doku**: Pin-Tabelle in README an `include/config.h` angepasst

### Version 1.2.0 (2026-03-03)

- ✅ **Device Parameters**: Neues Menü zur Anzeige/Bearbeitung aller Geräteparameter
  - Max/Min Level, Power On Level, System Failure Level
  - Fade Time, Fade Rate
  - Gruppenzuweisungen (0-15) anzeigen und ändern
  - Szenen-Level (0-15) anzeigen und setzen
- ✅ **Bugfix Broadcast Control**: Display-Refresh beim Dimmen korrigiert
- ✅ **Bugfix Serial Number**: Bessere Anzeige bei nicht zugewiesenen Seriennummern ("Not Assigned")

### Version 1.1.0 (2026-02-24)

- ✅ **Broadcast Control**: Neues Menü zum direkten Dimmen aller Leuchten (Drehen=Dimmen, Click=AN/AUS)
- ✅ **Device Info Kontextmenü**: Klick auf gescannte Geräte zeigt Details (Seriennummer, Typ, Status)
- ✅ **Locate im Scan**: Locate-Funktion direkt aus dem Device Info Screen (2s Intervall)
- ✅ **Device Type Namen**: Anzeige von Typnamen statt Nummern (Fluorescent, LED, etc.)
- ✅ **Pin-Belegung aktualisiert**: TFT_CS=GPIO2, TFT_DC=GPIO17 für PCB v1

### Version 1.0.0 (2026-02-22)

- ✅ Initiale Implementierung
- ✅ Alle 8 Hauptmenü-Funktionen
- ✅ Encoder-Steuerung mit Debouncing
- ✅ DALI Master via qqqDALI
- ✅ Bus Monitor mit Ring-Buffer (80 Einträge)
- ✅ Scrollbare Listen
- ✅ Confirm-Popup für PSU

## Bekannte Einschränkungen

1. **Bus Monitor RX**: Erkennt aktuell nur 8-Bit Backward Frames vollständig. Forward Frame Dekodierung für passives Mithören erfordert erweiterte qqqDALI Integration.

2. **Commissioning**: Nutzt die qqqDALI `commission()` Funktion. Komplexe Szenarien (Multi-Device, Conflicts) müssen ggf. manuell behandelt werden.

3. **Display Refresh**: Bei schnellen Änderungen kann leichtes Flackern auftreten.

## Assumptions (Designentscheidungen)

1. **Timer**: ESP32 Hardware Timer 0 mit 80 Prescaler (1µs Auflösung) für DALI Timing (104µs Interrupt)

2. **Encoder**: Typischer Quadrature-Encoder mit 4 Steps pro Detent

3. **DALI Timing**: Standard Manchester-Encoding mit qqqDALI Library

4. **Ring Buffer**: Statische Allokation (80 Einträge) ohne dynamischen Speicher

5. **Screen State**: Separate State-Structs pro Screen (statisch alloziert)

## Lizenz

Dieses Projekt verwendet die qqqDALI Library von qqqlab (MIT License).

---

*Dokumentation zuletzt aktualisiert: 2026-03-20*
