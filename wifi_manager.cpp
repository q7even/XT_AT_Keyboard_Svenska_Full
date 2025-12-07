#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <LittleFS.h>

static DNSServer dnsServer;
static bool captiveEnabled = false;
static IPAddress apIP(192,168,4,1);
static unsigned long lastConnectAttempt = 0;
static unsigned long connectBackoff = 5000;
static const unsigned long MAX_BACKOFF = 5 * 60 * 1000UL;
static AsyncWebServer *gServer = nullptr;

static String qrForAP() {
  String url = "http://";
  url += wifiIP();
  String q = "https://chart.googleapis.com/chart?cht=qr&chs=220x220&chl=" + url;
  return q;
}

static void startCaptiveDNS() {
  dnsServer.start(53, "*", apIP);
  captiveEnabled = true;
  Serial.println("[WIFI] Captive DNS started (all domains -> AP IP)");
}

static void stopCaptiveDNS() {
  dnsServer.stop();
  captiveEnabled = false;
  Serial.println("[WIFI] Captive DNS stopped");
}

static void registerCaptiveEndpoints(AsyncWebServer *server) {
  if (!server) return;
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *req){
    String html = "<!doctype html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Connect</title></head><body>";
    html += "<h3>ESP32 - Wifi Setup</h3>";
    html += "<p>Connect to your router by entering credentials below.</p>";
    html += "<form method='POST' action='/wifi_set'><label>SSID</label><input name='ssid'><br><label>Password</label><input name='pass' type='password'><br><button type='submit'>Connect</button></form>";
    html += "<p>Scan networks button: <a href='/scan'>Scan WiFi</a></p>";
    html += "<p>QR: <img src='" + qrForAP() + "' alt='qr'></p>";
    html += "</body></html>";
    req->send(200, "text/html", html);
  });

  server->on("/wifi_set", HTTP_POST, [](AsyncWebServerRequest *req){
    if (!req->hasParam("ssid", true) || !req->hasParam("pass", true)) { req->send(400, "text/plain", "Missing params"); return; }
    String ssid = req->getParam("ssid", true)->value();
    String pass = req->getParam("pass", true)->value();
    config.sta_ssid = ssid;
    config.sta_pass = pass;
    configSave();
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    String r = "<html><body>Saved. Attempting to connect to " + ssid + ".<br>Return to this page after a minute or check device status.</body></html>";
    req->send(200, "text/html", r);
  });

  server->on("/scan", HTTP_GET, [](AsyncWebServerRequest *req){
    int n = WiFi.scanNetworks();
    String s = "<!doctype html><html><body><h3>Networks</h3><ul>";
    for (int i = 0; i < n; ++i) {
      s += "<li>" + String(WiFi.SSID(i)) + " (" + String(WiFi.RSSI(i)) + " dBm) - ";
      s += (WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "OPEN" : "SEC") + String("</li>");
    }
    s += "</ul></body></html>";
    req->send(200, "text/html", s);
  });

  server->on("/wifi_info", HTTP_GET, [](AsyncWebServerRequest *req){
    String j = "{";
    j += ""ap_ip":"" + wifiIP() + "",";
    j += ""qr":"" + qrForAP() + """;
    j += "}";
    req->send(200, "application/json", j);
  });

  Serial.println("[WIFI] Captive endpoints registered");
}

void startAP() {
  const char* ap_ssid = config.ap_ssid.length() ? config.ap_ssid.c_str() : "ESP32-Keyboard";
  const char* ap_pass = config.ap_pass.length() ? config.ap_pass.c_str() : "12345678";
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255,255,255,0));
  bool ok = WiFi.softAP(ap_ssid, ap_pass);
  Serial.printf("[WIFI] SoftAP started SSID='%s' ok=%d\n", ap_ssid, ok);
  if (gServer) startCaptiveDNS();
}

void startSTA(const String &ssid, const String &pass) {
  config.sta_ssid = ssid;
  config.sta_pass = pass;
  configSave();
  WiFi.mode(WIFI_MODE_APSTA);
  Serial.printf("[WIFI] Starting STA -> SSID='%s'\n", ssid.c_str());
  WiFi.begin(ssid.c_str(), pass.c_str());
  lastConnectAttempt = millis();
  connectBackoff = 5000;
}

String wifiIP() {
  if (WiFi.status() == WL_CONNECTED) return WiFi.localIP().toString();
  return apIP.toString();
}

String scanNetworksJSON() {
  int n = WiFi.scanNetworks();
  String j = "[";
  for (int i = 0; i < n; ++i) {
    j += "{";
    j += ""ssid":"" + WiFi.SSID(i) + "",";
    j += ""rssi":" + String(WiFi.RSSI(i)) + ",";
    j += ""auth":"" + String(WiFi.encryptionType(i)) + """;
    j += "}";
    if (i < n-1) j += ",";
  }
  j += "]";
  return j;
}

void captiveRedirect(AsyncWebServerRequest *request) {
  if (!captiveEnabled) return;
  String host = request->host();
  if (!host.equalsIgnoreCase(wifiIP())) {
    String url = "http://" + wifiIP();
    request->redirect(url);
  }
}

void wifiHandlePeriodic() {
  if (captiveEnabled) dnsServer.processNextRequest();
  if (config.sta_ssid.length() > 0) {
    if (WiFi.status() != WL_CONNECTED) {
      unsigned long now = millis();
      if (now - lastConnectAttempt > connectBackoff) {
        Serial.println("[WIFI] Attempting STA reconnect...");
        WiFi.begin(config.sta_ssid.c_str(), config.sta_pass.c_str());
        lastConnectAttempt = now;
        connectBackoff = min(connectBackoff * 2, MAX_BACKOFF);
      }
    } else {
      if (captiveEnabled) {
        stopCaptiveDNS();
      }
    }
  }
}

void wifiStart(AsyncWebServer* server) {
  gServer = server;
  if (!LittleFS.begin(true)) {
    Serial.println("[WIFI] LittleFS mount failed (continuing)");
  }
  startAP();
  if (config.sta_ssid.length() > 0) {
    Serial.printf("[WIFI] Saved STA found, attempting connect to '%s'\n", config.sta_ssid.c_str());
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.begin(config.sta_ssid.c_str(), config.sta_pass.c_str());
    lastConnectAttempt = millis();
  }
  if (server) registerCaptiveEndpoints(server);
}
