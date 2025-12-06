/* main.ino
   Komplett startfil för Waveshare ESP32-S3-Zero
   Initerar alla moduler: WiFi, webserver (Async), WebSocket realtime,
   OTA (AsyncElegantOTA), keymap manager, USB-host, XT/AT output, detection, rollback.

   Anpassa PINNAR nedan vid behov.
*/

#include <Arduino.h>

// Files / modules generated earlier in convo
#include "config.h"
#include "wifi_manager.h"
#include "webui.h"
#include "api.h"
#include "ota.h"
#include "usb_host.h"
#include "xt_at_output.h"
#include "keymap_manager.h"
#include "keymap_ex.h"
#include "realtime_ws.h"
#include "detect_protocol.h"
#include "rollback.h"

// Async Web server
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Filesystems
#include <LittleFS.h>
#include <SPIFFS.h>

// Default pins (ändra här om din hårdvara kräver andra pinnar)
#ifndef XT_CLK_PIN_DEFAULT
  #define XT_CLK_PIN_DEFAULT 10
#endif
#ifndef XT_DATA_PIN_DEFAULT
  #define XT_DATA_PIN_DEFAULT 11
#endif
#ifndef XT_BIT_DELAY_US_DEFAULT
  #define XT_BIT_DELAY_US_DEFAULT 40  // öka om värddator behöver långsammare clock
#endif

// Server
AsyncWebServer server(80);

// OBS: Om du använder TinyUSB / USB-host krävs att usb_host.cpp använder rätt bibliotek.
// Vi antar att usbHostBegin() finns och fungerar för din board.

void setup() {
  // Serial för debug
  Serial.begin(115200);
  delay(100);

  Serial.println();
  Serial.println("=== ESP32-S3 XT/AT Keyboard Adapter - starting ===");

  // Mount filesystems used by web-ui/keymap/ota
  // Vi försöker mounta både SPIFFS och LittleFS (vissa setups använder bara en).
  if (!SPIFFS.begin(true)) {
    Serial.println("[FS] SPIFFS mount failed (continuing if LittleFS available)");
  } else {
    Serial.println("[FS] SPIFFS mounted");
  }

  if (!LittleFS.begin(true)) {
    Serial.println("[FS] LittleFS mount failed (continuing)");
  } else {
    Serial.println("[FS] LittleFS mounted");
  }

  // Load config from NVS (Preferences)
  configLoad();
  Serial.println("[CONFIG] Loaded configuration");

  // Initialize rollback subsystem first (so we can act on first boot after OTA)
  rollbackInitialize();

  // WiFi + captive portal endpoints (server passed so captive endpoints registered)
  wifiStart(&server);
  Serial.println("[WIFI] Started AP + STA (if configured)");

  // Init realtime websocket (needs server)
  realtimeInit(server);
  Serial.println("[WS] Realtime websocket initialized");

  // Register API endpoints (apiInit will register /api/* endpoints)
  apiInit(server);
  Serial.println("[API] REST endpoints initialized");

  // OTA (AsyncElegantOTA) and OTA UI endpoints
  setupOTA(server);
  Serial.println("[OTA] OTA endpoints initialized");

  // Keymap systems
  keymapInit(&server);        // legacy/simple keymap endpoints
  keymapExInit(&server);      // advanced keymap endpoints
  Serial.println("[KEYMAP] Keymap systems initialized");

  // Init USB host (TinyUSB / Adafruit_Host wrapper)
  usbHostBegin();
  Serial.println("[USB] USB-Host initialized - waiting for device");

  // XT/AT output init (pins and bit-delay)
  xtatBegin(XT_CLK_PIN_DEFAULT, XT_DATA_PIN_DEFAULT, XT_BIT_DELAY_US_DEFAULT);
  Serial.println("[XTAT] XT/AT output initialized");

  // Optionally enable debug modes (ändra efter behov)
  // Sätt true under debug/testning, false i produktion.
  extern bool xtat_debug_enabled;
  extern bool xtat_timestamp_enabled;
  extern bool xtat_bitdump_enabled;
  extern bool xtat_hostecho_enabled;

  xtat_debug_enabled     = true;   // JSON debug to Serial
  xtat_timestamp_enabled = true;   // include micros() timestamps
  xtat_bitdump_enabled   = false;  // if true, prints bit patterns to Serial
  xtat_hostecho_enabled  = true;   // enables host->device readback (needed for detection)

  Serial.println("[DEBUG] xtat debug/timestamp/hostecho flags set");

  // Start server (if not started by other module)
  server.begin();
  Serial.println("[HTTP] AsyncWebServer started on port 80");

  // Optionally print IP info after a short wait so WiFi has time to connect
  delay(500);
  Serial.printf("[NETWORK] IP (STA or AP): %s\n", wifiIP().c_str());

  // If first boot after OTA, rollback module will have flagged and may restore
  if (isFirstBootAfterOTA()) {
    Serial.println("[ROLLBACK] First boot after OTA detected (rollback module active)");
    // If required, you can inspect backup via /download-backup
  }

  Serial.println("=== setup() complete ===");
}

void loop() {
  // Periodics: do not block long here
  wifiHandlePeriodic();   // manages reconnect/captive DNS
  otaPeriodic();          // checks auto-OTA if enabled
  usbHostTask();          // process USB host/hid events
  xtatTask();             // process outgoing XT/AT queue & host-echo reads
  rollbackPeriodic();     // check and run any pending restore
  // small delay to let RTOS breathe; avoid large delays here
  delay(1);
}

// ----------------- Helper endpoints for easy manual test (optional) --------------
// If you want to provide a simple serial command or shell style interface you can expand here.

