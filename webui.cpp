#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include "webui.h"
#include "config.h"
#include "wifi_manager.h"

AsyncWebServer server(80);

void webuiStart() {
    SPIFFS.begin(true);

    server.serveStatic("/", SPIFFS, "/")
        .setDefaultFile("index.html");

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *req){
        String json = "{";
        json += "\"ip\":\"" + wifiIP() + "\",";
        json += "\"mode\":" + String(config.mode);
        json += "}";
        req->send(200, "application/json", json);
    });

    server.on("/save", HTTP_POST, [](AsyncWebServerRequest *req){
        if (req->hasParam("sta_ssid", true))
            config.sta_ssid = req->getParam("sta_ssid", true)->value();

        if (req->hasParam("sta_pass", true))
            config.sta_pass = req->getParam("sta_pass", true)->value();

        configSave();
        req->send(200, "text/plain", "OK");
    });

    server.begin();
}
