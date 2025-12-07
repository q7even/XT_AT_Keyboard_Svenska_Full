#ifndef API_H
#define API_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void apiInit(AsyncWebServer &server);
uint8_t charToHID(char c);

#endif
