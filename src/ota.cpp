#include "ota.h"
#include <AsyncElegantOTA.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <Update.h>
#include <LittleFS.h>

// Default firmware version (change this in code for each release)
String firmware_version = "1.0.0";

// Preferences namespace/key
static Preferences prefs;

// Keys used in Preferences
static const char *P_OTA_USER     = "ota_user";
static const char *P_OTA_PASS     = "ota_pass";
static const char *P_MANIFEST_URL = "ota_manifest";
static const char *P_LAST_OK      = "ota_ok";
static const char *P_AUTO_ENABLED = "auto_ota";
static const char *P_AUTO_MS      = "auto_ms";

// Runtime cached values
static String ota_user;
static String ota_pass;
static String ota_manifest_url;
static bool last_ota_success = true;
static bool auto_ota_enabled = false;
static unsigned long auto_ota_interval_ms = 3600000UL; // default 1h

// Auto-check bookkeeping
static unsigned long lastAutoCheck = 0;

// Forward declarations
static void loadPrefs();
static void savePrefs();
static bool ensureLittleFS();
static bool writeBackupToLittleFS(const String &json);

// Simple small HTML page for OTA settings (served at /ota-settings)
const char OTA_SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!doctype html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1"><title>OTA-inställningar</title>
<style>
body{font-family:Arial;padding:12px;background:#f3f3f3}
.card{background:#fff;padding:14px;border-radius:8px;max-width:520px;margin:auto}
label{display:block;margin-top:8px;font-weight:600}
input{width:100%;padding:8px;margin-top:6px;border-radius:6px;border:1px solid #ccc}
button{margin-top:10px;padding:10px;border-radius:6px;background:#007bff;color:#fff;border:none;cursor:pointer}
.small{font-size:0.9em;color:#666;margin-top:6px}
</style>
</head>
<body>
<div class="card">
<h2>OTA-inställningar</h2>
<form id="otaForm">
<label>OTA Användarnamn</label>
<input id="user" name="user" placeholder="admin">
<label>OTA Lösenord</label>
<input id="pass" name="pass" type="password" placeholder="password">
<label>Manifest URL (version.json)</label>
<input id="url" name="url" placeholder="http://server/firmware/version.json">
<div class="small">Manifest format: {"latest":"1.1.0","bin":"http://.../esp32s3_1.1.0.bin"}</div>
<button type="button" onclick="save()">Spara</button>
<button type="button" onclick="backup()">Backup nu</button>
</form>
<script>
async function save(){
  const user=document.getElementById('user').value;
  const pass=document.getElementById('pass').value;
  const url=document.getElementById('url').value;
  const body = new URLSearchParams();
  body.append('user', user);
  body.append('pass', pass);
  body.append('url', url);
  const res = await fetch('/save-ota', {method:'POST', body: body});
  alert(await res.text());
}
async function backup(){
  const res = await fetch('/do-backup');
  alert(await res.text());
}
// load current values
fetch('/update-info').then(r=>r.json()).then(j=>{
  document.getElementById('user').value = j.ota_user || '';
  document.getElementById('url').value = j.manifest_url || '';
});
</script>
</div>
</body>
</html>
)rawliteral";


// ----------------------- Helpers -----------------------
static void loadPrefs() {
  prefs.begin("ota", true);
  ota_user = prefs.getString(P_OTA_USER, "admin");
  ota_pass = prefs.getString(P_OTA_PASS, "keyboard");
  ota_manifest_url = prefs.getString(P_MANIFEST_URL, "");
  last_ota_success = prefs.getBool(P_LAST_OK, true);
  auto_ota_enabled = prefs.getBool(P_AUTO_ENABLED, false);
  auto_ota_interval_ms = prefs.getULong(P_AUTO_MS, 3600000UL);
  prefs.end();
}

static void savePrefs() {
  prefs.begin("ota", false);
  prefs.putString(P_OTA_USER, ota_user);
  prefs.putString(P_OTA_PASS, ota_pass);
  prefs.putString(P_MANIFEST_URL, ota_manifest_url);
  prefs.putBool(P_LAST_OK, last_ota_success);
  prefs.putBool(P_AUTO_ENABLED, auto_ota_enabled);
  prefs.putULong(P_AUTO_MS, auto_ota_interval_ms);
  prefs.end();
}

static bool ensureLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("[OTA] LittleFS mount failed");
    return false;
  }
  return true;
}

// Write backup JSON to /backup.json on LittleFS
static bool writeBackupToLittleFS(const String &json) {
  if (!ensureLittleFS()) return false;
  File f = LittleFS.open("/backup.json", FILE_WRITE);
  if (!f) {
    Serial.println("[OTA] Failed to open /backup.json for writing");
    return false;
  }
  f.print(json);
  f.close();
  Serial.println("[OTA] Backup written to /backup.json");
  return true;
}

// Build a simple backup JSON: firmware_version + ota settings
// If you want keymap included, extend this to call external provider functions.
static String buildBackupJson() {
  String j = "{";
  j += "\"firmware_version\":\"" + firmware_version + "\",";
  j += "\"ota_user\":\"" + ota_user + "\",";
  j += "\"manifest_url\":\"" + ota_manifest_url + "\",";
  j += "\"ota_ok\":\"" + String(last_ota_success ? "true":"false") + "\"";
  j += "}";
  return j;
}

// ----------------------- Backup API -----------------------
bool otaBackupNow() {
  String json = buildBackupJson();
  return writeBackupToLittleFS(json);
}

// ----------------------- Auto-OTA: manifest handling -----------------------
// Manifest expected format (JSON):
// { "latest": "1.1.0", "bin": "http://server/firmware/esp32s3_1.1.0.bin" }

static bool parseManifest(const String &payload, String &outLatest, String &outBinUrl) {
  // Very small/no-dependency JSON parse (crude but effective)
  int idxLatest = payload.indexOf("\"latest\"");
  int idxBin = payload.indexOf("\"bin\"");
  if (idxLatest < 0 || idxBin < 0) return false;
  auto extract = [&](int keyIndex)->String{
    int col = payload.indexOf(':', keyIndex);
    int q1 = payload.indexOf('"', col + 1);
    int q2 = payload.indexOf('"', q1 + 1);
    if (q1 < 0 || q2 < 0) return String();
    return payload.substring(q1 + 1, q2);
  };
  outLatest = extract(idxLatest);
  outBinUrl = extract(idxBin);
  return (outLatest.length() > 0 && outBinUrl.length() > 0);
}

void otaCheckNow() {
  if (ota_manifest_url.length() == 0) {
    Serial.println("[OTA] No manifest URL configured");
    return;
  }
  if (!WiFi.isConnected()) {
    Serial.println("[OTA] WiFi not connected; skipping auto-check");
    return;
  }

  Serial.println("[OTA] Fetching manifest: " + ota_manifest_url);
  HTTPClient http;
  http.begin(ota_manifest_url);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[OTA] Manifest GET failed: %d\n", code);
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  String latest, binUrl;
  if (!parseManifest(payload, latest, binUrl)) {
    Serial.println("[OTA] Manifest parse failed or missing fields");
    return;
  }

  Serial.printf("[OTA] Manifest latest=%s bin=%s\n", latest.c_str(), binUrl.c_str());
  if (latest == firmware_version) {
    Serial.println("[OTA] Already at latest firmware");
    return;
  }

  // Backup before performing OTA
  Serial.println("[OTA] Backing up configuration before update");
  otaBackupNow();

  // Download bin and perform OTA
  Serial.println("[OTA] Downloading firmware: " + binUrl);
  HTTPClient http2;
  http2.begin(binUrl);
  int code2 = http2.GET();
  if (code2 != 200) {
    Serial.printf("[OTA] Bin GET failed: %d\n", code2);
    http2.end();
    last_ota_success = false;
    savePrefs();
    return;
  }

  int len = http2.getSize();
  WiFiClient *stream = http2.getStreamPtr();

  if (!Update.begin(len > 0 ? len : 0xFFFFFFFF)) {
    Serial.println("[OTA] Not enough space for OTA");
    http2.end();
    last_ota_success = false;
    savePrefs();
    return;
  }

  uint8_t buff[1024];
  int written = 0;
  while (http2.connected()) {
    int r = stream->readBytes(buff, sizeof(buff));
    if (r > 0) {
      Update.write(buff, r);
      written += r;
    } else break;
  }

  if (Update.end()) {
    Serial.println("[OTA] Update finished. Restarting...");
    last_ota_success = true;
    savePrefs();
    http2.end();
    delay(500);
    ESP.restart();
  } else {
    Serial.println("[OTA] Update failed");
    last_ota_success = false;
    savePrefs();
    http2.end();
  }
}

// ----------------------- Setup endpoints / ElegantOTA -----------------------
void setupOTA(AsyncWebServer &server) {
  Serial.println("[OTA] Initializing OTA module");

  // load prefs into runtime
  loadPrefs();

  // Mount LittleFS so backup endpoints can work
  ensureLittleFS();

  // /update-info: returns JSON with version and ota status + manifest
  server.on("/update-info", HTTP_GET, [](AsyncWebServerRequest *req){
    String j = "{";
    j += "\"version\":\"" + firmware_version + "\",";
    j += "\"ota_ok\":\"" + String(last_ota_success ? "true":"false") + "\",";
    j += "\"manifest_url\":\"" + ota_manifest_url + "\"";
    j += "}";
    req->send(200, "application/json", j);
  });

  // /ota-settings: nice configuration page
  server.on("/ota-settings", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send_P(200, "text/html", OTA_SETTINGS_HTML);
  });

  // /save-ota: save OTA credentials and manifest (POST form)
  server.on("/save-ota", HTTP_POST, [](AsyncWebServerRequest *req){
    // reading form fields (body) - AsyncWebServer provides them via getParam with true flag
    if (req->hasParam("user", true)) ota_user = req->getParam("user", true)->value();
    if (req->hasParam("pass", true)) ota_pass = req->getParam("pass", true)->value();
    if (req->hasParam("url", true)) ota_manifest_url = req->getParam("url", true)->value();
    // save to prefs
    savePrefs();
    req->send(200, "text/plain", "Saved OTA settings");
  });

  // /do-backup: manual backup (writes /backup.json)
  server.on("/do-backup", HTTP_GET, [](AsyncWebServerRequest *req){
    bool ok = otaBackupNow();
    if (ok) req->send(200, "text/plain", "Backup saved to /backup.json");
    else req->send(500, "text/plain", "Backup failed");
  });

  // /download-backup: download backup file
  server.on("/download-backup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!LittleFS.exists("/backup.json")) { req->send(404, "text/plain", "No backup"); return; }
    req->send(LittleFS, "/backup.json", "application/json");
  });

  // Start AsyncElegantOTA with Basic Auth (user/pass loaded from prefs)
  AsyncElegantOTA.setAuth(ota_user.c_str(), ota_pass.c_str());
  AsyncElegantOTA.begin(&server); // registers /update
  Serial.println("[OTA] AsyncElegantOTA started at /update (auth configured)");
}

// ----------------------- Periodic handler -----------------------
void otaPeriodic() {
  // periodically reload prefs (in case UI changed things)
  static unsigned long lastPrefsReload = 0;
  if (millis() - lastPrefsReload > 15000) {
    lastPrefsReload = millis();
    loadPrefs(); // reload any changes
    // update ElegantOTA auth too
    AsyncElegantOTA.setAuth(ota_user.c_str(), ota_pass.c_str());
  }

  // Auto-OTA check if enabled
  if (auto_ota_enabled && (millis() - lastAutoCheck > auto_ota_interval_ms)) {
    lastAutoCheck = millis();
    otaCheckNow();
  }
}
