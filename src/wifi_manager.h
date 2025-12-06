#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

/// Start WiFi subsystem. If `server` is provided, captive portal endpoints will be registered.
void wifiStart(AsyncWebServer* server = nullptr);

/// Return IP (STA IP if connected, otherwise AP IP)
String wifiIP();

/// Start STA connection with given credentials (also saves to config)
void startSTA(const String &ssid, const String &pass);

/// Start AP (using config.ap_ssid and config.ap_pass)
void startAP();

/// Return JSON array of scanned networks (ssid, rssi, auth)
String scanNetworksJSON();

/// Should be called periodically from loop() to manage reconnect/backoff
void wifiHandlePeriodic();

/// If captive portal enabled, forcibly redirect request (helper)
void captiveRedirect(AsyncWebServerRequest *request);

#endif // WIFI_MANAGER_H
