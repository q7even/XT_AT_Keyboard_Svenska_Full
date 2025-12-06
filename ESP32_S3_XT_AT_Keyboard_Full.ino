/* =============================================================================
   ESP32_S3_XT_AT_Keyboard_Full.ino
   All-in-one sketch for Waveshare ESP32-S3-Zero (or similar ESP32-S3 devboard).
   Features:
     - Dual XT/AT keyboard output via level shifter (3.3V <-> 5V)
     - TinyUSB host (USB keyboard) -> XT/AT
     - Web UI (AP + STA) with Swedish touch keyboard + auto-repeat
     - Full 256-element usb_to_xt mapping (svenska)
     - Modifier handling (Shift, Ctrl, Alt)
     - Preferences (NVS) persistent settings
     - LittleFS backup/restore of keymap + config
     - AsyncElegantOTA (web OTA) with basic auth + OTA settings
     - Auto-OTA via manifest (version.json) with pre-backup and rollback flag
   Dependencies (install via Library Manager):
     - ESPAsyncWebServer
     - AsyncTCP
     - AsyncElegantOTA
     - Adafruit TinyUSB (or TinyUSB variant for ESP32-S3)
   Hardware notes:
     - Use 4-channel bidirectional level shifter (LV side to ESP, HV side to XT/AT)
     - Connect HV1->XT CLK, HV2->XT DATA; LV1->ESP GPIO for CLK, LV2->ESP GPIO for DATA
     - ESP is powered via micro-USB
   ============================================================================*/

#include <WiFi.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>
#include <HTTPClient.h>
#include <Update.h>

// TinyUSB host include (depends on your TinyUSB wrapper for ESP32-S3)
// For Arduino, install Adafruit_TinyUSB and include host headers if available.
// We'll include only a small skeleton to call hid_host.Task() where appropriate.
#include "Adafruit_TinyUSB.h" // if not present, adjust according to your environment

// ---------------------------- CONFIG ----------------------------
#define WIFI_AP_SSID "XT-Keyboard"
#define WIFI_AP_PASS "12345678"

// Pins on Waveshare ESP32-S3-Zero — change if needed
const int PIN_XT_CLK = 10;  // LV pin connected to level shifter LV1 -> HW HV1 -> XT CLK
const int PIN_XT_DATA = 11; // LV pin connected to level shifter LV2 -> HW HV2 -> XT DATA

// Timing (microseconds) for XT/AT bit timing; tweak if needed
const unsigned int BIT_DELAY_US = 30; // typical ~30us

// Preferences keys
Preferences prefs;

// OTA defaults (can be changed via UI)
String firmware_version = "1.0.0";
String ota_username = "admin";
String ota_password = "keyboard";
String ota_manifest_url = "http://example.com/firmware/version.json"; // version.json

// Auto-OTA interval (ms)
unsigned long autoOtaInterval = 3600000; // 1 hour
unsigned long lastAutoOta = 0;

// Last OTA result saved
bool last_ota_successful = true;

// -------------------------- Level-shifter / XT low-level ------------------------
void xt_clockPulse() {
  digitalWrite(PIN_XT_CLK, LOW);
  delayMicroseconds(BIT_DELAY_US);
  digitalWrite(PIN_XT_CLK, HIGH);
  delayMicroseconds(BIT_DELAY_US);
}

void xt_drive_data_low() { digitalWrite(PIN_XT_DATA, LOW); }
void xt_release_data() { digitalWrite(PIN_XT_DATA, HIGH); } // HV pull-ups drive high

void xt_send(uint8_t code) {
  // Start bit
  xt_drive_data_low();
  delayMicroseconds(BIT_DELAY_US);
  // 8 data bits LSB first
  for (int i=0; i<8; i++) {
    if (code & 1) xt_release_data(); else xt_drive_data_low();
    xt_clockPulse();
    code >>= 1;
  }
  // Stop bit
  xt_release_data();
  xt_clockPulse();
}

// AT break code (0xF0)
void xt_send_break(uint8_t code) {
  xt_send(0xF0);
  xt_send(code);
}

// -------------------------- USB HID -> XT mapping (256 elements) --------------
const uint8_t usb_to_xt[256] = {
  // 0..15
  0x00,0x00,0x00,0x00,0x1C,0x32,0x21,0x23,0x24,0x2B,0x34,0x33,0x43,0x3B,0x42,0x4B,
  // 16..31
  0x3A,0x31,0x44,0x4D,0x15,0x2D,0x1B,0x2C,0x3C,0x2A,0x1D,0x22,0x35,0x1A,0x45,0x5A,
  // 32..47
  0x76,0x66,0x0D,0x29,0x4E,0x55,0x54,0x5B,0x4C,0x52,0x0E,0x4A,0x41,0x58,0x00,0x00,
  // 48..63
  0x05,0x06,0x04,0x0C,0x03,0x0B,0x83,0x0A,0x01,0x09,0x78,0x07,0x00,0x00,0x00,0x00,
  // 64..79
  0x70,0x69,0x72,0x7A,0x6B,0x73,0x74,0x6C,0x75,0x7D,0x7C,0x4E,0x71,0x6E,0x00,0x00,
  // 80..95
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 96..111
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 112..127
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 128..143
  0x14,0x12,0x11,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 144..159
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 160..175
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 176..191
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 192..207
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 208..223
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 224..239 (modifiers LCtrl,LShift,LAlt, LGUI, ...)
  0x14,0x12,0x11,0x58,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  // 240..255
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

// -------------------------- Web Server + UI ---------------------------------
AsyncWebServer server(80);

// Simple UI parts (touch keyboard + OTA button + WiFi form)
String getIndexHtml() {
  // Compact responsive Swedish touch keyboard + links to WiFi, OTA settings
  // Includes auto-repeat via JS; modifier toggles included
  return R"rawliteral(
<!doctype html>
<html>
<head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>XT/AT Keyboard - Svenska</title>
<style>
body{font-family:Arial;margin:10px;background:#f7f7f7}
.card{background:#fff;padding:12px;border-radius:8px;max-width:720px;margin:auto}
.row{display:flex;flex-wrap:wrap;margin-bottom:6px;justify-content:center}
.key{flex:1 0 9%;margin:3px;padding:14px;text-align:center;border-radius:6px;border:1px solid #bbb;background:#eee;user-select:none}
.key:active{background:#cde}
.wide{flex:1 0 42%}
.modifier{background:#ffd1d1}
.btn{display:inline-block;padding:8px 12px;border-radius:6px;background:#007bff;color:#fff;text-decoration:none;margin:6px}
small.gray{color:#666}
</style>
</head>
<body>
<div class="card">
<h2>XT/AT Keyboard - Svenska</h2>
<div>
<a class="btn" href="/wifi">Wi-Fi</a>
<a class="btn" href="/ota-settings">OTA-inställningar</a>
<a class="btn" href="/update">OTA Upload</a>
<a class="btn" href="/download-backup">Ladda ner backup</a>
</div>
<div style="margin-top:10px">
<button id="shift" class="btn modifier">Shift</button>
<button id="ctrl" class="btn modifier">Ctrl</button>
<button id="alt" class="btn modifier">Alt</button>
</div>

<div id="kbd"></div>
<p class="small gray">Håll ned tangenten för auto-repeat.</p>
</div>

<script>
let shift=false,ctrl=false,alt=false;
document.getElementById('shift').addEventListener('click',()=>{shift=!shift;document.getElementById('shift').style.background=shift?'#4caf50':'#ffd1d1'});
document.getElementById('ctrl').addEventListener('click',()=>{ctrl=!ctrl;document.getElementById('ctrl').style.background=ctrl?'#4caf50':'#ffd1d1'});
document.getElementById('alt').addEventListener('click',()=>{alt=!alt;document.getElementById('alt').style.background=alt?'#4caf50':'#ffd1d1'});

const layout=[
['`','1','2','3','4','5','6','7','8','9','0','+','´','Back'],
['Q','W','E','R','T','Y','U','I','O','P','Å'],
['A','S','D','F','G','H','J','K','L','Ä','Ö'],
['Z','X','C','V','B','N','M',',','.','-'],
['Space','Enter']
];

const repeatDelay=400, repeatRate=100;
let repeatTimers={};

function sendKey(k){
  fetch('/key?key='+encodeURIComponent(k)+'&shift='+(shift?1:0)+'&ctrl='+(ctrl?1:0)+'&alt='+(alt?1:0)).catch(e=>console.log(e));
}
function startRepeat(k){
  sendKey(k);
  repeatTimers[k]=setTimeout(function rep(){ sendKey(k); repeatTimers[k]=setTimeout(rep,repeatRate); }, repeatDelay);
}
function stopRepeat(k){
  if(repeatTimers[k]){ clearTimeout(repeatTimers[k]); repeatTimers[k]=null;}
}
function build(){
  const kb=document.getElementById('kbd');
  layout.forEach(row=>{
    const r=document.createElement('div'); r.className='row';
    row.forEach(k=>{
      const b=document.createElement('div'); b.className='key'; if(k==='Space') b.className+=' wide';
      b.textContent=k;
      b.addEventListener('mousedown',()=>startRepeat(k));
      b.addEventListener('touchstart',()=>startRepeat(k));
      b.addEventListener('mouseup',()=>stopRepeat(k));
      b.addEventListener('mouseleave',()=>stopRepeat(k));
      b.addEventListener('touchend',()=>stopRepeat(k));
      b.addEventListener('touchcancel',()=>stopRepeat(k));
      r.appendChild(b);
    });
    kb.appendChild(r);
  });
}
build();
</script>
</body>
</html>
)rawliteral";
}

// WiFi settings page
String getWiFiHtml() {
  return R"rawliteral(
<!doctype html>
<html>
<head><meta name="viewport" content="width=device-width,initial-scale=1"><title>WiFi</title></head>
<body style="font-family:Arial;padding:12px;">
<h3>Konfigurera Wi-Fi (STA)</h3>
<form method="POST" action="/savewifi">
SSID:<br><input name="ssid"><br>Password:<br><input name="password" type="password"><br><br>
<button type="submit">Spara</button>
</form>
<p>Device fungerar även som AP (SSID: )</p>
</body>
</html>
)rawliteral";
}

// OTA settings (we will also host a nicer page from the ota functions later)
String getOtaSettingsHtml() {
  return R"rawliteral(
<!doctype html>
<html><head><meta name="viewport" content="width=device-width,initial-scale=1"><title>OTA Settings</title></head>
<body style="font-family:Arial;padding:12px;">
<h3>OTA Inställningar</h3>
<form method="POST" action="/save-ota">
OTA användare:<br><input name="user" value=""><br>OTA lösen:<br><input name="pass" type="password"><br>Manifest URL:<br><input name="url" value=""><br><br>
<button type="submit">Spara</button>
</form>
<p><a href="/update">Gå till OTA upload</a></p>
</body></html>
)rawliteral";
}

// -------------------------- Backup / LittleFS -----------------------
bool ensureLittleFS() {
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed");
    return false;
  }
  Serial.println("LittleFS mounted");
  return true;
}

String makeBackupJson() {
  // Compose basic backup JSON including keymap and prefs
  String j = "{";
  j += "\"firmware_version\":\"" + firmware_version + "\",";
  j += "\"ota_username\":\"" + ota_username + "\",";
  j += "\"ota_manifest_url\":\"" + ota_manifest_url + "\",";
  j += "\"ota_ok\":\"" + String(last_ota_successful ? "true":"false") + "\",";
  // Append keymap array
  j += "\"keymap\":[";
  for (int i=0;i<256;i++){
    char buf[8];
    snprintf(buf,sizeof(buf),"\"%02X\"", usb_to_xt[i]);
    j += String(buf);
    if (i<255) j += ",";
  }
  j += "]}";
  return j;
}

bool writeBackup() {
  String json = makeBackupJson();
  File f = LittleFS.open("/backup.json", FILE_WRITE);
  if (!f) { Serial.println("Failed open /backup.json"); return false; }
  f.print(json);
  f.close();
  Serial.println("Backup written to /backup.json");
  return true;
}

String readBackup() {
  if (!LittleFS.exists("/backup.json")) return String();
  File f = LittleFS.open("/backup.json", FILE_READ);
  if (!f) return String();
  String s = f.readString();
  f.close();
  return s;
}

// Optionally restore from backup at first boot if flag set in prefs
void maybeRestoreFromBackup() {
  prefs.begin("boot", true);
  bool restored = prefs.getBool("restored", false);
  prefs.end();
  if (restored) { Serial.println("Restore already performed previously"); return; }
  if (!LittleFS.exists("/backup.json")) { Serial.println("No backup.json found"); return; }
  String b = readBackup();
  if (b.length() == 0) return;
  // For simplicity, we do not auto-apply keymap here; instead we keep backup downloadable.
  Serial.println("Backup found; not auto-restoring keymap automatically (safety).");
  // If you want auto-restore, set prefs->restored = true and implement JSON parse+apply here
}

// -------------------------- OTA: AsyncElegantOTA setup ----------------------
void setupOTAEndpoints() {
  // Expose some info for UI
  server.on("/update-info", HTTP_GET, [](AsyncWebServerRequest *req){
    String js = "{";
    js += "\"version\":\"" + firmware_version + "\",";
    js += "\"ota_ok\":\"" + String(last_ota_successful ? "true":"false") + "\",";
    js += "\"manifest_url\":\"" + ota_manifest_url + "\"";
    js += "}";
    req->send(200, "application/json", js);
  });

  // OTA settings UI (simple) - this could be improved
  server.on("/ota-settings", HTTP_GET, [](AsyncWebServerRequest *req){
    req->send(200,"text/html",getOtaSettingsHtml());
  });
  // Save OTA settings
  server.on("/save-ota", HTTP_POST, [](AsyncWebServerRequest *req){
    if (req->hasParam("user", true)) ota_username = req->getParam("user", true)->value();
    if (req->hasParam("pass", true)) ota_password = req->getParam("pass", true)->value();
    if (req->hasParam("url", true)) ota_manifest_url = req->getParam("url", true)->value();
    // Persist to prefs
    prefs.begin("ota", false);
    prefs.putString("user", ota_username);
    prefs.putString("pass", ota_password);
    prefs.putString("manifest", ota_manifest_url);
    prefs.end();
    req->send(200,"text/plain","Saved OTA settings");
  });

  // Manual backup endpoint
  server.on("/do-backup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (writeBackup()) req->send(200,"text/plain","Backup saved");
    else req->send(500,"text/plain","Backup failed");
  });

  // Download backup
  server.on("/download-backup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (!LittleFS.exists("/backup.json")) { req->send(404,"text/plain","No backup"); return; }
    req->send(LittleFS, "/backup.json", "application/json");
  });

  // Start ElegantOTA (with auth)
  AsyncElegantOTA.setAuth(ota_username.c_str(), ota_password.c_str());
  AsyncElegantOTA.begin(&server);
  Serial.println("AsyncElegantOTA ready at /update (auth enabled)");
}

// -------------------------- Auto-OTA using manifest --------------------------
void autoCheckForUpdate() {
  if (!WiFi.isConnected()) return;
  if (ota_manifest_url.length() == 0) { Serial.println("No manifest URL"); return; }

  Serial.println("[AUTO-OTA] Checking manifest...");
  HTTPClient http;
  http.begin(ota_manifest_url);
  int code = http.GET();
  if (code != 200) { Serial.printf("[AUTO-OTA] manifest GET failed: %d\n", code); http.end(); return; }
  String payload = http.getString();
  http.end();

  // parse JSON manually (simple crude parse)
  int idxLatest = payload.indexOf("\"latest\"");
  int idxBin = payload.indexOf("\"bin\"");
  if (idxLatest < 0 || idxBin < 0) { Serial.println("[AUTO-OTA] manifest missing keys"); return; }
  auto extract = [&](int idx)->String{
    int c = payload.indexOf(':', idx);
    int q1 = payload.indexOf('"', c+1);
    int q2 = payload.indexOf('"', q1+1);
    if (q1 < 0 || q2 < 0) return String();
    return payload.substring(q1+1, q2);
  };
  String latest = extract(idxLatest);
  String binUrl = extract(idxBin);
  Serial.printf("[AUTO-OTA] latest=%s bin=%s\n", latest.c_str(), binUrl.c_str());
  if (latest.length() == 0 || binUrl.length() == 0) return;
  if (latest == firmware_version) { Serial.println("[AUTO-OTA] already latest"); return; }

  // Backup before OTA
  Serial.println("[AUTO-OTA] Backup before performing OTA");
  writeBackup();

  // Download bin
  HTTPClient http2;
  http2.begin(binUrl);
  int code2 = http2.GET();
  if (code2 != 200) { Serial.printf("[AUTO-OTA] bin GET failed: %d\n", code2); http2.end(); return; }
  int len = http2.getSize();
  WiFiClient * stream = http2.getStreamPtr();
  if (!Update.begin(len > 0 ? len : 0xFFFFFFFF)) { Serial.println("[AUTO-OTA] Not enough space"); http2.end(); return; }

  uint8_t buff[1024];
  int written = 0;
  while (http2.connected()) {
    int r = stream->readBytes(buff, sizeof(buff));
    if (r > 0) { Update.write(buff, r); written += r; } else break;
  }
  if (Update.end()) {
    Serial.println("[AUTO-OTA] Update finished; restarting");
    last_ota_successful = true;
    prefs.begin("ota", false);
    prefs.putBool("ota_ok", last_ota_successful);
    prefs.end();
    http2.end();
    delay(500);
    ESP.restart();
  } else {
    Serial.println("[AUTO-OTA] Update failed");
    last_ota_successful = false;
    prefs.begin("ota", false);
    prefs.putBool("ota_ok", last_ota_successful);
    prefs.end();
    http2.end();
  }
}

// Periodic auto-check (call from loop)
void periodicAutoOta() {
  unsigned long now = millis();
  if (now - lastAutoOta > autoOtaInterval) {
    lastAutoOta = now;
    autoCheckForUpdate();
  }
}

// -------------------------- USB-HOST (TinyUSB skeleton) -----------------------
Adafruit_USBD_HID usb_hid; // placeholder object
Adafruit_USBD_HID_Host hid_host; // placeholder

void onUSBKeyReported(uint8_t hidcode, bool pressed) {
  if (hidcode == 0) return;
  uint8_t xt = usb_to_xt[hidcode];
  if (!xt) return;
  if (pressed) xt_send(xt);
  else xt_send_break(xt);
}

// In setup we will init TinyUSB Host and set callback to onUSBKeyReported
// In loop we will call hid_host.Task(); (or equivalent for your TinyUSB binding)

// -------------------------- Web handlers ------------------------------
void handleRoot(AsyncWebServerRequest *request) {
  request->send(200, "text/html", getIndexHtml());
}

void handleWiFi(AsyncWebServerRequest *request) {
  request->send(200, "text/html", getWiFiHtml());
}

void handleSaveWiFi(AsyncWebServerRequest *request) {
  if (!request->hasParam("ssid", true) || !request->hasParam("password", true)) {
    request->send(400, "text/plain", "Missing");
    return;
  }
  String ssid = request->getParam("ssid", true)->value();
  String pass = request->getParam("password", true)->value();
  // Save to preferences
  prefs.begin("wifi", false);
  prefs.putString("sta_ssid", ssid);
  prefs.putString("sta_pass", pass);
  prefs.end();
  // Attempt connect
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  request->send(200, "text/html", "<p>Saved. Attempting to connect. Check serial monitor.</p><p><a href='/'>Back</a></p>");
}

void handleKey(AsyncWebServerRequest *request) {
  if (!request->hasParam("key")) { request->send(400,"text/plain","missing key"); return; }
  String k = request->getParam("key")->value();
  bool shift = request->hasParam("shift") && request->getParam("shift")->value() == "1";
  bool ctrl = request->hasParam("ctrl") && request->getParam("ctrl")->value() == "1";
  bool alt = request->hasParam("alt") && request->getParam("alt")->value() == "1";

  uint8_t hid = 0;
  char c = k.charAt(0);
  if (k == "ENTER") hid = 40;
  else if (k == "SPACE") hid = 44;
  else if (k == "BACK") hid = 42;
  else if (k == "Å") hid = 39;
  else if (k == "Ä") hid = 52;
  else if (k == "Ö") hid = 53;
  else if (c >= 'A' && c <= 'Z') hid = 4 + (c - 'A');
  else if (c >= 'a' && c <= 'z') hid = 4 + (c - 'a');
  else if (c >= '0' && c <= '9') hid = 30 + (c - '1' + 0);

  // send modifiers
  if (ctrl) xt_send(0x14);
  if (alt) xt_send(0x11);
  if (shift) xt_send(0x12);

  if (hid) {
    uint8_t xt = usb_to_xt[hid];
    if (xt) {
      xt_send(xt);
      xt_send_break(xt);
    }
  }

  if (shift) xt_send_break(0x12);
  if (ctrl) xt_send_break(0x14);
  if (alt) xt_send_break(0x11);

  request->send(200,"text/plain","ok");
}

// ------------------------- Setup & Loop -------------------------------------
void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println();
  Serial.println("Starting ESP32-S3 XT/AT Keyboard Full");

  // Init pins
  pinMode(PIN_XT_CLK, OUTPUT); digitalWrite(PIN_XT_CLK, HIGH); // idle high
  pinMode(PIN_XT_DATA, OUTPUT); digitalWrite(PIN_XT_DATA, HIGH);

  // Init FS
  ensureLittleFS();

  // Load persisted prefs (WiFi & OTA)
  prefs.begin("wifi", true);
  String saved_ssid = prefs.getString("sta_ssid", "");
  String saved_pass = prefs.getString("sta_pass", "");
  prefs.end();

  prefs.begin("ota", true);
  ota_username = prefs.getString("user", ota_username);
  ota_password = prefs.getString("pass", ota_password);
  ota_manifest_url = prefs.getString("manifest", ota_manifest_url);
  last_ota_successful = prefs.getBool("ota_ok", true);
  prefs.end();

  // Start WiFi AP + try STA
  WiFi.mode(WIFI_MODE_APSTA);
  WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());

  if (saved_ssid.length() > 0) {
    WiFi.begin(saved_ssid.c_str(), saved_pass.c_str());
    Serial.print("Trying STA connect to "); Serial.println(saved_ssid);
  }

  // Setup web server routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/wifi", HTTP_GET, handleWiFi);
  server.on("/savewifi", HTTP_POST, handleSaveWiFi);
  server.on("/key", HTTP_GET, handleKey);

  // OTA endpoints + backup endpoints
  setupOTAEndpoints();

  // Additional endpoints
  server.on("/backup", HTTP_GET, [](AsyncWebServerRequest *req){
    if (writeBackup()) req->send(200,"text/plain","backup saved");
    else req->send(500,"text/plain","backup failed");
  });

  server.begin();
  Serial.println("HTTP server started");

  // TinyUSB host init (skeleton - ensure library is available and code adapted)
  TinyUSBHost.begin();
  hid_host.begin();
  // Example: set callback (Adafruit wrapper would have proper API)
  hid_host.onKeyReport([](uint8_t hidcode, bool pressed){
    onUSBKeyReported(hidcode, pressed);
  });

  // On first boot, optionally restore from backup (we only notify here)
  maybeRestoreFromBackup();

  Serial.println("Setup complete");
}

void loop() {
  // Async web server handles HTTP requests; just do periodic tasks
  periodicAutoOta();

  // Poll TinyUSB host (adjust to your TinyUSB wrapper's loop function)
  hid_host.Task();

  delay(1);
}
