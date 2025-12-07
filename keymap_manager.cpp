#include "keymap_manager.h"
#include "keymap.h"
#include "config.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char *KEYMAP_PATH = "/keymap.json";

bool writeKeymapToFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[KEYMAP] LittleFS mount failed (write)");
    return false;
  }
  File f = LittleFS.open(KEYMAP_PATH, FILE_WRITE);
  if (!f) { Serial.println("[KEYMAP] Failed to open keymap file for writing"); return false; }
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < 256; ++i) arr.add((uint8_t)config.keymap[i]);
  if (serializeJson(doc, f) == 0) {
    Serial.println("[KEYMAP] Failed to write JSON to file");
    f.close();
    return false;
  }
  f.close();
  Serial.println("[KEYMAP] Saved /keymap.json");
  return true;
}

bool readKeymapFromFS() {
  if (!LittleFS.begin(true)) { Serial.println("[KEYMAP] LittleFS mount failed (read)"); return false; }
  if (!LittleFS.exists(KEYMAP_PATH)) { Serial.println("[KEYMAP] No keymap file at " KEYMAP_PATH); return false; }
  File f = LittleFS.open(KEYMAP_PATH, FILE_READ);
  if (!f) { Serial.println("[KEYMAP] Failed open keymap file"); return false; }
  size_t size = f.size();
  if (size == 0) { f.close(); Serial.println("[KEYMAP] Empty keymap file"); return false; }
  String body; body.reserve(size + 8);
  while (f.available()) body += (char)f.read();
  f.close();
  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) { Serial.printf("[KEYMAP] JSON parse failed: %s\n", err.c_str()); return false; }
  if (!doc.is<JsonArray>()) { Serial.println("[KEYMAP] keymap JSON is not array"); return false; }
  JsonArray arr = doc.as<JsonArray>();
  if ((int)arr.size() != 256) { Serial.printf("[KEYMAP] keymap array size mismatch: %d\n", (int)arr.size()); return false; }
  for (int i = 0; i < 256; ++i) {
    int v = arr[i] | 0;
    config.keymap[i] = (uint8_t)v;
  }
  configSave();
  Serial.println("[KEYMAP] Loaded keymap from FS and saved to config");
  return true;
}

void keymapInit(AsyncWebServer *server) {
  bool allZero = true;
  for (int i=0;i<256;i++) if (config.keymap[i] != 0x00) { allZero = false; break; }
  if (allZero) {
    if (!readKeymapFromFS()) {
      for (int i=0;i<256;i++) config.keymap[i] = default_usb_to_xt[i];
      configSave();
      Serial.println("[KEYMAP] No existing keymap: default loaded into config");
    }
  } else {
    writeKeymapToFS();
    Serial.println("[KEYMAP] Using keymap from config");
  }
  if (server) registerKeymapEndpoints(server);
}

bool keymapSave() {
  configSave();
  return writeKeymapToFS();
}

bool keymapLoadFromFS() {
  return readKeymapFromFS();
}

void keymapResetToDefault() {
  for (int i=0;i<256;i++) config.keymap[i] = default_usb_to_xt[i];
  configSave();
  writeKeymapToFS();
  Serial.println("[KEYMAP] Reset to default and saved");
}

String getKeymapJSON() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < 256; ++i) arr.add((uint8_t)config.keymap[i]);
  String out;
  serializeJson(doc, out);
  return out;
}

bool setKeymapEntry(uint8_t usbCode, uint8_t xtCode) {
  if (usbCode > 255) return false;
  config.keymap[usbCode] = xtCode;
  configSave();
  writeKeymapToFS();
  return true;
}

uint8_t mapHIDtoXT(uint8_t hidCode) {
  return config.keymap[hidCode];
}

void registerKeymapEndpoints(AsyncWebServer *server) {
  if (!server) return;
  server->on("/api/map", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", getKeymapJSON());
  });
  server->on("/api/map_set", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{"status":"ok"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { req->send(400,"application/json","{"error":"bad json"}"); return; }
    int usb = doc["usb"] | -1; int xt = doc["xt"] | -1;
    if (usb < 0 || usb > 255 || xt < 0 || xt > 255) { req->send(400,"application/json","{"error":"invalid params"}"); return; }
    setKeymapEntry((uint8_t)usb, (uint8_t)xt);
    req->send(200,"application/json","{"status":"saved"}");
  });
  server->on("/api/map_upload", HTTP_POST, [](AsyncWebServerRequest *req){ req->send(200, "application/json", "{"status":"ok"}"); }, NULL,
    [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
      StaticJsonDocument<8192> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err || !doc.is<JsonArray>()) { req->send(400,"application/json","{"error":"bad json or not array"}"); return; }
      JsonArray arr = doc.as<JsonArray>();
      if ((int)arr.size() != 256) { req->send(400,"application/json","{"error":"array must have 256 elements"}"); return; }
      for (int i=0;i<256;i++){ int v = arr[i] | 0; config.keymap[i] = (uint8_t)v; }
      configSave(); writeKeymapToFS(); req->send(200,"application/json","{"status":"saved"}");
  });
  server->on("/api/map_download", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!LittleFS.begin(true) || !LittleFS.exists(KEYMAP_PATH)) { req->send(404, "application/json", "{"error":"no keymap file"}"); return; }
    req->send(LittleFS, KEYMAP_PATH, "application/json");
  });
  server->on("/api/map_reset", HTTP_POST, [](AsyncWebServerRequest *req){ keymapResetToDefault(); req->send(200, "application/json", "{"status":"reset"}"); });
  Serial.println("[KEYMAP] Endpoints registered");
}
