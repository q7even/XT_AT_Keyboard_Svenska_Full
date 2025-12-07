#include "ota.h"
#include <Arduino.h>

static bool ota_initialized = false;

// Optional: OTA password protection
static const char* OTA_USERNAME = "admin";     // ändra vid behov
static const char* OTA_PASSWORD = "espupdate"; // ändra vid behov

void setupOTA(AsyncWebServer &server)
{
    if (ota_initialized) return;
    ota_initialized = true;

    Serial.println("[OTA] Initializing AsyncElegantOTA...");

    // ==========================
    // Uncomment ONE of these:
    // ==========================

    // 1) OTA UTAN lösenord (enkelt)
    // AsyncElegantOTA.begin(&server);

    // 2) OTA MED användarnamn/lösenord (rekommenderas)
    AsyncElegantOTA.begin(&server, OTA_USERNAME, OTA_PASSWORD);

    Serial.println("[OTA] AsyncElegantOTA ready at /update");
}

// Körs i loopen
void otaPeriodic()
{
    // Måste köras för att OTA ska fungera korrekt i Async-miljö
    AsyncElegantOTA.loop();
}
