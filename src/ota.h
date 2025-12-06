#ifndef OTA_MODULE_H
#define OTA_MODULE_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/// Exposed firmware version (defined in ota.cpp)
extern String firmware_version;

/// Initialize OTA endpoints and AsyncElegantOTA (auth is read from prefs/values)
void setupOTA(AsyncWebServer &server);

/// Call periodically from loop() to let module perform auto-OTA checks
void otaPeriodic();

/// Trigger manual backup now (returns true on success)
bool otaBackupNow();

/// Force an immediate auto-check for update (blocking network call)
void otaCheckNow();

#endif // OTA_MODULE_H
