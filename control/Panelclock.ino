#include <Arduino.h>
#include "Application.hpp"
#include "MemoryLogger.hpp"

// Das einzige globale Objekt, das wir noch brauchen.
Application* app = nullptr;

// Der globale Mutex für den Logger bleibt hier, da er von überall genutzt wird.
SemaphoreHandle_t serialMutex;

void setup() {
  Serial.begin(115200);
  delay(1000);

  serialMutex = xSemaphoreCreateMutex();

  Serial.println("\n=== Panelclock startup ===");
  
  // Erstelle die Application-Klasse. Ab hier übernimmt sie die Kontrolle.
  app = new Application();
  
  // Die begin()-Methode führt den gesamten Startvorgang aus.
  app->begin();

  Serial.println("=== Setup abgeschlossen. Application läuft. ===");
}

void loop() {
  // Die loop() delegiert einfach an die update()-Methode der Application.
  // Die gesamte Komplexität ist jetzt dort gekapselt.
  if (app) {
    app->update();
  }
}