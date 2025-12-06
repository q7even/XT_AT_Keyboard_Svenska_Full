#include "keymap_manager.h"
#include "keymap.h"    // provides default_usb_to_xt[]
#include "config.h"    // config.keymap and configSave()
#include <LittleFS.h>
#include <ArduinoJson.h>

// Path on LittleFS
static const char *KEYMAP_PATH = "/keymap.json";

// In-memory keymap is stored in config.keymap (AppConfig in config.h)
// We assume config is available and configLoad() has been called before init.

// -------- Utility: save to LittleFS as JSON ----------
bool writeKeymapToFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[KEYMAP] LittleFS mount failed (write)");
    return false;
  }

  File f = LittleFS.open(KEYMAP_PATH, FILE_WRITE);
  if (!f) {
    Serial.println("[KEYMAP] Failed to open keymap file for writing");
    return false;
  }

  // Use ArduinoJson to serialize
  StaticJsonDocument<4096> doc; // should be enough for 256 ints
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

// -------- Utility: read from LittleFS JSON ----------
bool readKeymapFromFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[KEYMAP] LittleFS mount failed (read)");
    return false;
  }
  if (!LittleFS.exists(KEYMAP_PATH)) {
    Serial.println("[KEYMAP] No keymap file at " KEYMAP_PATH);
    return false;
  }
  File f = LittleFS.open(KEYMAP_PATH, FILE_READ);
  if (!f) {
    Serial.println("[KEYMAP] Failed open keymap file");
    return false;
  }

  // Read into buffer (file not huge)
  size_t size = f.size();
  if (size == 0) { f.close(); Serial.println("[KEYMAP] Empty keymap file"); return false; }
  String body; body.reserve(size + 8);
  while (f.available()) body += (char)f.read();
  f.close();

  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[KEYMAP] JSON parse failed: %s\n", err.c_str());
    return false;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("[KEYMAP] keymap JSON is not array");
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if ((int)arr.size() != 256) {
    Serial.printf("[KEYMAP] keymap array size mismatch: %d\n", (int)arr.size());
    return false;
  }

  for (int i = 0; i < 256; ++i) {
    int v = arr[i] | 0;
    config.keymap[i] = (uint8_t)v;
  }

  // Persist to NVS via configSave()
  configSave();
  Serial.println("[KEYMAP] Loaded keymap from FS and saved to config");
  return true;
}

// --------- Public wrappers ------------

void keymapInit(AsyncWebServer *server) {
  // If user keymap in config is all zeros, try to load from FS or set defaults
  bool allZero = true;
  for (int i=0;i<256;i++) if (config.keymap[i] != 0x00) { allZero = false; break; }

  if (allZero) {
    // try load from FS
    if (!readKeymapFromFS()) {
      // fallback: copy defaults from default_usb_to_xt[]
      for (int i=0;i<256;i++) config.keymap[i] = default_usb_to_xt[i];
      configSave();
      Serial.println("[KEYMAP] No existing keymap: default loaded into config");
    }
  } else {
    // ensure FS has current keymap (optionally)
    writeKeymapToFS();
    Serial.println("[KEYMAP] Using keymap from config");
  }

  // Register endpoints if server provided
  if (server) registerKeymapEndpoints(server);
}

// Save both to FS and NVS
bool keymapSave() {
  // first save config (NVS)
  configSave();
  // then write to FS
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
  // return JSON array string
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
  // persist (quick save)
  configSave();
  writeKeymapToFS();
  return true;
}

uint8_t mapHIDtoXT(uint8_t hidCode) {
  return config.keymap[hidCode];
}

// --------- HTTP endpoints registration -----------
void handle_map_get(AsyncWebServerRequest *request) {
  String body = getKeymapJSON();
  request->send(200, "application/json", body);
}

void handle_map_set_single(AsyncWebServerRequest *request) {
  // Expects POST with JSON: { "usb": <int>, "xt": <int> }
  // Use onBody handler to receive raw body
  request->send(200, "application/json", "{\"status\":\"ok\"}");
}

void handle_map_upload(AsyncWebServerRequest *request) {
  // Expects raw JSON array in body (256 ints)
  // We use onBody callback; this stub only responds OK here
  request->send(200, "application/json", "{\"status\":\"uploaded\"}");
}

void handle_map_download(AsyncWebServerRequest *request) {
  if (!LittleFS.begin(true) || !LittleFS.exists(KEYMAP_PATH)) {
    request->send(404, "application/json", "{\"error\":\"no keymap\"}");
    return;
  }
  request->send(LittleFS, KEYMAP_PATH, "application/json");
}

void handle_map_reset(AsyncWebServerRequest *request) {
  keymapResetToDefault();
  request->send(200, "application/json", "{\"status\":\"reset\"}");
}

void registerKeymapEndpoints(AsyncWebServer *server) {
  if (!server) return;

  // GET current map
  server->on("/api/map", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", getKeymapJSON());
  });

  // POST set single map (use body)
  server->on("/api/map_set", HTTP_POST, [](AsyncWebServerRequest *req){
    // Early response; body processed in onBody
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    // parse JSON body
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
      req->send(400, "application/json", "{\"error\":\"bad json\"}");
      return;
    }
    int usb = doc["usb"] | -1;
    int xt  = doc["xt"]  | -1;
    if (usb < 0 || usb > 255 || xt < 0 || xt > 255) {
      req->send(400, "application/json", "{\"error\":\"invalid params\"}");
      return;
    }
    setKeymapEntry((uint8_t)usb, (uint8_t)xt);
    req->send(200, "application/json", "{\"status\":\"saved\"}");
  });

  // POST upload full keymap (array JSON) -> /api/map_upload
  server->on("/api/map_upload", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    // parse array
    StaticJsonDocument<8192> doc; // large enough for 256 ints
    DeserializationError err = deserializeJson(doc, data, len);
    if (err || !doc.is<JsonArray>()) {
      req->send(400,"application/json","{\"error\":\"bad json or not array\"}");
      return;
    }
    JsonArray arr = doc.as<JsonArray>();
    if ((int)arr.size() != 256) {
      req->send(400,"application/json","{\"error\":\"array must have 256 elements\"}");
      return;
    }
    for (int i=0;i<256;i++){
      int v = arr[i] | 0;
      config.keymap[i] = (uint8_t)v;
    }
    configSave();
    writeKeymapToFS();
    req->send(200,"application/json","{\"status\":\"saved\"}");
  });

  // GET download current keymap file
  server->on("/api/map_download", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!LittleFS.begin(true) || !LittleFS.exists(KEYMAP_PATH)) {
      req->send(404, "application/json", "{\"error\":\"no keymap file\"}");
      return;
    }
    req->send(LittleFS, KEYMAP_PATH, "application/json");
  });

  // POST reset to default
  server->on("/api/map_reset", HTTP_POST, [](AsyncWebServerRequest *req){
    keymapResetToDefault();
    req->send(200, "application/json", "{\"status\":\"reset\"}");
  });

  Serial.println("[KEYMAP] Endpoints registered: /api/map, /api/map_set, /api/map_upload, /api/map_download, /api/map_reset");
}
