#include "api.h"
#include "config.h"
#include "xt_at_output.h"
#include "keymap.h"
#include <ArduinoJson.h>
#include <WiFi.h>
#include <LittleFS.h>

// Extern från ota/config om definierat (falls)
extern String firmware_version;

// Helper: make JSON response with code+message
static void sendJson(AsyncWebServerRequest *req, int code, const String &bodyJson) {
  req->send(code, "application/json", bodyJson);
}

// Convert a single char to USB HID code (basic letters/numbers)
uint8_t charToHID(char c) {
  if (c >= 'a' && c <= 'z') return 4 + (c - 'a');
  if (c >= 'A' && c <= 'Z') return 4 + (c - 'A');
  if (c >= '1' && c <= '9') return 30 + (c - '1');
  if (c == '0') return 39; // HID 39 is '0'
  // some special cases used in UI
  switch (c) {
    case ' ': return 44; // space
    case '\n': return 40; // enter
    case '\r': return 40;
    default: return 0;
  }
}

// Build JSON array from config.keymap
static String keymapToJsonArray() {
  StaticJsonDocument<2048> doc; // 256 * small ints fits in 2K+
  JsonArray arr = doc.to<JsonArray>();
  for (int i = 0; i < 256; ++i) arr.add((uint8_t)config.keymap[i]);
  String out;
  serializeJson(doc, out);
  return out;
}

void apiInit(AsyncWebServer &server) {
  // GET firmware info
  server.on("/api/fw", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    String ver = (typeof(firmware_version) != void(0) ? firmware_version : String());
    // if firmware_version not defined, fallback
    if (ver.length() == 0) ver = "unknown";
    doc["version"] = ver;
    doc["ota_ok"] = prefs.getBool("ota_ok", true);
    String out; serializeJson(doc, out);
    sendJson(req, 200, out);
  });

  // GET WiFi status / saved STA SSID
  server.on("/api/wifi", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<256> doc;
    doc["ssid"] = config.sta_ssid;
    doc["ap_ssid"] = config.ap_ssid;
    doc["connected"] = (WiFi.status() == WL_CONNECTED);
    if (WiFi.status() == WL_CONNECTED) doc["ip"] = WiFi.localIP().toString();
    String out; serializeJson(doc, out);
    sendJson(req, 200, out);
  });

  // POST set WiFi (expects JSON {ssid, pass})
  server.on("/api/wifi_set", HTTP_POST, [](AsyncWebServerRequest *req){
    // body will be processed in onBody callback below
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    // parse JSON body
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) {
      req->send(400, "application/json", "{\"error\":\"bad json\"}");
      return;
    }
    const char* ssid = doc["ssid"] | "";
    const char* pass = doc["pass"] | "";
    config.sta_ssid = String(ssid);
    config.sta_pass = String(pass);
    configSave();
    // attempt connect
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(config.sta_ssid.c_str(), config.sta_pass.c_str());
    req->send(200, "application/json", "{\"status\":\"connecting\"}");
  });

  // GET mode
  server.on("/api/mode", HTTP_GET, [](AsyncWebServerRequest *req){
    StaticJsonDocument<128> doc;
    String modeName = (config.kb_mode == MODE_XT) ? "XT" : (config.kb_mode == MODE_AT ? "AT" : "PS2");
    doc["mode"] = modeName;
    String out; serializeJson(doc, out);
    sendJson(req, 200, out);
  });

  // POST set mode {mode:"XT"|"AT"|"PS2"}
  server.on("/api/mode_set", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { req->send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
    String mode = String((const char*)doc["mode"] | "");
    if (mode == "XT") config.kb_mode = MODE_XT;
    else if (mode == "AT") config.kb_mode = MODE_AT;
    else config.kb_mode = MODE_PS2;
    configSave();
    req->send(200, "application/json", "{\"status\":\"saved\"}");
  });

  // POST send key {key:"A"} same as UI sendKey
  server.on("/api/send_key", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { req->send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
    const char* key = doc["key"] | "";
    if (!key || strlen(key) == 0) { req->send(400, "application/json", "{\"error\":\"missing key\"}"); return; }
    char c = key[0];
    uint8_t hid = charToHID(c);
    if (hid == 0) {
      // handle special keys from UI (Å,Ä,Ö,Space,Enter,Back)
      String ks = String(key);
      if (ks == "Space") hid = 44;
      else if (ks == "Enter") hid = 40;
      else if (ks == "Back") hid = 42;
      else if (ks == "Å") hid = 39;
      else if (ks == "Ä") hid = 52;
      else if (ks == "Ö") hid = 53;
    }
    if (hid == 0) { req->send(400,"application/json","{\"error\":\"unmapped key\"}"); return; }

    // forward as a press and release (press -> break)
    xtatSendFromUSB(hid, true);
    delay(5);
    xtatSendFromUSB(hid, false);

    req->send(200, "application/json", "{\"status\":\"sent\"}");
  });

  // GET map
  server.on("/api/map", HTTP_GET, [](AsyncWebServerRequest *req){
    String arr = keymapToJsonArray();
    sendJson(req, 200, arr);
  });

  // POST map_set {usb: int, xt: int}
  server.on("/api/map_set", HTTP_POST, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{\"status\":\"ok\"}");
  }, NULL, [](AsyncWebServerRequest *req, uint8_t *data, size_t len, size_t index, size_t total){
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, data, len);
    if (err) { req->send(400, "application/json", "{\"error\":\"bad json\"}"); return; }
    int usb = doc["usb"] | -1;
    int xt  = doc["xt"]  | -1;
    if (usb < 0 || usb > 255 || xt < 0 || xt > 255) { req->send(400,"application/json","{\"error\":\"invalid params\"}"); return; }
    config.keymap[usb] = (uint8_t)xt;
    configSave();
    req->send(200, "application/json", "{\"status\":\"saved\"}");
  });

  // simple health
  server.on("/api/ping", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200, "application/json", "{\"pong\":1}");
  });

  Serial.println("[API] Endpoints registered");
}
