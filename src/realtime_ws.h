#ifndef REALTIME_WS_H
#define REALTIME_WS_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void realtimeInit(AsyncWebServer &server);
void realtimeBroadcastScancode(const String &msg); // anropa från xtatTask / queue när du skickar något

// Detector API
void detectProtocolAsync(AsyncWebServerRequest *req); // starta detektering (asynkront)
String detectProtocolSync(unsigned long timeoutMs = 5000); // synkront (blocking) returnerar "XT"/"AT"/"unknown"

#endif
