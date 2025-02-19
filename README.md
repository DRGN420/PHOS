# PHOS


PHÔS² - The Interactive Square
Über das Projekt
PHÔS² ist ein modularer, interaktiver Reaktionstisch, der mit kapazitiven Touchsensoren, LED-Ringen und WLAN-Steuerung arbeitet. Dieses Projekt kombiniert moderne Sensortechnik mit Echtzeit-LED-Feedback und bietet drei verschiedene Spielmodi:

✅ Game Mode – Ein schnelles Reaktionsspiel, bei dem Spieler aufleuchtende Felder so schnell wie möglich deaktivieren müssen.

✅ Memory Mode – Spieler müssen sich eine zufällige Sequenz merken und diese korrekt wiederholen.

✅ Ambient Mode – Der Tisch reagiert dynamisch auf Berührungen mit individuellen Lichteffekten.

Technische Details
Mikrocontroller: Arduino Mega 2560
Berührungserkennung: 16x Kapazitive Touch-Sensoren (F47-A)
Beleuchtung: 16x WS2812B LED-Ringe + 9 LED-Streifen für Animationen
WLAN-Steuerung: ESP8266 für Web-Interface zur Modusauswahl
Stromversorgung: 5V / 70A Netzteil mit Step-Up-Modul

Repository-Inhalte

📂 /firmware – Arduino-Sketch für die Steuerung von Sensoren & LEDs

📂 /docs – Detaillierte Dokumentation mit DIY-Anleitung

Setup & Installation

Sketch aus /firmware in der Arduino IDE hochladen und benötigte Bibliotheken installieren:

#include <Adafruit_NeoPixel.h>

#include "WiFiEsp.h"

ESP8266 mit WLAN konfigurieren

Web-Interface aufrufen: Nach Verbindung über WLAN im Browser http://<IP-Adresse> öffnen.

Spielmodi über das Web-Interface auswählen und loslegen!

Beitrag & Weiterentwicklung

Pull Requests willkommen! Falls du Ideen für neue Features hast (z. B. Soundeffekte oder API-Anbindung), erstelle bitte ein Issue.

Lizenz: Open Source

