#pragma once
#include <AsyncElegantOTA.h>
#include <ESPAsyncWebServer.h>

void setupOTA(AsyncWebServer &server);
void otaPeriodic();
