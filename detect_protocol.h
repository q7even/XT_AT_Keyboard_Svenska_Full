#ifndef DETECT_PROTOCOL_H
#define DETECT_PROTOCOL_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

String detectProtocolSync(unsigned long timeoutMs=3000);
void detectProtocolAsync(AsyncWebServerRequest *req);

#endif
