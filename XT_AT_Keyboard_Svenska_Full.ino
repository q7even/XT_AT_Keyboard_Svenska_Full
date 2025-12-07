/* main.ino
   Komplett startfil för Waveshare ESP32-S3-Zero
*/
#include <Arduino.h>

#include "config.h"
#include "wifi_manager.h"
#include "api.h"
#include "ota.h"
#include "usb_host.h"
#include "xt_at_output.h"
#include "keymap_manager.h"
#include "keymap_ex.h"
#include "realtime_ws.h"
#include "detect_protocol.h"
#include "rollback.h"

#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <SPIFFS.h>
#include "ota.h"

void setup() {
  startOTA("DittSSID", "DittLösenord");
}

#ifndef XT_CLK_PIN_DEFAULT
  #define XT_CLK_PIN_DEFAULT 10
#endif
#ifndef XT_DATA_PIN_DEFAULT
  #define XT_DATA_PIN_DEFAULT 11
#endif
#ifndef XT_BIT_DELAY_US_DEFAULT
  #define XT_BIT_DELAY_US_DEFAULT 40
#endif

AsyncWebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println("=== ESP32-S3 XT/AT Keyboard Adapter - starting ===");

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

  configLoad();
  Serial.println("[CONFIG] Loaded configuration");

  rollbackInitialize();

  wifiStart(&server);
  Serial.println("[WIFI] Started AP + STA (if configured)");

  realtimeInit(server);
  Serial.println("[WS] Realtime websocket initialized");

  apiInit(server);
  Serial.println("[API] REST endpoints initialized");

  setupOTA(server);
  Serial.println("[OTA] OTA endpoints initialized");

  keymapInit(&server);
  keymapExInit(&server);
  Serial.println("[KEYMAP] Keymap systems initialized");

  usbHostBegin();
  Serial.println("[USB] USB-Host initialized - waiting for device");

  xtatBegin(XT_CLK_PIN_DEFAULT, XT_DATA_PIN_DEFAULT, XT_BIT_DELAY_US_DEFAULT);
  Serial.println("[XTAT] XT/AT output initialized");

  extern bool xtat_debug_enabled;
  extern bool xtat_timestamp_enabled;
  extern bool xtat_bitdump_enabled;
  extern bool xtat_hostecho_enabled;

  xtat_debug_enabled     = true;
  xtat_timestamp_enabled = true;
  xtat_bitdump_enabled   = false;
  xtat_hostecho_enabled  = true;

  setupOTA(server);
  Serial.println("[OTA] OTA endpoints initialized");

  server.begin();
  delay(500);
  Serial.printf("[NETWORK] IP (STA or AP): %s\n", wifiIP().c_str());

  if (isFirstBootAfterOTA()) {
    Serial.println("[ROLLBACK] First boot after OTA detected (rollback module active)");
  }

  Serial.println("=== setup() complete ===");
}

void loop() {
  wifiHandlePeriodic();
  otaPeriodic();
  usbHostTask();
  xtatTask();
  rollbackPeriodic();
  delay(1);
  otaPeriodic();
}
