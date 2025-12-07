#ifndef REALTIME_WS_H
#define REALTIME_WS_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void realtimeInit(AsyncWebServer &server);
void realtimeBroadcastScancode(const String &msg);
void detectProtocolAsync(AsyncWebServerRequest *req);

#endif
