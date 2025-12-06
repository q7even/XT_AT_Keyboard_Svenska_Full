#ifndef API_H
#define API_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Initiera API (anropar server.start internt)
void apiInit(AsyncWebServer &server);

// Utility: konvertera char/string -> HID code (enkel)
uint8_t charToHID(char c);

#endif // API_H
