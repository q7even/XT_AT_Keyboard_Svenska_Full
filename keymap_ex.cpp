#include "keymap_ex.h"
#include "config.h"
#include "keymap.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

KeymapEntry keymapEx[256];
static const char *KEX_PATH = "/keymap_ex.json";

static void loadFromLegacy() {
    for (int i = 0; i < 256; i++) {
        uint8_t base = config.keymap[i];
        keymapEx[i] = { base, base, base, base, 0 };
    }
}

bool keymapExSaveFS() {
    if (!LittleFS.begin(true)) return false;
    File f = LittleFS.open(KEX_PATH, FILE_WRITE);
    if (!f) return false;
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 256; i++) {
        JsonObject o = arr.createNestedObject();
        o["base"]  = keymapEx[i].base;
        o["shift"] = keymapEx[i].shift;
        o["altgr"] = keymapEx[i].altgr;
        o["ctrl"]  = keymapEx[i].ctrl;
        o["dead"]  = keymapEx[i].dead;
    }
    serializeJson(doc, f);
    f.close();
    return true;
}

bool keymapExLoadFS() {
    if (!LittleFS.begin(true)) return false;
    if (!LittleFS.exists(KEX_PATH)) return false;
    File f = LittleFS.open(KEX_PATH, FILE_READ);
    if (!f) return false;
    StaticJsonDocument<8192> doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) return false;
    JsonArray arr = doc.as<JsonArray>();
    if (arr.size() != 256) return false;
    for (int i = 0; i < 256; i++) {
        JsonObject o = arr[i];
        keymapEx[i].base  = o["base"]  | 0;
        keymapEx[i].shift = o["shift"] | 0;
        keymapEx[i].altgr = o["altgr"] | 0;
        keymapEx[i].ctrl  = o["ctrl"]  | 0;
        keymapEx[i].dead  = o["dead"]  | 0;
    }
    return true;
}

String keymapExJSON() {
    StaticJsonDocument<8192> doc;
    JsonArray arr = doc.to<JsonArray>();
    for (int i = 0; i < 256; i++) {
        JsonObject o = arr.createNestedObject();
        o["base"]  = keymapEx[i].base;
        o["shift"] = keymapEx[i].shift;
        o["altgr"] = keymapEx[i].altgr;
        o["ctrl"]  = keymapEx[i].ctrl;
        o["dead"]  = keymapEx[i].dead;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

bool keymapExSet(uint8_t usb, KeymapEntry e) {
    if (usb > 255) return false;
    keymapEx[usb] = e;
    keymapExSaveFS();
    return true;
}

void keymapExResetDefault() {
    loadFromLegacy();
    keymapExSaveFS();
}

uint8_t mapHIDToXT_Advanced(uint8_t hid, bool shift, bool altgr, bool ctrl) {
    if (hid > 255) return 0;
    KeymapEntry &e = keymapEx[hid];
    if (e.dead) {
        return e.base;
    }
    if (ctrl)  return e.ctrl;
    if (altgr) return e.altgr;
    if (shift) return e.shift;
    return e.base;
}

void keymapExInit(AsyncWebServer *server) {
    if (!keymapExLoadFS()) {
        Serial.println("[KEYMAP-EX] No extended keymap found, loading legacy base map...");
        loadFromLegacy();
        keymapExSaveFS();
    } else {
        Serial.println("[KEYMAP-EX] Loaded extended keymap.");
    }
    if (server) registerKeymapExEndpoints(server);
}

void registerKeymapExEndpoints(AsyncWebServer *server) {
    if (!server) return;
    server->on("/api/map_ex", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(200, "application/json", keymapExJSON());
    });
    server->on("/api/map_ex_reset", HTTP_POST, [](AsyncWebServerRequest *req) {
        keymapExResetDefault();
        req->send(200, "application/json", "{"status":"reset"}");
    });
    server->on("/api/map_ex_set", HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200,"application/json","{"status":"ok"}"); }, NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<512> doc;
            auto err = deserializeJson(doc, data, len);
            if (err) { req->send(400,"application/json","{"error":"bad json"}"); return; }
            int usb = doc["usb"] | -1;
            if (usb < 0 || usb > 255) { req->send(400,"application/json","{"error":"usb out of range"}"); return; }
            KeymapEntry e;
            e.base  = doc["base"]  | 0;
            e.shift = doc["shift"] | e.base;
            e.altgr = doc["altgr"] | e.base;
            e.ctrl  = doc["ctrl"]  | e.base;
            e.dead  = doc["dead"]  | 0;
            keymapExSet(usb, e);
            req->send(200,"application/json","{"status":"saved"}");
    });
    server->on("/api/map_ex_upload", HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200,"application/json","{"status":"ok"}"); }, NULL,
        [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
            StaticJsonDocument<8192> doc;
            auto err = deserializeJson(doc, data, len);
            if (err || !doc.is<JsonArray>() || doc.size() != 256) { req->send(400,"application/json","{"error":"invalid array"}"); return; }
            JsonArray arr = doc.as<JsonArray>();
            for (int i = 0; i < 256; i++) {
                JsonObject o = arr[i];
                keymapEx[i].base  = o["base"]  | 0;
                keymapEx[i].shift = o["shift"] | keymapEx[i].base;
                keymapEx[i].altgr = o["altgr"] | keymapEx[i].base;
                keymapEx[i].ctrl  = o["ctrl"]  | keymapEx[i].base;
                keymapEx[i].dead  = o["dead"]  | 0;
            }
            keymapExSaveFS();
            req->send(200,"application/json","{"status":"saved"}");
    });
    server->on("/api/map_ex_download", HTTP_GET, [](AsyncWebServerRequest *req){
        if (!LittleFS.begin(true) || !LittleFS.exists(KEX_PATH)) { req->send(404,"application/json","{"error":"no file"}"); return; }
        req->send(LittleFS, KEX_PATH, "application/json");
    });
    Serial.println("[KEYMAP-EX] Endpoints registered.");
}
