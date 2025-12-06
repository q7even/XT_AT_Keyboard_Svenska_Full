#ifndef DETECT_PROTOCOL_H
#define DETECT_PROTOCOL_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Synchronously try to detect protocol; blocking but short.
// Returns "XT", "AT", "PS2", or "unknown" (heuristic).
String detectProtocolSync(unsigned long timeoutMs=3000);

// Asynchronous handler that can be bound to webserver (returns JSON immediately)
void detectProtocolAsync(AsyncWebServerRequest *req);

#endif
