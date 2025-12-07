#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

void wifiStart(AsyncWebServer* server = nullptr);
String wifiIP();
void startSTA(const String &ssid, const String &pass);
void startAP();
String scanNetworksJSON();
void wifiHandlePeriodic();
void captiveRedirect(AsyncWebServerRequest *request);

#endif
